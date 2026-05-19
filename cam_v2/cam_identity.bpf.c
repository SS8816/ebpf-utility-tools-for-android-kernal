#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "camtrace.h"

#define CAMSERVER_MATCH_WINDOW_NS 2000000000ULL /* 2 seconds */
#define BINDER_DEDUP_WINDOW_NS    500000000ULL  /* 0.5 second */

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} config_map SEC(".maps");

/* Most recent app that talked to CameraService, stored as pid -> timestamp. */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);
    __type(value, __u64);
} recent_caller SEC(".maps");

/* Last selected caller used as a heuristic to attribute camera-session state. */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} last_caller SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} last_caller_ns SEC(".maps");

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

struct common_event_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
};

static __always_inline int emit_event(__u32 event_type,
                                      __u32 pid,
                                      __u32 uid,
                                      __u32 camera_id,
                                      __u32 buffer_id,
                                      __u64 extra_1,
                                      __u64 extra_2,
                                      const char *entity)
{
    struct cam_event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp  = bpf_ktime_get_ns();
    e->pid        = pid;
    e->tgid       = pid;
    e->uid        = uid;
    e->event_type = event_type;
    e->camera_id  = camera_id;
    e->buffer_id  = buffer_id;
    e->extra_1    = extra_1;
    e->extra_2    = extra_2;
    bpf_get_current_comm(e->comm, sizeof(e->comm));

    if (entity) {
        __builtin_memcpy(e->entity, entity, sizeof(e->entity));
    } else {
        __builtin_memset(e->entity, 0, sizeof(e->entity));
    }

    bpf_ringbuf_submit(e, 0);
    return 0;
}

static __always_inline void remember_caller(__u32 caller_pid, __u64 now)
{
    bpf_map_update_elem(&recent_caller, &caller_pid, &now, BPF_ANY);

    __u32 key = 0;
    bpf_map_update_elem(&last_caller, &key, &caller_pid, BPF_ANY);
    bpf_map_update_elem(&last_caller_ns, &key, &now, BPF_ANY);
}

static __always_inline __u32 get_last_caller(__u64 *age_ns)
{
    __u32 key = 0;
    __u32 *pid = bpf_map_lookup_elem(&last_caller, &key);
    __u64 *ts  = bpf_map_lookup_elem(&last_caller_ns, &key);

    if (!pid || !ts)
        return 0;

    __u64 now = bpf_ktime_get_ns();
    if (age_ns)
        *age_ns = now - *ts;

    if ((now - *ts) > CAMSERVER_MATCH_WINDOW_NS)
        return 0;

    return *pid;
}

SEC("tracepoint/binder/binder_transaction")
int trace_binder_transaction(struct binder_transaction_args *ctx)
{
    __s32 reply = 0;
    __s32 to_proc = 0;
    __u32 code = 0;

    bpf_probe_read_kernel(&reply, sizeof(reply), &ctx->reply);
    bpf_probe_read_kernel(&to_proc, sizeof(to_proc), &ctx->to_proc);
    bpf_probe_read_kernel(&code, sizeof(code), &ctx->code);

    if (reply != 0)
        return 0;

    __u32 key = 0;
    __u32 *camserver_pid = bpf_map_lookup_elem(&config_map, &key);
    if (!camserver_pid || *camserver_pid == 0)
        return 0;

    if ((__u32)to_proc != *camserver_pid)
        return 0;

    __u32 caller_pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    if (caller_pid == 0)
        return 0;

    __u32 uid = (__u32)(bpf_get_current_uid_gid() & 0xFFFFFFFFULL);
    if (uid < 10000)
        return 0;

    __u64 now = bpf_ktime_get_ns();

    __u64 *last_seen = bpf_map_lookup_elem(&recent_caller, &caller_pid);
    if (last_seen && (now - *last_seen) < BINDER_DEDUP_WINDOW_NS)
        return 0;

    remember_caller(caller_pid, now);

    /* Emit the actual delegation edge: caller -> cameraserver */
    emit_event(EVENT_DELEGATION_EDGE,
               caller_pid,
               uid,
               0,
               0,
               (__u64)(*camserver_pid),
               code,
               "binder->cameraserver");

    /* Keep a human-readable camera-call event as well. */
    emit_event(EVENT_BINDER_CALL,
               caller_pid,
               uid,
               0,
               0,
               (__u64)(*camserver_pid),
               code,
               "cameraserver");

    return 0;
}

SEC("tracepoint/camera/cam_context_state")
int trace_cam_context_state(struct common_event_args *ctx)
{
    __u32 pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    __u32 uid = (__u32)(bpf_get_current_uid_gid() & 0xFFFFFFFFULL);
    __u64 age_ns = 0;
    __u32 inferred_pid = get_last_caller(&age_ns);

    /* Use the last caller when it is fresh; otherwise fall back to current pid. */
    if (inferred_pid == 0)
        inferred_pid = pid;

    emit_event(EVENT_APP_SESSION,
               inferred_pid,
               uid,
               0,
               0,
               0,
               age_ns,
               "cam_context_state");
    return 0;
}

SEC("tracepoint/camera/cam_apply_req")
int trace_cam_apply_req(struct common_event_args *ctx)
{
    __u32 pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    __u32 uid = (__u32)(bpf_get_current_uid_gid() & 0xFFFFFFFFULL);
    (void)ctx;

    emit_event(EVENT_CAPTURE_REQUEST,
               pid,
               uid,
               0,
               0,
               0,
               0,
               "cam_apply_req");
    return 0;
}

SEC("tracepoint/camera/cam_buf_done")
int trace_cam_buf_done(struct common_event_args *ctx)
{
    __u32 pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    __u32 uid = (__u32)(bpf_get_current_uid_gid() & 0xFFFFFFFFULL);
    (void)ctx;

    emit_event(EVENT_FRAME_DONE,
               pid,
               uid,
               0,
               0,
               0,
               0,
               "cam_buf_done");
    return 0;
}

SEC("tracepoint/v4l2/v4l2_qbuf")
int trace_v4l2_qbuf(struct common_event_args *ctx)
{
    __u32 pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    __u32 uid = (__u32)(bpf_get_current_uid_gid() & 0xFFFFFFFFULL);
    (void)ctx;

    emit_event(EVENT_FRAME_BUFFER_QBUF,
               pid,
               uid,
               0,
               0,
               0,
               0,
               "v4l2_qbuf");
    return 0;
}

SEC("tracepoint/v4l2/v4l2_dqbuf")
int trace_v4l2_dqbuf(struct common_event_args *ctx)
{
    __u32 pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    __u32 uid = (__u32)(bpf_get_current_uid_gid() & 0xFFFFFFFFULL);
    (void)ctx;

    emit_event(EVENT_FRAME_BUFFER_DQBUF,
               pid,
               uid,
               0,
               0,
               0,
               0,
               "v4l2_dqbuf");
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
