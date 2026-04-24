#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "execguard.skel.h"

#define TASK_COMM_LEN 16
#define PATH_LEN      128
#define CMDLINE_LEN   256

struct event {
    unsigned int       pid;
    unsigned int       ppid;
    unsigned int       uid;
    unsigned int       ns_inum;
    unsigned long long timestamp_ns;

    unsigned int       exec_count;

    char               comm[TASK_COMM_LEN];
    char               parent_comm[TASK_COMM_LEN];
    char               binary_path[PATH_LEN];
    char               cmdline[CMDLINE_LEN];
    unsigned int       cmdline_len;
};

static volatile sig_atomic_t stop = 0;

static void handle_sig(int sig)
{
    (void)sig;
    stop = 1;
}

static const char *basename_of(const char *path)
{
    const char *base = path;
    size_t i;

    if (!path || !path[0])
        return "";

    for (i = 0; path[i]; i++) {
        if (path[i] == '/')
            base = &path[i + 1];
    }
    return base;
}

static bool is_shellish(const char *name)
{
    static const char *bad[] = {
        "sh", "bash", "dash", "zsh", "su",
        "toybox", "busybox", "nc", "netcat", "ncat",
        "curl", "wget", "python", "python3",
        "perl", "php", "node"
    };
    size_t i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        if (strcmp(name, bad[i]) == 0)
            return true;
    }
    return false;
}

static void normalize_cmdline(char *dst, size_t dst_sz, const char *src, unsigned int src_len)
{
    size_t i, j = 0;

    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';

    if (!src || src_len == 0) return;

    for (i = 0; i < src_len && j + 1 < dst_sz; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\0')
            c = ' ';
        if (iscntrl(c) && c != ' ')
            c = ' ';
        dst[j++] = (char)c;
    }

    while (j > 0 && dst[j - 1] == ' ')
        j--;

    dst[j] = '\0';
}

static const char *severity_for(const struct event *e)
{
    const char *base = basename_of(e->binary_path);

    if (!base[0])
        base = basename_of(e->comm);

    if (is_shellish(base)) {
        if (strcmp(base, "su") == 0 && e->uid != 0)
            return "ALERT";
        if ((strcmp(base, "sh") == 0 || strcmp(base, "bash") == 0 ||
             strcmp(base, "toybox") == 0 || strcmp(base, "busybox") == 0) && e->uid != 0)
            return "ALERT";
        return "WARN";
    }

    if (e->exec_count >= 20)
        return "ALERT";
    if (e->exec_count >= 12)
        return "WARN";

    return "INFO";
}

static int handle_event(void *ctx, void *data, size_t len)
{
    (void)ctx;
    (void)len;

    const struct event *e = data;
    char ts[32];
    char cmdline[CMDLINE_LEN];
    const char *sev = severity_for(e);
    time_t sec;
    struct tm tm;

    sec = (time_t)(e->timestamp_ns / 1000000000ULL);
    localtime_r(&sec, &tm);
    strftime(ts, sizeof(ts), "%H:%M:%S", &tm);

    normalize_cmdline(cmdline, sizeof(cmdline), e->cmdline, e->cmdline_len);

    printf("[%s] %-5s pid=%-6u ppid=%-6u uid=%-5u count=%-3u ns=%-12u comm=%-16s parent=%-16s\n",
           ts, sev, e->pid, e->ppid, e->uid, e->exec_count, e->ns_inum, e->comm, e->parent_comm);

    printf("         bin=%s\n", e->binary_path[0] ? e->binary_path : "(unknown)");
    if (cmdline[0])
        printf("         cmd=%s\n", cmdline);
    else
        printf("         cmd=(empty)\n");

    return 0;
}

int main(void)
{
    struct execguard_bpf *skel = NULL;
    struct ring_buffer *rb = NULL;
    int err;

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);


    skel = execguard_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    err = execguard_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    err = execguard_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton: %d\n", err);
        goto cleanup;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        err = -1;
        goto cleanup;
    }

    printf("Monitoring process execution... Ctrl+C to stop.\n");
    printf("This tool is standalone and does not depend on privwatch.\n\n");

    while (!stop) {
        err = ring_buffer__poll(rb, 200);
        if (err == -EINTR) {
            err = 0;
            break;
        }
        if (err < 0) {
            fprintf(stderr, "Ring buffer poll error: %d\n", err);
            break;
        }
    }

cleanup:
    if (rb)
        ring_buffer__free(rb);
    if (skel)
        execguard_bpf__destroy(skel);

    return err < 0 ? 1 : 0;
}