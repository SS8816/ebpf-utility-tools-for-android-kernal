# Demo Commands (Android + Two-Shell Setup)

Replace placeholders:
- <IFACE> with your host interface name (e.g., wlan0, eth0).
- <DEVICE_TMP> with a writable device path (e.g., /data/local/tmp).

## 0) One-time setup
Shell A (host):
- Build all tools:
  make -C priviwatch
  make -C execguard
  make -C filewatch
  make -C procwatch
  make -C pktrace_v2

Shell B (host):
- Start adb shell:
  ./adb shell

## 1) Procwatch (process lifecycle)
Shell A (host):
  sudo ./procwatch/procwatch

Shell B (adb shell):
  id
  ls /
  sh -c "echo hello"

Expected: EXEC/FORK/EXIT lines with args and comm.

## 2) Execguard (exec behavior + rate)
Shell A (host):
  sudo ./execguard/execguard

Shell B (adb shell):
  sh -c "echo test"
  sh -c "id"
  for i in $(seq 1 15); do sh -c "echo spike"; done

Expected: WARN/ALERT for shell-like execs and higher counts.

## 3) Priviwatch (privilege changes)
Shell A (host):
  sudo ./priviwatch/privwatch

Shell B (adb shell):
  id
  su -c id

Expected: uid/gid change and capability diff when switching to root.

## 4) Filewatch (file activity)
Shell A (host):
  sudo ./filewatch/filewatch

Shell B (adb shell):
  cd <DEVICE_TMP>
  echo hi > demo.txt
  cat demo.txt
  mv demo.txt demo2.txt
  rm demo2.txt

Expected: OPEN/WRITE/RENAME/DELETE events with inode/dev.

## 5) Pktrace (packet capture)
Shell A (host):
  sudo ./pktrace_v2/pktrace <IFACE> --log packets.log --pcap packets.pcap --payload 64

Shell B (adb shell):
  ping -c 1 8.8.8.8
  curl https://example.com

Expected: TCP/UDP packet lines, optional payload preview, PCAP output.


