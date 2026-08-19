# moe moe kyun <3
# Makefile raiz do OvsbOS
# Kernel: delega pro OvsbMk/ (Makefile interno).
# Userland: compila .c → .macho → mcopy pro disk.img.

# Variáveis de caminho
KERNEL_DIR := OvsbMk
ISO        := OvsbOS.iso

# Metas que sempre executam
.PHONY: all kernel iso disk boot-binaries run run-curses run-test clean userland

# all: kernel + ISO
all: kernel iso

# kernel: compila o kernel
kernel:
	$(MAKE) -C $(KERNEL_DIR)

# iso: gera ISO bootável
iso: kernel
	cp $(KERNEL_DIR)/build/kernel.elf $(KERNEL_DIR)/iso/boot/kernel.elf
	grub-mkrescue -o $(ISO) $(KERNEL_DIR)/iso 2>/dev/null || true

# disk.img: cria disco FAT32 de 64MB
disk.img:
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	/sbin/mkfs.fat -F 32 $@ > /dev/null 2>&1
	mmd -i $@ ::/BIN ::/USR ::/APPS ::/LOCAL 2>/dev/null || true

# Testes ELF freestanding usados pelo boot do kernel.
boot-binaries: disk.img
	@gcc -static -nostdlib -ffreestanding -fno-stack-protector -no-pie \
		-Wl,--build-id=none -e _start OvsbMk/tests/hello.c -o /tmp/HELLO
	@mcopy -o -i disk.img /tmp/HELLO ::/HELLO
	@gcc -static -nostdlib -ffreestanding -fno-stack-protector -no-pie \
		-Wl,--build-id=none -e _start OvsbMk/tests/ttest.c -o /tmp/TTEST
	@mcopy -o -i disk.img /tmp/TTEST ::/TTEST

# userland: compila apenas o userland local (graphy)
# Os repos irmãos ../disp e ../term são opcionais
userland: disk.img
	$(MAKE) -C src/userland install
	@echo "✅ Userland compilado!"
	@echo "ℹ️ Repos irmãos (disp/term) são opcionais e não foram encontrados"
	@echo "ℹ️ O graphy já está instalado no disk.img"

# run: QEMU com VGA std
run: all disk.img boot-binaries
	qemu-system-x86_64 -vga std -boot order=d -cdrom $(ISO) -m 512M -serial stdio \
		-drive file=disk.img,format=raw,index=0 

# run-curses: QEMU em modo texto
run-curses: all disk.img
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 512M -display curses \
		-drive file=disk.img,format=raw,index=0

# run-test: headless
run-test: all disk.img boot-binaries
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 512M -no-reboot \
		-drive file=disk.img,format=raw,index=0 \
			-serial file:/tmp/ovsbos-boot.log -display none

# clean: limpa build
clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	rm -f $(ISO) disk.img
	rm -rf build/
