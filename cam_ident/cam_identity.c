#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <bpf/libbpf.h>
#include "cam_identity.skel.h"
#include "camtrace.h"

#define EVENT_BINDER_CALL 20
#define EVENT_APP_SESSION 21

static volatile int running = 1;
static void sig_handler(int sig) { running = 0; }

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
    for (size_t i = 0; i < n; i++)
        if (buf[i] == '\0') buf[i] = ' ';
    char *space = strchr(buf, ' ');
    if (space) *space = '\0';
    return 0;
}

static int find_pid_by_cmdline(const char *target)
{
    DIR *dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent *ent;
    char cmdline[256];
    int found = -1;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_DIR) continue;
        int pid = atoi(ent->d_name);
        if (pid <= 0) continue;

        if (read_cmdline(pid, cmdline, sizeof(cmdline)) < 0) continue;

        if (strstr(cmdline, target)) {
            found = pid;
            break;
        }
    }
    closedir(dir);
    return found;
}

static int handle_event(void *ctx, void *data, size_t size)
{
    struct cam_event *e = data;
    __u64 sec = e->timestamp / 1000000000;
    __u64 ms  = (e->timestamp % 1000000000) / 1000000;
    char appname[256];

    if (e->event_type == EVENT_BINDER_CALL) {
        read_cmdline((__u32)e->pid, appname, sizeof(appname));
        printf("[%5llu.%03llu] BINDER_CALL   APP=%-40s PID=%-6u -> cameraserver\n",
               sec, ms, appname, (__u32)e->pid);

    } else if (e->event_type == EVENT_APP_SESSION) {
        __u32 app_pid = (__u32)e->extra_2;
        if (app_pid > 0) {
            read_cmdline(app_pid, appname, sizeof(appname));
        } else {
            snprintf(appname, sizeof(appname), "<unknown>");
        }
        printf("[%5llu.%03llu] CAM_SESSION   APP=%-40s PID=%-6u CAM=0x%x STATE=%llu\n",
               sec, ms, appname, app_pid,
               e->camera_id, e->extra_1);
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

    printf("cam_identity running — tracking which app opens the camera\n\n");
    printf("%-12s %-14s %-40s %s\n", "TIMESTAMP", "EVENT", "APP", "DETAIL");
    printf("%-12s %-14s %-40s %s\n", "---------", "-----", "---", "------");

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