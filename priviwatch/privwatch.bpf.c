// SPDX-License-Identifier: GPL-2.0
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "GPL";

#define TASK_COMM_LEN 16

struct event {
    __u32 pid;
    __u32 ppid;
    __u64 timestamp;

    __u32 old_uid;
    __u32 new_uid;
    __u32 old_gid;
    __u32 new_gid;

    __u64 old_caps_eff;
    __u64 new_caps_eff;

    __u32 ns_inum;

    char comm[TASK_COMM_LEN];
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

/* Plain local buffer so we do not CO-RE-relocate kernel_cap_t.val */
struct cap_words {
    __u32 val[2];
};

static __always_inline __u64 read_caps(const struct cred *cred)
{
    struct cap_words caps = {};
    __u64 combined;

    /*
     * Read the entire capability object into a plain local struct.
     * This avoids CO-RE relocation on kernel_cap_t.val, which is what
     * failed in your previous versions.
     */
    bpf_probe_read_kernel(&caps, sizeof(caps), &cred->cap_effective);

    combined = ((__u64)caps.val[1] << 32) | caps.val[0];
    return combined;
}

SEC("kprobe/commit_creds")
int BPF_KPROBE(trace_commit_creds, struct cred *new_cred)
{
    struct task_struct *task;
    const struct cred *old_cred;
    struct event *e;

    __u32 old_uid, new_uid;
    __u32 old_gid, new_gid;
    __u64 old_caps, new_caps;

    task = (struct task_struct *)bpf_get_current_task();
    old_cred = BPF_CORE_READ(task, cred);

    old_uid = BPF_CORE_READ(old_cred, uid.val);
    new_uid = BPF_CORE_READ(new_cred, uid.val);
    old_gid = BPF_CORE_READ(old_cred, gid.val);
    new_gid = BPF_CORE_READ(new_cred, gid.val);

    old_caps = read_caps(old_cred);
    new_caps = read_caps(new_cred);

    /* Suppress noise: only emit on actual privilege changes */
    if (old_uid == new_uid &&
        old_gid == new_gid &&
        old_caps == new_caps)
        return 0;

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->timestamp    = bpf_ktime_get_ns();
    e->pid          = (__u32)(bpf_get_current_pid_tgid() >> 32);
    e->ppid         = BPF_CORE_READ(task, real_parent, tgid);
    e->old_uid      = old_uid;
    e->new_uid      = new_uid;
    e->old_gid      = old_gid;
    e->new_gid      = new_gid;
    e->old_caps_eff = old_caps;
    e->new_caps_eff = new_caps;
    e->ns_inum      = BPF_CORE_READ(task, nsproxy, pid_ns_for_children, ns.inum);

    bpf_get_current_comm(e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);
    return 0;
}