#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "privwatch.skel.h"

#define TASK_COMM_LEN 16

struct event {
    unsigned int       pid;
    unsigned int       ppid;
    unsigned long long timestamp;

    unsigned int       old_uid;
    unsigned int       new_uid;
    unsigned int       old_gid;
    unsigned int       new_gid;

    unsigned long long old_caps_eff;
    unsigned long long new_caps_eff;

    unsigned int       ns_inum;

    char               comm[TASK_COMM_LEN];
};

static volatile sig_atomic_t stop = 0;

static void handle_sig(int sig) { (void)sig; stop = 1; }

/* Print a compact capability diff — only show if changed */
static void print_caps(unsigned long long old_c, unsigned long long new_c)
{
    if (old_c == new_c)
        return;
    printf("  caps=0x%llx->0x%llx", old_c, new_c);

    /* Flag the most security-relevant capability gains */
    unsigned long long gained = new_c & ~old_c;
    if (gained & (1ULL << 0))  printf(" [+CAP_CHOWN]");
    if (gained & (1ULL << 1))  printf(" [+CAP_DAC_OVERRIDE]");
    if (gained & (1ULL << 3))  printf(" [+CAP_FOWNER]");
    if (gained & (1ULL << 6))  printf(" [+CAP_SETUID]");
    if (gained & (1ULL << 7))  printf(" [+CAP_SETGID]");
    if (gained & (1ULL << 21)) printf(" [+CAP_SYS_ADMIN]");
    if (gained & (1ULL << 27)) printf(" [+CAP_SYS_PTRACE]");
}

static int handle_event(void *ctx, void *data, size_t len)
{
    (void)ctx; (void)len;

    const struct event *e = data;

    /* Timestamp */
    char ts[16];
    time_t t = (time_t)(e->timestamp / 1000000000ULL);
    struct tm *tm = localtime(&t);
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);

    /* Severity hint: uid drop to 0 is most critical */
    const char *severity = "INFO";
    if (e->new_uid == 0 && e->old_uid != 0) severity = "WARN";
    if (e->new_uid == 0 && e->old_uid > 10000) severity = "ALERT"; /* Android app -> root */

    printf("[%s] %-5s pid=%-6u ppid=%-6u comm=%-16s ns=%-12u",
           ts, severity, e->pid, e->ppid, e->comm, e->ns_inum);

    /* UID change */
    if (e->old_uid != e->new_uid)
        printf("  uid=%u->%u", e->old_uid, e->new_uid);

    /* GID change */
    if (e->old_gid != e->new_gid)
        printf("  gid=%u->%u", e->old_gid, e->new_gid);

    /* Caps change */
    print_caps(e->old_caps_eff, e->new_caps_eff);

    printf("\n");
    return 0;
}

int main(void)
{
    struct privwatch_bpf *skel = NULL;
    struct ring_buffer   *rb   = NULL;
    int err;

    signal(SIGINT,  handle_sig);
    signal(SIGTERM, handle_sig);

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    skel = privwatch_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    err = privwatch_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    err = privwatch_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach: %d\n", err);
        goto cleanup;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                          handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    printf("Monitoring privilege changes... Ctrl+C to stop.\n");
    printf("%-8s %-5s %-7s %-7s %-16s %-12s  %s\n",
           "TIME", "SEV", "PID", "PPID", "COMM", "NS_INUM", "CHANGES");
    printf("-------------------------------------------------------------------------------------\n");

    while (!stop) {
        err = ring_buffer__poll(rb, 200);
        if (err == -EINTR) { err = 0; break; }
        if (err < 0) {
            fprintf(stderr, "Ring buffer poll error: %d\n", err);
            break;
        }
    }

cleanup:
    if (rb)   ring_buffer__free(rb);
    if (skel) privwatch_bpf__destroy(skel);
    return err < 0 ? 1 : 0;
}