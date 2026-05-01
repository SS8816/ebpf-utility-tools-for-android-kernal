#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "camtrace.h"

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256);
    __type(key,   __u32);
    __type(value, __u32);
} pid_to_camera SEC(".maps");

static __always_inline int comm_is_cameraserver(void)
{
    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(comm, sizeof(comm));
    return (comm[0]=='c' && comm[1]=='a' && comm[2]=='m' &&
            comm[3]=='e' && comm[4]=='r' && comm[5]=='a' &&
            comm[6]=='s' && comm[7]=='e' && comm[8]=='r' &&
            comm[9]=='v' && comm[10]=='e' && comm[11]=='r');
}

static __always_inline int comm_is_camera_app(void)
{
    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(comm, sizeof(comm));
    return (comm[0]=='c' && comm[1]=='o' && comm[2]=='m' &&
            comm[3]=='.' && comm[4]=='a' && comm[5]=='n' &&
            comm[6]=='d' && comm[7]=='r' && comm[8]=='o' &&
            comm[9]=='i' && comm[10]=='d');
}

SEC("kprobe/binder_transaction")
int BPF_KPROBE(trace_binder_transaction)
{
    __u64 id  = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;
    __u32 uid = bpf_get_current_uid_gid() & 0xFFFFFFFF;

    if (!comm_is_cameraserver() && !comm_is_camera_app())
        return 0;

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = bpf_ktime_get_ns();
    e->pid        = pid;
    e->tgid       = id & 0xFFFFFFFF;
    e->uid        = uid;
    e->event_type = EVENT_CAMERA_BINDER_REQ;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = 0;
    e->extra_2    = 0;
    bpf_get_current_comm(e->comm, sizeof(e->comm));

    __u32 cam_id = 0;
    bpf_map_update_elem(&pid_to_camera, &pid, &cam_id, BPF_ANY);

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("kprobe/do_sys_openat2")
int BPF_KPROBE(trace_openat, int dfd, const char *filename)
{
    __u64 id  = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;

    __u32 *known = bpf_map_lookup_elem(&pid_to_camera, &pid);
    if (!known)
        return 0;

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = bpf_ktime_get_ns();
    e->pid        = pid;
    e->tgid       = id & 0xFFFFFFFF;
    e->uid        = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = EVENT_CAMERA_OPEN;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = 0;
    e->extra_2    = 0;
    bpf_get_current_comm(e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";