# TipOS — Build System
#
# Kernel: OvsbMkM — delega ao Makefile interno
# Userland: src/userland/ — compila .c → .macho → disk.img

KERNEL_DIR := OvsbMk
ISO        := TipOS.iso
RUST_DIR   := src/rust

.PHONY: all rust kernel iso disk run run-curses clean

all: rust kernel iso

# ── Rust kernel crate ──

rust:
	cd $(RUST_DIR) && rustup run nightly cargo build --release --target x86_64-unknown-none 2>&1

# ── Kernel (delega ao Makefile do OvsbMk) ──

kernel: rust
	$(MAKE) -C $(KERNEL_DIR)

iso: kernel
	cp $(KERNEL_DIR)/build/kernel.elf $(KERNEL_DIR)/iso/boot/kernel.elf
	grub-mkrescue -o $(ISO) $(KERNEL_DIR)/iso 2>/dev/null || true

# ── Disco FAT32 (cria se não existir) ──

disk.img:
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	mkfs.fat -F 32 $@ > /dev/null 2>&1
	mmd -i $@ ::/BIN ::/USR ::/APPS ::/LOCAL 2>/dev/null || true

userland: disk.img
	$(MAKE) -C src/userland install
	$(MAKE) -C disp-wm install TIPOS_SDK=$(CURDIR)

# ── Run ──

run: all disk.img
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 512M \
		-drive file=disk.img,format=raw,index=0

run-curses: all disk.img
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 512M -display curses \
		-drive file=disk.img,format=raw,index=0

run-test: all disk.img userland
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 512M -no-reboot \
		-drive file=disk.img,format=raw,index=0 \
		-serial file:/tmp/tipos-boot.log -display none

clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	rm -f $(ISO) disk.img
