# SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
#
# pksniff Makefile
# Cross-compiles for arm64 Android physical devices (no kernel BTF required).
#
# Usage:
#   make              — build for the host arch (works inside Debian chroot)
#   make ARCH=arm64   — explicitly target arm64
#
# Prerequisites (Debian chroot on device):
#   apt install clang llvm bpftool libelf-dev zlib1g-dev libncurses-dev

OUTPUT    := .output
CLANG     ?= clang
CC        ?= gcc

LIBBPF_SRC := $(abspath ../third_party/bpftool/libbpf/src)
LIBBPF_OBJ := $(abspath $(OUTPUT)/libbpf.a)

BPFTOOL := $(shell command -v bpftool 2>/dev/null)
ifeq ($(strip $(BPFTOOL)),)
$(error bpftool not found in PATH. Install it: apt install linux-tools-common)
endif

# Detect arch (same logic as original pktrace Makefile)
ARCH ?= $(shell uname -m \
          | sed 's/x86_64/x86/'  \
          | sed 's/arm.*/arm/'   \
          | sed 's/aarch64/arm64/' \
          | sed 's/ppc64le/powerpc/' \
          | sed 's/mips.*/mips/' \
          | sed 's/riscv64/riscv/' \
          | sed 's/loongarch64/loongarch/')

VMLINUX := ../third_party/vmlinux/$(ARCH)/vmlinux.h

INCLUDES := -I$(OUTPUT) \
            -I../third_party/bpftool/libbpf/include/uapi \
            -I$(dir $(VMLINUX)) \
            -I.

CFLAGS        := -g -Wall -Wextra -Wno-unused-parameter
ALL_LDFLAGS   := $(LDFLAGS) $(EXTRA_LDFLAGS)

# ncurses: try ncursesw first, fall back to ncurses
NCURSES_LIB   := $(shell pkg-config --libs ncursesw 2>/dev/null || echo -lncurses)
NCURSES_FLAGS := $(shell pkg-config --cflags ncursesw 2>/dev/null || true)

APPS := pksniff

CLANG_BPF_SYS_INCLUDES ?= $(shell $(CLANG) -v -E - </dev/null 2>&1 \
        | sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

.PHONY: all clean btf-help
all: $(APPS)

# ── libbpf static library ─────────────────────────────────────────────────
$(OUTPUT):
	mkdir -p $@

$(OUTPUT)/libbpf:
	mkdir -p $@

$(LIBBPF_OBJ): $(wildcard $(LIBBPF_SRC)/*.[ch] $(LIBBPF_SRC)/Makefile) | $(OUTPUT)/libbpf
	$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1 \
		OBJDIR=$(dir $@)/libbpf DESTDIR=$(dir $@) \
		INCLUDEDIR= LIBDIR= UAPIDIR= install

# ── BPF object ────────────────────────────────────────────────────────────
$(OUTPUT)/%.bpf.o: %.bpf.c $(VMLINUX) $(LIBBPF_OBJ) | $(OUTPUT)
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) -Wno-unknown-attributes \
 		$(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
 		-c $< -o $@

# ── BPF skeleton ──────────────────────────────────────────────────────────
$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT)
	$(BPFTOOL) gen skeleton $< > $@

# ── userspace object ──────────────────────────────────────────────────────
$(OUTPUT)/%.o: %.c $(OUTPUT)/%.skel.h | $(OUTPUT)
	$(CC) $(CFLAGS) $(INCLUDES) $(NCURSES_FLAGS) -c $< -o $@

# ── final binary ──────────────────────────────────────────────────────────
$(APPS): %: $(OUTPUT)/%.o $(LIBBPF_OBJ)
	$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz $(NCURSES_LIB) -lpthread -o $@
	@echo ""
	@echo "Build complete: $@"
	@echo "Usage: ./$@ <ifname> [-b vmlinux.btf]"
	@echo "  e.g. ./$@ wlan0 -b /data/local/tmp/vmlinux.btf"

# ── BTF generation helper ─────────────────────────────────────────────────
# Run this on a host with matching kernel sources to generate vmlinux.btf
# then adb push it to the device.
btf-help:
	@echo "To generate vmlinux.btf for a kernel without CONFIG_DEBUG_INFO_BTF:"
	@echo ""
	@echo "  Option A — from vmlinux ELF (if you have the kernel build):"
	@echo "    pahole --btf_encode_detached vmlinux.btf /path/to/vmlinux"
	@echo ""
	@echo "  Option B — from /proc/config.gz on device (approximate):"
	@echo "    1) On device: zcat /proc/config.gz > kernel.config"
	@echo "    2) Build kernel with same config + CONFIG_DEBUG_INFO_BTF=y"
	@echo "    3) pahole --btf_encode_detached vmlinux.btf vmlinux"
	@echo ""
	@echo "  Then push:"
	@echo "    adb push vmlinux.btf /data/local/tmp/"
	@echo "    ./pksniff wlan0 -b /data/local/tmp/vmlinux.btf"

clean:
	rm -rf $(OUTPUT) $(APPS)

.DELETE_ON_ERROR:
.SECONDARY: