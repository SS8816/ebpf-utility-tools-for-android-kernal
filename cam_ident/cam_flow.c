#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <bpf/libbpf.h>
#include "cam_flow.skel.h"
#include "camtrace.h"

static volatile int running = 1;
static void sig_handler(int sig) { running = 0; }

static int handle_event(void *ctx, void *data, size_t size)
{
    struct cam_event *e = data;
    __u64 sec = e->timestamp / 1000000000;
    __u64 ms  = (e->timestamp % 1000000000) / 1000000;

    switch (e->event_type) {
    case EVENT_CAMERA_OPEN:
        printf("[%5llu.%03llu] SESSION_START  CAM=0x%x  PID=%-6u COMM=%s\n",
               sec, ms, e->camera_id, e->pid, e->comm);
        break;
    case EVENT_CAMERA_CLOSE:
        printf("[%5llu.%03llu] SESSION_END    CAM=0x%x  PID=%-6u COMM=%s\n",
               sec, ms, e->camera_id, e->pid, e->comm);
        break;
    case EVENT_REQ_SUBMITTED:
        printf("[%5llu.%03llu] SUBMIT        REQ=%-8llu PID=%-6u COMM=%s\n",
               sec, ms, e->extra_1, e->pid, e->comm);
        break;
    case EVENT_BUFFER_FILLED:
        printf("[%5llu.%03llu] BUF_DONE      REQ=%-8llu LATENCY=%.2f ms\n",
               sec, ms, e->extra_1,
               (double)e->extra_2 / 1000000.0);
        break;
    case EVENT_FLUSH:
        printf("[%5llu.%03llu] FRAME_SKIP    PID=%-6u COMM=%s\n",
               sec, ms, e->pid, e->comm);
        break;
    default:
        printf("[%5llu.%03llu] UNKNOWN       type=%u\n",
               sec, ms, e->event_type);
    }
    return 0;
}

int main(void)
{
    struct cam_flow_bpf *skel;
    struct ring_buffer  *rb;

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    skel = cam_flow_bpf__open_and_load();
    if (!skel) {
        fprintf(stderr, "Failed to load BPF skeleton\n");
        return 1;
    }

    if (cam_flow_bpf__attach(skel)) {
        fprintf(stderr, "Failed to attach BPF programs\n");
        goto cleanup;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                          handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    printf("cam_flow running — full pipeline correlation\n\n");

    while (running) {
        int err = ring_buffer__poll(rb, 100);
        if (err < 0 && err != -EINTR) {
            fprintf(stderr, "Poll error: %d\n", err);
            break;
        }
    }

    ring_buffer__free(rb);
cleanup:
    cam_flow_bpf__destroy(skel);
    return 0;
}