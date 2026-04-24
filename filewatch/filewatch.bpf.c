#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "GPL";

#define OP_OPEN   1
#define OP_WRITE  2
#define OP_DELETE 3
#define OP_RENAME 4

#define NAME_LEN 64

/* Regular-file checks */
#ifndef S_IFMT
#define S_IFMT  0170000
#endif

#ifndef S_IFREG
#define S_IFREG 0100000
#endif

struct event {
    __u32 pid;
    __u32 uid;
    __u32 op;

    __u64 inode;
    __u32 dev;

    char filename[NAME_LEN];
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

/* PID to ignore (our own tracer process) */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} ignore_pid SEC(".maps");

static __always_inline int should_ignore_self(void)
{
    __u32 key = 0;
    __u32 *pidp = bpf_map_lookup_elem(&ignore_pid, &key);
    __u32 pid;

    if (!pidp)
        return 0;

    pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    return pid == *pidp;
}

static __always_inline int is_regular_file(struct dentry *de)
{
    struct inode *inode;
    __u16 mode;

    if (!de)
        return 0;

    inode = BPF_CORE_READ(de, d_inode);
    if (!inode)
        return 0;

    mode = BPF_CORE_READ(inode, i_mode);
    return (mode & S_IFMT) == S_IFREG;
}

static __always_inline int submit_event(struct dentry *de, __u32 op)
{
    struct event *e;
    struct inode *inode;
    const char *name;
    __u64 pid_tgid, uid_gid;

    if (!de)
        return 0;

    if (should_ignore_self())
        return 0;

    if (!is_regular_file(de))
        return 0;

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    pid_tgid = bpf_get_current_pid_tgid();
    uid_gid  = bpf_get_current_uid_gid();

    e->pid = pid_tgid >> 32;
    e->uid = uid_gid & 0xffffffff;
    e->op  = op;

    inode = BPF_CORE_READ(de, d_inode);
    if (inode) {
        e->inode = BPF_CORE_READ(inode, i_ino);
        e->dev   = BPF_CORE_READ(inode, i_sb, s_dev);
    } else {
        e->inode = 0;
        e->dev   = 0;
    }

    name = BPF_CORE_READ(de, d_name.name);
    if (name)
        bpf_probe_read_kernel_str(e->filename, sizeof(e->filename), name);
    else
        e->filename[0] = '\0';

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("kprobe/vfs_open")
int BPF_KPROBE(trace_open, struct path *path)
{
    struct dentry *de = BPF_CORE_READ(path, dentry);
    return submit_event(de, OP_OPEN);
}

SEC("kprobe/vfs_write")
int BPF_KPROBE(trace_write, struct file *file)
{
    struct dentry *de = BPF_CORE_READ(file, f_path.dentry);
    return submit_event(de, OP_WRITE);
}

SEC("kprobe/vfs_unlink")
int BPF_KPROBE(trace_unlink, struct inode *dir, struct dentry *de)
{
    return submit_event(de, OP_DELETE);
}

SEC("kprobe/vfs_rename")
int BPF_KPROBE(trace_rename,
               struct inode *old_dir,
               struct dentry *old_de,
               struct inode *new_dir,
               struct dentry *new_de)
{
    return submit_event(old_de, OP_RENAME);
}