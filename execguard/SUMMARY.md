# Execguard Summary

## 1) What the tool does
Execguard monitors process execution events and flags suspicious patterns. It captures each exec, tags it with UID, PID/PPID, namespace, and a rolling per-UID execution rate, then prints severity hints.

## 2) High-level architecture
- Kernel space (eBPF): tracepoint `sched_process_exec` records exec metadata and updates a per-UID + binary rate counter.
- User space: libbpf loader reads ring buffer events, computes severity, and prints details.

Data flow:
1. A process executes a new program.
2. eBPF program records exec metadata and updates a 60s counter for the UID + binary key.
3. Event is emitted through a ring buffer.
4. User space renders severity and prints the event.

## 3) Kernel-side details (execguard.bpf.c)
- Hook: `SEC("tracepoint/sched/sched_process_exec")`.
- Event fields:
  - `pid`, `ppid`, `uid`, namespace inode (`ns_inum`), timestamp.
  - `comm`, parent `comm`.
  - `binary_path` and `cmdline` fields exist in the event, but this implementation keeps `cmdline` empty and uses `comm` as identity.
- Noise/volume control:
  - Maintains a hash map (`stats`) keyed by `uid + binary_path`.
  - Tracks a 60s sliding window with `exec_count`.
- Rationale for missing cmdline:
  - The code avoids user-memory reads for argv/mm because this kernel's verifier rejects the variable-length path. It prioritizes stable load over deep argv visibility.

## 4) User-space details (execguard.c)
- Loads and attaches BPF skeleton; reads from ring buffer.
- Severity heuristic:
  - ALERT for `su` or shell-like tools launched by non-root.
  - WARN for other shell-like binaries or high exec rate.
  - ALERT if exec rate >= 20 within 60s; WARN if >= 12.
- Output includes:
  - timestamp, severity, PID/PPID, UID, exec_count, namespace inode
  - executable name (from `binary_path`), parent comm
  - cmdline line (empty in this build)

## 5) Build and run (Makefile)
- Uses local libbpf from `../third_party/libbpf`.
- Assumes x86 vmlinux header at `../third_party/vmlinux/x86/vmlinux.h`.
- Requires `bpftool` and `clang`.

Typical steps:
- `make`
- `sudo ./execguard`

## 6) What is new or notable vs existing tools
- Focused on exec frequency and suspicious shells, rather than auditing all syscalls.
- UID + binary sliding window counters allow rate-based anomaly hints without user-space state.
- Android-friendly severity bias (non-root `su` / shell invocation).
- Minimal verifier-risk by avoiding user argv/mm reads, improving portability on constrained kernels.

## 7) Comparison with privwatch (same / different)
Same:
- Both use libbpf skeletons + ring buffer output.
- Both tag events with PID/PPID, comm, and PID namespace inode.
- Both are low-noise: each emits only one record per event (exec or cred change).

Different:
- Execguard watches exec events and patterns (process creation behavior).
- Privwatch watches privilege transitions (credential changes).
- Execguard uses a per-UID exec-rate counter; privwatch compares old vs new credentials.
- Execguard relies on tracepoint `sched_process_exec`; privwatch uses kprobe `commit_creds`.
- Execguard currently does not capture argv; privwatch has complete cred fields (uid/gid/caps).

## 8) How they can be combined to detect attacks
Combined signal improves detection across stages:
- Stage 1 (initial access): execguard flags suspicious shells/tools and high exec rate.
- Stage 2 (privilege escalation): privwatch flags uid/gid/cap changes.
- Stage 3 (post-exploitation): execguard highlights rapid tool spawning after privilege change.

Simple correlation logic (user space):
- If execguard sees shell-like exec by non-root and privwatch shows uid 0 within N seconds, raise a higher alert.
- If privwatch reports CAP_SYS_ADMIN gain and execguard shows an immediate spike in exec_count, mark likely post-exploitation behavior.

Implementation idea:
- Run both tools and merge streams by PID/PPID + timestamp window.
- Or embed both eBPF programs into one user-space binary and correlate in one process.

## 9) Known limitations / considerations
- Execguard does not read argv or full binary path (uses comm). It is more robust but less precise.
- The UID+binary counter assumes a 60s window; long-running low-rate execs are not highlighted.
- Both tools require kernel BPF support and elevated privileges to load.
