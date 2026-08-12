# moe moe kyun <3
# Makefile raiz do TipOS — orquestra build do kernel, ISO, disco FAT32, userland.
# Kernel: delega pro OvsbMk/ (Makefile interno).
# Userland: compila .c → .macho → mcopy pro disk.img.

# ── Variáveis de caminho ──
# KERNEL_DIR aponta pro diretório do kernel OvsbMk, ISO é o nome da imagem CD-ROM
KERNEL_DIR := OvsbMk
ISO        := TipOS.iso

# ── Metas que n correspondem a arquivos (sempre executa) ──
.PHONY: all kernel iso disk run run-curses clean

# ── all: build padrão — kernel + ISO, sem userland ──
all: kernel iso

# ── kernel: compila o kernel OvsbMk via delegação ao Makefile interno ──
# Chama make -C OvsbMk, que gera kernel.elf com C + ASM + Zig
kernel:
	$(MAKE) -C $(KERNEL_DIR)

# ── iso: empacota kernel.elf em ISO bootável via GRUB ──
# Copia o ELF pro diretório ISO do kernel, depois grub-mkrescue gera a ISO
iso: kernel
	cp $(KERNEL_DIR)/build/kernel.elf $(KERNEL_DIR)/iso/boot/kernel.elf
	grub-mkrescue -o $(ISO) $(KERNEL_DIR)/iso 2>/dev/null || true

# ── disk.img: cria imagem de disco FAT32 de 64MB ──
# Usa dd pra alocar, mkfs.fat pra formatar FAT32, mmd pra criar diretórios padrão
disk.img:
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	/sbin/mkfs.fat -F 32 $@ > /dev/null 2>&1
	mmd -i $@ ::/BIN ::/USR ::/APPS ::/LOCAL 2>/dev/null || true

# ── userland: compila e instala programas userland no disco FAT32 ──
# Depende de disk.img existir. Compila libc, progs, disp-wm e term.
userland: disk.img
	$(MAKE) -C src/userland install
	$(MAKE) -C ../disp install TIPOS_SDK=$(CURDIR)
	$(MAKE) -C ../term install TIPOS_SDK=$(CURDIR)

# ── run: boota o TipOS no QEMU com VGA std e serial no terminal ──
# 512MB RAM, cdrom + disk img, serial redirecionado pra stdio
run: all disk.img
	qemu-system-x86_64 -vga std -boot order=d -cdrom $(ISO) -m 512M -serial stdio \
		-drive file=disk.img,format=raw,index=0 

# ── run-curses: boota no QEMU com display curses (terminal text mode) ──
# Ideal pra rodar sem X11, só no terminal
run-curses: all disk.img
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 512M -display curses \
		-drive file=disk.img,format=raw,index=0

# ── run-test: boota headless, captura log serial, sem reboot ──
# Usa -no-reboot (para ao invés de rebootar), saída serial em arquivo
run-test: all disk.img userland
	qemu-system-x86_64 -boot order=d -cdrom $(ISO) -m 512M -no-reboot \
		-drive file=disk.img,format=raw,index=0 \
		-serial file:/tmp/tipos-boot.log -display none

# ── clean: limpa artefatos de build ──
# Delega clean pro kernel, remove ISO e disk.img
clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	rm -f $(ISO) disk.img
