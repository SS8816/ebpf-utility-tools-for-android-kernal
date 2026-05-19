#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "camtrace.h"

struct session_info {
    __u64 start_time;
    __u64 last_req_id;
    __u32 frame_count;
    __u32 drop_count;
    __u64 last_submit_ts;
    __u64 last_done_ts;
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 64);
    __type(key,   __u32);
    __type(value, struct session_info);
} session_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256);
    __type(key,   __u64);
    __type(value, __u64);
} req_submit_ts SEC(".maps");

struct cam_context_state_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    void *ctx;
    __u32 state;
    __u32 name_loc;
};

struct cam_req_mgr_apply_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    __u32 name_loc;
    __u32 dev_id;
    __u64 req_id;
    __s32 link_hdl;
    __u32 pad;
    void *link;
    void *session;
};

struct cam_buf_done_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    __u32 ctx_type_loc;
    __u32 pad;
    void *ctx;
    __s32 link_hdl;
    __u32 pad2;
    __u64 request;
};

struct cam_notify_frame_skip_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
};

SEC("tracepoint/camera/cam_context_state")
int flow_cam_context_state(struct cam_context_state_args *ctx)
{
    __u32 key = (__u32)(unsigned long)ctx->ctx & 0xFFFF;
    __u64 now = bpf_ktime_get_ns();

    if (ctx->state != 0) {
        struct session_info si = {
            .start_time     = now,
            .last_req_id    = 0,
            .frame_count    = 0,
            .drop_count     = 0,
            .last_submit_ts = 0,
            .last_done_ts   = 0,
        };
        bpf_map_update_elem(&session_map, &key, &si, BPF_ANY);
    } else {
        bpf_map_delete_elem(&session_map, &key);
    }

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = now;
    e->pid        = bpf_get_current_pid_tgid() >> 32;
    e->tgid       = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
    e->uid        = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = (ctx->state == 0) ? EVENT_CAMERA_CLOSE : EVENT_CAMERA_OPEN;
    e->camera_id  = key;
    e->buffer_id  = 0;
    e->extra_1    = ctx->state;
    e->extra_2    = 0;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/camera/cam_req_mgr_apply_request")
int flow_cam_req_mgr_apply(struct cam_req_mgr_apply_args *ctx)
{
    __u64 now    = bpf_ktime_get_ns();
    __u64 req_id = ctx->req_id;

    bpf_map_update_elem(&req_submit_ts, &req_id, &now, BPF_ANY);
    return 0;
}

SEC("tracepoint/camera/cam_buf_done")
int flow_cam_buf_done(struct cam_buf_done_args *ctx)
{
    __u64 now     = bpf_ktime_get_ns();
    __u64 req     = ctx->request;
    __u64 latency = 0;

    __u64 *submit_ts = bpf_map_lookup_elem(&req_submit_ts, &req);
    if (submit_ts) {
        latency = now - *submit_ts;
        bpf_map_delete_elem(&req_submit_ts, &req);
    }

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = now;
    e->pid        = bpf_get_current_pid_tgid() >> 32;
    e->tgid       = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
    e->uid        = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = EVENT_BUFFER_FILLED;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = req;
    e->extra_2    = latency;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/camera/cam_notify_frame_skip")
int flow_cam_notify_frame_skip(struct cam_notify_frame_skip_args *ctx)
{
    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = bpf_ktime_get_ns();
    e->pid        = bpf_get_current_pid_tgid() >> 32;
    e->tgid       = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
    e->uid        = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = EVENT_FLUSH;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = 0;
    e->extra_2    = 0;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";