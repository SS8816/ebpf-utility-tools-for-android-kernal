#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>       
#include <time.h>
#include <bpf/libbpf.h>
#include "procwatch.skel.h"

#define TASK_COMM_LEN 16
#define MAX_FILENAME_LEN 256
#define MAX_ARGS_LEN 256

enum event_type { EVENT_EXEC = 1, EVENT_FORK = 2, EVENT_EXIT = 3 };

struct event {
    unsigned int  pid;
    unsigned int  ppid;
    unsigned int  uid;
    unsigned int  gid;
    unsigned int  inum;
    int           exit_code;
    unsigned long long timestamp;
    unsigned char type;
    char comm[TASK_COMM_LEN];
    char filename[MAX_FILENAME_LEN];
    char args[MAX_ARGS_LEN];
};

static volatile int running = 1;

static void sig_handler(int sig) { running = 0; }

static const char *event_str(int t) {
    switch (t) {
        case EVENT_EXEC: return "EXEC";
        case EVENT_FORK: return "FORK";
        case EVENT_EXIT: return "EXIT";
        default:         return "????";
    }
}

// Sanitize args: replace null bytes between argv entries with spaces
static void sanitize_args(char *args, int len) {
    for (int i = 0; i < len - 1; i++)
        if (args[i] == '\0' && args[i+1] != '\0')
            args[i] = ' ';
}

static int handle_event(void *ctx, void *data, size_t sz)
{
    struct event *e = data;
    char ts[32];
    time_t t = (time_t)(e->timestamp / 1000000000ULL);
    struct tm *tm = localtime(&t);
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);

    sanitize_args(e->args, sizeof(e->args));

    printf("[%s] %-4s  pid=%-6u ppid=%-6u uid=%-5u gid=%-5u ns_inum=%-10u",
           ts, event_str(e->type),
           e->pid, e->ppid, e->uid, e->gid, e->inum);

    if (e->type == EVENT_EXEC) {
        printf("  bin=%-30s  args=%s", e->filename, e->args);
    } else if (e->type == EVENT_FORK) {
        printf("  parent_comm=%s", e->comm);
    } else if (e->type == EVENT_EXIT) {
        printf("  comm=%-16s  exit_code=%d", e->comm, e->exit_code);
    }

    printf("\n");
    return 0;
}

int main(void)
{
    struct procwatch_bpf *skel;
    struct ring_buffer   *rb = NULL;
    int err;

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    skel = procwatch_bpf__open_and_load();
    if (!skel) { fprintf(stderr, "Failed to open/load BPF skeleton\n"); return 1; }

    err = procwatch_bpf__attach(skel);
    if (err) { fprintf(stderr, "Failed to attach: %d\n", err); goto cleanup; }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);
    if (!rb) { fprintf(stderr, "Failed to create ring buffer\n"); goto cleanup; }

    printf("%-8s %-4s  %-7s %-7s %-6s %-6s %-12s  %-30s  %s\n",
           "TIME", "TYPE", "PID", "PPID", "UID", "GID", "NS_INUM",
           "BINARY", "ARGS/COMM");
    printf("%s\n", "------------------------------------------------------------------------------------");

    while (running) {
        err = ring_buffer__poll(rb, 100);
        if (err == -EINTR) { err = 0; break; }
        if (err < 0)       { fprintf(stderr, "ring_buffer__poll error: %d\n", err); break; }
    }

cleanup:
    ring_buffer__free(rb);
    procwatch_bpf__destroy(skel);
    return err < 0 ? 1 : 0;
}