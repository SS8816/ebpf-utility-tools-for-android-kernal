# Procwatch Summary

## 1) What the tool does
Procwatch monitors process lifecycle events (exec, fork, exit) and prints a timestamped stream with PID/PPID, UID/GID, namespace inode, and key details like binary path, args, and exit code.

## 2) High-level architecture
- Kernel space (eBPF): tracepoints for exec/fork/exit send events to a ring buffer.
- User space: libbpf loader polls the ring buffer and formats output.

Data flow:
1. Process execs, forks, or exits.
2. eBPF program gathers metadata for that event type.
3. Event is emitted to ring buffer.
4. User space prints a single line per event.

## 3) Kernel-side details (procwatch.bpf.c)
- Hooks:
  - `tracepoint/sched/sched_process_exec`
  - `tracepoint/sched/sched_process_fork`
  - `tracepoint/sched/sched_process_exit`
- Common fields: pid, ppid, uid/gid, timestamp, comm.
- Exec-specific:
  - Reads executable filename from tracepoint context (`__data_loc_filename`).
  - Attempts to read argv bytes from `mm->arg_start` to `arg_end` (bounded to 256 bytes).
- Fork-specific:
  - Uses tracepoint data for parent comm.
  - Namespace inode is set to 0 because task pointer is not available in this tracepoint path.
- Exit-specific:
  - Reads exit code from `task->exit_code` (shifted to byte).
  - Gets namespace inode via `task->nsproxy->pid_ns_for_children->ns.inum`.

## 4) User-space details (procwatch.c)
- Loads and attaches BPF skeleton; reads ring buffer.
- Sanitizes args by replacing null separators with spaces for display.
- Output formats differ by event type:
  - EXEC: prints binary filename and args.
  - FORK: prints parent comm.
  - EXIT: prints comm and exit code.

## 5) Build and run (Makefile)
- Builds libbpf locally from `../third_party/libbpf`.
- Requires:
  - clang, gcc
  - `bpftool` in PATH
  - `vmlinux.h` in `../third_party/vmlinux/<arch>/vmlinux.h`

Typical steps:
- `make`
- `sudo ./procwatch`

## 6) What is new or notable vs existing tools
- Single lightweight tracer covering exec, fork, and exit in one stream.
- Captures executable path and a bounded argv preview at exec time.
- Namespace-aware for exec and exit (pid namespace inode).

## 7) Known limitations / considerations
- Fork events do not include namespace inode (no task pointer in that tracepoint).
- Args capture is bounded to 256 bytes and may be rejected on some kernels with strict verifier rules.
- No filtering by UID/PID in kernel; all process events are streamed.
