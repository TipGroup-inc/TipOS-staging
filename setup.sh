#!/bin/bash
# Setup automático do TipOS
# Uso: ./setup.sh

echo "════════════════════════════════════════════"
echo "🚀 SETUP DO TipOS"
echo "════════════════════════════════════════════"

# 1. Dependências
echo "📦 [1/3] Verificando dependências..."
DEPS="gcc nasm qemu-system-x86 mtools grub2-common xorriso"
for dep in $DEPS; do
    if ! command -v $dep &> /dev/null; then
        sudo apt install -y $dep
    else
        echo "✅ $dep"
    fi
done

# 2. Zig
echo "📦 [2/3] Verificando Zig..."
if ! command -v zig &> /dev/null; then
    cd ~
    wget https://ziglang.org/download/0.16.0/zig-x86_64-linux-0.16.0.tar.xz
    tar -xf zig-x86_64-linux-0.16.0.tar.xz
    mkdir -p ~/.local
    mv zig-x86_64-linux-0.16.0 ~/.local/
    sudo ln -s ~/.local/zig-x86_64-linux-0.16.0/zig /usr/local/bin/zig
    echo "✅ Zig instalado"
else
    echo "✅ Zig já instalado"
fi

# 3. Correções
echo "📝 [3/3] Aplicando correções..."
cd ~/TipOS-staging

# Caminho do Zig
sed -i 's|/home/.*/\.local/zig-x86_64-linux-0.16.0/zig|$(HOME)/.local/zig-x86_64-linux-0.16.0/zig|' OvsbMk/Makefile

# Inline assembly
sed -i 's|"inw %1, %0" : "=a"(buf\[i\]) : "Nd"(ATA_DATA)|"inw %w1, %0" : "=a"(buf[i]) : "d"(ATA_DATA)|' OvsbMk/drivers/ata.c
sed -i 's|"inw %1, %0" : "=a"(d) : "Nd"(ATA_DATA)|"inw %w1, %0" : "=a"(d) : "d"(ATA_DATA)|' OvsbMk/drivers/ata.c
sed -i 's|"outw %0, %1" :: "a"(d), "Nd"(ATA_DATA)|"outw %0, %w1" :: "a"(d), "d"(ATA_DATA)|' OvsbMk/drivers/ata.c
sed -i 's|"inw %1, %0" : "=a"(v) : "Nd"(p)|"inw %w1, %0" : "=a"(v) : "d"(p)|' OvsbMk/drivers/usb.c
sed -i 's|"outw %0, %1" :: "a"(v), "Nd"(p)|"outw %0, %w1" :: "a"(v), "d"(p)|' OvsbMk/drivers/usb.c

echo "════════════════════════════════════════════"
echo "✅ SETUP COMPLETO!"
echo "════════════════════════════════════════════"
echo ""
echo "Para compilar tudo:"
echo "  make all disk.img userland"
echo ""
echo "Para rodar:"
echo "  make run"
