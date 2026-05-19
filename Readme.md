# Android eBPF Utility Suite
# Added a couple more tools, like extended pktrace to a mini-wireshark type tool, and also added another tool called camtrace for tracing camera data packets.

A collection of eBPF-based monitoring tools built for an Android emulator + Debian chroot workflow. The goal of the suite is to observe security-relevant kernel activity in real time with low overhead.

## What each tool covers

- **`pktrace_v2`** — packet / traffic visibility
- **`filewatch`** — file activity monitoring with noise filtering
- **`procwatch`** — process lifecycle monitoring
- **`priviwatch`** — privilege escalation / credential change monitoring
- **`execguard`** — command execution / suspicious exec behavior monitoring

> Note: `filewatch_broad` was mentioned during development, but the source files attached here contain the core tools above.

---

## 1) `pktrace_v2`

### Purpose
Captures packet metadata for traffic seen on a selected interface and prints TCP/UDP flows with addresses and ports.

### What it detects
- IPv4 traffic
- TCP packets
- UDP packets
- Source / destination IPs and ports
- Payload offsets and captured packet bytes

### Hook / probe
- `SEC("tc")`
- TC eBPF program attached to the interface path by the userspace loader

### What the probe sees
The program runs on `struct __sk_buff *skb`, inspects Ethernet and IPv4 headers, then extracts L4 ports and copies a bounded packet snapshot into a ring buffer event. The code uses safe byte copies and bounded checks to satisfy the verifier. fileciteturn3file2

### Important implementation details
- Only IPv4 is handled
- TCP and UDP are supported
- Packet bytes are captured up to `MAX_CAPTURE_BYTES = 64`
- The program emits source/destination IPs and ports, protocol, and offsets into the packet

### Run command
```bash
./pktrace_v2 wlan0
```

### Notes
- In your emulator workflow, `wlan0` was the useful interface.
- The tool was developed with safe packet copying because direct ring-buffer writes from packet memory were rejected earlier in the project.

---

## 2) `filewatch`

### Purpose
Monitors file activity and emits only meaningful file events, while reducing emulator noise and avoiding self-recursion.

### What it detects
- File open
- File write
- File delete
- File rename

### Hooks / probes
- `kprobe/vfs_open`
- `kprobe/vfs_write`
- `kprobe/vfs_unlink`
- `kprobe/vfs_rename`

### What the probes see
Each probe resolves the target dentry / file, checks whether the target is a regular file, and then emits a ring-buffer event containing:
- PID
- UID
- operation type
- inode
- device identifier
- file name

The BPF side also contains an ignore-PID map so the tracer can ignore its own process and avoid recursive write spam. fileciteturn3file1

### Important implementation details
- Non-regular files are filtered out
- Self-generated events are filtered out
- This reduces noise from emulator sockets, IPC, and tracing output

### Run command
```bash
./filewatch
```

---

## 3) `procwatch`

### Purpose
Tracks process lifecycle events in real time and shows process trees and execution context.

### What it detects
- Process creation
- Process execution
- Process exit
- Parent / child relationships
- UID / GID context
- PID namespace context
- Command / filename details

### Hooks / probes
- `tracepoint/sched/sched_process_exec`
- `tracepoint/sched/sched_process_fork`
- `tracepoint/sched/sched_process_exit`

### What the probes see
The BPF program fills a shared event structure for all three tracepoints. It records:
- PID
- PPID
- UID
- GID
- PID namespace inode
- Timestamp
- Event type (`EXEC`, `FORK`, `EXIT`)
- Process name (`comm`)
- Filename for exec events
- Command-line bytes for exec events
- Exit code for exit events

The exec handler reads the filename from the tracepoint context and also attempts to read argv from `mm->arg_start`. fileciteturn3file4

### Important implementation details
- `sched_process_exec` is the best place to observe a new binary image being executed
- `sched_process_fork` gives parent/child process lineage
- `sched_process_exit` gives exit visibility and exit code context

### Run command
```bash
./procwatch
```

---

## 4) `priviwatch`

### Purpose
Monitors privilege changes by tracing kernel credential commits.

### What it detects
- UID changes
- GID changes
- Capability changes
- Root escalation attempts
- Privilege drops

### Hook / probe
- `kprobe/commit_creds`

### Why this hook is used
`commit_creds()` is the kernel function that applies credential changes. By tracing it, the tool sees privilege updates such as setuid, setgid, capability grants, and privilege drops through one central hook. The implementation compares old and new credentials and only emits an event when a real change occurs. fileciteturn3file3

### What the probe sees
The event includes:
- PID
- PPID
- Timestamp
- Old UID / new UID
- Old GID / new GID
- Old capability mask / new capability mask
- Namespace inode
- `comm`

The BPF program reads the old credentials from the current task and the new credentials from the `new_cred` argument passed into `commit_creds()`. It also reads capability values through a local buffer to avoid CO-RE relocation problems. fileciteturn3file3

### Important implementation details
- It suppresses noise when UID, GID, and capability state do not change
- It is ideal for spotting Android shell-to-root transitions
- It also captures legitimate privilege drops from system services

### Run command
```bash
./priviwatch
```

---

## 5) `execguard`

### Purpose
Monitors process execution behavior and highlights suspicious command execution patterns.

### What it detects
- Shell spawning shell
- `sh`, `su`, `bash`, `toybox`, `busybox`
- Repeated command execution in a short window
- Malware-like script behavior
- Persistence-style execution loops

### Hook / probe
- `tracepoint/sched/sched_process_exec`

### What the probe sees
The BPF program records:
- PID
- PPID
- UID
- PID namespace inode
- Timestamp
- Process name (`comm`)
- Parent process name
- Execution count for the same UID + binary identity in a 60-second window

The current stable version intentionally avoids reading `argv` / raw command-line bytes from userspace memory because that verifier path was unstable in this environment. Instead, it uses `comm` as the executable identity and leaves `cmdline` empty. fileciteturn3file0

### Important implementation details
- A hash map tracks executions per UID + binary identity
- The counter resets after 60 seconds
- Shell-like binaries are scored more aggressively than normal commands
- Repeated execution is tracked, but normal repetition is not automatically treated as malicious

### Severity logic in the current version
- **INFO**: normal execution
- **WARN**: suspicious behavior or repeated execution above the lower threshold
- **ALERT**: shell-like binaries such as `sh` / `su` from a non-root context, or heavy repetition

### Run command
```bash
./execguard
```

### Notes
- This tool is standalone and does not depend on `priviwatch`
- It is best used alongside `priviwatch` to correlate exec chains with privilege changes

---

## How the tools work together

### Example attack chain
1. `procwatch` sees an app spawn `sh`
2. `execguard` flags the shell-like exec
3. `priviwatch` catches `uid=2000 -> 0`
4. `filewatch` can show files written by the new root shell
5. `pktrace_v2` can show traffic generated afterward

That combination gives a much clearer story than any single tool alone.

---

## Build and run pattern

Most tools were built with the same pattern:

```bash
make clean
touch *
make V=1
./<tool-name>
```

If the emulator filesystem or mounts reset, BTF / debug mounts may need to be restored before rebuilding or running eBPF programs.

---

## Environment notes

- Built for an Android emulator plus Debian chroot workflow
- Uses BTF / CO-RE where possible
- Some probes include filtering to reduce emulator-generated noise
- Several tools were tuned specifically for Android-style process and privilege behavior

---

## Repository layout

Each tool generally includes:
- `<tool>.bpf.c` — kernel-side eBPF program
- `<tool>.c` — userspace loader and event printer
- `Makefile` — build logic

---

## Quick command reference

```bash
./pktrace_v2 wlan0
./filewatch
./procwatch
./priviwatch
./execguard
```

---

## License

GPL-compatible eBPF programs and userspace loaders as used in the tutorial environment.

