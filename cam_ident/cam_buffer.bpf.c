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
    __uint(max_entries, 1024);
    __type(key,   __u64);
    __type(value, struct buffer_info);
} buffer_map SEC(".maps");

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

struct cam_irq_activated_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    __u32 entity_loc;
    __u32 irq_type;
};

SEC("tracepoint/camera/cam_buf_done")
int trace_cam_buf_done(struct cam_buf_done_args *ctx)
{
    __u64 id  = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;

    __u64 req = ctx->request;

    struct buffer_info bi = {
        .timestamp = bpf_ktime_get_ns(),
        .owner_pid = pid,
        .state     = BUF_STATE_FILLED,
    };
    bpf_map_update_elem(&buffer_map, &req, &bi, BPF_ANY);

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = bpf_ktime_get_ns();
    e->pid        = pid;
    e->tgid       = id & 0xFFFFFFFF;
    e->uid        = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = EVENT_BUFFER_FILLED;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = req;
    e->extra_2    = (__u32)ctx->link_hdl;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/camera/cam_irq_activated")
int trace_cam_irq_activated(struct cam_irq_activated_args *ctx)
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
    e->event_type = EVENT_BUFFER_SHARED;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = ctx->irq_type;
    e->extra_2    = 0;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("kprobe/dma_buf_export")
int BPF_KPROBE(trace_dma_buf_export)
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
    e->event_type = EVENT_BUFFER_SHARED;
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