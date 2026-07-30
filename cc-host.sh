#!/bin/bash
# moe moe kyun <3
# cc-host: compilador cross que transforma .c do TipOS em Mach-O 64-bit.
# Extrai o fonte do disk.img via mcopy, compila com toolchain freestanding,
# empacota com macho_pack.py, e copia de volta pro disco.
# Uso: ./cc-host.sh <nome_sem_extensao>
# Ex: ./cc-host.sh hello   (compila /SRC/HELLO.C → /BIN/HELLO.MACHO)

set -e
NAME="$1"
if [ -z "$NAME" ]; then
    echo "Uso: $0 <nome>"
    echo "Compila /SRC/<NOME>.C do disk.img e escreve /BIN/<NOME>.MACHO"
    exit 1
fi

DISK="/root/projects/TipOS/disk.img"
ULIB="/root/projects/TipOS/src/userland"
BUILD="/root/projects/TipOS/build/userland"
SRC_FILE="${NAME}.c"
MACHO_FILE="${NAME}.macho"

WORK=$(mktemp -d)
trap "rm -rf $WORK" EXIT

# Extrai fonte do disco
if ! mcopy -i "$DISK" -o "::/SRC/${SRC_FILE}" "$WORK/${SRC_FILE}" 2>/dev/null; then
    echo "Erro: /SRC/${SRC_FILE} nao encontrado no disk.img"
    echo "No TipOS: 'cc ${SRC_FILE}' salva o fonte em /SRC/"
    exit 1
fi

cd "$ULIB"

# Compila com o mesmo toolchain do build system
CC=gcc
CFLAGS="-ffreestanding -nostdlib -static -fno-PIC -mno-red-zone -mno-mmx -mno-sse -mgeneral-regs-only -nostartfiles -fno-asynchronous-unwind-tables -fno-unwind-tables -Wl,--build-id=none -fno-stack-protector -O1 -I include"
LDFLAGS="-e _start -T libc/link.ld"
LIBC="libc/crt0.c libc/stdio.c libc/stdlib.c libc/string.c"

echo "Compilando ${NAME}..."
$CC $CFLAGS $LDFLAGS -o "$BUILD/${NAME}.elf" "$WORK/${SRC_FILE}" $LIBC 2>&1
objcopy -O binary -j .text "$BUILD/${NAME}.elf" "$BUILD/${NAME}.bin"
python3 tools/macho_pack.py "$BUILD/${NAME}.bin" "$BUILD/${NAME}.macho"

# Copia pro disco
mcopy -i "$DISK" -o "$BUILD/${NAME}.macho" "::/BIN/${NAME}.MACHO"
SIZE=$(wc -c < "$BUILD/${NAME}.macho")
echo "OK: /BIN/${NAME}.MACHO (${SIZE} bytes)"
echo "Rode 'exec ${NAME}' no TipOS"
