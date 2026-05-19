#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <bpf/libbpf.h>
#include "cam_identity.skel.h"
#include "camtrace.h"

#define EVENT_BINDER_CALL          20
#define EVENT_APP_SESSION          21
#define EVENT_FRAME_BUFFER_QBUF    30
#define EVENT_FRAME_BUFFER_DQBUF   31
#define EVENT_FRAME_DONE           32
#define EVENT_CAPTURE_REQUEST      33
#define EVENT_DELEGATION_EDGE      34

#define MAX_EDGES 64
#define MAX_CMDLINE 256
#define MAX_FOREGROUND 128
#define FOREGROUND_CACHE_NS 1000000000ULL
#define CHAIN_WINDOW_NS 2000000000ULL

static volatile int running = 1;

struct edge_record {
    __u64 ts;
    __u32 src_pid;
    __u32 dst_pid;
    __u32 code;
    char src_name[MAX_CMDLINE];
    char dst_name[MAX_CMDLINE];
};

static struct edge_record edges[MAX_EDGES];
static int edge_pos;

static void sig_handler(int sig)
{
    (void)sig;
    running = 0;
}

static int read_cmdline(int pid, char *buf, size_t size)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(buf, size, "<unknown:%d>", pid);
        return -1;
    }

    size_t n = fread(buf, 1, size - 1, f);
    fclose(f);
    if (n == 0) {
        snprintf(buf, size, "<empty:%d>", pid);
        return -1;
    }

    buf[n] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\0')
            buf[i] = ' ';
    }

    char *space = strchr(buf, ' ');
    if (space)
        *space = '\0';

    return 0;
}

static int find_pid_by_cmdline(const char *target)
{
    DIR *dir = opendir("/proc");
    if (!dir)
        return -1;

    struct dirent *ent;
    char cmdline[MAX_CMDLINE];
    int found = -1;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_DIR)
            continue;

        int pid = atoi(ent->d_name);
        if (pid <= 0)
            continue;

        if (read_cmdline(pid, cmdline, sizeof(cmdline)) < 0)
            continue;

        if (strstr(cmdline, target)) {
            found = pid;
            break;
        }
    }

    closedir(dir);
    return found;
}

static __u64 now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (__u64)ts.tv_sec * 1000000000ULL + (__u64)ts.tv_nsec;
}

static int read_foreground_package(char *buf, size_t size)
{
    static char cached[MAX_FOREGROUND];
    static __u64 cached_ts;
    __u64 now = now_ns();

    if (cached[0] != '\0' && (now - cached_ts) < FOREGROUND_CACHE_NS) {
        snprintf(buf, size, "%s", cached);
        return 0;
    }

    FILE *fp = popen("dumpsys window 2>/dev/null | grep -m 1 -E 'mCurrentFocus|mFocusedApp'", "r");
    if (!fp) {
        snprintf(buf, size, "<foreground-unknown>");
        return -1;
    }

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        pclose(fp);
        snprintf(buf, size, "<foreground-unknown>");
        return -1;
    }
    pclose(fp);

    char *pkg = strstr(line, "u0 ");
    if (!pkg)
        pkg = strstr(line, "u0/");

    if (pkg) {
        pkg += 3;
        size_t i = 0;
        while (pkg[i] && pkg[i] != '/' && pkg[i] != ' ' && pkg[i] != '}' && i < size - 1) {
            buf[i] = pkg[i];
            i++;
        }
        buf[i] = '\0';
    } else {
        snprintf(buf, size, "<foreground-unknown>");
    }

    if (buf[0] != '\0') {
        snprintf(cached, sizeof(cached), "%s", buf);
        cached_ts = now;
    }

    return 0;
}

static const char *event_name(__u32 event_type)
{
    switch (event_type) {
    case EVENT_BINDER_CALL:         return "BINDER_CALL";
    case EVENT_APP_SESSION:         return "CAM_SESSION";
    case EVENT_FRAME_BUFFER_QBUF:   return "FRAME_QBUF";
    case EVENT_FRAME_BUFFER_DQBUF:   return "FRAME_DQBUF";
    case EVENT_FRAME_DONE:          return "FRAME_DONE";
    case EVENT_CAPTURE_REQUEST:     return "CAPTURE_REQ";
    case EVENT_DELEGATION_EDGE:     return "DELEGATION";
    default:                        return "UNKNOWN";
    }
}

static void remember_edge(__u32 src_pid, __u32 dst_pid, __u32 code, const char *src_name, const char *dst_name, __u64 ts)
{
    struct edge_record *e = &edges[edge_pos % MAX_EDGES];
    memset(e, 0, sizeof(*e));
    e->ts = ts;
    e->src_pid = src_pid;
    e->dst_pid = dst_pid;
    e->code = code;
    snprintf(e->src_name, sizeof(e->src_name), "%s", src_name ? src_name : "<unknown>");
    snprintf(e->dst_name, sizeof(e->dst_name), "%s", dst_name ? dst_name : "<unknown>");
    edge_pos++;
}

static const struct edge_record *find_recent_edge(__u32 pid, __u64 ts)
{
    for (int i = 0; i < MAX_EDGES; i++) {
        int idx = (edge_pos - 1 - i);
        if (idx < 0)
            break;
        const struct edge_record *e = &edges[idx % MAX_EDGES];
        if (e->src_pid != pid)
            continue;
        if (ts >= e->ts && (ts - e->ts) <= CHAIN_WINDOW_NS)
            return e;
    }
    return NULL;
}

static int handle_event(void *ctx, void *data, size_t size)
{
    (void)ctx;
    (void)size;

    struct cam_event *e = data;
    __u64 sec = e->timestamp / 1000000000ULL;
    __u64 ms  = (e->timestamp % 1000000000ULL) / 1000000ULL;
    char appname[MAX_CMDLINE];
    char fg[MAX_FOREGROUND];
    const struct edge_record *edge;

    if (e->pid > 0)
        read_cmdline((int)e->pid, appname, sizeof(appname));
    else
        snprintf(appname, sizeof(appname), "<unknown>");

    read_foreground_package(fg, sizeof(fg));

    if (e->event_type == EVENT_DELEGATION_EDGE) {
        printf("[%5llu.%03llu] %-14s SRC=%-38s PID=%-6u -> cameraserver(code=%llu)\n",
               sec, ms, event_name(e->event_type), appname, e->pid, e->extra_2);

        remember_edge(e->pid, (__u32)e->extra_1, (__u32)e->extra_2, appname, "cameraserver", e->timestamp);

        if (fg[0] && strcmp(fg, appname) != 0 && strstr(appname, "gms.ui")) {
            printf("              CHAIN: foreground=%s -> %s -> cameraserver\n", fg, appname);
        }

    } else if (e->event_type == EVENT_BINDER_CALL) {
        printf("[%5llu.%03llu] %-14s APP=%-38s PID=%-6u -> cameraserver\n",
               sec, ms, event_name(e->event_type), appname, e->pid);

        edge = find_recent_edge(e->pid, e->timestamp);
        if (edge && fg[0] && strcmp(fg, appname) != 0) {
            printf("              CHAIN: foreground=%s -> %s -> cameraserver\n", fg, appname);
        }

    } else if (e->event_type == EVENT_APP_SESSION) {
        printf("[%5llu.%03llu] %-14s APP=%-38s PID=%-6u STATE=%llu AGE=%llums\n",
               sec, ms, event_name(e->event_type), appname, e->pid,
               e->extra_1, e->extra_2 / 1000000ULL);

    } else if (e->event_type == EVENT_CAPTURE_REQUEST ||
               e->event_type == EVENT_FRAME_DONE ||
               e->event_type == EVENT_FRAME_BUFFER_QBUF ||
               e->event_type == EVENT_FRAME_BUFFER_DQBUF) {
        printf("[%5llu.%03llu] %-14s APP=%-38s PID=%-6u DETAIL=%s\n",
               sec, ms, event_name(e->event_type), appname, e->pid, e->entity);

        edge = find_recent_edge(e->pid, e->timestamp);
        if (edge) {
            printf("              LINK: %s -> cameraserver -> frame activity\n", edge->src_name);
        }
    }

    return 0;
}

int main(void)
{
    struct cam_identity_bpf *skel;
    struct ring_buffer *rb;

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    int camserver_pid = find_pid_by_cmdline("cameraserver");
    if (camserver_pid < 0) {
        fprintf(stderr, "cameraserver not found in /proc. Is camera subsystem running?\n");
        return 1;
    }

    printf("Found cameraserver PID: %d\n", camserver_pid);

    skel = cam_identity_bpf__open_and_load();
    if (!skel) {
        fprintf(stderr, "Failed to load BPF skeleton\n");
        return 1;
    }

    __u32 key = 0;
    __u32 val = (__u32)camserver_pid;
    if (bpf_map__update_elem(skel->maps.config_map,
                             &key, sizeof(key),
                             &val, sizeof(val), 0)) {
        fprintf(stderr, "Failed to set cameraserver PID in config map\n");
        goto cleanup;
    }

    if (cam_identity_bpf__attach(skel)) {
        fprintf(stderr, "Failed to attach BPF programs\n");
        goto cleanup;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                          handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    printf("cam_identity running — tracking camera requests, frame flow, and delegation chains\n\n");
    printf("%-12s %-14s %-38s %s\n", "TIMESTAMP", "EVENT", "APP", "DETAIL");
    printf("%-12s %-14s %-38s %s\n", "---------", "-----", "---", "------");

    while (running) {
        int err = ring_buffer__poll(rb, 100);
        if (err < 0 && err != -EINTR) {
            fprintf(stderr, "Poll error: %d\n", err);
            break;
        }
    }

    ring_buffer__free(rb);
cleanup:
    cam_identity_bpf__destroy(skel);
    return 0;
}
