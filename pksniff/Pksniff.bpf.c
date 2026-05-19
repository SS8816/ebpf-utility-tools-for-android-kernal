// SPDX-License-Identifier: GPL-2.0
/*
 * pksniff.bpf.c — extended packet capture BPF program
 *
 * Attaches as TC classifier on ingress AND egress.
 * Captures: Ethernet, IPv4, TCP, UDP, ICMP, ARP
 * Parses app-layer hints for: DNS, HTTP/1.x, TLS (SNI)
 *
 * Physical-device note:
 *   Compiled against the bundled vmlinux.h (arm64).  The userspace loader
 *   supplies a custom BTF path so this works on kernels without
 *   CONFIG_DEBUG_INFO_BTF (common on physical Android devices).
 */

#include <linux/types.h>
#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

/* ── re-declare constants (vmlinux.h may not expose them all) ─────────────── */
#define TC_ACT_OK       0
#define ETH_P_IP        0x0800
#define ETH_P_ARP       0x0806
#define ETH_P_IPV6      0x86DD
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_ICMPv6  58

#define MAX_CAPTURE_BYTES 256
#define MAX_DNS_NAME       64
#define MAX_HTTP_INFO      48
#define MAX_TLS_SNI        64

#define DIR_INGRESS 0
#define DIR_EGRESS  1

#define APP_NONE  0
#define APP_DNS   1
#define APP_HTTP  2
#define APP_TLS   3
#define APP_ICMP  4

#define TF_FIN  0x01
#define TF_SYN  0x02
#define TF_RST  0x04
#define TF_PSH  0x08
#define TF_ACK  0x10
#define TF_URG  0x20

/* ── event (mirror of pksniff.h — keep in sync) ──────────────────────────── */
struct event {
    __u64 ktime_ns;

    __u32 src_ip;
    __u32 dst_ip;
    __u16 ip_tot_len;
    __u8  ttl;
    __u8  ip_proto;
    __u8  ip_flags;

    __u16 src_port;
    __u16 dst_port;
    __u8  tcp_flags;
    __u16 l4_off;
    __u16 payload_off;
    __u16 payload_len;

    __u32 packet_len;
    __u32 cap_len;

    __u8  direction;
    __u8  app_tag;
    __u8  reserved[2];

    char  dns_name[MAX_DNS_NAME];
    char  http_info[MAX_HTTP_INFO];
    char  tls_sni[MAX_TLS_SNI];

    __u8  packet[MAX_CAPTURE_BYTES];
};

/* ── ring buffer map ─────────────────────────────────────────────────────── */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 25);   /* 32 MB — bigger for richer events      */
} rb SEC(".maps");

/* ── per-CPU scratch (avoids large stack frames that upset the verifier) ─── */
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct event);
} scratch_map SEC(".maps");

/* ═══════════════════════════════════════════════════════════════════════════
 * App-layer parsers (called from BPF — must be inlined, no loops > verifier
 * bounds, all pointer arithmetic checked before use)
 * ═════════════════════════════════════════════════════════════════════════ */

/*
 * parse_dns_name — decode the first label chain from a DNS message.
 * out[] receives a dot-joined name, NUL-terminated, truncated to out_max-1.
 */
static __always_inline void
parse_dns_name(struct __sk_buff *skb, __u32 off, __u32 pkt_end,
               char *out, __u32 out_max)
{
    __u32 pos = off + 12;   /* skip DNS header (12 bytes) */
    __u32 out_pos = 0;
    int jumps = 0;

    /* 8 labels max — keeps the verifier happy without full unrolling */
#pragma unroll
    for (int label = 0; label < 8; label++) {
        if (pos + 1 > pkt_end || out_pos >= out_max - 1)
            break;

        __u8 len = 0;
        if (bpf_skb_load_bytes(skb, pos, &len, 1) < 0)
            break;

        if (len == 0)
            break;                       /* root label — done                */

        if ((len & 0xC0) == 0xC0) {     /* pointer — stop parsing           */
            break;
        }

        pos++;                           /* skip length byte                 */

        if (label > 0 && out_pos < out_max - 1)
            out[out_pos++] = '.';

        if (pos + len > pkt_end)
            break;

        /* Load one byte at a time — avoids memcpy in BPF context */
        /* simplified DNS label copy */
        /* ultra-light DNS extraction */
__u8 ch = 0;

if (len > 0) {
    if (bpf_skb_load_bytes(skb, pos, &ch, 1) == 0)
        out[out_pos++] = (char)ch;
}

if (len > 1 && out_pos < out_max - 1) {
    if (bpf_skb_load_bytes(skb, pos + 1, &ch, 1) == 0)
        out[out_pos++] = (char)ch;
}

if (len > 2 && out_pos < out_max - 1) {
    if (bpf_skb_load_bytes(skb, pos + 2, &ch, 1) == 0)
        out[out_pos++] = (char)ch;
}

if (len > 3 && out_pos < out_max - 1) {
    if (bpf_skb_load_bytes(skb, pos + 3, &ch, 1) == 0)
        out[out_pos++] = (char)ch;
}

        pos += len;
    }

    if (out_pos < out_max)
        out[out_pos] = '\0';
}

/*
 * parse_http_info — grab the first 47 bytes of an HTTP/1.x request or
 * response line into out[].
 */
static __always_inline void
parse_http_info(struct __sk_buff *skb, __u32 payload_off, __u32 pkt_end,
                char *out, __u32 out_max)
{
    __u32 copy = out_max - 1;
    if (payload_off + copy > pkt_end)
        copy = pkt_end - payload_off;
    if (copy == 0 || copy > out_max - 1)
        return;

    char tmp[MAX_HTTP_INFO] = {};
    if (bpf_skb_load_bytes(skb, payload_off, tmp, MAX_HTTP_INFO - 1) < 0)
        return;

#pragma unroll
    for (int i = 0; i < MAX_HTTP_INFO - 1; i++) {
        char c = tmp[i];
        /* replace CR/LF with space so the string stays single-line          */
        if (c == '\r' || c == '\n') c = ' ';
        out[i] = c;
    }
    out[MAX_HTTP_INFO - 1] = '\0';
}

/*
 * is_http — quick heuristic: payload starts with an HTTP verb or "HTTP/"
 */
static __always_inline int
is_http(struct __sk_buff *skb, __u32 payload_off, __u32 pkt_end)
{
    if (payload_off + 8 > pkt_end)
        return 0;

    char hdr[8] = {};
    if (bpf_skb_load_bytes(skb, payload_off, hdr, 8) < 0)
        return 0;

    /* common HTTP/1.x verbs and response prefix */
    if (hdr[0]=='G' && hdr[1]=='E' && hdr[2]=='T' && hdr[3]==' ') return 1;
    if (hdr[0]=='P' && hdr[1]=='O' && hdr[2]=='S' && hdr[3]=='T') return 1;
    if (hdr[0]=='H' && hdr[1]=='E' && hdr[2]=='A' && hdr[3]=='D') return 1;
    if (hdr[0]=='P' && hdr[1]=='U' && hdr[2]=='T' && hdr[3]==' ') return 1;
    if (hdr[0]=='D' && hdr[1]=='E' && hdr[2]=='L') return 1;
    if (hdr[0]=='H' && hdr[1]=='T' && hdr[2]=='T' && hdr[3]=='P') return 1;
    return 0;
}

/*
 * parse_tls_sni — scan a TLS ClientHello for the SNI extension.
 * Enough of the handshake is in the first 256 bytes for short hostnames.
 */
static __always_inline void
parse_tls_sni(struct __sk_buff *skb, __u32 payload_off, __u32 pkt_end,
              char *out, __u32 out_max)
{
    /*
     * TLS record layout:
     *   [0]   content_type  (0x16 = handshake)
     *   [1-2] version
     *   [3-4] record length
     *   [5]   handshake type (0x01 = ClientHello)
     *   [6-8] handshake length (3 bytes)
     *   [9-10] client_version
     *   [11-42] random (32 bytes)
     *   [43]  session_id length
     */
    if (payload_off + 5 > pkt_end)
        return;

    __u8 rec[6] = {};
    if (bpf_skb_load_bytes(skb, payload_off, rec, 6) < 0)
        return;

    if (rec[0] != 0x16 || rec[5] != 0x01)  /* not a ClientHello handshake  */
        return;

    /* position past: record hdr(5) + hs hdr(4) + version(2) + random(32) */
    __u32 pos = payload_off + 5 + 4 + 2 + 32;

    if (pos + 1 > pkt_end) return;
    __u8 sid_len = 0;
    if (bpf_skb_load_bytes(skb, pos, &sid_len, 1) < 0) return;
    pos += 1 + sid_len;

    /* cipher suites length (2 bytes) */
    if (pos + 2 > pkt_end) return;
    __u8 cs[2] = {};
    if (bpf_skb_load_bytes(skb, pos, cs, 2) < 0) return;
    __u16 cs_len = ((__u16)cs[0] << 8) | cs[1];
    pos += 2 + cs_len;

    /* compression methods length (1 byte) */
    if (pos + 1 > pkt_end) return;
    __u8 cm_len = 0;
    if (bpf_skb_load_bytes(skb, pos, &cm_len, 1) < 0) return;
    pos += 1 + cm_len;

    /* extensions total length (2 bytes) */
    if (pos + 2 > pkt_end) return;
    __u8 ext_tot[2] = {};
    if (bpf_skb_load_bytes(skb, pos, ext_tot, 2) < 0) return;
    pos += 2;
    __u32 ext_end = pos + (((__u32)ext_tot[0] << 8) | ext_tot[1]);
    if (ext_end > pkt_end) ext_end = pkt_end;

    /* walk extensions — max 16 iterations to satisfy verifier */
#pragma unroll
    for (int ext = 0; ext < 16; ext++) {
        if (pos + 4 > ext_end) break;

        __u8 etype[4] = {};
        if (bpf_skb_load_bytes(skb, pos, etype, 4) < 0) break;

        __u16 ext_type = ((__u16)etype[0] << 8) | etype[1];
        __u16 ext_len  = ((__u16)etype[2] << 8) | etype[3];
        pos += 4;

        if (ext_type == 0x0000) {   /* SNI extension type = 0              */
            /*
             * SNI extension data:
             *  [0-1] server_name_list_length
             *  [2]   name_type (0 = host_name)
             *  [3-4] name_length
             *  [5..] hostname bytes
             */
            if (pos + 5 > ext_end) break;
            __u8 sni_hdr[5] = {};
            if (bpf_skb_load_bytes(skb, pos, sni_hdr, 5) < 0) break;

            __u16 name_len = ((__u16)sni_hdr[3] << 8) | sni_hdr[4];
if (name_len == 0)
    break;

if (name_len >= out_max)
    name_len = out_max - 1;

if (pos + 5 + name_len > ext_end)
    break;

/* copy hostname using a fixed-size read */
/* verifier-friendly SNI copy */
__u8 ch = 0;

#pragma unroll
for (int c = 0; c < 16; c++) {
    if ((__u16)c >= name_len)
        break;

    if (bpf_skb_load_bytes(skb, pos + 5 + c, &ch, 1) < 0)
        break;

    out[c] = ch;
}

out[16 - 1] = '\0';
            break;
        }

        pos += ext_len;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Common packet processing core — shared by ingress and egress programs
 * ═════════════════════════════════════════════════════════════════════════ */
static __always_inline int
process_packet(struct __sk_buff *skb, __u8 direction)
{
    void *data     = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;

    /* ── Ethernet header ─────────────────────────────────────────────────  */
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return TC_ACT_OK;

    __u16 eth_proto = bpf_ntohs(eth->h_proto);

    /* allocate event from scratch_map to avoid large BPF stack frame */
    __u32 key = 0;
    struct event *e = bpf_map_lookup_elem(&scratch_map, &key);
    if (!e)
        return TC_ACT_OK;

    /* zero out app-layer string fields */
    e->dns_name[0]  = '\0';
    e->http_info[0] = '\0';
    e->tls_sni[0]   = '\0';
    e->tcp_flags    = 0;
    e->app_tag      = APP_NONE;
    e->direction    = direction;
    e->ktime_ns     = bpf_ktime_get_ns();

    /* ── ARP — just capture, no further parsing ──────────────────────────  */
    if (eth_proto == ETH_P_ARP) {
        e->ip_proto   = 0;
        e->src_ip     = 0;
        e->dst_ip     = 0;
        e->src_port   = 0;
        e->dst_port   = 0;
        e->ttl        = 0;
        e->ip_flags   = 0;
        e->ip_tot_len = 0;
        e->l4_off     = sizeof(struct ethhdr);
        e->payload_off = sizeof(struct ethhdr);
        e->payload_len = 0;
        e->packet_len  = skb->len;
        e->cap_len     = skb->len < MAX_CAPTURE_BYTES ? skb->len : MAX_CAPTURE_BYTES;
        goto capture_bytes;
    }

    if (eth_proto != ETH_P_IP)
        return TC_ACT_OK;   /* skip IPv6 for now — keeps code within limits */

    /* ── IPv4 header ─────────────────────────────────────────────────────  */
    struct iphdr *ip = (void *)(eth + 1);
    if ((void *)(ip + 1) > data_end)
        return TC_ACT_OK;

    __u16 ip_hdr_len = ip->ihl * 4;
    if (ip_hdr_len < sizeof(*ip))
        return TC_ACT_OK;
    if ((void *)ip + ip_hdr_len > data_end)
        return TC_ACT_OK;

    __u16 l4_off = sizeof(struct ethhdr) + ip_hdr_len;
    __u16 payload_off = l4_off;

    __u16 src_port = 0, dst_port = 0;
    __u8 tcp_flags = 0;

    /* ── L4 protocols ────────────────────────────────────────────────────  */
    if (ip->protocol == IPPROTO_TCP) {
        struct tcphdr *tcp = (void *)ip + ip_hdr_len;
        if ((void *)(tcp + 1) > data_end)
            return TC_ACT_OK;

        __u16 tcp_hdr_len = tcp->doff * 4;
        if (tcp_hdr_len < sizeof(*tcp))
            tcp_hdr_len = sizeof(*tcp);
        if ((void *)tcp + tcp_hdr_len > data_end)
            return TC_ACT_OK;

        src_port = bpf_ntohs(tcp->source);
        dst_port = bpf_ntohs(tcp->dest);
        payload_off = l4_off + tcp_hdr_len;

        /* pack TCP flags into one byte */
        __u8 raw = 0;
        if (tcp->fin) raw |= TF_FIN;
        if (tcp->syn) raw |= TF_SYN;
        if (tcp->rst) raw |= TF_RST;
        if (tcp->psh) raw |= TF_PSH;
        if (tcp->ack) raw |= TF_ACK;
        if (tcp->urg) raw |= TF_URG;
        tcp_flags = raw;

    } else if (ip->protocol == IPPROTO_UDP) {
        struct udphdr *udp = (void *)ip + ip_hdr_len;
        if ((void *)(udp + 1) > data_end)
            return TC_ACT_OK;

        src_port = bpf_ntohs(udp->source);
        dst_port = bpf_ntohs(udp->dest);
        payload_off = l4_off + sizeof(struct udphdr);

    } else if (ip->protocol == IPPROTO_ICMP) {
        /* ICMP — no ports, app tag set below */
    }

    /* ── populate IP fields ──────────────────────────────────────────────  */
    e->src_ip     = ip->saddr;
    e->dst_ip     = ip->daddr;
    e->ttl        = ip->ttl;
    e->ip_proto   = ip->protocol;
    e->ip_tot_len = bpf_ntohs(ip->tot_len);
    e->ip_flags   = (bpf_ntohs(ip->frag_off) >> 12) & 0x07; /* DF/MF bits */
    e->src_port   = src_port;
    e->dst_port   = dst_port;
    e->tcp_flags  = tcp_flags;
    e->l4_off     = l4_off;
    e->payload_off = payload_off;
    e->packet_len  = skb->len;
    e->cap_len     = skb->len < MAX_CAPTURE_BYTES ? skb->len : MAX_CAPTURE_BYTES;
    e->payload_len = (skb->len > payload_off) ? (skb->len - payload_off) : 0;

    /* ── app-layer hinting ───────────────────────────────────────────────  */
    if (ip->protocol == IPPROTO_ICMP) {
        e->app_tag = APP_ICMP;

    } else if (ip->protocol == IPPROTO_UDP) {
        if (src_port == 53 || dst_port == 53) {
            e->app_tag = APP_DNS;
            parse_dns_name(skb, payload_off, skb->len, e->dns_name, MAX_DNS_NAME);
        }

    } else if (ip->protocol == IPPROTO_TCP) {
        if (src_port == 443 || dst_port == 443) {
            e->app_tag = APP_TLS;
            /* only parse ClientHello on the outbound side */
            if (direction == DIR_EGRESS || dst_port == 443)
                parse_tls_sni(skb, payload_off, skb->len, e->tls_sni, MAX_TLS_SNI);
        } else if (src_port == 80 || dst_port == 80 ||
                   src_port == 8080 || dst_port == 8080) {
            if (is_http(skb, payload_off, skb->len)) {
                e->app_tag = APP_HTTP;
                parse_http_info(skb, payload_off, skb->len,
                                e->http_info, MAX_HTTP_INFO);
            }
        } else {
            /* opportunistic HTTP detection on any port */
            if (is_http(skb, payload_off, skb->len)) {
                e->app_tag = APP_HTTP;
                parse_http_info(skb, payload_off, skb->len,
                                e->http_info, MAX_HTTP_INFO);
            }
        }
    }

capture_bytes: {
    /* reserve ring-buffer slot and copy scratch → ring-buf event */
    struct event *re = bpf_ringbuf_reserve(&rb, sizeof(*re), 0);
    if (!re)
        return TC_ACT_OK;

    /* copy fixed fields */
    re->ktime_ns   = e->ktime_ns;
    re->src_ip     = e->src_ip;
    re->dst_ip     = e->dst_ip;
    re->ip_tot_len = e->ip_tot_len;
    re->ttl        = e->ttl;
    re->ip_proto   = e->ip_proto;
    re->ip_flags   = e->ip_flags;
    re->src_port   = e->src_port;
    re->dst_port   = e->dst_port;
    re->tcp_flags  = e->tcp_flags;
    re->l4_off     = e->l4_off;
    re->payload_off = e->payload_off;
    re->payload_len = e->payload_len;
    re->packet_len  = e->packet_len;
    re->cap_len     = e->cap_len;
    re->direction   = e->direction;
    re->app_tag     = e->app_tag;
    re->reserved[0] = 0;
    re->reserved[1] = 0;

    /* copy string fields */
#pragma unroll
    for (int i = 0; i < MAX_DNS_NAME;   i++) re->dns_name[i]  = e->dns_name[i];
#pragma unroll
    for (int i = 0; i < MAX_HTTP_INFO;  i++) re->http_info[i] = e->http_info[i];
#pragma unroll
    for (int i = 0; i < MAX_TLS_SNI;    i++) re->tls_sni[i]   = e->tls_sni[i];

    /* capture raw bytes in verifier-friendly 16-byte chunks */
__u32 cap_len = skb->len;
if (cap_len > MAX_CAPTURE_BYTES)
    cap_len = MAX_CAPTURE_BYTES;

/* round down to a 16-byte boundary */
cap_len &= ~0xF;
re->cap_len = cap_len;

#pragma unroll
for (int off = 0; off < MAX_CAPTURE_BYTES; off += 16) {
    if ((__u32)off >= cap_len)
        break;

    __u8 tmp[16] = {};
    if (bpf_skb_load_bytes(skb, off, tmp, 16) < 0) {
        bpf_ringbuf_discard(re, 0);
        return TC_ACT_OK;
    }

    __builtin_memcpy(re->packet + off, tmp, 16);
}

    bpf_ringbuf_submit(re, 0);
    }

    return TC_ACT_OK;
}

/* ── TC ingress entry point ──────────────────────────────────────────────── */
SEC("tc")
int pksniff_ingress(struct __sk_buff *skb)
{
    return process_packet(skb, DIR_INGRESS);
}

/* ── TC egress entry point ───────────────────────────────────────────────── */
SEC("tc")
int pksniff_egress(struct __sk_buff *skb)
{
    return process_packet(skb, DIR_EGRESS);
}

char LICENSE[] SEC("license") = "GPL";