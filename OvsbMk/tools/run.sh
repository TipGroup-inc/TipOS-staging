/* moe moe kyun <3 */
#!/bin/bash
# ============================================================================
# ovsbMicroKernelMac (MkM) - Script de Execução (Linux/macOS)
# ============================================================================
# Arquivo: scripts/run.sh
# Descrição: Build e executa o kernel no QEMU
# Uso: bash scripts/run.sh [clean|build|run|all]
# ============================================================================

set -e

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ============================================================================
# FUNÇÕES AUXILIARES
# ============================================================================

print_status() {
    echo -e "${GREEN}[MkM]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERRO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[AVISO]${NC} $1"
}

check_tool() {
    if ! command -v $1 &> /dev/null; then
        print_error "$1 não encontrado. Instale via: apt install $2 (Linux) ou brew install $2 (macOS)"
        exit 1
    fi
}

# ============================================================================
# VERIFICAR REQUISITOS
# ============================================================================

print_status "Verificando requisitos..."

check_tool "nasm" "nasm"
check_tool "gcc" "gcc"
check_tool "ld" "binutils"
check_tool "make" "make"
check_tool "qemu-system-x86_64" "qemu"

print_status "Todos os requisitos OK!"

# ============================================================================
# BUILD
# ============================================================================

build() {
    print_status "Limpando build anterior..."
    make clean

    print_status "Compilando kernel MkM..."
    make all

    if [ -f "build/kernel.elf" ]; then
        print_status "Kernel compilado com sucesso!"
        file build/kernel.elf
    else
        print_error "Falha na compilação!"
        exit 1
    fi
}

# ============================================================================
# EXECUTAR
# ============================================================================

run() {
    if [ ! -f "build/kernel.elf" ]; then
        print_warning "Kernel não encontrado, compilando..."
        build
    fi

    print_status "Iniciando QEMU..."
    echo ""
    echo "========================================="
    echo "ovsbMicroKernelMac (MkM) Fase 1"
    echo "========================================="
    echo ""
    echo "Kernel: build/kernel.elf"
    echo "Memória: 256 MB"
    echo ""
    echo "Comandos disponíveis:"
    echo "  help, clear, echo, about, shutdown"
    echo ""
    echo "Para sair do QEMU: Ctrl+A, depois X"
    echo "=========================================\n"

    qemu-system-x86_64 \
        -m 256M \
        -kernel build/kernel.elf \
        -serial stdio \
        -no-reboot \
        -display none

    print_status "QEMU encerrado."
}

# ============================================================================
# MAIN
# ============================================================================

case "${1:-all}" in
    build)
        build
        ;;
    run)
        run
        ;;
    all)
        build
        run
        ;;
    clean)
        print_status "Limpando..."
        make clean
        ;;
    *)
        echo "Uso: bash scripts/run.sh [build|run|all|clean]"
        echo ""
        echo "Exemplos:"
        echo "  bash scripts/run.sh build      # Apenas compilar"
        echo "  bash scripts/run.sh run        # Apenas executar (se compilado)"
        echo "  bash scripts/run.sh all        # Compilar e executar (padrão)"
        echo "  bash scripts/run.sh clean      # Limpar build"
        exit 1
        ;;
esac
