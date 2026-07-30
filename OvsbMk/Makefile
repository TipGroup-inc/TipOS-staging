/* moe moe kyun <3 */
/* moe moe kyun <3 */
# ♥ Makefile OvsbMkM ~ Kernel OvsbMk para TipOS
CC := gcc
NASM := nasm
CFLAGS := -ffreestanding -nostdlib -mno-red-zone -mno-mmx -mno-sse
CFLAGS += -mgeneral-regs-only -m64 -O2 -Wall
LDFLAGS := -nostdlib -no-pie -Wl,-nostdlib -T kernel/linker.ld

BUILD_DIR := build

VPATH := kernel:drivers:lib/gui:lib/owt:lib/wm:fs

C_SRCS := $(notdir $(wildcard kernel/*.c drivers/*.c lib/gui/*.c lib/owt/*.c lib/wm/*.c fs/*.c))
ASM_SRCS := boot64.o idt_asm.o keyboard_asm.o switch_asm.o
OBJS := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o) $(ASM_SRCS))

all: $(BUILD_DIR)/kernel.elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot64.o: boot64.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

$(BUILD_DIR)/idt_asm.o: idt.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

$(BUILD_DIR)/keyboard_asm.o: keyboard_asm.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

$(BUILD_DIR)/switch_asm.o: switch.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I. -Ikernel -Idrivers -Ilib/gui -Ilib/owt -Ilib/wm -Ifs -c -o $@ $<

$(BUILD_DIR)/kernel.elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

iso: OvsbMkM.iso
	@echo "OvsbMkM.iso gerada!"

disk.img:
	dd if=/dev/zero of=$@ bs=1M count=64
	mkfs.fat -F 32 $@
	@echo "disk.img criado!"

user_prog.rebuild: user_prog.asm
	nasm -f bin -o user_prog.bin $<
	python3 -c "import sys; data=open('user_prog.bin','rb').read(); print('const uint8_t _binary_user_prog_start[] = {'); [print('    '+','.join(f'0x{b:02x}' for b in data[i:i+16])+',') for i in range(0,len(data),16)]; print('};'); print(f'const int _binary_user_prog_size = {len(data)};')" > kernel/user_prog_bin.h
	rm -f user_prog.bin
	@echo "user_prog_bin.h atualizado!"

clean:
	rm -rf $(BUILD_DIR) *.o kernel.elf OvsbMkM.iso user_prog.bin
	rm -rf iso/boot/kernel.elf

run: OvsbMkM.iso disk.img
	qemu-system-x86_64 -vga virtio -boot d -cdrom OvsbMkM.iso -m 512M -serial stdio -enable-kvm -drive file=disk.img,format=raw,if=ide

run-no-kvm: OvsbMkM.iso disk.img
	qemu-system-x86_64 -vga virtio -boot d -cdrom OvsbMkM.iso -m 512M -serial stdio -drive file=disk.img,format=raw,if=ide

OvsbMkM.iso: $(BUILD_DIR)/kernel.elf
	mkdir -p iso/boot
	cp $(BUILD_DIR)/kernel.elf iso/boot/
	grub-mkrescue -o OvsbMkM.iso iso 2>/dev/null || true

.PHONY: all clean run iso user_prog.rebuild
