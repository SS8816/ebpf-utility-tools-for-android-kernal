#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "filewatch.skel.h"

static volatile sig_atomic_t stop = 0;

#define NAME_LEN 64

struct event {
    __u32 pid;
    __u32 uid;
    __u32 op;
    __u64 inode;
    __u32 dev;
    char filename[NAME_LEN];
};

static void handle_sig(int sig)
{
    (void)sig;
    stop = 1;
}

static const char *op_name(__u32 op)
{
    switch (op) {
    case 1: return "OPEN";
    case 2: return "WRITE";
    case 3: return "DELETE";
    case 4: return "RENAME";
    default: return "UNKNOWN";
    }
}

static int handle_event(void *ctx, void *data, size_t len)
{
    (void)ctx;
    (void)len;

    const struct event *e = data;

    printf("%-8s pid=%-6u uid=%-6u inode=%-8llu dev=%-6u file=%s\n",
           op_name(e->op),
           e->pid,
           e->uid,
           (unsigned long long)e->inode,
           e->dev,
           e->filename[0] ? e->filename : "-");

    return 0;
}

int main(void)
{
    struct filewatch_bpf *skel = NULL;
    struct ring_buffer *rb = NULL;
    __u32 key = 0;
    __u32 ignore_pid;
    int err;

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    skel = filewatch_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    err = filewatch_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    ignore_pid = (__u32)getpid();
    err = bpf_map_update_elem(bpf_map__fd(skel->maps.ignore_pid), &key, &ignore_pid, BPF_ANY);
    if (err) {
        fprintf(stderr, "Failed to set ignore PID: %d\n", err);
        goto cleanup;
    }

    err = filewatch_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach probes: %d\n", err);
        goto cleanup;
    }

    rb = ring_buffer__new(
        bpf_map__fd(skel->maps.events),
        handle_event,
        NULL,
        NULL
    );

    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    printf("Monitoring file activity... Ctrl+C to stop.\n");
    printf("Ignored self PID: %u\n\n", ignore_pid);

    while (!stop) {
        err = ring_buffer__poll(rb, 200);
        if (err == -EINTR)
            break;
        if (err < 0) {
            fprintf(stderr, "Ring buffer poll error: %d\n", err);
            break;
        }
    }

cleanup:
    if (rb)
        ring_buffer__free(rb);
    if (skel)
        filewatch_bpf__destroy(skel);

    return 0;
}