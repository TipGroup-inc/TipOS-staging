% ============================================================================
% OvsbMkM - INÍCIO RÁPIDO
% ============================================================================
% Arquivo: QUICKSTART.md
% Descrição: 5 minutos para ter o kernel rodando
% ============================================================================

# ⚡ INÍCIO RÁPIDO — Fase 1 do OvsbMkM

**Tempo estimado:** 5-10 minutos  
**Dificuldade:** Fácil  
**Requisitos:** Linux, macOS ou Windows com ferramentas instaladas

---

## 📦 Pré-requisitos (1 min)

Você já tem estas ferramentas?

```bash
nasm --version          # NASM assembler
gcc --version           # GCC compiler
ld --version            # GNU linker
qemu-system-x86_64 --version    # QEMU emulator
```

Se algum comando falhar, veja [INSTALAÇÃO](#instalação).

---

## 🚀 Compilar e Executar (2 min)

### Opção 1: Script (Mais Fácil) ✨

**Linux/macOS:**
```bash
cd OvsbMkM
chmod +x scripts/run.sh
./scripts/run.sh
```

**Windows PowerShell:**
```powershell
cd OvsbMkM
.\scripts\run.ps1
```

### Opção 2: Makefile

```bash
cd OvsbMkM
make run
```

### Opção 3: Manual

```bash
cd OvsbMkM
make clean
make all
qemu-system-x86_64 -m 256M -kernel build/kernel.elf -serial stdio -no-reboot
```

---

## ✅ O Que Você Verá

Se tudo funcionar, em segundos verá:

```
OvsbMkM Micro Kernel v0.1.0 Terminal inicializado. Digite 'help' para comandos.
MkM > 
```

**Cursor piscando**, pronto para digitar. ✅

---

## 🎮 Testes Rápidos

### Teste 1: Listar Comandos

```
MkM > help
Comandos disponiveis:
  help   - Mostra esta ajuda
  clear  - Limpa a tela
  echo   - Repete o texto digitado
  about  - Sobre o MkM
  shutdown - Desliga o sistema
MkM > 
```

### Teste 2: Echo

```
MkM > echo Ola Mundo!
Ola Mundo!
MkM > 
```

### Teste 3: About

```
MkM > about
OvsbMkM v0.1.0
Micro kernel para executar binarios macOS
Alvo: High Sierra x86-64
Feito do zero, sem XNU

Arquitetura: 64-bit x86-64
Boot: Multiboot2
Fase: 1 (Terminal Interativo)
MkM > 
```

### Teste 4: Clear

```
MkM > clear
```

Tela fica vazia, cursor volta ao topo.

### Teste 5: Sair

```
MkM > shutdown

Desligando...
[QEMU fecha]
```

---

## 🛠️ Instalação

Se você não tem os requisitos:

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y nasm gcc make qemu-system-x86
```

### Linux (Fedora/CentOS/RHEL)

```bash
sudo dnf install -y nasm gcc make qemu-system-x86
```

### macOS (Homebrew)

```bash
brew install nasm gcc make qemu
```

### Windows 10/11 (Chocolatey)

```powershell
choco install nasm mingw make qemu
```

### Windows 10/11 (Manual)

1. **NASM:** https://www.nasm.us/ → Download Windows installer
2. **GCC (MinGW-w64):** https://mingw-w64.org/ → Download installer
3. **QEMU:** https://www.qemu.org/download/ → Download Windows installer
4. Adicione pastas ao PATH (C:\nasm, C:\mingw64\bin, C:\Program Files\QEMU)
5. Restart PowerShell e teste

---

## ❌ Algo Deu Errado?

### Erro: `nasm: command not found`

→ Veja [Instalação](#instalação) acima

### Erro: `gcc: command not found`

→ Veja [Instalação](#instalação) acima

### Erro: `qemu: command not found`

→ Veja [Instalação](#instalação) acima

### Kernel compila mas tela fica preta

→ Veja [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)

### Teclado não funciona

→ Veja [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)

### Preciso de mais ajuda?

Leia os documentos completos:
- **README.md** — Visão geral completa
- **docs/ARCHITECTURE_PHASE1.md** — Detalhes técnicos
- **docs/TROUBLESHOOTING.md** — Solução de problemas

---

## 📚 Próximos Passos

Parabéns, Fase 1 funciona! 🎉

Agora pode:

1. **Explorar o código** — Leia `kernel/boot/boot.asm` e `kernel/core/kernel.c`
2. **Modificar algo** — Altere uma mensagem e recompile
3. **Adicionar comando** — Veja como `cmd_help()` funciona, crie `cmd_status()`
4. **Ler documentação** — Entenda GDT, paginação, PS/2, VGA
5. **Preparar Fase 2** — Comece planejamento de escalonador

---

## 🎯 Checklist Final

- [ ] Compilou sem erros
- [ ] `build/kernel.elf` existe
- [ ] QEMU executa sem crash
- [ ] Prompt "MkM > " aparece
- [ ] Teclado funciona
- [ ] `help` funciona
- [ ] `echo test` funciona
- [ ] `clear` funciona
- [ ] `about` funciona
- [ ] `shutdown` funciona

Se todos marcados: **Você tem OvsbMkM rodando! ✅**

---

## 🔗 Links Úteis

- **Código-fonte:** GitHub (a definir)
- **Issues/Bugs:** GitHub Issues
- **Discussões:** GitHub Discussions
- **Wiki:** (em construção)

---

**Status:** Pronto para usar! 🚀

Qualquer dúvida, verifique os documentos em `/docs/`.
