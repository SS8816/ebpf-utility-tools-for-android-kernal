# Pktrace Summary

## 1) What the tool does
Pktrace captures a small slice of each inbound IPv4 packet at the TC ingress hook, extracts L3/L4 metadata, and optionally logs a payload preview to console, a log file, and/or a pcap file.

## 2) High-level architecture
- Kernel space (eBPF): TC program parses Ethernet + IPv4 + TCP/UDP headers and copies the first 64 bytes of the packet into a ring buffer event.
- User space: libbpf loader attaches the TC program to an interface and consumes ring buffer events, printing and optionally writing PCAP.

Data flow:
1. Packet arrives at TC ingress.
2. eBPF program validates headers, extracts addresses/ports, copies a bounded capture.
3. Event is sent via ring buffer.
4. User space prints and/or writes to log/pcap.

## 3) Kernel-side details (pktrace.bpf.c)
- Hook: `SEC("tc")` program for TC ingress.
- Protocol handling:
  - Only IPv4 is accepted (`ETH_P_IP`).
  - TCP and UDP ports are extracted if present; other protocols still emit a record but with ports as 0.
- Safety checks:
  - Verifies header bounds (`data_end`) before access.
  - Ensures IP header length is valid.
- Capture policy:
  - Copies up to `MAX_CAPTURE_BYTES` (64) using `bpf_skb_load_bytes`.
  - Calculates payload offset and payload length based on L4 header sizes.
- Emits: `packet_len`, `cap_len`, `src/dst ip`, `src/dst port`, offsets, protocol, and packet bytes.

## 4) User-space details (pktrace.c)
- Attaches a TC ingress hook using `bpf_tc_hook_create` and `bpf_tc_attach`.
- CLI options:
  - `--log <file>` to write human-readable logs.
  - `--pcap <file>` to write binary pcap output.
  - `--payload N` to cap hex preview length (max 64).
- Output:
  - Prints `PROTO SRC:PORT -> DST:PORT` with packet size and payload size.
  - Shows hex payload preview if available.
- PCAP output:
  - Writes a global PCAP header then per-packet records using the captured bytes.

## 5) Build and run (Makefile)
- Builds libbpf locally from `../third_party/libbpf`.
- Requires:
  - clang, gcc
  - `bpftool` in PATH
  - `vmlinux.h` in `../third_party/vmlinux/<arch>/vmlinux.h`

Typical steps:
- `make`
- `sudo ./pktrace <ifname> --log packets.log --pcap packets.pcap --payload 64`

## 6) What is new or notable vs existing tools
- Minimal packet tracing with bounded capture for safety and low overhead.
- Works at TC ingress, which sees packets before higher-level processing.
- Direct PCAP output for Wireshark analysis without tcpdump.
- Clear separation of fast-path capture (BPF) and flexible formatting in user space.

## 7) Known limitations / considerations
- Only IPv4 is decoded; IPv6 is ignored.
- Only the first 64 bytes are captured; large payloads are truncated.
- No filtering by port/IP in-kernel; all IPv4 packets are traced.
- Requires root privileges and TC attach support.
