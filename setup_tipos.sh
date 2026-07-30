#!/usr/bin/env bash
# moe moe kyun <3
# Script de setup automatizado que prepara o repositório TipOS-staging
# com commits organizados por subsistema. Roda LOCALMENTE na sua máquina,
# dentro da pasta "tipos" já extraída do zip. NUNCA cole token no script, ok?
set -e

# 1) Configure seu token como variável de ambiente (NUNCA cole o token direto no script)
#    export GH_TOKEN=xxxxxxxx   (rode isso no terminal antes de rodar este script)
if [ -z "$GH_TOKEN" ]; then
  echo "Erro: defina a variável GH_TOKEN antes de rodar (export GH_TOKEN=seu_token)"
  exit 1
fi

git init
git checkout -b master 2>/dev/null || git checkout master
git remote remove origin 2>/dev/null || true
git remote add origin "https://${GH_TOKEN}@github.com/TipGroup-inc/TipOS-staging.git"

commit() {
  git add -- "$@"
  git commit -m "$MSG"
}

# Kernel
MSG="kernel: core boot, process, memory, syscall e IDT" commit OvsbMk/kernel

# Filesystem
MSG="fs: driver FAT32" commit OvsbMk/fs

# Drivers
MSG="drivers: ata, teclado, mouse, usb, pci, virtio_gpu" commit OvsbMk/drivers

# Lib (gui, owt, wm)
MSG="lib: gui (vesa), owt (widgets) e wm" commit OvsbMk/lib

# user_prog.asm solto na raiz do OvsbMk
MSG="userland: user_prog.asm de exemplo" commit OvsbMk/user_prog.asm

# src/dock
MSG="dock: implementação inicial" commit src/dock

# src/rust
MSG="rust: módulos em Rust" commit src/rust

# src/userland
MSG="userland: libc, tools e includes" commit src/userland

echo "Commits criados. Revise com 'git log --oneline' antes de dar push."
echo "Para subir: git push -u origin master"
