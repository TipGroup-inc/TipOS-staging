# moe moe kyun <3
# ♥ Makefile OvsbMkM ~ Kernel OvsbMk para TipOS
# Compila C, NASM, e Zig pra gerar kernel.elf~
# Depois empacota em ISO com GRUB~ tudo automático! kyun!

# ♥ Toolchain: GCC pra C, NASM pra assembly, Zig pros drivers modernos~
CC := gcc
NASM := nasm
ZIG ?= zig
ifeq (,$(shell command -v $(ZIG) 2>/dev/null))
ZIG := /home/vinberkuko/.local/zig-x86_64-linux-0.16.0/zig
endif
# ♥ CFLAGS: freestanding (sem libc), no-red-zone, sem SSE/MMX~
# Só registradores de propósito geral ~ modo 64 bits puro!
CFLAGS := -ffreestanding -nostdlib -mno-red-zone -mno-mmx -mno-sse
CFLAGS += -mgeneral-regs-only -m64 -O2 -Wall
# ♥ ZIGFLAGS: target freestanding, soft-float só, sem SIMD~
ZIGFLAGS := -target x86_64-freestanding -mno-red-zone -O ReleaseSmall
ZIGFLAGS += -mcpu x86_64+soft_float-mmx-sse-sse2-sse3-ssse3-sse4_1-sse4_2-avx-avx2
# ♥ LDFLAGS: link cru, sem libc, sem PIE, linker script customizado
LDFLAGS := -nostdlib -no-pie -Wl,-nostdlib -T kernel/linker.ld

# ♥ BUILD_DIR: tudo vai pra build/ (mantém src limpinho)
BUILD_DIR := build

# ♥ VPATH: procura sources em kernel, drivers, libs, e fs~
VPATH := kernel:drivers:lib/gui:lib/owt:lib/wm:fs

# ♥ C_SRCS: todos .c (keyboard/mouse em Zig, não em C)
C_SRCS := $(notdir $(wildcard kernel/*.c drivers/*.c lib/gui/*.c lib/owt/*.c lib/wm/*.c fs/*.c))
# ♥ ASM_SRCS: boot, IDT assembly, keyboard asm, context switch, syscall entry
ASM_SRCS := boot64.o idt_asm.o keyboard_asm.o switch_asm.o syscall_entry.o
# ♥ ZIG_SRCS: syscall handler, ELF loader, ext2, initramfs
ZIG_SRCS := syscall_linux.o elf64.o ext2.o initramfs.o
# ♥ ZIG_DRV_SRCS: keyboard e mouse em Zig (moderno~)
ZIG_DRV_SRCS := keyboard.o mouse.o
# ♥ OBJS: todos os .o concatenados (C + ASM + Zig)
OBJS := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o) $(ASM_SRCS) $(ZIG_SRCS) $(ZIG_DRV_SRCS))

# ♥ all: target padrão ~ compila kernel.elf
all: $(BUILD_DIR)/kernel.elf

# ♥ Cria diretório build/ se não existir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ♥ Monta boot64.asm (entry point do kernel ~ multiboot header)
$(BUILD_DIR)/boot64.o: boot64.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

# ♥ Monta idt.asm (save/restore de regs + iretq)
$(BUILD_DIR)/idt_asm.o: idt.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

# ♥ Monta keyboard_asm.asm (ISR do teclado em asm)
$(BUILD_DIR)/keyboard_asm.o: keyboard_asm.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

# ♥ Monta switch.asm (context_switch ~ salva/restaura regs + CR3)
$(BUILD_DIR)/switch_asm.o: switch.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

# ♥ Monta syscall_entry.asm (syscall instruction handler ~ MSR LSTAR)
$(BUILD_DIR)/syscall_entry.o: kernel/syscall_entry.asm | $(BUILD_DIR)
	nasm -f elf64 -o $@ $<

# ♥ Compila syscall_linux.zig (handler de syscall via int 0x80)
$(BUILD_DIR)/syscall_linux.o: kernel/syscall_linux.zig | $(BUILD_DIR)
	$(ZIG) build-obj $(ZIGFLAGS) -femit-bin=$@ $<

# ♥ Compila elf64.zig (loader de ELF64)
$(BUILD_DIR)/elf64.o: kernel/elf64.zig | $(BUILD_DIR)
	$(ZIG) build-obj $(ZIGFLAGS) -femit-bin=$@ $<

# ♥ Compila keyboard.zig (driver de teclado PS/2 em Zig)
$(BUILD_DIR)/keyboard.o: kernel/drivers/keyboard.zig | $(BUILD_DIR)
	$(ZIG) build-obj $(ZIGFLAGS) -femit-bin=$@ $<

# ♥ Compila mouse.zig (driver de mouse PS/2 em Zig)
$(BUILD_DIR)/mouse.o: kernel/drivers/mouse.zig | $(BUILD_DIR)
	$(ZIG) build-obj $(ZIGFLAGS) -femit-bin=$@ $<

# ♥ Compila ext2.zig (sistema de arquivos ext2)
$(BUILD_DIR)/ext2.o: fs/ext2.zig | $(BUILD_DIR)
	$(ZIG) build-obj $(ZIGFLAGS) -femit-bin=$@ $<

# ♥ Compila initramfs.zig (initramfs ~ arquivos iniciais)
$(BUILD_DIR)/initramfs.o: fs/initramfs.zig | $(BUILD_DIR)
	$(ZIG) build-obj $(ZIGFLAGS) -femit-bin=$@ $<

# ♥ Regra genérica: compila .c → .o (com includes nos diretórios)
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I. -Ikernel -Idrivers -Ilib/gui -Ilib/owt -Ilib/wm -Ifs -c -o $@ $<

# ♥ Link final: junta todos os .o no kernel.elf
$(BUILD_DIR)/kernel.elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# ♥ iso: gera ISO bootável com GRUB~
iso: OvsbMkM.iso
	@echo "OvsbMkM.iso gerada!"

# ♥ Tests: ELF test binaries compilados com musl-gcc
TESTS_DIR := tests
BUILD_TESTS := /tmp/HELLO /tmp/TTEST /tmp/SOCKTEST

$(BUILD_TESTS): $(TESTS_DIR)/hello.c $(TESTS_DIR)/ttest.c $(TESTS_DIR)/socktest.c
	@echo "  Compilando HELLO..."
	@test -f /tmp/musl-gcc-custom && \
		/tmp/musl-gcc-custom -static -O2 -nostartfiles -fno-stack-protector \
			-nostdlib -ffreestanding -e _start $(TESTS_DIR)/hello.c -o /tmp/HELLO \
			-Wl,--build-id=none 2>/dev/null || true
	@echo "  Compilando TTEST..."
	@test -f /tmp/musl-gcc-custom && \
		/tmp/musl-gcc-custom -static -O2 -nostartfiles -fno-stack-protector \
			-nostdlib -ffreestanding -e _start $(TESTS_DIR)/ttest.c -o /tmp/TTEST \
			-Wl,--build-id=none 2>/dev/null || true
	@echo "  Compilando SOCKTEST..."
	@test -f /tmp/musl-gcc-custom && \
		/tmp/musl-gcc-custom -static -O2 -nostartfiles -fno-stack-protector \
			-nostdlib -ffreestanding -e _start $(TESTS_DIR)/socktest.c -o /tmp/SOCKTEST \
			-Wl,--build-id=none 2>/dev/null || true

# ♥ disk.img: disco virtual de 64MB com FAT32 (pra testes)
disk.img: $(BUILD_TESTS)
	dd if=/dev/zero of=$@ bs=1M count=64
	/sbin/mkfs.fat -F 32 $@
	@echo "disk.img criado!"
	@echo "  Instalando HELLO..."
	@mcopy -o -i $@ /tmp/HELLO ::/HELLO 2>/dev/null && echo "    OK" || echo "    FAIL"
	@echo "  Instalando TTEST..."
	@mcopy -o -i $@ /tmp/TTEST ::/TTEST 2>/dev/null && echo "    OK" || echo "    FAIL"

# ♥ clean: limpa tudo (build/, isos, bins)
clean:
	rm -rf $(BUILD_DIR) *.o kernel.elf OvsbMkM.iso
	rm -rf iso/boot/kernel.elf

# ♥ run: gera ISO + disco, sobe QEMU com VGA virtio, 512MB RAM, serial, KVM
run: OvsbMkM.iso disk.img
	qemu-system-x86_64 -vga virtio -boot d -cdrom OvsbMkM.iso -m 512M -serial stdio -enable-kvm -drive file=disk.img,format=raw,if=ide

# ♥ run-no-kvm: mesma coisa mas sem KVM (WSL2 / sem aceleração)
run-no-kvm: OvsbMkM.iso disk.img
	qemu-system-x86_64 -vga virtio -boot d -cdrom OvsbMkM.iso -m 512M -serial stdio -drive file=disk.img,format=raw,if=ide

# ♥ Gera ISO com GRUB: copia kernel pra iso/boot e roda grub-mkrescue
OvsbMkM.iso: $(BUILD_DIR)/kernel.elf
	mkdir -p iso/boot
	cp $(BUILD_DIR)/kernel.elf iso/boot/
	grub-mkrescue -o OvsbMkM.iso iso 2>/dev/null || true

.PHONY: all clean run iso