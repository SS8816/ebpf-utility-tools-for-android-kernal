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

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256);
    __type(key,   __u32);
    __type(value, char[TASK_COMM_LEN]);
} pid_to_comm SEC(".maps");

struct cam_context_state_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    void *ctx;
    __u32 state;
    __u32 name_loc;
};

SEC("tracepoint/camera/cam_context_state")
int trace_cam_context_state(struct cam_context_state_args *ctx)
{
    __u64 id  = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;
    __u32 tid = id & 0xFFFFFFFF;
    __u32 uid = bpf_get_current_uid_gid() & 0xFFFFFFFF;

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = bpf_ktime_get_ns();
    e->pid        = pid;
    e->tgid       = tid;
    e->uid        = uid;
    e->event_type = (ctx->state == 0) ? EVENT_CAMERA_CLOSE : EVENT_CAMERA_OPEN;
    e->camera_id  = (__u32)(unsigned long)ctx->ctx & 0xFFFF;
    e->buffer_id  = 0;
    e->extra_1    = (__u64)(unsigned long)ctx->ctx;
    e->extra_2    = ctx->state;
    bpf_get_current_comm(e->comm, sizeof(e->comm));

    __builtin_memset(e->entity, 0, sizeof(e->entity));

    __u32 cam_id = e->camera_id;
    bpf_map_update_elem(&pid_to_camera, &pid, &cam_id, BPF_ANY);

    char comm_buf[TASK_COMM_LEN];
    bpf_get_current_comm(comm_buf, sizeof(comm_buf));
    bpf_map_update_elem(&pid_to_comm, &pid, &comm_buf, BPF_ANY);

    bpf_ringbuf_submit(e, 0);
    return 0;
}

struct cam_flush_req_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    __u32 type;
    __u32 pad;
    __s64 req_id;
    void *link;
    void *session;
};

SEC("tracepoint/camera/cam_flush_req")
int trace_cam_flush_req(struct cam_flush_req_args *ctx)
{
    __u64 id  = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = bpf_ktime_get_ns();
    e->pid        = pid;
    e->tgid       = id & 0xFFFFFFFF;
    e->uid        = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = EVENT_FLUSH;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = ctx->req_id;
    e->extra_2    = ctx->type;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_map_delete_elem(&pid_to_camera, &pid);

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";