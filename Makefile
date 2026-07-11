# TipOS — Build System
#
# Kernel: OvsbMkM (Bugsappetit.inc) — fonte intocável
# Userland: TipOS (src/userland/) — display server, WM, apps
#
# O kernel tem issues de compilação no GCC 15:
# - ata.c: inw/outw com %edx em vez de %dx
# A correção é aplicada via sed no assembly, sem tocar no fonte.

KERNEL_DIR := OvsbMkM
BUILD_DIR  := build
ISO_DIR    := iso/boot
ISO        := TipOS.iso

CC := gcc
AS := as
LD := ld

KERNEL_CFLAGS := -ffreestanding -nostdlib -mno-red-zone -mno-mmx -mno-sse \
                 -mgeneral-regs-only -Wall -O0 \
                 -I $(KERNEL_DIR)/src/kernel \
                 -I $(KERNEL_DIR)/src/drivers \
                 -I $(KERNEL_DIR)/src/fs \
                 -I $(KERNEL_DIR)/src/commands \
                 -I $(KERNEL_DIR)

USER_CFLAGS := -ffreestanding -nostdlib -Wall -Wextra -O0 -g \
               -I $(KERNEL_DIR)/src/kernel \
               -I src/userland/include

.PHONY: all kernel userland iso disk run clean

all: kernel userland iso

# ── Kernel OvsbMkM (compila fora da árvore) ──

KERNEL_SRCS := \
    $(KERNEL_DIR)/src/kernel/kernel.c \
    $(KERNEL_DIR)/src/kernel/syscall.c \
    $(KERNEL_DIR)/src/kernel/idt.c \
    $(KERNEL_DIR)/src/kernel/test_idt.c \
    $(KERNEL_DIR)/src/kernel/memory.c \
    $(KERNEL_DIR)/src/kernel/mach_o.c \
    $(KERNEL_DIR)/src/kernel/smc.c \
    $(KERNEL_DIR)/src/kernel/nvram.c \
    $(KERNEL_DIR)/src/kernel/bash_bin.c \
    $(KERNEL_DIR)/src/kernel/dyld.c \
    $(KERNEL_DIR)/src/kernel/libsystem_bin.c \
    $(KERNEL_DIR)/src/kernel/ls_bin.c \
    $(KERNEL_DIR)/src/kernel/test_macho.c \
    $(KERNEL_DIR)/src/drivers/keyboard.c \
    $(KERNEL_DIR)/src/drivers/vga_gfx.c \
    $(KERNEL_DIR)/src/commands/shell_cmds.c \
    $(KERNEL_DIR)/src/commands/compositor.c \
    $(KERNEL_DIR)/src/kernel/pic.c \
    $(KERNEL_DIR)/src/fs/fat32.c

# ata.c compila com fix de assembly (GCC 15 + inw/outw)
KERNEL_ATA_OBJ := $(BUILD_DIR)/ata.o
KERNEL_ASM_OBJS := \
    $(BUILD_DIR)/boot64.o \
    $(BUILD_DIR)/idt_asm.o \
    $(BUILD_DIR)/syscall_entry.o \
    $(BUILD_DIR)/keyboard_asm.o

KERNEL_OBJS := $(KERNEL_ASM_OBJS) $(KERNEL_ATA_OBJ)
KERNEL_OBJS += $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))

# Montar assembly do kernel
$(BUILD_DIR)/boot64.o: $(KERNEL_DIR)/src/kernel/boot64.asm
	mkdir -p $(dir $@)
	nasm -f elf64 -o $@ $<

$(BUILD_DIR)/idt_asm.o: $(KERNEL_DIR)/src/kernel/idt.asm
	mkdir -p $(dir $@)
	nasm -f elf64 -o $@ $<

$(BUILD_DIR)/syscall_entry.o: $(KERNEL_DIR)/src/kernel/syscall_entry.asm
	mkdir -p $(dir $@)
	nasm -f elf64 -o $@ $<

$(BUILD_DIR)/keyboard_asm.o: $(KERNEL_DIR)/src/drivers/keyboard_asm.asm
	mkdir -p $(dir $@)
	nasm -f elf64 -o $@ $<

# ata.c com fix de compatibilidade (GCC 15 → inw/outw)
$(KERNEL_DIR)/build/ata.s: $(KERNEL_DIR)/src/drivers/ata.c
	mkdir -p $(dir $@)
	$(CC) $(KERNEL_CFLAGS) -S $< -o $@
	sed -i 's/inw %edx, %ax/inw %dx, %ax/g; s/outw %ax, %edx/outw %ax, %dx/g' $@

$(KERNEL_ATA_OBJ): $(KERNEL_DIR)/build/ata.s
	$(AS) --64 -o $@ $<

# Compilar C do kernel
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# Link do kernel
$(BUILD_DIR)/kernel.elf: $(KERNEL_OBJS) $(KERNEL_DIR)/src/kernel/linker.ld
	$(CC) -ffreestanding -nostdlib -mno-red-zone -mno-mmx -mno-sse \
	      -mgeneral-regs-only -Wall -O0 -nostdlib -no-pie \
	      -o $@ $(KERNEL_OBJS) -Wl,-T $(KERNEL_DIR)/src/kernel/linker.ld

kernel: $(BUILD_DIR)/kernel.elf

# ── Userland (TipOS) ──

disk.img:
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	mkfs.fat -F 32 $@ > /dev/null 2>&1

userland: disk.img
	$(MAKE) -C src/userland install

# ── ISO ──

iso: kernel
	mkdir -p $(ISO_DIR)
	cp $(BUILD_DIR)/kernel.elf $(ISO_DIR)/kernel.elf
	cp $(KERNEL_DIR)/iso/boot/grub/grub.cfg $(ISO_DIR)/grub/ 2>/dev/null || true
	grub-mkrescue -o $(ISO) iso 2>/dev/null || true

run: all
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 256M -serial stdio -drive file=disk.img,format=raw,if=ide

run-curses: all
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 256M -display curses -drive file=disk.img,format=raw,if=ide

clean:
	rm -rf $(BUILD_DIR) $(ISO) $(KERNEL_DIR)/build/
