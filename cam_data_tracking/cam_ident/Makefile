APP := cam_ident
OUTPUT := .output

CLANG ?= clang
CC ?= cc
BPFTOOL ?= /usr/sbin/bpftool

VMLINUX_DIR := ../third_party/vmlinux/x86
LIBBPF_DIR  := ../third_party/libbpf
LIBBPF_SRC  := $(LIBBPF_DIR)/src
LIBBPF_BUILD := $(OUTPUT)/libbpf-build
LIBBPF_A    := $(OUTPUT)/libbpf.a

BPF_C    := $(APP).bpf.c
USER_C   := $(APP).c
BPF_OBJ  := $(OUTPUT)/$(APP).bpf.o
SKEL_H   := $(OUTPUT)/$(APP).skel.h
USER_OBJ := $(OUTPUT)/$(APP).o
TARGET   := $(APP)

CFLAGS     := -g -Wall -O2 -I$(OUTPUT) -I$(VMLINUX_DIR) -I$(LIBBPF_DIR)/include -I$(LIBBPF_DIR)/include/uapi -I.
BPF_CFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_x86 -Wno-unknown-attributes \
              -I$(VMLINUX_DIR) -I$(LIBBPF_DIR)/include -I$(LIBBPF_DIR)/include/uapi -I.

all: $(TARGET)

$(OUTPUT):
	mkdir -p $(OUTPUT)

$(BPF_OBJ): $(BPF_C) $(VMLINUX_DIR)/vmlinux.h | $(OUTPUT)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

$(SKEL_H): $(BPF_OBJ)
	$(BPFTOOL) gen skeleton $< > $@

$(LIBBPF_A): | $(OUTPUT)
	mkdir -p $(LIBBPF_BUILD)
	$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1 OBJDIR=$(abspath $(LIBBPF_BUILD))
	cp $(LIBBPF_BUILD)/libbpf.a $@

$(USER_OBJ): $(USER_C) $(SKEL_H) | $(OUTPUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(USER_OBJ) $(LIBBPF_A)
	$(CC) -g -Wall $(USER_OBJ) $(LIBBPF_A) -lelf -lz -o $@

clean:
	rm -rf $(OUTPUT) $(TARGET)

.PHONY: all clean