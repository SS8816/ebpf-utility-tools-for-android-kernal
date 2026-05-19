#ifndef PKSNIFF_H
#define PKSNIFF_H

#include <stdint.h>

/* ── capture limits ──────────────────────────────────────────────────────── */
#define MAX_CAPTURE_BYTES  256
#define MAX_DNS_NAME       64
#define MAX_HTTP_INFO      48   /* "GET /path" or "HTTP/1.1 200" prefix       */
#define MAX_TLS_SNI        64

/* ── protocol IDs ────────────────────────────────────────────────────────── */
#define PROTO_OTHER   0
#define PROTO_TCP     6
#define PROTO_UDP     17
#define PROTO_ICMP    1
#define PROTO_ICMPv6  58

/* ── app-layer hint tags ─────────────────────────────────────────────────── */
#define APP_NONE      0
#define APP_DNS       1
#define APP_HTTP      2
#define APP_TLS       3
#define APP_ICMP      4

/* ── TCP flag bits (stored in tcp_flags field) ───────────────────────────── */
#define TF_FIN  0x01
#define TF_SYN  0x02
#define TF_RST  0x04
#define TF_PSH  0x08
#define TF_ACK  0x10
#define TF_URG  0x20

/* ── direction ───────────────────────────────────────────────────────────── */
#define DIR_INGRESS 0
#define DIR_EGRESS  1

/*
 * The event emitted to userspace for every captured packet.
 * Fields are ordered to avoid padding holes; total size ≤ 512 B.
 */
struct event {
    /* timing */
    uint64_t ktime_ns;          /* bpf_ktime_get_ns()                        */

    /* IP layer */
    uint32_t src_ip;            /* IPv4 network-byte-order                   */
    uint32_t dst_ip;
    uint16_t ip_tot_len;        /* total length from IP header               */
    uint8_t  ttl;
    uint8_t  ip_proto;          /* IPPROTO_*                                 */
    uint8_t  ip_flags;          /* DF / MF bits from IP header               */

    /* L4 */
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  tcp_flags;         /* TF_* bitmask; 0 for non-TCP               */
    uint16_t l4_off;            /* byte offset of L4 header inside frame     */
    uint16_t payload_off;       /* byte offset of app payload inside frame   */
    uint16_t payload_len;       /* full payload length (may exceed cap_len)  */

    /* raw capture */
    uint32_t packet_len;        /* original on-wire length                   */
    uint32_t cap_len;           /* bytes actually captured (≤ MAX_CAPTURE_BYTES) */

    /* meta */
    uint8_t  direction;         /* DIR_INGRESS / DIR_EGRESS                  */
    uint8_t  app_tag;           /* APP_* hint                                */
    uint8_t  reserved[2];

    /* app-layer snippets (NUL-terminated strings) */
    char     dns_name[MAX_DNS_NAME];    /* first DNS query label chain       */
    char     http_info[MAX_HTTP_INFO];  /* request line or status prefix     */
    char     tls_sni[MAX_TLS_SNI];      /* TLS SNI hostname                  */

    /* raw frame bytes */
    uint8_t  packet[MAX_CAPTURE_BYTES];
};

#endif /* PKSNIFF_H */