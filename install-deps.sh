#!/bin/bash
# Instalador Universal de Dependências do TipOS

set -e

echo "════════════════════════════════════════════"
echo "🔧 Instalador de Dependências do TipOS"
echo "════════════════════════════════════════════"

# Detectar distribuição
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    OS="unknown"
fi
echo "Sistema detectado: $OS"

# Instalar via apt (Debian/Ubuntu)
install_apt() {
    echo "📦 Usando APT..."
    sudo apt update
    sudo apt install -y gcc make nasm qemu-system-x86 mtools grub2-common xorriso wget tar
}

# Instalar via pacman (Arch)
install_pacman() {
    echo "📦 Usando Pacman..."
    sudo pacman -Sy --noconfirm gcc make nasm qemu-system-x86 mtools grub xorriso wget tar
}

# Instalar via dnf (Fedora)
install_dnf() {
    echo "📦 Usando DNF..."
    sudo dnf install -y gcc make nasm qemu-system-x86 mtools grub2-common xorriso wget tar
}

# Instalar Zig
install_zig() {
    if command -v zig &> /dev/null; then
        echo "✅ Zig já instalado: $(zig version)"
        return
    fi
    
    echo "📥 Baixando Zig 0.16.0..."
    cd ~
    if [ ! -f zig-x86_64-linux-0.16.0.tar.xz ]; then
        wget https://ziglang.org/download/0.16.0/zig-x86_64-linux-0.16.0.tar.xz
    fi
    
    echo "📦 Extraindo..."
    tar -xf zig-x86_64-linux-0.16.0.tar.xz
    mkdir -p ~/.local
    mv zig-x86_64-linux-0.16.0 ~/.local/
    sudo ln -sf ~/.local/zig-x86_64-linux-0.16.0/zig /usr/local/bin/zig
    echo "✅ Zig instalado: $(zig version)"
}

# Verificar dependências
verify() {
    echo ""
    echo "📋 Verificando:"
    for dep in gcc make nasm qemu-system-x86_64 mtools grub-mkrescue xorriso zig; do
        if command -v $dep &> /dev/null; then
            echo "  ✅ $dep"
        else
            echo "  ❌ $dep"
        fi
    done
}

# Executar
case $OS in
    debian|ubuntu|linuxmint|pop|elementary)
        install_apt
        ;;
    arch|manjaro|endeavouros)
        install_pacman
        ;;
    fedora|rhel|centos)
        install_dnf
        ;;
    *)
        echo "⚠️ Distribuição não reconhecida: $OS"
        echo "Instale manualmente: gcc, make, nasm, qemu, mtools, grub, xorriso, zig"
        ;;
esac

install_zig
verify

echo ""
echo "🎉 PRONTO!"
echo "Agora: cd TipOS-staging && make all disk.img userland"
