// SPDX-License-Identifier: GPL-2.0
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#define TASK_COMM_LEN 16
#define MAX_FILENAME_LEN 256
#define MAX_ARGS_LEN 256

enum event_type {
    EVENT_EXEC = 1,
    EVENT_FORK = 2,
    EVENT_EXIT = 3,
};

struct event {
    u32 pid;
    u32 ppid;
    u32 uid;
    u32 gid;
    u32 inum;           // namespace inode (for pid namespace)
    int exit_code;
    u64 timestamp;
    u8  type;           // EVENT_EXEC / FORK / EXIT
    char comm[TASK_COMM_LEN];
    char filename[MAX_FILENAME_LEN];
    char args[MAX_ARGS_LEN];
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); // 16 MB
} rb SEC(".maps");

// --- helpers ---

static __always_inline u32 get_ppid(struct task_struct *task) {
    struct task_struct *parent;
    u32 ppid;
    parent = BPF_CORE_READ(task, real_parent);
    ppid   = BPF_CORE_READ(parent, tgid);
    return ppid;
}

static __always_inline u32 get_ns_inum(struct task_struct *task) {
    return BPF_CORE_READ(task, nsproxy, pid_ns_for_children, ns.inum);
}

// --- tracepoints ---

SEC("tracepoint/sched/sched_process_exec")
int handle_exec(struct trace_event_raw_sched_process_exec *ctx)
{
    struct event *e;
    struct task_struct *task;
    u64 id;

    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e) return 0;

    id   = bpf_get_current_pid_tgid();
    task = (struct task_struct *)bpf_get_current_task();

    e->type      = EVENT_EXEC;
    e->timestamp = bpf_ktime_get_ns();
    e->pid       = id >> 32;
    e->ppid      = get_ppid(task);
    e->uid       = (u32)bpf_get_current_uid_gid();
    e->gid       = (u32)(bpf_get_current_uid_gid() >> 32);
    e->inum      = get_ns_inum(task);

    bpf_get_current_comm(e->comm, sizeof(e->comm));

    // filename lives in the tracepoint ctx
    unsigned int fname_off = ctx->__data_loc_filename & 0xFFFF;
    bpf_probe_read_str(e->filename, sizeof(e->filename),
                       (void *)ctx + fname_off);

    // Try to read argv[0] + argv[1] from mm->arg_start
    struct mm_struct *mm = BPF_CORE_READ(task, mm);
    if (mm) {
        unsigned long arg_start = BPF_CORE_READ(mm, arg_start);
        unsigned long arg_end   = BPF_CORE_READ(mm, arg_end);
        long len = arg_end - arg_start;
        if (len > MAX_ARGS_LEN) len = MAX_ARGS_LEN;
        if (len > 0)
            bpf_probe_read_user(e->args, len, (void *)arg_start);
    }

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/sched/sched_process_fork")
int handle_fork(struct trace_event_raw_sched_process_fork *ctx)
{
    struct event *e;

    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e) return 0;

    e->type      = EVENT_FORK;
    e->timestamp = bpf_ktime_get_ns();
    e->pid       = ctx->child_pid;
    e->ppid      = ctx->parent_pid;
    e->uid       = (u32)bpf_get_current_uid_gid();
    e->gid       = (u32)(bpf_get_current_uid_gid() >> 32);
    e->inum      = 0; // not easily available here without task pointer
    e->exit_code = 0;

    // parent comm is available in ctx
    bpf_probe_read_kernel_str(e->comm, sizeof(e->comm), ctx->parent_comm);

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/sched/sched_process_exit")
int handle_exit(struct trace_event_raw_sched_process_template *ctx)
{
    struct event *e;
    struct task_struct *task;
    u64 id;

    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e) return 0;

    id   = bpf_get_current_pid_tgid();
    task = (struct task_struct *)bpf_get_current_task();

    e->type      = EVENT_EXIT;
    e->timestamp = bpf_ktime_get_ns();
    e->pid       = id >> 32;
    e->ppid      = get_ppid(task);
    e->uid       = (u32)bpf_get_current_uid_gid();
    e->gid       = (u32)(bpf_get_current_uid_gid() >> 32);
    e->inum      = get_ns_inum(task);
    e->exit_code = (BPF_CORE_READ(task, exit_code) >> 8) & 0xff;

    bpf_get_current_comm(e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";