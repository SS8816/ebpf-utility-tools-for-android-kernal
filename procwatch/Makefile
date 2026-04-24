# SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)

OUTPUT := .output
CLANG ?= clang
CC ?= gcc

LIBBPF_SRC := $(abspath ../third_party/libbpf/src)
LIBBPF_OBJ := $(abspath $(OUTPUT)/libbpf.a)

BPFTOOL := $(shell command -v bpftool 2>/dev/null)
ifeq ($(strip $(BPFTOOL)),)
$(error bpftool not found in PATH. Install it or set BPFTOOL=/full/path/to/bpftool)
endif

ARCH ?= $(shell uname -m | sed 's/x86_64/x86/' \
                         | sed 's/arm.*/arm/' \
                         | sed 's/aarch64/arm64/' \
                         | sed 's/ppc64le/powerpc/' \
                         | sed 's/mips.*/mips/' \
                         | sed 's/riscv64/riscv/' \
                         | sed 's/loongarch64/loongarch/')
VMLINUX := ../third_party/vmlinux/$(ARCH)/vmlinux.h

INCLUDES := -I$(OUTPUT) \
            -I../third_party/libbpf/include/uapi \
            -I$(dir $(VMLINUX))

CFLAGS := -g -Wall
ALL_LDFLAGS := $(LDFLAGS) $(EXTRA_LDFLAGS)

APP := procwatch

CLANG_BPF_SYS_INCLUDES ?= $(shell $(CLANG) -v -E - </dev/null 2>&1 \
        | sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

.PHONY: all
all: $(APP)

$(OUTPUT):
	mkdir -p $@

$(OUTPUT)/libbpf:
	mkdir -p $@

$(LIBBPF_OBJ): $(wildcard $(LIBBPF_SRC)/*.[ch] $(LIBBPF_SRC)/Makefile) | $(OUTPUT)/libbpf
	$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1 \
		OBJDIR=$(dir $@)/libbpf DESTDIR=$(dir $@) \
		INCLUDEDIR= LIBDIR= UAPIDIR= install

$(OUTPUT)/%.bpf.o: %.bpf.c $(VMLINUX) $(LIBBPF_OBJ) | $(OUTPUT)
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		$(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		-c $< -o $@

$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT) $(BPFTOOL)
	$(BPFTOOL) gen skeleton $< > $@

$(OUTPUT)/%.o: %.c $(OUTPUT)/%.skel.h | $(OUTPUT)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(APP): $(OUTPUT)/$(APP).o $(LIBBPF_OBJ)
	$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz -o $@

.PHONY: clean
clean:
	rm -rf $(OUTPUT) $(APP)

.DELETE_ON_ERROR:
.SECONDARY: