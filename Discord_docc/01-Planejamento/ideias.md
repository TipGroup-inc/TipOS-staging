💡 **IDEIAS & SONHOS — TipOS**

Espaço livre pra ideias que ainda não viraram tarefa no roadmap oficial.

---

**🏗️ Doca (Dock HAL) — driver framework MIT, sem GPL** (`tipos-doca.md`)

A ideia mais ambiciosa em aberto: uma camada de abstração de hardware (HAL) que permite carregar **drivers em user-space via ELF**, com:
- ABI canônica estável (`abi_stable.h`) cobrindo memória, sincronização, PCI, rede, bloco, input, vídeo (framebuffer/DRM) e USB
- Manifesto de driver em `pkg.json`
- Carregador ELF (`loader.c`) + sandbox de memória por driver
- Resolvedor de símbolos (`resolver.c`) — drivers só enxergam a ABI, não o kernel inteiro
- Plano de implementação em 6 fases: PoC → PCI enum → Rede → Bloco/FS → Input/USB → Vídeo → Amadurecimento

Isso resolve o problema de licença (kernel pode ficar GPL/proprietário enquanto drivers ficam 100% MIT, compilados separadamente). Quem quiser puxar essa frente, o doc já tem a ABI rascunhada.

---

**Decisões arquiteturais em aberto** (`tipos-dev-stack.md`, seção 6):

| Decisão | Opção cogitada | Alternativa |
|---|---|---|
| Bootloader | Manter próprio, migrar pra **Limine** se precisar UEFI/GOP | GRUB legado |
| IPC | Mach-style (portas + msgs assíncronas) | seL4-style sync IPC |
| Syscall gate | `syscall`/`sysret` | `int 0x80` (atual) |
| Scheduler | Round-robin com prioridades | Lottery / O(1) |
| Binário nativo | Mach-O (já prototipado) | ELF-only |
| Filesystem | FAT32 (atual) | ext2 |
| Rede | lwIP portado como servidor user-space | stack própria |
| Áudio | PulseAudio portado / servidor simples | ALSA |

Bora debater cada uma antes de travar a decisão?

---

**Outras ideias soltas:**
- Splash de boot com bolinhas acendendo (`○ ○ ○ → ● ● ●`) antes de ir pro log técnico (`tipos-vision.md`)
- Separação física em 2 partições: kernel/bootloader vs. apps/config
- Tradutores de syscall: Linux (ELF), Wine (Windows sobre Linux), Mach-O nativo
- Hypervisor VT-x/AMD-V (Fase 8 do roadmap em equipe)
- Compositor gráfico com múltiplas janelas (já existe protótipo: `disp/compositor.c`)
- Multi-arch (ARM?) — bem no futuro

Toda ideia é bem-vinda aqui, mesmo sem prazo definido. 🚀
