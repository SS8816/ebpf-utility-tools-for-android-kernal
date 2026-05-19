#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "camtrace.h"

#define EVENT_BINDER_CALL   20
#define EVENT_APP_SESSION   21

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key,   __u32);
    __type(value, __u32);
} config_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 16);
    __type(key,   __u32);
    __type(value, __u32);
} recent_caller SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256);
    __type(key,   __u32);
    __type(value, __u64);
} seen_callers SEC(".maps");

struct binder_transaction_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    __s32 debug_id;
    __s32 target_node;
    __s32 to_proc;
    __s32 to_thread;
    __s32 reply;
    __u32 code;
    __u32 flags;
};

struct cam_context_state_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    void *ctx;
    __u32 state;
    __u32 name_loc;
};

SEC("tracepoint/binder/binder_transaction")
int trace_binder_transaction(struct binder_transaction_args *ctx)
{
    __s32 reply = 0;
    __s32 to_proc = 0;

    bpf_probe_read_kernel(&reply,   sizeof(reply),   &ctx->reply);
    bpf_probe_read_kernel(&to_proc, sizeof(to_proc), &ctx->to_proc);

    if (reply != 0)
        return 0;

    __u32 key = 0;
    __u32 *camserver_pid = bpf_map_lookup_elem(&config_map, &key);
    if (!camserver_pid || *camserver_pid == 0)
        return 0;

    if ((__u32)to_proc != *camserver_pid)
        return 0;

    __u32 caller_pid = bpf_get_current_pid_tgid() >> 32;
    if (caller_pid == 0)
        return 0;

    __u32 uid = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    if (uid < 10000)
        return 0;

    __u64 now = bpf_ktime_get_ns();

    __u64 *last_seen = bpf_map_lookup_elem(&seen_callers, &caller_pid);
    if (last_seen && (now - *last_seen) < 500000000ULL)
        return 0;

    bpf_map_update_elem(&seen_callers, &caller_pid, &now, BPF_ANY);

    __u32 zero = 0;
    bpf_map_update_elem(&recent_caller, &zero, &caller_pid, BPF_ANY);

    __u32 code = 0;
    bpf_probe_read_kernel(&code, sizeof(code), &ctx->code);

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = now;
    e->pid        = caller_pid;
    e->tgid       = caller_pid;
    e->uid        = uid;
    e->event_type = EVENT_BINDER_CALL;
    e->camera_id  = 0;
    e->buffer_id  = 0;
    e->extra_1    = (__u32)to_proc;
    e->extra_2    = code;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/camera/cam_context_state")
int trace_cam_context_state(struct cam_context_state_args *ctx)
{
    __u32 state = 0;
    bpf_probe_read_kernel(&state, sizeof(state), &ctx->state);

    if (state == 0)
        return 0;

    void *ctx_ptr = NULL;
    bpf_probe_read_kernel(&ctx_ptr, sizeof(ctx_ptr), &ctx->ctx);

    __u32 zero = 0;
    __u32 *app_pid = bpf_map_lookup_elem(&recent_caller, &zero);

    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = bpf_ktime_get_ns();
    e->pid        = app_pid ? *app_pid : 0;
    e->tgid       = app_pid ? *app_pid : 0;
    e->uid        = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = EVENT_APP_SESSION;
    e->camera_id  = (__u32)(unsigned long)ctx_ptr & 0xFFFF;
    e->buffer_id  = 0;
    e->extra_1    = state;
    e->extra_2    = app_pid ? *app_pid : 0;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    __builtin_memset(e->entity, 0, sizeof(e->entity));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";