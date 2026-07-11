CC := gcc
NASM := nasm
GRUB := grub-mkrescue
QEMU := qemu-system-x86_64

BUILD_DIR := build
ISO_DIR := iso/boot
ISO := OvsbMkM.iso

CFLAGS := -ffreestanding -nostdlib -mno-red-zone -mno-mmx -mno-sse -mgeneral-regs-only -Wall -O0 -I src/kernel -I src/drivers -I src/fs -I src/commands -I .
NASM_FLAGS := -f elf64
LDFLAGS := -T src/kernel/linker.ld

SRCS := \
    src/kernel/kernel.c \
    src/kernel/syscall.c \
    src/kernel/idt.c \
    src/kernel/test_idt.c \
    src/kernel/memory.c \
    src/kernel/mach_o.c \
    src/kernel/smc.c \
    src/kernel/nvram.c \
    src/kernel/test_macho.c \
    src/kernel/bash_bin.c \
    src/kernel/dyld.c \
    src/kernel/libsystem_bin.c \
    src/kernel/ls_bin.c

SRCS += src/drivers/keyboard.c
SRCS += src/drivers/ata.c
SRCS += src/fs/fat32.c
SRCS += src/commands/shell_cmds.c
SRCS += src/kernel/pic.c

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all iso run clean

all: $(BUILD_DIR)/kernel.elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot64.o: src/kernel/boot64.asm | $(BUILD_DIR)
	$(NASM) $(NASM_FLAGS) -o $@ $<

IDT_ASM := src/kernel/idt.asm
IDT_OBJ := $(BUILD_DIR)/idt_asm.o

$(IDT_OBJ): $(IDT_ASM) | $(BUILD_DIR)
	$(NASM) -f elf64 -o $@ $<

SYSCALL_ASM := src/kernel/syscall_entry.asm
SYSCALL_OBJ := $(BUILD_DIR)/syscall_entry.o
$(SYSCALL_OBJ): $(SYSCALL_ASM) | $(BUILD_DIR)
	$(NASM) -f elf64 -o $@ $<

KEYBOARD_ASM := src/drivers/keyboard_asm.asm
KEYBOARD_OBJ := $(BUILD_DIR)/keyboard_asm.o
$(KEYBOARD_OBJ): $(KEYBOARD_ASM) | $(BUILD_DIR)
	$(NASM) -f elf64 -o $@ $<


$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/boot64.o $(IDT_OBJ) $(SYSCALL_OBJ) $(KEYBOARD_OBJ) $(OBJS) src/kernel/linker.ld | $(BUILD_DIR)
	$(CC) $(CFLAGS) -nostdlib -no-pie -o $@ $(BUILD_DIR)/boot64.o $(IDT_OBJ) $(SYSCALL_OBJ) $(KEYBOARD_OBJ) $(OBJS) -Wl,$(LDFLAGS)

iso: $(BUILD_DIR)/kernel.elf
	mkdir -p $(ISO_DIR)
	cp $(BUILD_DIR)/kernel.elf $(ISO_DIR)/kernel.elf
	$(GRUB) -o $(ISO) iso 2>/dev/null || true

run: iso
	$(QEMU) -cdrom $(ISO) -m 256M

clean:
	rm -rf $(BUILD_DIR) $(ISO)
