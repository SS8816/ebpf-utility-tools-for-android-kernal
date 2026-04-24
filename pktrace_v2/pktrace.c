#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "pktrace.skel.h"

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
    __u8  protocol;
    __u8  reserved[3];
    __u8  packet[MAX_CAPTURE_BYTES];
};

struct output_cfg {
    FILE *logf;
    FILE *pcapf;
    size_t payload_preview;
};

static volatile bool running = true;

static void sig_handler(int sig)
{
    (void)sig;
    running = false;
}

static const char *proto_to_str(__u8 proto)
{
    if (proto == 6)
        return "TCP";
    if (proto == 17)
        return "UDP";
    return "OTHER";
}

static void print_hex_bytes(FILE *fp, const __u8 *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        fprintf(fp, "%02x", data[i]);
        if (i + 1 < len)
            fputc(' ', fp);
    }
}

static void write_pcap_global_header(FILE *fp)
{
    struct __attribute__((packed)) pcap_global_header {
        uint32_t magic_number;
        uint16_t version_major;
        uint16_t version_minor;
        int32_t  thiszone;
        uint32_t sigfigs;
        uint32_t snaplen;
        uint32_t network;
    } hdr = {
        .magic_number = 0xa1b2c3d4,
        .version_major = 2,
        .version_minor = 4,
        .thiszone = 0,
        .sigfigs = 0,
        .snaplen = MAX_CAPTURE_BYTES,
        .network = 1, /* LINKTYPE_ETHERNET */
    };

    fwrite(&hdr, sizeof(hdr), 1, fp);
    fflush(fp);
}

static void write_pcap_record(FILE *fp, const struct event *e)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct __attribute__((packed)) pcap_record_header {
        uint32_t ts_sec;
        uint32_t ts_usec;
        uint32_t incl_len;
        uint32_t orig_len;
    } rec = {
        .ts_sec = (uint32_t)tv.tv_sec,
        .ts_usec = (uint32_t)tv.tv_usec,
        .incl_len = e->cap_len,
        .orig_len = e->packet_len,
    };

    fwrite(&rec, sizeof(rec), 1, fp);
    fwrite(e->packet, 1, rec.incl_len, fp);
    fflush(fp);
}

static int handle_event(void *ctx, void *data, size_t size)
{
    (void)size;

    struct output_cfg *out = ctx;
    struct event *e = data;

    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];
    struct in_addr s = { .s_addr = e->src_ip };
    struct in_addr d = { .s_addr = e->dst_ip };

    inet_ntop(AF_INET, &s, src, sizeof(src));
    inet_ntop(AF_INET, &d, dst, sizeof(dst));

    const char *proto = proto_to_str(e->protocol);

    size_t payload_avail = 0;
    if (e->payload_off < e->cap_len)
        payload_avail = e->cap_len - e->payload_off;

    size_t payload_show = payload_avail;
    if (out->payload_preview < payload_show)
        payload_show = out->payload_preview;

    char src_port_buf[16];
    char dst_port_buf[16];

    if (e->src_port)
        snprintf(src_port_buf, sizeof(src_port_buf), "%u", e->src_port);
    else
        snprintf(src_port_buf, sizeof(src_port_buf), "-");

    if (e->dst_port)
        snprintf(dst_port_buf, sizeof(dst_port_buf), "%u", e->dst_port);
    else
        snprintf(dst_port_buf, sizeof(dst_port_buf), "-");

    printf("%-6s %s:%-5s -> %s:%-5s  pkt=%u cap=%u payload=%zu",
           proto, src, src_port_buf, dst, dst_port_buf,
           e->packet_len, e->cap_len, payload_avail);

    if (payload_show > 0) {
        printf("  data=");
        print_hex_bytes(stdout, e->packet + e->payload_off, payload_show);
    }

    printf("\n");

    if (out->logf) {
        fprintf(out->logf, "%-6s %s:%-5s -> %s:%-5s  pkt=%u cap=%u payload=%zu",
                proto, src, src_port_buf, dst, dst_port_buf,
                e->packet_len, e->cap_len, payload_avail);

        if (payload_show > 0) {
            fprintf(out->logf, "  data=");
            print_hex_bytes(out->logf, e->packet + e->payload_off, payload_show);
        }

        fputc('\n', out->logf);
        fflush(out->logf);
    }

    if (out->pcapf)
        write_pcap_record(out->pcapf, e);

    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <ifname> [--log file] [--pcap file] [--payload N]\n"
            "Example: %s wlan0 --log packets.log --pcap packets.pcap --payload 64\n",
            prog, prog);
}

int main(int argc, char **argv)
{
    struct pktrace_bpf *skel = NULL;
    struct ring_buffer *rb = NULL;
    struct bpf_tc_hook hook = {};
    struct bpf_tc_opts opts = {};
    struct output_cfg out = {
        .logf = NULL,
        .pcapf = NULL,
        .payload_preview = 64,
    };

    const char *ifname = NULL;
    const char *log_path = NULL;
    const char *pcap_path = NULL;

    int ifindex, err = 0;
    int attached = 0;

    static const struct option long_opts[] = {
        {"log", required_argument, NULL, 'l'},
        {"pcap", required_argument, NULL, 'p'},
        {"payload", required_argument, NULL, 'n'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "l:p:n:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'l':
            log_path = optarg;
            break;
        case 'p':
            pcap_path = optarg;
            break;
        case 'n':
            out.payload_preview = strtoul(optarg, NULL, 10);
            if (out.payload_preview > MAX_CAPTURE_BYTES)
                out.payload_preview = MAX_CAPTURE_BYTES;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (optind < argc)
        ifname = argv[optind];

    if (!ifname) {
        usage(argv[0]);
        return 1;
    }

    ifindex = if_nametoindex(ifname);
    if (!ifindex) {
        perror("if_nametoindex");
        return 1;
    }

    if (log_path) {
        out.logf = fopen(log_path, "w");
        if (!out.logf) {
            perror("fopen log");
            return 1;
        }
        setvbuf(out.logf, NULL, _IOLBF, 0);
    }

    if (pcap_path) {
        out.pcapf = fopen(pcap_path, "wb");
        if (!out.pcapf) {
            perror("fopen pcap");
            if (out.logf)
                fclose(out.logf);
            return 1;
        }
        write_pcap_global_header(out.pcapf);
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    skel = pktrace_bpf__open_and_load();
    if (!skel) {
        fprintf(stderr, "Failed to open and load BPF skeleton\n");
        goto cleanup;
    }

    hook.sz = sizeof(hook);
    hook.ifindex = ifindex;
    hook.attach_point = BPF_TC_INGRESS;

    err = bpf_tc_hook_create(&hook);
    if (err && err != -EEXIST) {
        fprintf(stderr, "Failed to create TC hook: %d\n", err);
        goto cleanup;
    }

    opts.sz = sizeof(opts);
    opts.prog_fd = bpf_program__fd(skel->progs.pktrace_fn);
    opts.priority = 1;
    opts.handle = 1;

    err = bpf_tc_attach(&hook, &opts);
    if (err) {
        fprintf(stderr, "Failed to attach TC program: %d\n", err);
        goto cleanup;
    }
    attached = 1;

    rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, &out, NULL);
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    printf("Tracing packets on %s... Ctrl-C to stop.\n\n", ifname);
    printf("%-6s %-21s %s\n", "PROTO", "SRC", "DST");
    printf("%-6s %-21s %s\n", "-----", "---", "---");

    while (running) {
        err = ring_buffer__poll(rb, 100);
        if (err < 0 && err != -EINTR) {
            fprintf(stderr, "Ring buffer poll error: %d\n", err);
            break;
        }
    }

cleanup:
    if (rb)
        ring_buffer__free(rb);

    if (attached) {
        bpf_tc_detach(&hook, &opts);
        bpf_tc_hook_destroy(&hook);
    }

    if (out.pcapf)
        fclose(out.pcapf);

    if (out.logf)
        fclose(out.logf);

    pktrace_bpf__destroy(skel);
    return err < 0 ? -err : 0;
}