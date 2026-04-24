#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define TC_ACT_OK 0
#define ETH_P_IP 0x0800
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define MAX_CAPTURE_BYTES 64

struct event {
    __u32 packet_len;
    __u32 cap_len;

    __u32 src_ip;
    __u32 dst_ip;

    __u16 src_port;
    __u16 dst_port;

    __u16 l4_off;
    __u16 payload_off;
    __u16 payload_len;

    __u8 protocol;
    __u8 reserved[3];

    __u8 packet[MAX_CAPTURE_BYTES];
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} rb SEC(".maps");

SEC("tc")
int pktrace_fn(struct __sk_buff *skb)
{
    void *data = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;

    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return TC_ACT_OK;

    if (bpf_ntohs(eth->h_proto) != ETH_P_IP)
        return TC_ACT_OK;

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

    __u16 src_port = 0;
    __u16 dst_port = 0;

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

    } else if (ip->protocol == IPPROTO_UDP) {
        struct udphdr *udp = (void *)ip + ip_hdr_len;

        if ((void *)(udp + 1) > data_end)
            return TC_ACT_OK;

        src_port = bpf_ntohs(udp->source);
        dst_port = bpf_ntohs(udp->dest);

        payload_off = l4_off + sizeof(*udp);
    }

    struct event *e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e)
        return TC_ACT_OK;

    e->packet_len = skb->len;
    e->cap_len = skb->len;

    if (e->cap_len > MAX_CAPTURE_BYTES)
        e->cap_len = MAX_CAPTURE_BYTES;

    e->src_ip = ip->saddr;
    e->dst_ip = ip->daddr;
    e->src_port = src_port;
    e->dst_port = dst_port;
    e->l4_off = l4_off;
    e->payload_off = payload_off;
    e->protocol = ip->protocol;

    e->payload_len = 0;
    if (skb->len > payload_off)
        e->payload_len = skb->len - payload_off;

    __u8 tmp[MAX_CAPTURE_BYTES] = {};

    if (bpf_skb_load_bytes(skb, 0, tmp, MAX_CAPTURE_BYTES) < 0) {
        bpf_ringbuf_discard(e, 0);
        return TC_ACT_OK;
    }

#pragma unroll
    for (int i = 0; i < MAX_CAPTURE_BYTES; i++)
        e->packet[i] = tmp[i];

    bpf_ringbuf_submit(e, 0);
    return TC_ACT_OK;
}

char LICENSE[] SEC("license") = "GPL";