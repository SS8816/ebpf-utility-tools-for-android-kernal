#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <bpf/libbpf.h>
#include "cam_control.skel.h"
#include "camtrace.h"

static volatile int running = 1;
static void sig_handler(int sig) { running = 0; }

static const char *event_name(__u32 type)
{
    switch (type) {
    case EVENT_REQ_ADDED:     return "REQ_ADDED";
    case EVENT_REQ_APPLIED:   return "REQ_APPLIED";
    case EVENT_REQ_SUBMITTED: return "REQ_SUBMITTED";
    default:                  return "UNKNOWN";
    }
}

static int handle_event(void *ctx, void *data, size_t size)
{
    struct cam_event *e = data;
    __u64 sec = e->timestamp / 1000000000;
    __u64 ms  = (e->timestamp % 1000000000) / 1000000;

    printf("[%5llu.%03llu] %-14s PID=%-6u CAM=%-4u REQ=%-8llu EXTRA=%llu\n",
           sec, ms,
           event_name(e->event_type),
           e->pid, e->camera_id,
           e->extra_1, e->extra_2);
    return 0;
}

int main(void)
{
    struct cam_control_bpf *skel;
    struct ring_buffer     *rb;

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    skel = cam_control_bpf__open_and_load();
    if (!skel) {
        fprintf(stderr, "Failed to load BPF skeleton\n");
        return 1;
    }

    if (cam_control_bpf__attach(skel)) {
        fprintf(stderr, "Failed to attach BPF programs\n");
        goto cleanup;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                          handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    printf("cam_control running — tracking camera request lifecycle\n\n");
    printf("%-12s %-14s %-7s %-5s %-9s %s\n",
           "TIMESTAMP", "EVENT", "PID", "CAM", "REQ_ID", "EXTRA");
    printf("%-12s %-14s %-7s %-5s %-9s %s\n",
           "---------", "-----", "---", "---", "------", "-----");

    while (running) {
        int err = ring_buffer__poll(rb, 100);
        if (err < 0 && err != -EINTR) {
            fprintf(stderr, "Poll error: %d\n", err);
            break;
        }
    }

    ring_buffer__free(rb);
cleanup:
    cam_control_bpf__destroy(skel);
    return 0;
}