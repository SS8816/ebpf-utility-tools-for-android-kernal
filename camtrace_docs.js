const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  HeadingLevel, AlignmentType, BorderStyle, WidthType, ShadingType,
  LevelFormat, PageNumber, PageBreak, TabStopType, TabStopPosition
} = require('docx');
const fs = require('fs');

const BLUE = "1F4E79";
const LIGHT_BLUE = "D6E4F0";
const MED_BLUE = "2E75B6";
const DARK_GRAY = "404040";
const CODE_BG = "F2F2F2";
const WHITE = "FFFFFF";
const ACCENT = "C00000";

const border = { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" };
const borders = { top: border, bottom: border, left: border, right: border };
const noBorder = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
const noBorders = { top: noBorder, bottom: noBorder, left: noBorder, right: noBorder };

function h1(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 400, after: 200 },
    children: [new TextRun({ text, bold: true, size: 36, color: WHITE, font: "Arial" })],
    shading: { fill: BLUE, type: ShadingType.CLEAR },
    indent: { left: 200, right: 200 },
    border: { bottom: { style: BorderStyle.SINGLE, size: 6, color: MED_BLUE, space: 1 } }
  });
}

function h2(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 320, after: 160 },
    border: { bottom: { style: BorderStyle.SINGLE, size: 4, color: MED_BLUE, space: 1 } },
    children: [new TextRun({ text, bold: true, size: 28, color: BLUE, font: "Arial" })]
  });
}

function h3(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_3,
    spacing: { before: 240, after: 120 },
    children: [new TextRun({ text, bold: true, size: 24, color: MED_BLUE, font: "Arial" })]
  });
}

function h4(text) {
  return new Paragraph({
    spacing: { before: 200, after: 80 },
    children: [new TextRun({ text, bold: true, size: 22, color: DARK_GRAY, font: "Arial" })]
  });
}

function p(text, opts = {}) {
  return new Paragraph({
    spacing: { before: 80, after: 80, line: 276 },
    children: [new TextRun({ text, size: 22, font: "Arial", color: DARK_GRAY, ...opts })]
  });
}

function pRich(runs) {
  return new Paragraph({
    spacing: { before: 80, after: 80, line: 276 },
    children: runs
  });
}

function run(text, opts = {}) {
  return new TextRun({ text, size: 22, font: "Arial", color: DARK_GRAY, ...opts });
}

function code(text) {
  return new TextRun({ text, font: "Courier New", size: 20, color: "7B2D8B" });
}

function codeBlock(lines) {
  return lines.map(line =>
    new Paragraph({
      spacing: { before: 0, after: 0, line: 240 },
      indent: { left: 360, right: 360 },
      shading: { fill: CODE_BG, type: ShadingType.CLEAR },
      children: [new TextRun({ text: line, font: "Courier New", size: 19, color: "1A1A1A" })]
    })
  );
}

function bullet(text, level = 0) {
  return new Paragraph({
    numbering: { reference: "bullets", level },
    spacing: { before: 60, after: 60, line: 276 },
    children: [new TextRun({ text, size: 22, font: "Arial", color: DARK_GRAY })]
  });
}

function numbered(text, level = 0) {
  return new Paragraph({
    numbering: { reference: "numbers", level },
    spacing: { before: 60, after: 60, line: 276 },
    children: [new TextRun({ text, size: 22, font: "Arial", color: DARK_GRAY })]
  });
}

function spacer(size = 160) {
  return new Paragraph({ spacing: { before: size, after: 0 }, children: [new TextRun("")] });
}

function infoBox(title, text, fillColor = LIGHT_BLUE) {
  return new Table({
    width: { size: 9360, type: WidthType.DXA },
    columnWidths: [9360],
    rows: [
      new TableRow({
        children: [new TableCell({
          borders,
          width: { size: 9360, type: WidthType.DXA },
          shading: { fill: fillColor, type: ShadingType.CLEAR },
          margins: { top: 120, bottom: 120, left: 200, right: 200 },
          children: [
            new Paragraph({ children: [new TextRun({ text: title, bold: true, size: 22, font: "Arial", color: BLUE })] }),
            new Paragraph({ spacing: { before: 60 }, children: [new TextRun({ text, size: 21, font: "Arial", color: DARK_GRAY })] })
          ]
        })]
      })
    ]
  });
}

function twoColTable(rows, header1, header2) {
  const headerRow = new TableRow({
    tableHeader: true,
    children: [
      new TableCell({
        borders, width: { size: 2800, type: WidthType.DXA },
        shading: { fill: BLUE, type: ShadingType.CLEAR },
        margins: { top: 100, bottom: 100, left: 150, right: 150 },
        children: [new Paragraph({ children: [new TextRun({ text: header1, bold: true, color: WHITE, size: 21, font: "Arial" })] })]
      }),
      new TableCell({
        borders, width: { size: 6560, type: WidthType.DXA },
        shading: { fill: BLUE, type: ShadingType.CLEAR },
        margins: { top: 100, bottom: 100, left: 150, right: 150 },
        children: [new Paragraph({ children: [new TextRun({ text: header2, bold: true, color: WHITE, size: 21, font: "Arial" })] })]
      })
    ]
  });

  const dataRows = rows.map((r, i) => new TableRow({
    children: [
      new TableCell({
        borders, width: { size: 2800, type: WidthType.DXA },
        shading: { fill: i % 2 === 0 ? WHITE : "F5F9FF", type: ShadingType.CLEAR },
        margins: { top: 80, bottom: 80, left: 150, right: 150 },
        children: [new Paragraph({ children: [new TextRun({ text: r[0], size: 20, font: "Courier New", color: "7B2D8B", bold: true })] })]
      }),
      new TableCell({
        borders, width: { size: 6560, type: WidthType.DXA },
        shading: { fill: i % 2 === 0 ? WHITE : "F5F9FF", type: ShadingType.CLEAR },
        margins: { top: 80, bottom: 80, left: 150, right: 150 },
        children: [new Paragraph({ children: [new TextRun({ text: r[1], size: 21, font: "Arial", color: DARK_GRAY })] })]
      })
    ]
  }));

  return new Table({
    width: { size: 9360, type: WidthType.DXA },
    columnWidths: [2800, 6560],
    rows: [headerRow, ...dataRows]
  });
}

function threeColTable(rows, h1t, h2t, h3t, widths = [2400, 3000, 3960]) {
  const headerRow = new TableRow({
    tableHeader: true,
    children: [h1t, h2t, h3t].map((h, i) => new TableCell({
      borders, width: { size: widths[i], type: WidthType.DXA },
      shading: { fill: MED_BLUE, type: ShadingType.CLEAR },
      margins: { top: 100, bottom: 100, left: 150, right: 150 },
      children: [new Paragraph({ children: [new TextRun({ text: h, bold: true, color: WHITE, size: 21, font: "Arial" })] })]
    }))
  });

  const dataRows = rows.map((r, i) => new TableRow({
    children: r.map((cell, j) => new TableCell({
      borders, width: { size: widths[j], type: WidthType.DXA },
      shading: { fill: i % 2 === 0 ? WHITE : "F5F9FF", type: ShadingType.CLEAR },
      margins: { top: 80, bottom: 80, left: 150, right: 150 },
      children: [new Paragraph({ children: [new TextRun({ text: cell, size: j === 0 ? 20 : 21, font: j === 0 ? "Courier New" : "Arial", color: j === 0 ? "7B2D8B" : DARK_GRAY })] })]
    }))
  }));

  return new Table({
    width: { size: 9360, type: WidthType.DXA },
    columnWidths: widths,
    rows: [headerRow, ...dataRows]
  });
}

const doc = new Document({
  numbering: {
    config: [
      {
        reference: "bullets",
        levels: [{
          level: 0, format: LevelFormat.BULLET, text: "\u2022", alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } }
        }, {
          level: 1, format: LevelFormat.BULLET, text: "\u25E6", alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 1080, hanging: 360 } } }
        }]
      },
      {
        reference: "numbers",
        levels: [{
          level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } }
        }]
      }
    ]
  },
  styles: {
    default: { document: { run: { font: "Arial", size: 22 } } },
    paragraphStyles: [
      {
        id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 36, bold: true, font: "Arial", color: WHITE },
        paragraph: { spacing: { before: 400, after: 200 }, outlineLevel: 0 }
      },
      {
        id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 28, bold: true, font: "Arial", color: BLUE },
        paragraph: { spacing: { before: 320, after: 160 }, outlineLevel: 1 }
      },
      {
        id: "Heading3", name: "Heading 3", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 24, bold: true, font: "Arial", color: MED_BLUE },
        paragraph: { spacing: { before: 240, after: 120 }, outlineLevel: 2 }
      }
    ]
  },
  sections: [{
    properties: {
      page: {
        size: { width: 12240, height: 15840 },
        margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 }
      }
    },
    children: [

      // ============================================================
      // TITLE PAGE
      // ============================================================
      new Paragraph({
        spacing: { before: 1440, after: 200 },
        alignment: AlignmentType.CENTER,
        children: [new TextRun({ text: "camtrace", bold: true, size: 72, font: "Arial", color: BLUE })]
      }),
      new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { before: 0, after: 160 },
        children: [new TextRun({ text: "eBPF-Based Camera Observability Tool Suite", size: 32, font: "Arial", color: MED_BLUE })]
      }),
      new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { before: 0, after: 600 },
        children: [new TextRun({ text: "for the Android Kernel on Qualcomm Hardware", size: 28, font: "Arial", color: DARK_GRAY })]
      }),
      new Table({
        width: { size: 6000, type: WidthType.DXA },
        columnWidths: [6000],
        rows: [new TableRow({ children: [new TableCell({
          borders,
          width: { size: 6000, type: WidthType.DXA },
          shading: { fill: LIGHT_BLUE, type: ShadingType.CLEAR },
          margins: { top: 200, bottom: 200, left: 300, right: 300 },
          children: [
            new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "Project: Android Kernel eBPF Research", size: 22, font: "Arial", color: DARK_GRAY })] }),
            new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "Institution: MNIT Jaipur", size: 22, font: "Arial", color: DARK_GRAY })] }),
            new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "Device: Motorola G96 5G (Qualcomm SM7450)", size: 22, font: "Arial", color: DARK_GRAY })] }),
            new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "Kernel: Linux 5.10.233-android12", size: 22, font: "Arial", color: DARK_GRAY })] }),
          ]
        })]})],
      }),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 1: OVERVIEW
      // ============================================================
      h1("1. Project Overview"),
      spacer(120),
      p("camtrace is a suite of four eBPF (extended Berkeley Packet Filter) tools designed to observe the complete lifecycle of camera usage on an Android device at the kernel level. Without modifying any application, system service, or camera driver, camtrace attaches to kernel hook points and captures real-time events showing who uses the camera, what control operations happen, how frame buffers flow through the pipeline, and what the end-to-end latency of each frame looks like."),
      spacer(80),
      p("The suite was developed and tested on a rooted Motorola G96 5G running Android 12 with Linux kernel 5.10. All tools run inside a Debian chroot environment on the device, use libbpf for BPF program loading, and communicate with the kernel via BPF ring buffers."),
      spacer(160),

      h2("1.1 Architecture Overview"),
      spacer(80),
      p("The Android camera stack has multiple layers. From top to bottom:"),
      spacer(80),
      ...codeBlock([
        "  Android App (e.g. WhatsApp, Google Camera)",
        "       |",
        "       | Android Camera2 API (Java/Kotlin)",
        "       v",
        "  cameraserver  (system service, PID ~468)",
        "       |",
        "       | AIDL/HIDL Binder IPC",
        "       v",
        "  Camera HAL (Hardware Abstraction Layer)",
        "  vendor.qti.camera.provider  (PID ~1462, UID 1047)",
        "       |",
        "       | Qualcomm-specific kernel tracepoints",
        "       v",
        "  Qualcomm Camera Kernel Driver (cam_context, cam_req_mgr)",
        "       |",
        "       | V4L2 / DMA-BUF",
        "       v",
        "  Physical Hardware: Sensor, ISP, Lens, Flash",
      ]),
      spacer(120),
      p("camtrace hooks into the Qualcomm camera kernel driver layer using native tracepoints that Qualcomm exposes in the kernel at /sys/kernel/tracing/events/camera/. This is significantly more powerful than hooking at the HAL or app layer because it captures all camera activity regardless of which app initiated it, and gives microsecond-precision timestamps directly from the kernel clock."),
      spacer(160),

      h2("1.2 Why eBPF"),
      spacer(80),
      p("eBPF programs run inside the Linux kernel in a sandboxed virtual machine. The kernel's verifier checks every eBPF program before loading it to guarantee it cannot crash the kernel, cannot loop infinitely, and cannot access memory it should not. This makes eBPF the right tool for production-safe kernel instrumentation."),
      spacer(80),
      p("The alternative approaches and why they were not used:"),
      spacer(60),
      twoColTable([
        ["ptrace / strace", "High overhead, traces one process at a time, misses kernel-internal events"],
        ["printk / ftrace", "Requires kernel recompilation or module insertion, not safe for production"],
        ["Android Perfetto", "Application-level only, cannot see kernel driver internals"],
        ["tcpdump / libpcap", "Network only, not relevant to camera pipeline"],
        ["eBPF (chosen)", "Zero kernel modification, safe, multi-process, kernel-level precision"],
      ], "Approach", "Reason Not Used / Why eBPF"),
      spacer(160),

      h2("1.3 Tool Summary"),
      spacer(80),
      threeColTable([
        ["cam_ident", "WHO + WHEN", "Tracks which process opened the camera, when sessions start and stop, using cam_context_state and cam_flush_req tracepoints"],
        ["cam_control", "WHAT", "Tracks the frame request pipeline: when requests are added, applied to hardware, and submitted, using cam_apply_req, cam_req_mgr_add_req, and cam_submit_to_hw tracepoints"],
        ["cam_buffer", "DATA FLOW", "Tracks frame buffer lifecycle: when buffers are filled with image data and when they are exported via DMA-BUF to other components, using cam_buf_done and cam_irq_activated tracepoints plus dma_buf_export kprobe"],
        ["cam_flow", "CORRELATION", "Combines all three layers into a unified timeline with per-frame latency calculation from submit to done"],
      ], "Tool", "Layer", "Description"),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 2: ENVIRONMENT
      // ============================================================
      h1("2. Environment and Setup"),
      spacer(120),

      h2("2.1 Target Device"),
      spacer(80),
      twoColTable([
        ["Device", "Motorola G96 5G"],
        ["SoC", "Qualcomm SM7450 (Snapdragon 695)"],
        ["Architecture", "AArch64 (ARM 64-bit)"],
        ["Android Version", "Android 12"],
        ["Kernel Version", "5.10.233-android12-9"],
        ["Kernel Compiler", "Android Clang 12.0.5"],
        ["Root Method", "Magisk (rooted)"],
        ["Build Environment", "Debian chroot on-device"],
        ["Camera Devices", "/dev/video0, /dev/video1, /dev/video32, /dev/video33"],
        ["Camera HAL Process", "vendor.qti.camera.provider (PID 1462, UID 1047)"],
      ], "Property", "Value"),
      spacer(160),

      h2("2.2 Kernel Capabilities Verified"),
      spacer(80),
      p("Before writing any code, the following kernel capabilities were verified on the target device:"),
      spacer(80),
      twoColTable([
        ["BPF ring buffer", "Confirmed via bpftool prog list"],
        ["kprobe attachment", "Confirmed: binder_transaction, dma_buf_export present in /proc/kallsyms"],
        ["Qualcomm camera tracepoints", "Confirmed: /sys/kernel/tracing/events/camera/ contains 18 tracepoints"],
        ["dma_buf_export", "Confirmed: T symbol in /proc/kallsyms (exported, hookable)"],
        ["tracefs mounted", "Confirmed: /sys/kernel/tracing/ mounted and accessible"],
        ["kptr_restrict", "Set to 0 for development (echo 0 > /proc/sys/kernel/kptr_restrict)"],
        ["syscall tracepoints", "NOT available (CONFIG_FTRACE_SYSCALLS not enabled in this kernel)"],
        ["V4L2 devices", "/dev/video0-33 present (real Qualcomm camera driver)"],
      ], "Capability", "Status"),
      spacer(80),
      infoBox("Important Note on syscall tracepoints",
        "The Android kernel on this device was compiled without CONFIG_FTRACE_SYSCALLS, which means tracepoints like tracepoint/syscalls/sys_enter_openat are not available. This is a common constraint on Android kernels. The original design plan used these tracepoints to detect /dev/video* file opens. The solution was to use the Qualcomm-native camera tracepoints instead, which are actually more informative than syscall-level hooks.",
        "FFF3CD"),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      h2("2.3 Build System"),
      spacer(80),
      p("All tools share a single Makefile in the camtrace/ directory. The build chain is:"),
      spacer(80),
      ...codeBlock([
        "1. clang (target bpf, D__TARGET_ARCH_arm64)",
        "   Compiles .bpf.c → .bpf.o (eBPF bytecode)",
        "",
        "2. bpftool gen skeleton",
        "   Reads .bpf.o → generates .skel.h",
        "   (auto-generated C header with typed map/program accessors)",
        "",
        "3. cc (host compiler)",
        "   Compiles .c (userspace) + .skel.h → .o",
        "",
        "4. cc (linker)",
        "   Links .o + libbpf.a + libelf + libz → binary",
      ]),
      spacer(120),
      p("Key Makefile variables:"),
      spacer(80),
      twoColTable([
        ["VMLINUX_DIR", "../third_party/vmlinux/arm64  — pre-generated vmlinux.h for arm64"],
        ["LIBBPF_DIR", "../third_party/bpftool/libbpf  — libbpf source (built as static library)"],
        ["BPF_CFLAGS", "-g -O2 -target bpf -D__TARGET_ARCH_arm64 -Wno-unknown-attributes"],
        ["CFLAGS", "-g -Wall -O2 -I$(OUTPUT) (includes .output/ for skel.h access)"],
      ], "Variable", "Purpose"),
      spacer(160),

      h2("2.4 vmlinux.h"),
      spacer(80),
      p("Instead of including individual Linux kernel headers, all BPF programs include a single vmlinux.h file. This file is auto-generated from the kernel's BTF (BPF Type Format) data and contains every kernel struct definition used by the running kernel. The arm64 version was pre-bundled in the bpf-developer-tutorial repository."),
      spacer(80),
      p("For a production deployment on a new device, vmlinux.h should be regenerated directly from that device's kernel to ensure exact struct layout matches:"),
      spacer(60),
      ...codeBlock([
        "adb shell bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h",
      ]),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 3: eBPF FUNDAMENTALS
      // ============================================================
      h1("3. eBPF Fundamentals"),
      spacer(120),

      h2("3.1 How eBPF Works"),
      spacer(80),
      p("An eBPF program has two parts: a kernel-side program written in C that runs inside the kernel when a hook point fires, and a userspace program that loads the kernel program, attaches it to hook points, and reads data out."),
      spacer(80),
      ...codeBlock([
        "Kernel Side (.bpf.c)              Userspace Side (.c)",
        "─────────────────────             ──────────────────────────────",
        "SEC(\"tracepoint/camera/...\")       skel = tool_bpf__open_and_load()",
        "int my_prog(struct ctx *ctx) {     tool_bpf__attach(skel)",
        "  // runs in kernel               rb = ring_buffer__new(...)",
        "  // when tracepoint fires        while(1) ring_buffer__poll(rb, 100)",
        "  write to ring buffer            // handle_event called per event",
        "}",
      ]),
      spacer(120),

      h2("3.2 Hook Point Types Used"),
      spacer(80),
      twoColTable([
        ["tracepoint", "Static hook points compiled into the kernel at fixed, stable locations. More reliable than kprobes because they have a defined ABI. The Qualcomm camera tracepoints (tracepoint/camera/*) are the primary mechanism used in camtrace."],
        ["kprobe", "Dynamic hook on any exported kernel function. Fires when that function is called anywhere in the kernel. Used for dma_buf_export to catch DMA buffer exports. Less stable than tracepoints because function signatures can change between kernel versions."],
        ["BPF ring buffer", "Shared memory region between kernel and userspace used to pass events. The kernel-side program reserves a slot, fills it, and submits it. The userspace poll loop reads and processes it. Zero-copy and lock-free."],
      ], "Type", "Description"),
      spacer(160),

      h2("3.3 BPF Maps"),
      spacer(80),
      p("BPF maps are key-value stores in kernel memory accessible from both the eBPF program and userspace. camtrace uses three map types:"),
      spacer(80),
      twoColTable([
        ["BPF_MAP_TYPE_RINGBUF", "The event ring buffer. eBPF programs write cam_event structs here, userspace polls and reads them. Used in all 4 tools."],
        ["BPF_MAP_TYPE_HASH", "Generic hash map. Used in cam_ident to store pid_to_camera and pid_to_comm mappings. Used in cam_buffer to store buffer_info per request. Used in cam_flow to store req_submit_ts for latency calculation."],
      ], "Map Type", "Usage in camtrace"),
      spacer(160),

      h2("3.4 The BPF Verifier"),
      spacer(80),
      p("Before any eBPF program runs in the kernel, the kernel's BPF verifier statically analyzes it. The verifier rejects programs that could be unsafe. This caused one significant build error during development:"),
      spacer(80),
      infoBox("Verifier Error Encountered",
        "Error: R2 type=ctx expected=fp, pkt, pkt_meta, map_value\n\nThis error occurred when passing ctx->request (a pointer into the tracepoint context struct) directly as the key argument to bpf_map_update_elem(). The verifier requires that map keys and values reside on the stack (fp = frame pointer), not in the tracepoint context. Fix: copy the value to a local stack variable first, then pass its address.",
        "FFE0E0"),
      spacer(80),
      ...codeBlock([
        "// WRONG - verifier rejects this",
        "bpf_map_update_elem(&map, &ctx->request, &value, BPF_ANY);",
        "",
        "// CORRECT - copy to stack first",
        "__u64 req = ctx->request;",
        "bpf_map_update_elem(&map, &req, &value, BPF_ANY);",
      ]),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 4: SHARED DATA STRUCTURES
      // ============================================================
      h1("4. Shared Data Structures"),
      spacer(120),
      p("All four tools share a common header file camtrace.h that defines the event structure and event type constants. This ensures consistent data representation across the entire suite."),
      spacer(160),

      h2("4.1 cam_event Structure"),
      spacer(80),
      ...codeBlock([
        "struct cam_event {",
        "    __u64 timestamp;        // kernel monotonic time in nanoseconds (bpf_ktime_get_ns)",
        "    __u32 pid;              // process ID of the process active when event fired",
        "    __u32 tgid;             // thread group ID (the main PID of the process)",
        "    __u32 uid;              // user ID (Android UID = app identity)",
        "    char  comm[16];         // process name, truncated to 16 chars (kernel limit)",
        "    __u32 event_type;       // one of the EVENT_* constants below",
        "    __u32 camera_id;        // camera context ID (lower 16 bits of ctx pointer)",
        "    __u32 buffer_id;        // buffer slot ID where applicable",
        "    __u64 extra_1;          // event-specific data (req_id, ctx pointer, etc.)",
        "    __u64 extra_2;          // event-specific data (state, latency, link_hdl, etc.)",
        "    char  entity[16];       // pipeline entity name where applicable",
        "};",
      ]),
      spacer(160),

      h2("4.2 Event Type Constants"),
      spacer(80),
      twoColTable([
        ["EVENT_CAMERA_OPEN (1)", "Camera context transitioned to an active state (state != 0)"],
        ["EVENT_CAMERA_CLOSE (2)", "Camera context transitioned to uninitialized state (state == 0)"],
        ["EVENT_CAMERA_BINDER_REQ (3)", "Binder transaction from a known camera process (used in early design)"],
        ["EVENT_STREAM_START (4)", "Camera stream started (V4L2 STREAMON, not used on this device)"],
        ["EVENT_STREAM_STOP (5)", "Camera stream stopped (V4L2 STREAMOFF, not used on this device)"],
        ["EVENT_BUFFER_ALLOC (6)", "Buffer allocation event"],
        ["EVENT_BUFFER_QUEUE (7)", "Buffer queued to driver (VIDIOC_QBUF equivalent)"],
        ["EVENT_BUFFER_DEQUEUE (8)", "Buffer dequeued from driver (VIDIOC_DQBUF equivalent)"],
        ["EVENT_BUFFER_CREATED (9)", "New buffer created in the pipeline"],
        ["EVENT_BUFFER_FILLED (10)", "Buffer filled with image data (cam_buf_done fired)"],
        ["EVENT_BUFFER_SHARED (11)", "Buffer exported via DMA-BUF to another component"],
        ["EVENT_REQ_ADDED (12)", "New frame request added to the request manager queue"],
        ["EVENT_REQ_APPLIED (13)", "Frame request applied to hardware registers"],
        ["EVENT_REQ_SUBMITTED (14)", "Frame request submitted to hardware pipeline"],
        ["EVENT_IRQ_ACTIVATED (15)", "Hardware IRQ fired (camera interrupt event)"],
        ["EVENT_FLUSH (16)", "Pipeline flush or frame skip event"],
      ], "Constant", "Meaning"),
      spacer(160),

      h2("4.3 buffer_info Structure"),
      spacer(80),
      ...codeBlock([
        "struct buffer_info {",
        "    __u64 timestamp;    // when this buffer entered its current state",
        "    __u32 owner_pid;    // which process owns this buffer",
        "    __u32 state;        // BUF_STATE_QUEUED=0, BUF_STATE_FILLED=1, BUF_STATE_SHARED=2",
        "};",
      ]),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 5: CAM_IDENT
      // ============================================================
      h1("5. cam_ident — Session Identity Tracker"),
      spacer(120),

      h2("5.1 Purpose"),
      spacer(80),
      p("cam_ident answers the question: which process is using the camera, when did they start, and when did they stop? It is the entry point of the pipeline — without knowing when a session opens and closes, the other tools have no context for the frame-level events they capture."),
      spacer(160),

      h2("5.2 What Happens When an App Opens the Camera"),
      spacer(80),
      p("When an Android app calls camera2.CameraManager.openCamera(), the following chain of events occurs in the kernel:"),
      spacer(80),
      ...codeBlock([
        "App calls CameraManager.openCamera()",
        "  → Camera2 API sends Binder IPC to cameraserver",
        "  → cameraserver sends HIDL Binder call to vendor.qti.camera.provider",
        "  → HAL calls into the Qualcomm camera kernel driver",
        "  → Kernel driver creates cam_context objects for each pipeline component",
        "  → cam_context_state tracepoint fires for each context created",
        "     (one context per hardware block: sensor, ISP, lens, flash, etc.)",
      ]),
      spacer(120),
      p("The cam_context_state tracepoint is defined by Qualcomm in the kernel driver and fires every time a camera context changes state. This is the primary hook for cam_ident."),
      spacer(160),

      h2("5.3 Tracepoint: cam_context_state"),
      spacer(80),
      p("Format as read from /sys/kernel/tracing/events/camera/cam_context_state/format:"),
      spacer(80),
      ...codeBlock([
        "name: cam_context_state",
        "ID: 1529",
        "format:",
        "  field:unsigned short common_type;        offset:0;  size:2;",
        "  field:unsigned char  common_flags;       offset:2;  size:1;",
        "  field:unsigned char  common_preempt_count; offset:3; size:1;",
        "  field:int            common_pid;         offset:4;  size:4;",
        "  field:void*          ctx;                offset:8;  size:8;",
        "  field:uint32_t       state;              offset:16; size:4;",
        "  field:__data_loc char[] name;            offset:20; size:4;",
        "",
        "print fmt: \"%s: State ctx=%p ctx_state=%u\", __get_str(name), REC->ctx, REC->state",
      ]),
      spacer(120),
      p("Field explanations:"),
      spacer(80),
      twoColTable([
        ["common_type", "Tracepoint event type ID (1529). Common to all tracepoints, used internally by ftrace."],
        ["common_pid", "PID of the process active when the tracepoint fired. This is the HAL process (vendor.qti.camera.provider), not the app."],
        ["ctx", "Pointer to the cam_context kernel object. Each hardware pipeline component (sensor, ISP, lens) has its own context. This pointer serves as a unique session identifier."],
        ["state", "The new state of this context: 0=UNINIT, 1=AVAILABLE, 2=ACQUIRED, 3=READY, 4=ACTIVATED, 5=FLUSHING"],
        ["name", "Dynamic string: the name of the camera context type (e.g. \"isp\", \"sensor\"). Uses __data_loc encoding (offset + length packed into u32)."],
      ], "Field", "Meaning"),
      spacer(160),

      h2("5.4 Context State Machine"),
      spacer(80),
      p("Each cam_context goes through a defined state machine during a camera session:"),
      spacer(80),
      ...codeBlock([
        "  UNINIT (0) ──► AVAILABLE (1) ──► ACQUIRED (2) ──► READY (3)",
        "                      ▲                                  |",
        "                      |                                  ▼",
        "                      └──────────── FLUSHING (5) ◄── ACTIVATED (4)",
        "",
        "  Opening camera:   1 → 2 → 3  (AVAILABLE → ACQUIRED → READY)",
        "  Preview running:  3 → 2 → 3  (cycles READY → ACQUIRED → READY per config change)",
        "  Taking a photo:   3 → 4 → 5 → 2  (READY → ACTIVATED → FLUSHING → ACQUIRED)",
        "  Closing camera:   any → 1 → 0  (release back to AVAILABLE then UNINIT)",
      ]),
      spacer(160),

      h2("5.5 Tracepoint: cam_flush_req"),
      spacer(80),
      p("cam_ident also attaches to cam_flush_req which fires when the pipeline is flushed, indicating a session close or mid-session reset."),
      spacer(80),
      ...codeBlock([
        "name: cam_flush_req",
        "ID: 1531",
        "format:",
        "  field:uint32_t  type;     offset:8;  size:4;   // flush type",
        "  field:int64_t   req_id;   offset:16; size:8;   // request ID being flushed",
        "  field:void*     link;     offset:24; size:8;   // pipeline link pointer",
        "  field:void*     session;  offset:32; size:8;   // session pointer",
      ]),
      spacer(160),

      h2("5.6 BPF Maps in cam_ident"),
      spacer(80),
      twoColTable([
        ["events (RINGBUF)", "Output ring buffer. All cam_event structs are written here and read by userspace."],
        ["pid_to_camera (HASH)", "Maps PID → camera_id. When a process opens a camera context, its PID is stored here so other hooks can filter by camera-using processes."],
        ["pid_to_comm (HASH)", "Maps PID → comm[16]. Stores the process name associated with each camera-using PID."],
      ], "Map", "Purpose"),
      spacer(160),

      h2("5.7 Sample Output Explained"),
      spacer(80),
      ...codeBlock([
        "TIMESTAMP      EVENT          PID    UID    COMM              CTX/STATE",
        "[114560.353]  CAMERA_OPEN    1462   1047   HwBinder:1462_3   CTX=0xffffffe520688d38 STATE=2",
        "[114560.417]  CAMERA_OPEN    1462   1047   vendor.qti.came   CTX=0xffffff8041251000 STATE=3",
        "[114569.004]  CAMERA_OPEN    1462   1047   vendor.qti.came   CTX=0xffffffe520689968 STATE=1",
        "[114570.503]  FLUSH          7312   0      kworker/u17:0     CTX=0x115 STATE=0",
      ]),
      spacer(80),
      twoColTable([
        ["PID=1462", "Always the HAL process. The tracepoint fires in HAL context regardless of which app requested the camera."],
        ["UID=1047", "The system UID of cameraserver/HAL. To identify the requesting app, cam_flow correlates with Binder transactions."],
        ["COMM=HwBinder:1462_3", "Thread 3 of the HwBinder thread pool inside the HAL process, handling IPC from cameraserver."],
        ["CTX=0xffffffe520688d38", "Kernel pointer to the cam_context struct for this hardware component. Each unique address = one pipeline stage."],
        ["STATE=2 (CAMERA_OPEN)", "Context moved to ACQUIRED state — session initializing."],
        ["STATE=1 (still CAMERA_OPEN)", "Context moved to AVAILABLE — released back to pool. Our code maps this as OPEN because state==0 means CLOSE."],
        ["FLUSH kworker", "Kernel worker thread performing pipeline flush between captures."],
      ], "Value", "Meaning"),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 6: CAM_CONTROL
      // ============================================================
      h1("6. cam_control — Frame Request Pipeline Tracker"),
      spacer(120),

      h2("6.1 Purpose"),
      spacer(80),
      p("cam_control answers the question: what is the camera pipeline doing, frame by frame? It tracks the lifecycle of each capture request as it moves through the Qualcomm camera request manager, from the moment a request is queued to the moment it is applied to hardware registers."),
      spacer(160),

      h2("6.2 The Qualcomm Camera Request Pipeline"),
      spacer(80),
      p("The Qualcomm camera HAL uses a request manager (cam_req_mgr) to orchestrate frame captures. Every frame goes through three stages:"),
      spacer(80),
      ...codeBlock([
        "Stage 1: REQ_ADDED",
        "  cam_req_mgr_add_req tracepoint fires",
        "  A new capture request has been added to the pipeline queue",
        "  Multiple devices (sensor, ISP, lens) each receive the request separately",
        "  → You see REQ=N appear for CAM=1, CAM=2, CAM=3, CAM=4 in quick succession",
        "",
        "Stage 2: REQ_APPLIED",
        "  cam_apply_req tracepoint fires",
        "  The request has been applied to the hardware device registers",
        "  The hardware is now programmed to capture frame N",
        "  → Typically 1-2 frames behind REQ_ADDED (pipeline depth)",
        "",
        "Stage 3: REQ_SUBMITTED  (cam_flow only)",
        "  cam_submit_to_hw tracepoint fires",
        "  The request has been physically submitted to the hardware DMA engine",
        "  This is the last software-visible moment before hardware takes over",
      ]),
      spacer(160),

      h2("6.3 Tracepoint: cam_req_mgr_add_req"),
      spacer(80),
      ...codeBlock([
        "name: cam_req_mgr_add_req",
        "ID: 1539",
        "format:",
        "  field:__data_loc char[] name;   offset:8;   // device name",
        "  field:uint32_t  dev_id;         offset:12;  // device ID (CAM column)",
        "  field:uint64_t  req_id;         offset:16;  // request/frame ID (REQ column)",
        "  field:uint32_t  slot_id;        offset:24;  // slot in request manager queue",
        "  field:uint32_t  delay;          offset:28;  // pipeline depth/delay (EXTRA column)",
        "  field:uint32_t  readymap;       offset:32;  // bitmask of ready devices",
        "  field:uint32_t  devicemap;      offset:36;  // bitmask of all devices",
        "  field:void*     link;           offset:40;  // pipeline link pointer",
        "  field:void*     session;        offset:48;  // session pointer",
        "  field:int32_t   link_hdl;       offset:56;  // link handle",
      ]),
      spacer(160),

      h2("6.4 Tracepoint: cam_apply_req"),
      spacer(80),
      ...codeBlock([
        "name: cam_apply_req",
        "ID: 1526",
        "format:",
        "  field:__data_loc char[] entity; offset:8;   // pipeline entity name",
        "  field:uint32_t  id;             offset:12;  // device ID (CAM column)",
        "  field:uint64_t  req_id;         offset:16;  // request ID being applied",
        "  field:int32_t   link_hdl;       offset:24;  // link handle (EXTRA column = 525069)",
      ]),
      spacer(160),

      h2("6.5 Tracepoint: cam_submit_to_hw"),
      spacer(80),
      ...codeBlock([
        "name: cam_submit_to_hw",
        "ID: 1542",
        "format:",
        "  field:__data_loc char[] entity; offset:8;   // pipeline entity",
        "  field:uint64_t  req_id;         offset:16;  // request ID submitted to hardware",
      ]),
      spacer(160),

      h2("6.6 Sample Output Explained"),
      spacer(80),
      ...codeBlock([
        "TIMESTAMP       EVENT         PID    CAM  REQ_ID   EXTRA",
        "[119526.455]   REQ_ADDED     20373  3    2        1",
        "[119526.456]   REQ_ADDED     22625  1    2        2",
        "[119526.519]   REQ_APPLIED   20373  0    2        525069",
        "[119526.552]   REQ_APPLIED   20373  2    2        525069",
      ]),
      spacer(80),
      twoColTable([
        ["CAM=0,1,2,3,4", "Different hardware pipeline stages. 0=main ISP, 1=sensor frontend, 2=ISP backend, 3=lens control, 4=flash/auxiliary. Each frame request is distributed to all relevant devices."],
        ["REQ=2,3,4...", "Frame request ID, incrementing by 1 per frame. At 30fps you see ~30 new REQ values per second. When REQ resets to 1, a new session started (e.g. still capture)."],
        ["EXTRA=1 or 2 (REQ_ADDED)", "The pipeline delay/depth for this device. EXTRA=2 means 2 frames of pipeline depth for sensor, EXTRA=1 for ISP stages."],
        ["EXTRA=525069 (REQ_APPLIED)", "The link handle (0x80349 hex) — a constant identifier for this camera pipeline link. Same value throughout the entire session."],
        ["REQ=18446744073709551615", "This is (uint64_t)-1, the maximum u64 value. Used as a sentinel value by the Qualcomm HAL to signal a flush/reset request rather than a real frame."],
        ["Multiple PIDs", "The HAL uses a thread pool (PIDs 19559, 20373, 21701, 22625 seen). Different threads handle different pipeline stages concurrently."],
      ], "Value", "Meaning"),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 7: CAM_BUFFER
      // ============================================================
      h1("7. cam_buffer — Buffer Lifecycle Tracker"),
      spacer(120),

      h2("7.1 Purpose"),
      spacer(80),
      p("cam_buffer tracks what happens to the actual image data — the frame buffers — after the hardware finishes processing them. It answers: when is each frame ready? and when is that frame handed off to the next consumer (display, encoder, app)?"),
      spacer(160),

      h2("7.2 Buffer Flow in the Qualcomm Camera Pipeline"),
      spacer(80),
      ...codeBlock([
        "Hardware captures frame",
        "  → ISP processes raw sensor data",
        "  → DMA writes processed pixels to buffer in memory",
        "  → cam_buf_done tracepoint fires: \"buffer for request N is ready\"",
        "  → cam_irq_activated fires: hardware IRQ signaling buffer availability",
        "  → HAL calls dma_buf_export: exports buffer as DMA-BUF file descriptor",
        "  → Buffer shared with display server / video encoder / app",
      ]),
      spacer(160),

      h2("7.3 Tracepoint: cam_buf_done"),
      spacer(80),
      ...codeBlock([
        "name: cam_buf_done",
        "ID: 1527",
        "format:",
        "  field:__data_loc char[]  ctx_type;  offset:8;   // context type (\"ISP\", \"sensor\", etc.)",
        "  field:void*              ctx;        offset:16;  // cam_context pointer",
        "  field:int32_t            link_hdl;   offset:24;  // link handle (EXTRA column)",
        "  field:uint64_t           request;    offset:32;  // request/frame ID (REQ column)",
        "",
        "print fmt: \"%5s: BufDone ctx=%p request=%llu link_hdl=0x%x\"",
      ]),
      spacer(80),
      p("This tracepoint fires inside interrupt context (PID=0 in output) or in the HAL worker thread after the hardware signals buffer completion. Each frame triggers this multiple times: once per pipeline component that processed the frame (ISP, stats, face detection, etc.), which is why you see 7-8 BUF_FILLED events per REQ_ID."),
      spacer(160),

      h2("7.4 Tracepoint: cam_irq_activated"),
      spacer(80),
      ...codeBlock([
        "name: cam_irq_activated",
        "ID: 1533",
        "format:",
        "  field:__data_loc char[]  entity;   offset:8;   // which hardware block raised IRQ",
        "  field:uint32_t           irq_type;  offset:12;  // type of interrupt",
        "",
        "print fmt: \"%8s: got irq type=%d\"",
      ]),
      spacer(80),
      p("This tracepoint fires when a hardware interrupt arrives from the camera hardware. It indicates that a hardware event (frame ready, error, timeout) has occurred. In cam_buffer this is used as a BUF_SHARED event because IRQs typically precede or accompany buffer handoff operations."),
      spacer(160),

      h2("7.5 Kprobe: dma_buf_export"),
      spacer(80),
      p("In addition to tracepoints, cam_buffer attaches a kprobe to the kernel function dma_buf_export(). This function is called when any kernel component exports a buffer as a DMA-BUF file descriptor, making it accessible to userspace or other kernel components."),
      spacer(80),
      ...codeBlock([
        "// Kernel function signature:",
        "struct dma_buf *dma_buf_export(const struct dma_buf_export_info *exp_info);",
        "",
        "// Symbol verified present on device:",
        "grep dma_buf_export /proc/kallsyms",
        "0000000000000000 T dma_buf_export        ← T = exported text symbol, hookable",
        "0000000000000000 T virtio_dma_buf_export  ← virtio variant also present",
      ]),
      spacer(80),
      p("When dma_buf_export fires during camera operation, it means a frame buffer is being exported from the camera driver to be consumed by the display pipeline, media codec, or directly by an application. This is the moment a captured frame crosses from the camera subsystem to the rest of the Android graphics stack."),
      spacer(160),

      h2("7.6 Sample Output Explained"),
      spacer(80),
      ...codeBlock([
        "TIMESTAMP       EVENT        PID    REQ_ID   EXTRA",
        "[121254.466]   BUF_SHARED   1462   0        0",
        "[121254.772]   BUF_FILLED   0      1        1442573",
        "[121254.782]   BUF_FILLED   19614  1        1442573",
        "[121254.782]   BUF_FILLED   19614  1        1442573   (x6 more)",
        "[121254.815]   BUF_FILLED   18396  1        4294967295",
      ]),
      spacer(80),
      twoColTable([
        ["BUF_SHARED REQ=0", "From dma_buf_export kprobe or cam_irq_activated. REQ=0 because these hooks do not carry request IDs — they fire on every DMA export/IRQ regardless of which frame."],
        ["BUF_FILLED REQ=1", "From cam_buf_done. Frame request 1 has completed hardware processing. The buffer is ready."],
        ["PID=0 on BUF_FILLED", "Zero PID means the event fired in interrupt context — no process was running, the CPU was handling a hardware interrupt. Normal for hardware completion events."],
        ["7-8 BUF_FILLED per REQ", "Each frame is processed by multiple parallel pipeline stages (ISP main, ISP stats, face detection, video encoder, display, etc.). Each stage gets its own buf_done notification."],
        ["EXTRA=1442573", "The link handle for this session (different from cam_control session because different camera was opened)."],
        ["EXTRA=4294967295", "This is (uint32_t)-1. Indicates this particular buf_done came from a different pipeline path (still capture vs preview), where link_hdl is set to -1."],
      ], "Value", "Meaning"),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 8: CAM_FLOW
      // ============================================================
      h1("8. cam_flow — Correlation Engine"),
      spacer(120),

      h2("8.1 Purpose"),
      spacer(80),
      p("cam_flow is the correlation engine that ties the other three tools together into a single unified timeline. It attaches to all three levels simultaneously: session lifecycle (cam_context_state), frame submission (cam_submit_to_hw), and frame completion (cam_buf_done). It then attempts to calculate per-frame latency by measuring the time between submission and completion."),
      spacer(160),

      h2("8.2 Correlation Strategy"),
      spacer(80),
      p("The latency calculation uses a BPF hash map as a timestamp store:"),
      spacer(80),
      ...codeBlock([
        "When cam_submit_to_hw fires for request N:",
        "  → Store: req_submit_ts[N] = bpf_ktime_get_ns()",
        "",
        "When cam_buf_done fires for request N:",
        "  → Look up: submit_ts = req_submit_ts[N]",
        "  → Calculate: latency = now - submit_ts",
        "  → Delete: req_submit_ts[N]  (clean up to avoid map overflow)",
        "  → Report: BUF_DONE REQ=N LATENCY=X.XX ms",
      ]),
      spacer(80),
      p("The session tracking uses a separate hash map:"),
      spacer(80),
      ...codeBlock([
        "When cam_context_state fires with state != 0:",
        "  → Create: session_map[ctx_id] = {start_time, frame_count=0, ...}",
        "  → Emit: SESSION_START event",
        "",
        "When cam_context_state fires with state == 0:",
        "  → Delete: session_map[ctx_id]",
        "  → Emit: SESSION_END event",
      ]),
      spacer(160),

      h2("8.3 The Latency Issue"),
      spacer(80),
      infoBox("Known Limitation: Latency Shows 0.00ms",
        "In the current implementation, the per-frame latency always shows 0.00ms. This is because the request IDs used by cam_submit_to_hw and cam_buf_done are not the same value. cam_submit_to_hw uses an entity-level request ID from the request manager, while cam_buf_done uses a link-level request ID from a different pipeline abstraction layer. The lookup in cam_buf_done finds no matching entry in req_submit_ts, so latency defaults to zero.\n\nThe fix would involve understanding how Qualcomm maps between these two ID spaces, or using the cam_irq_activated event as a proxy completion signal instead of cam_buf_done.",
        "FFF3CD"),
      spacer(160),

      h2("8.4 Sample Output Explained"),
      spacer(80),
      ...codeBlock([
        "[122075.081]  SESSION_START  CAM=0xf4d0  PID=1462  COMM=HwBinder:1462_1",
        "[122075.220]  BUF_DONE      REQ=1        LATENCY=0.00 ms",
        "[122075.383]  FRAME_SKIP    PID=25052    COMM=kworker/u17:3",
        "[122076.610]  SESSION_START  CAM=0x9968  PID=1462  COMM=vendor.qti.came",
        "[122079.137]  SESSION_START  CAM=0x1208  PID=1462  COMM=HwBinder:1462_2",
      ]),
      spacer(80),
      twoColTable([
        ["SESSION_START CAM=0xf4d0", "Lower 16 bits of the ctx pointer, used as session ID. Multiple CAM IDs appear simultaneously because preview uses 4-5 pipeline contexts (ISP, sensor, lens, flash, stats)."],
        ["BUF_DONE REQ=1,2,3...", "Each preview frame completing at ~33ms intervals. REQ counts up continuously. When REQ resets to 1, a new session context has started (e.g. the still capture pipeline)."],
        ["FRAME_SKIP kworker", "cam_notify_frame_skip tracepoint fired. The pipeline could not deliver a frame on time. This happens during session transitions (app open/close, photo capture) when the pipeline is reconfiguring."],
        ["SESSION_START during BUF_DONE", "At timestamp 122076.610, new SESSION_START events appear while BUF_DONE events are still flowing. This is the moment you pressed the shutter — the still capture pipeline starts up alongside the preview pipeline."],
        ["SESSION_START at 122079", "The original preview contexts are re-acquired after photo capture completes. This is normal Qualcomm HAL behavior: contexts are recycled between preview and capture modes."],
      ], "Value", "Meaning"),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 9: PROBLEMS AND SOLUTIONS
      // ============================================================
      h1("9. Problems Encountered and Solutions"),
      spacer(120),

      threeColTable([
        [
          "vmlinux.h path mismatch",
          "Makefile referenced ../../third_party/vmlinux but the actual path was ../third_party/vmlinux. Build failed with 'No such file or directory'.",
          "Used 'find / -name vmlinux.h' to locate the actual path, then corrected the Makefile VMLINUX_DIR variable."
        ],
        [
          "libbpf headers missing",
          "clang could not find bpf/bpf_helpers.h because the libbpf include directory structure was non-standard — headers were in src/ not include/bpf/.",
          "Created symlinks: mkdir -p libbpf/src/bpf && ln -sf ../bpf_helpers.h libbpf/src/bpf/bpf_helpers.h (and similar for all required headers)."
        ],
        [
          "EINTR undeclared",
          "cam_ident.c failed to compile with 'EINTR undeclared'. The errno constants are in <errno.h> which was not included.",
          "Added #include <errno.h> to all userspace .c files."
        ],
        [
          "TC_ACT_OK undeclared",
          "pktrace.bpf.c referenced TC_ACT_OK which is defined in <linux/pkt_cls.h>. When using vmlinux.h, kernel #define constants are not available.",
          "Added manual #define TC_ACT_OK 0 directly in the BPF source. TC constants are stable kernel ABI so hardcoding is safe."
        ],
        [
          "BPF verifier: R2 type=ctx",
          "bpf_map_update_elem was called with &ctx->field as the key argument. The verifier requires map keys/values to be on the stack (fp), not in the tracepoint context.",
          "Copy the context field to a local stack variable first, then pass its address: __u64 req = ctx->request; bpf_map_update_elem(&map, &req, &val, BPF_ANY);"
        ],
        [
          "No syscall tracepoints",
          "The original design used tracepoint/syscalls/sys_enter_openat to detect /dev/video* opens. This tracepoint did not exist because the Android kernel was compiled without CONFIG_FTRACE_SYSCALLS.",
          "Pivoted to using Qualcomm's native camera tracepoints (cam_context_state) which provide richer information than syscall-level hooks."
        ],
        [
          "No V4L2 on emulator",
          "During emulator development, /dev/video* did not exist. The entire cam_ident filter chain depended on a device path that was never opened.",
          "Discovered that the emulator camera uses a fully virtual path through Binder with no V4L2 layer. Physical device testing was required for full functionality."
        ],
        [
          "cam_flow latency = 0.00ms",
          "The req_id values from cam_submit_to_hw do not match the request values in cam_buf_done. The map lookup always misses, so latency is always zero.",
          "Known limitation. Partial fix would be to use cam_irq_activated as the completion event instead, or to investigate the Qualcomm req_id mapping between pipeline layers."
        ],
        [
          "Makefile tab vs spaces",
          "Pasting Makefile content from editors that convert tabs to spaces caused 'missing separator' errors in make.",
          "Verified with 'cat -A Makefile | grep mkdir' to check for ^I (tab) vs spaces. Replaced file content using heredoc or manual tab insertion."
        ],
        [
          "adb push permission denied",
          "Pushing files directly to /data/eadb/debian/ via adb failed with Permission Denied because the Debian chroot is owned by root.",
          "Used two-step process: adb push to /data/local/tmp/ (writable by shell), then cp to the chroot path inside an adb shell with su."
        ],
      ], "Problem", "Description", "Solution"),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 10: HOW TO RUN
      // ============================================================
      h1("10. Running the Tools"),
      spacer(120),

      h2("10.1 Prerequisites"),
      spacer(80),
      bullet("Rooted Android device with Magisk or equivalent"),
      bullet("Debian chroot installed on device (sh /data/eadb/run to enter)"),
      bullet("bpf-developer-tutorial repository cloned with submodules initialized"),
      bullet("kptr_restrict set to 0: echo 0 > /proc/sys/kernel/kptr_restrict"),
      spacer(160),

      h2("10.2 Build All Tools"),
      spacer(80),
      ...codeBlock([
        "# Enter Debian chroot",
        "sh /data/eadb/run",
        "",
        "# Navigate to camtrace directory",
        "cd /bpf-developer-tutorial/src/camtrace",
        "",
        "# Build all four tools",
        "make cam_ident",
        "make cam_control",
        "make cam_buffer",
        "make cam_flow",
        "",
        "# Or build everything at once",
        "make",
      ]),
      spacer(160),

      h2("10.3 Run Each Tool"),
      spacer(80),
      ...codeBlock([
        "# Tool 1: Session identity tracker",
        "./cam_ident",
        "# Then open/use/close camera app. Ctrl-C to stop.",
        "",
        "# Tool 2: Frame request pipeline tracker",
        "./cam_control",
        "# Open camera, take photos. High-frequency output at 30fps.",
        "",
        "# Tool 3: Buffer lifecycle tracker",
        "./cam_buffer",
        "# Open camera, take photos.",
        "",
        "# Tool 4: Correlation engine",
        "./cam_flow",
        "# Shows SESSION_START, BUF_DONE, FRAME_SKIP in unified timeline.",
      ]),
      spacer(160),

      h2("10.4 Running Multiple Tools Simultaneously"),
      spacer(80),
      p("All four tools can run simultaneously in separate terminal sessions (separate adb shell connections). They use separate BPF programs and ring buffers so they do not interfere with each other. This is the recommended way to run them for a complete picture:"),
      spacer(80),
      ...codeBlock([
        "# Terminal 1",
        "adb shell -> su -> sh /data/eadb/run -> cd camtrace -> ./cam_ident",
        "",
        "# Terminal 2",
        "adb shell -> su -> sh /data/eadb/run -> cd camtrace -> ./cam_control",
        "",
        "# Terminal 3",
        "adb shell -> su -> sh /data/eadb/run -> cd camtrace -> ./cam_buffer",
        "",
        "# Terminal 4",
        "adb shell -> su -> sh /data/eadb/run -> cd camtrace -> ./cam_flow",
      ]),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 11: KEY OBSERVATIONS
      // ============================================================
      h1("11. Key Observations from Real-Device Testing"),
      spacer(120),

      h2("11.1 Camera Stack on Motorola G96"),
      spacer(80),
      bullet("The camera HAL runs as a single process (vendor.qti.camera.provider, PID 1462) with a thread pool. All camera activity regardless of requesting app passes through this one process."),
      bullet("The HAL spawns 4-5 simultaneous camera contexts per session (ISP, sensor, lens, flash, stats processor). All contexts are managed concurrently."),
      bullet("The cam_context_state tracepoint cycles through states 2→3→2→3 (ACQUIRED→READY) for each preview frame reconfiguration. This is normal and expected behavior."),
      bullet("When taking a still photo, the pipeline briefly shows state 5 (FLUSHING) as it switches from preview mode to capture mode and back."),
      spacer(160),

      h2("11.2 Frame Rate Verification"),
      spacer(80),
      p("From cam_control output, REQ_ADDED events for a given device appear at consistent 33ms intervals during preview, confirming 30fps operation. From cam_buffer output, BUF_FILLED events with consecutive REQ values also appear at 33ms intervals, independently confirming the 30fps frame rate at the buffer completion level."),
      spacer(160),

      h2("11.3 Multi-Consumer Buffer Architecture"),
      spacer(80),
      p("Each frame buffer (each REQ_ID) generates 7-8 BUF_FILLED events in cam_buffer. This is because the Qualcomm camera driver notifies multiple consumers when a frame is ready: the display pipeline, video encoder, face detection module, auto-focus stats processor, auto-exposure stats processor, and the application buffer queue. All receive the same buffer via DMA-BUF sharing."),
      spacer(160),

      h2("11.4 App Identity Limitation"),
      spacer(80),
      p("All four tools report PID=1462 (the HAL) for camera events because the kernel tracepoints fire in the HAL process context. The originating app (WhatsApp, Google Camera, PDF reader, etc.) is two Binder IPC hops away from where the tracepoints fire. Identifying the requesting app requires correlating Binder transaction events with the HAL tracepoints using the PID-to-app mapping from Android's package manager, which is a planned enhancement for cam_flow."),
      spacer(160),

      h2("11.5 Frame Skips During App Transitions"),
      spacer(80),
      p("cam_flow detected multiple FRAME_SKIP events (from cam_notify_frame_skip) during the test session. These occurred at predictable moments: immediately after opening the camera app (pipeline warming up), when pressing the shutter button (switching from preview to capture mode), and when closing the camera app (pipeline teardown). No frame skips occurred during steady-state preview or during still photo processing itself."),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 12: PORTING TO OTHER DEVICES
      // ============================================================
      h1("12. Porting to Other Devices"),
      spacer(120),

      h2("12.1 Porting from Motorola G96 to Another Qualcomm Device"),
      spacer(80),
      p("Since cam_ident, cam_control, cam_buffer, and cam_flow all use Qualcomm-specific camera tracepoints, they should work on any rooted Android device running a Qualcomm Snapdragon SoC with these tracepoints present. The porting steps are:"),
      spacer(80),
      numbered("Verify Qualcomm camera tracepoints exist: ls /sys/kernel/tracing/events/camera/"),
      numbered("Regenerate vmlinux.h from the target device's kernel BTF: adb shell bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h"),
      numbered("Update Makefile VMLINUX_DIR and __TARGET_ARCH flag (arm64 for most Android devices)"),
      numbered("Check kptr_restrict and set to 0 if needed"),
      numbered("Verify dma_buf_export is present: grep dma_buf_export /proc/kallsyms"),
      spacer(160),

      h2("12.2 Porting to Non-Qualcomm Devices (MediaTek, Samsung Exynos)"),
      spacer(80),
      p("Non-Qualcomm devices will not have the camera tracepoints used by this suite. The porting approach would be:"),
      spacer(80),
      bullet("Replace all tracepoint/camera/* hooks with kprobes on the equivalent HAL functions for that SoC"),
      bullet("Check /sys/kernel/tracing/events/ for any vendor-specific camera tracepoints"),
      bullet("As a fallback, use kprobe/binder_transaction filtered to cameraserver PID for session tracking"),
      bullet("Use kprobe/dma_buf_export (universal across all Linux kernels) for buffer tracking"),
      spacer(160),

      h2("12.3 Porting from Physical Device Back to Emulator"),
      spacer(80),
      p("The emulator (Pixel 6a via Android Studio) does not support the Qualcomm camera tracepoints and has no /dev/video* devices. The emulator camera uses a fully virtual path. For emulator-compatible operation, cam_ident would need to be rewritten to use kprobe/binder_transaction filtered by comm name matching cameraserver, and cam_control, cam_buffer, and cam_flow would have no equivalent hooks available."),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 13: FILE STRUCTURE
      // ============================================================
      h1("13. Project File Structure"),
      spacer(120),
      ...codeBlock([
        "bpf-developer-tutorial/src/camtrace/",
        "├── Makefile              Build system for all 4 tools",
        "├── camtrace.h            Shared structs and event type constants",
        "│",
        "├── cam_ident.bpf.c       Kernel-side BPF: cam_context_state + cam_flush_req",
        "├── cam_ident.c           Userspace: loads, attaches, prints session events",
        "│",
        "├── cam_control.bpf.c     Kernel-side BPF: cam_apply_req + cam_req_mgr_add_req",
        "│                                         + cam_submit_to_hw",
        "├── cam_control.c         Userspace: prints frame request pipeline events",
        "│",
        "├── cam_buffer.bpf.c      Kernel-side BPF: cam_buf_done + cam_irq_activated",
        "│                                         + kprobe/dma_buf_export",
        "├── cam_buffer.c          Userspace: prints buffer lifecycle events",
        "│",
        "├── cam_flow.bpf.c        Kernel-side BPF: cam_context_state + cam_submit_to_hw",
        "│                                         + cam_buf_done + cam_notify_frame_skip",
        "├── cam_flow.c            Userspace: unified timeline with latency calculation",
        "│",
        "└── .output/              Build artifacts (auto-generated)",
        "    ├── libbpf.a          Static libbpf built from source",
        "    ├── *.bpf.o           Compiled BPF bytecode",
        "    ├── *.skel.h          Auto-generated BPF skeleton headers",
        "    └── *.o               Compiled userspace objects",
      ]),
      spacer(80),
      new Paragraph({ children: [new PageBreak()] }),

      // ============================================================
      // SECTION 14: FUTURE WORK
      // ============================================================
      h1("14. Future Work and Enhancements"),
      spacer(120),

      h2("14.1 Fix Latency Calculation"),
      spacer(80),
      p("The most impactful improvement would be correctly measuring per-frame submit-to-done latency. This requires mapping between the req_id namespace in cam_submit_to_hw and the request namespace in cam_buf_done. This could be done by reading the intermediate fields from cam_apply_req which sits between the two and may expose the mapping."),
      spacer(160),

      h2("14.2 App Identity Correlation"),
      spacer(80),
      p("The most important missing feature is identifying which app initiated each camera session. This requires adding a Binder transaction hook to catch the IPC call from cameraserver to the HAL, extracting the caller PID from the Binder metadata, and then mapping that PID to an Android package name via /proc/PID/cmdline. This would make cam_flow able to report [APP=com.whatsapp] for each session."),
      spacer(160),

      h2("14.3 FPS Calculator"),
      spacer(80),
      p("Count BUF_FILLED events per second for a given REQ sequence and compute rolling FPS. This could be implemented entirely in userspace by tracking the timestamp difference between consecutive REQ completions."),
      spacer(160),

      h2("14.4 Suspicious Camera Access Detection"),
      spacer(80),
      p("Once app identity is working, add a detection mode that alerts when an app opens the camera while in the background (foreground app PID does not match camera-opening PID). This is the privacy/security use case that makes this research directly applicable to Android security."),
      spacer(160),

      h2("14.5 Persistent Logging"),
      spacer(80),
      p("Add output file mode so events are written to a structured log file (JSON or CSV) rather than just printed to terminal. This would allow post-processing, visualization, and comparison across sessions."),
      spacer(80),

      spacer(200),
      new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { before: 400 },
        children: [new TextRun({ text: "— End of Documentation —", size: 22, font: "Arial", color: "888888", italics: true })]
      }),
    ]
  }]
});

Packer.toBuffer(doc).then(buffer => {
  fs.writeFileSync('./camtrace_documentation.docx', buffer);
  console.log('Done: camtrace_documentation.docx');
});
