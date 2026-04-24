// SPDX-License-Identifier: GPL-2.0
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "GPL";

#define TASK_COMM_LEN 16
#define PATH_LEN      128
#define WINDOW_NS     60000000000ULL  /* 60 seconds */

struct event {
    __u32 pid;
    __u32 ppid;
    __u32 uid;
    __u32 ns_inum;
    __u64 timestamp_ns;

    __u32 exec_count;   /* executions in current 60s window */

    char comm[TASK_COMM_LEN];
    char parent_comm[TASK_COMM_LEN];
    char binary_path[PATH_LEN];
    char cmdline[256];  /* kept for compatibility, left empty here */
    __u32 cmdline_len;
};

struct exec_key {
    __u32 uid;
    char binary_path[PATH_LEN];
};

struct exec_stat {
    __u64 window_start_ns;
    __u32 count;
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, struct exec_key);
    __type(value, struct exec_stat);
} stats SEC(".maps");

SEC("tracepoint/sched/sched_process_exec")
int handle_exec(void *ctx)
{
    struct task_struct *task = (struct task_struct *)bpf_get_current_task();
    struct event *e;
    struct exec_key key = {};
    struct exec_stat *st;
    struct exec_stat init = {};
    __u64 now;
    __u64 uid_gid;
    __u32 uid;

    (void)ctx;

    now = bpf_ktime_get_ns();
    uid_gid = bpf_get_current_uid_gid();
    uid = (__u32)uid_gid;

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp_ns = now;
    e->pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    e->ppid = BPF_CORE_READ(task, real_parent, tgid);
    e->uid = uid;
    e->ns_inum = BPF_CORE_READ(task, nsproxy, pid_ns_for_children, ns.inum);

    bpf_get_current_comm(e->comm, sizeof(e->comm));
    BPF_CORE_READ_STR_INTO(e->parent_comm, task, real_parent, comm);

    /*
     * We avoid reading argv/mm here because this kernel's verifier
     * rejects the variable-length user read path. For stable loading,
     * use comm as the executable identity and keep cmdline empty.
     */
    __builtin_memset(e->binary_path, 0, sizeof(e->binary_path));
    __builtin_memcpy(e->binary_path, e->comm, sizeof(e->comm));
    e->cmdline[0] = '\0';
    e->cmdline_len = 0;

    key.uid = uid;
    __builtin_memcpy(key.binary_path, e->binary_path, sizeof(key.binary_path));

    st = bpf_map_lookup_elem(&stats, &key);
    if (!st) {
        init.window_start_ns = now;
        init.count = 1;
        bpf_map_update_elem(&stats, &key, &init, BPF_ANY);
        e->exec_count = 1;
    } else {
        if (now - st->window_start_ns > WINDOW_NS) {
            st->window_start_ns = now;
            st->count = 1;
        } else {
            st->count++;
        }
        e->exec_count = st->count;
    }

    bpf_ringbuf_submit(e, 0);
    return 0;
}