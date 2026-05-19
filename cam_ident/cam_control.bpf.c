#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "camtrace.h"

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

struct cam_apply_req_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    __u32 entity_loc;
    __u32 id;
    __u64 req_id;
    __s32 link_hdl;
};

struct cam_req_mgr_add_req_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    __u32 name_loc;
    __u32 dev_id;
    __u64 req_id;
    __u32 slot_id;
    __u32 delay;
    __u32 readymap;
    __u32 devicemap;
    void *link;
    void *session;
    __s32 link_hdl;
};

struct cam_submit_to_hw_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    __u32 entity_loc;
    __u64 req_id;
};

SEC("tracepoint/camera/cam_apply_req")
int trace_cam_apply_req(struct cam_apply_req_args *ctx)
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
    e->event_type = EVENT_REQ_APPLIED;
    e->camera_id  = ctx->id;
    e->buffer_id  = 0;
    e->extra_1    = ctx->req_id;
    e->extra_2    = (__u32)ctx->link_hdl;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/camera/cam_req_mgr_add_req")
int trace_cam_req_mgr_add_req(struct cam_req_mgr_add_req_args *ctx)
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
    e->event_type = EVENT_REQ_ADDED;
    e->camera_id  = ctx->dev_id;
    e->buffer_id  = ctx->slot_id;
    e->extra_1    = ctx->req_id;
    e->extra_2    = ctx->delay;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/camera/cam_submit_to_hw")
int trace_cam_submit_to_hw(struct cam_submit_to_hw_args *ctx)
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
    e->event_type = EVENT_REQ_SUBMITTED;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = ctx->req_id;
    e->extra_2    = 0;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";