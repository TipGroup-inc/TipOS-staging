# TipOS — Build System
#
# Kernel: OvsbMkM — delega ao Makefile interno
# Userland: src/userland/ — compila .c → .macho → disk.img

KERNEL_DIR := OvsbMkM
ISO        := TipOS.iso

.PHONY: all kernel iso disk run run-curses clean

all: kernel iso

# ── Kernel (delega ao Makefile do OvsbMkM) ──

kernel:
	$(MAKE) -C $(KERNEL_DIR)

iso: kernel
	cp $(KERNEL_DIR)/build/kernel.elf $(KERNEL_DIR)/iso/boot/kernel.elf
	grub-mkrescue -o $(ISO) $(KERNEL_DIR)/iso 2>/dev/null || true

# ── Disco FAT32 (cria se não existir) ──

disk.img:
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	mkfs.fat -F 32 $@ > /dev/null 2>&1

userland: disk.img
	$(MAKE) -C src/userland install

# ── Run ──

run: all userland
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 256M \
		-drive file=disk.img,format=raw,index=0

run-curses: all userland
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 256M -display curses \
		-drive file=disk.img,format=raw,index=0

clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	rm -f $(ISO)
