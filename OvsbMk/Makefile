CC := gcc
NASM := nasm
GRUB := grub-mkrescue

BUILD_DIR := build
ISO_DIR := iso/boot
ISO := OvsbOS.iso

CFLAGS := -ffreestanding -nostdlib -mno-red-zone -mno-mmx -mno-sse -mgeneral-regs-only -Wall -O0 -I src/kernel -I src/drivers -I src/fs -I src/lib/gui -I os/shell -I os/wm
NASM_FLAGS := -f elf64
LDFLAGS := -T src/kernel/linker.ld

SRCS := src/kernel/kernel.c src/kernel/vga.c src/kernel/serial.c src/kernel/utils.c
SRCS += src/kernel/key.c src/kernel/env.c src/kernel/pit.c src/kernel/rtc.c src/kernel/parse_mb2.c
SRCS += src/kernel/syscall.c src/kernel/idt.c src/kernel/memory.c src/kernel/pic.c
SRCS += src/kernel/ring3.c src/kernel/process.c
SRCS += src/kernel/test_idt.c src/kernel/mach_o.c src/kernel/disp_api.c
SRCS += src/drivers/keyboard.c src/drivers/ata.c src/drivers/vga_gfx.c src/drivers/mouse.c
SRCS += src/fs/fat32.c
SRCS += src/lib/gui/vesa.c src/lib/gui/gui.c
SRCS += os/shell/shell_cmds.c os/shell/execute_command.c
SRCS += os/wm/compositor.c

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

all: iso

$(BUILD_DIR): ; mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot64.o: src/kernel/boot64.asm | $(BUILD_DIR)
	$(NASM) $(NASM_FLAGS) -o $@ $<
$(BUILD_DIR)/idt_asm.o: src/kernel/idt.asm | $(BUILD_DIR)
	$(NASM) -f elf64 -o $@ $<
$(BUILD_DIR)/syscall_entry.o: src/kernel/syscall_entry.asm | $(BUILD_DIR)
	$(NASM) -f elf64 -o $@ $<
$(BUILD_DIR)/keyboard_asm.o: src/drivers/keyboard_asm.asm | $(BUILD_DIR)
	$(NASM) -f elf64 -o $@ $<
$(BUILD_DIR)/switch_asm.o: src/kernel/switch.asm | $(BUILD_DIR)
	$(NASM) -f elf64 -o $@ $<

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
$(KERNEL_ELF): $(BUILD_DIR)/boot64.o $(BUILD_DIR)/idt_asm.o $(BUILD_DIR)/syscall_entry.o $(BUILD_DIR)/keyboard_asm.o $(BUILD_DIR)/switch_asm.o $(OBJS)
	$(CC) $(CFLAGS) -nostdlib -no-pie -o $@ $^ -Wl,$(LDFLAGS)

iso: $(KERNEL_ELF)
	mkdir -p $(ISO_DIR)
	cp $(KERNEL_ELF) $(ISO_DIR)/kernel.elf
	$(GRUB) -o $(ISO) iso 2>/dev/null || true

clean:
	rm -rf $(BUILD_DIR) $(ISO)
