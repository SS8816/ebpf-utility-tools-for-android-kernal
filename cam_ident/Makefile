OUTPUT := .output

CLANG ?= clang
CC ?= cc
BPFTOOL ?= /usr/sbin/bpftool

VMLINUX_DIR := ../third_party/vmlinux/arm64
LIBBPF_DIR := ../third_party/bpftool/libbpf
LIBBPF_SRC := $(LIBBPF_DIR)/src
LIBBPF_BUILD := $(OUTPUT)/libbpf-build
LIBBPF_A := $(OUTPUT)/libbpf.a

CFLAGS := -g -Wall -O2 -I$(OUTPUT) -I$(VMLINUX_DIR) -I$(LIBBPF_SRC) -I$(LIBBPF_DIR)/include -I$(LIBBPF_DIR)/include/uapi -I.
BPF_CFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_arm64 -Wno-unknown-attributes -I$(VMLINUX_DIR) -I$(LIBBPF_SRC) -I$(LIBBPF_DIR)/include -I$(LIBBPF_DIR)/include/uapi -I.

all: cam_ident cam_control cam_buffer cam_flow cam_identity

$(OUTPUT):
	mkdir -p $(OUTPUT)

$(LIBBPF_A): | $(OUTPUT)
	mkdir -p $(LIBBPF_BUILD)
	$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1 OBJDIR=$(abspath $(LIBBPF_BUILD))
	cp $(LIBBPF_BUILD)/libbpf.a $@

$(OUTPUT)/%.bpf.o: %.bpf.c $(VMLINUX_DIR)/vmlinux.h | $(OUTPUT)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

$(OUTPUT)/%.o: %.c $(OUTPUT)/%.skel.h | $(OUTPUT)
	$(CC) $(CFLAGS) -c $< -o $@

cam_ident: $(OUTPUT)/cam_ident.o $(LIBBPF_A)
	$(CC) -g -Wall $(OUTPUT)/cam_ident.o $(LIBBPF_A) -lelf -lz -o $@

cam_control: $(OUTPUT)/cam_control.o $(LIBBPF_A)
	$(CC) -g -Wall $(OUTPUT)/cam_control.o $(LIBBPF_A) -lelf -lz -o $@

cam_buffer: $(OUTPUT)/cam_buffer.o $(LIBBPF_A)
	$(CC) -g -Wall $(OUTPUT)/cam_buffer.o $(LIBBPF_A) -lelf -lz -o $@

cam_flow: $(OUTPUT)/cam_flow.o $(LIBBPF_A)
	$(CC) -g -Wall $(OUTPUT)/cam_flow.o $(LIBBPF_A) -lelf -lz -o $@

cam_identity: $(OUTPUT)/cam_identity.o $(LIBBPF_A)
	$(CC) -g -Wall $(OUTPUT)/cam_identity.o $(LIBBPF_A) -lelf -lz -o $@

clean:
	rm -rf $(OUTPUT) cam_ident cam_control cam_buffer cam_flow

.PHONY: all clean