# disp-wm — Makefile
#
# Uso:
#   make                     # compila disp-wm.macho
#   make TIPOS_SDK=../..     # com caminho customizado pro TipOS
#   make install             # copia pro disco FAT32 do TipOS
#

TIPOS_SDK ?= ..

# Toolchain TipOS — mesmas flags do userland do TipOS
CC = gcc
GCC_INC := $(shell gcc -print-file-name=include 2>/dev/null)

CFLAGS = -ffreestanding -nostdlib -static -fno-PIC \
         -mno-red-zone -mno-mmx -mno-sse -mgeneral-regs-only \
         -nostartfiles -fno-asynchronous-unwind-tables \
         -fno-unwind-tables -Wl,--build-id=none \
         -fno-stack-protector -nostdinc -O1 \
         -I. -I$(TIPOS_SDK)/src/userland/include \
         -I$(GCC_INC)

LINK_LD = $(TIPOS_SDK)/src/userland/libc/link.ld
CRT0    = $(TIPOS_SDK)/src/userland/libc/crt0.c
LIBC    = $(TIPOS_SDK)/src/userland/libc/stdio.c \
          $(TIPOS_SDK)/src/userland/libc/stdlib.c \
          $(TIPOS_SDK)/src/userland/libc/string.c



OBJ_DIR = build
PROG    = dispwm

.PHONY: all clean install run

all: $(PROG)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: examples/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

LIBC_OBJS = $(OBJ_DIR)/crt0.o $(OBJ_DIR)/stdio.o $(OBJ_DIR)/stdlib.o $(OBJ_DIR)/string.o

$(OBJ_DIR)/crt0.o: $(CRT0) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/stdio.o: $(TIPOS_SDK)/src/userland/libc/stdio.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/stdlib.o: $(TIPOS_SDK)/src/userland/libc/stdlib.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/string.o: $(TIPOS_SDK)/src/userland/libc/string.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJ_DIR)/compositor.o $(OBJ_DIR)/font8x8.o $(LIBC_OBJS)
	$(CC) $(CFLAGS) -e _start -T $(LINK_LD) \
	    -o $(OBJ_DIR)/disp-wm.elf \
	    $(LIBC_OBJS) \
	    $(OBJ_DIR)/compositor.o \
	    $(OBJ_DIR)/font8x8.o
	objcopy -O binary -j .text $(OBJ_DIR)/disp-wm.elf $(OBJ_DIR)/disp-wm.bin
	python3 $(TIPOS_SDK)/src/userland/tools/macho_pack.py \
	    $(OBJ_DIR)/disp-wm.bin $(PROG)
	rm -f $(OBJ_DIR)/disp-wm.elf $(OBJ_DIR)/disp-wm.bin

clean:
	rm -rf $(OBJ_DIR) $(PROG)

QEMU ?= qemu-system-x86_64
QEMU_MEM ?= 512M

run: all
	@if [ ! -f "$(DISK_IMG)" ]; then \
	    echo "AVISO: $(DISK_IMG) não encontrado — rodando QEMU sem disco"; \
	    $(QEMU) -boot d -cdrom $(TIPOS_SDK)/TipOS.iso -m $(QEMU_MEM); \
	else \
	    $(QEMU) -boot d -cdrom $(TIPOS_SDK)/TipOS.iso -m $(QEMU_MEM) \
	        -drive file=$(DISK_IMG),format=raw,index=0; \
	fi

# Instala no disco FAT32 do TipOS
DISK_IMG = $(TIPOS_SDK)/disk.img

install: $(PROG)
	@if [ ! -f "$(DISK_IMG)" ]; then \
	    echo "AVISO: $(DISK_IMG) não encontrado — skip install"; \
	elif ! mcopy -o -i "$(DISK_IMG)" $(PROG) ::/BIN/$(PROG) 2>&1; then \
	    echo "AVISO: falha ao copiar para o disco (QEMU rodando? disk montado?)"; \
	else \
	    echo "Instalado em /BIN/disp-wm.macho no disco TipOS"; \
	fi
