```markdown
# Kora os – Plano de Desenvolvimento

**Sistema:** Kora os  
**Base inicial:** protótipo OvsbMkM (bootloader 64-bit, terminal VGA, teclado PS/2, IDT/PIC básicos)  
**Créditos:** kora os equipe  

---

## 1. Propósito

Kora os é um sistema operativo construído sobre um microkernel, desenhado para **programadores, criadores de conteúdo e profissionais que exigem o máximo desempenho da máquina**.  
O sistema oferece um ambiente híbrido (shell + janelas) onde é possível correr aplicações nativas e também software compilado para Linux, Windows e macOS, recorrendo a tradução de syscalls em espaço de utilizador.  
A filosofia é dar ao utilizador controlo total sobre os recursos de hardware, podendo conceder às aplicações acesso directo a portas I/O, regiões MMIO e núcleos de CPU, sem nunca comprometer a estabilidade geral do sistema.

---

## 2. Arquitectura geral

- **Microkernel puro** – apenas gestão de processos, memória virtual, IPC Mach e controlo de capacidades no núcleo.
- **Drivers e serviços em user‑space** – vídeo, áudio, rede, armazenamento, sistemas de ficheiros correm fora do kernel.
- **IPC baseado em portas Mach** – toda a comunicação entre processos (incluindo drivers) usa mensagens assíncronas.
- **Compatibilidade multi‑SO** – processos tradutores convertem syscalls Linux/Windows/macOS para as nativas do Kora os.
- **Modos de performance** – o utilizador pode atribuir permissões especiais a um processo (`/ring0`, `/perf`) sem o colocar no anel 0 do CPU, mantendo a segurança.

---

## 3. Funcionalidades planeadas (resumo)

- Shell interativo com suporte a comandos e lançamento de aplicações.
- Ambiente gráfico com janelas (gerido por um servidor de display em user‑space).
- Acesso a ficheiros em FAT32, ext2/4 e NTFS.
- Pilha de rede completa (TCP/IP).
- Aceleração gráfica 2D/3D (via port do Mesa para Vulkan/OpenGL).
- Som (HDA e USB Audio).
- Virtualização nativa opcional (hypervisor tipo 1 integrado).
- Loja de aplicações (futuro distante).

---

## 4. Estrutura do kernel

**Componentes obrigatórios no núcleo:**
- Escalonador de tarefas (preemptivo, com suporte a prioridades e afinidade de CPU).
- Gestão de memória virtual (paging, alocação de páginas, `mmap`/`munmap`).
- Mecanismo de IPC (portas Mach, mensagens, memória partilhada).
- Controlo de interrupções (APIC, I/O APIC) e tratamento de exceções.
- Gestão de capacidades por processo (IOPB, lista de MMIO permitidas, recursos atribuídos).
- Syscall interface limpa e documentada.

**O kernel nunca incluirá:**
- Drivers de dispositivos.
- Pilhas de protocolos (rede, sistema de ficheiros).
- Código específico de sistemas convidados (isso fica nos tradutores).

---

## 5. Drivers necessários (todos em user‑space)

### 5.1 Vídeo
- **Driver de framebuffer linear** – obtido via Multiboot2/GOP, expõe um buffer de memória partilhada.
- **Driver de GPU** – inicialmente um driver simples para Intel i915 (framebuffer + aceleração 2D básica). Evoluir para suporte Vulkan através do port do Mesa.
- **WindowServer** – processo que compõe janelas e lida com eventos de entrada. Comunica com o driver de GPU.

### 5.2 Entrada
- **Teclado PS/2** – transformado num driver em user‑space que lê as interrupções via IPC e envia eventos para o WindowServer.
- **Rato PS/2 / USB** – driver para dispositivos apontadores.
- **Controlador USB (XHCI)** – driver de host USB para suportar periféricos modernos. Inclui classes HID (teclado/rato), armazenamento em massa e áudio.

### 5.3 Armazenamento
- **AHCI (SATA)** – driver para leitura/escrita em discos rígidos e SSDs SATA.
- **NVMe** – driver para SSDs NVMe de alto desempenho.
- **Servidor VFS** – camada que unifica o acesso a diferentes sistemas de ficheiros. Implementar primeiro FAT32 (leitura/escrita), depois ext2 e, futuramente, NTFS.

### 5.4 Rede
- **Driver NIC** – começar com Intel e1000 (para testes no QEMU) e Realtek 8139. Em hardware real, adicionar drivers para Intel i219, Realtek RTL8125, etc.
- **Pilha de rede** – um processo servidor que implementa TCP/IP, UDP, sockets BSD. Pode ser um port do lwIP ou uma implementação própria.

### 5.5 Áudio
- **Driver HDA (Intel High Definition Audio)** – controlador comum em PCs modernos.
- **Servidor de som** – mistura de fluxos e comunicação com aplicações. Pode ser um port do PulseAudio ou um servidor simples.

---

## 6. Camadas de compatibilidade com outros SOs

O objectivo é executar binários **não modificados** de Linux, Windows e macOS. A abordagem escolhida é **tradução de syscalls em user‑space**:

- **Linux:** um processo “linux‑emu” carrega um binário ELF e intercepta as chamadas `int 0x80`/`syscall`. Mapeia cada syscall Linux para uma ou mais syscalls nativas do Kora os. Bibliotecas do utilizador (glibc) podem ser usadas directamente.
- **Windows:** port do Wine, que depende de uma camada POSIX que será fornecida pelo tradutor Linux (ou directamente pelas syscalls nativas). Alternativa de longo prazo: um subsistema Windows nativo.
- **macOS:** o Kora os já usa conceitos Mach (portas, mensagens). Um tradutor Mach‑O pode mapear syscalls BSD e chamadas Mach directamente para as nativas. Idealmente, muitas aplicações de linha de comandos do macOS correriam com poucas adaptações.

**Plano de implementação:**
1. Primeiro, completar as syscalls nativas do Kora os (pelo menos 100‑150 chamadas) para suportar um ambiente POSIX mínimo.
2. Construir o tradutor Linux.
3. Usar o tradutor Linux como base para o Wine.
4. Adaptar o loader Mach‑O (já prototipado) e adicionar o mapeamento das syscalls BSD/Mach.

---

## 7. Modos de performance

O utilizador pode atribuir capacidades especiais a qualquer processo através do shell:

| Comando | Efeito |
|---------|--------|
| `/ring3 <app>` | Processo normal, sem acesso directo a hardware. |
| `/ring0 <app>` | Concede I/O Permission Bitmap, mapeamento de regiões MMIO e prioridade de CPU elevada. O processo continua em ring 3, mas pode aceder ao hardware sem syscalls. |
| `/perf <app> --mode=direct-io` | Define manualmente as portas e MMIO permitidas. |
| `/realtime <app>` | Aloca um núcleo exclusivo para o processo e desactiva interrupções nesse núcleo. |

**Implementação:**
- No `exec()`, o kernel recebe uma lista de capacidades. Para `/ring0`, o kernel configura o IOPB do processo, chama `map_physical` para as regiões de MMIO requisitadas e ajusta a prioridade.
- O sistema mantém uma base de dados de perfis para aplicações conhecidas (ex.: o comando `paint` recebe automaticamente acesso à GPU e à framebuffer).

---

## 8. Roteiro de desenvolvimento (passo a passo)

### Fase 1 – Fundações (semanas 1‑4)
*Objetivo: kernel capaz de executar múltiplos processos simples em modo utilizador com memória isolada.*

- [x] Bootloader 64-bit e terminal VGA (já existente).
- [x] IDT e tratamento de exceções básico (já parcial).
- [ ] Paginação activada com identity mapping para os primeiros 2 MiB e alocação dinâmica de páginas.
- [ ] Alocador físico (bitmap) e heap do kernel (`kmalloc`/`kfree`).
- [ ] Estrutura de PCB, escalonador round‑robin simples.
- [ ] Syscalls: `exit`, `yield`, `sleep`, `spawn`, `wait`.
- [ ] Carregador Mach‑O (completar protótipo).
- [ ] Carregador ELF básico (cabeçalhos e secções simples).

### Fase 2 – IPC e drivers mínimos (semanas 5‑8)
*Objetivo: comunicação entre processos e primeiro servidor de janelas.*

- [ ] IPC Mach: criar porta, enviar/receber mensagens, partilhar memória.
- [ ] Driver de teclado PS/2 em user‑space (usando IRQ 1 redireccionada para um processo).
- [ ] Servidor de janelas mínimo – obtém framebuffer, permite criar janelas rectangulares, desenhar texto e receber eventos.
- [ ] Terminal migrado para correr sobre o WindowServer.
- [ ] Comando `/Abrir <app>` funcional.

### Fase 3 – Sistema de ficheiros e armazenamento (semanas 9‑12)
*Objetivo: ler e escrever ficheiros num disco real.*

- [ ] Driver AHCI em user‑space.
- [ ] Servidor VFS com suporte a FAT32 (leitura/escrita).
- [ ] Syscalls de ficheiros: `open`, `read`, `write`, `close`, `seek`, `stat`.
- [ ] Shell pode listar directórios, mostrar conteúdo de ficheiros, executar binários a partir do disco.

### Fase 4 – Pilha de rede (semanas 13‑16)
*Objetivo: conectividade TCP/IP.*

- [ ] Driver Intel e1000 em user‑space.
- [ ] Port do lwIP como servidor de rede.
- [ ] Syscalls de sockets: `socket`, `bind`, `listen`, `connect`, `send`, `recv`.
- [ ] Ferramentas simples: `ping`, `wget`.

### Fase 5 – Compatibilidade Linux (semanas 17‑20)
*Objetivo: executar binários ELF do Linux sem modificações.*

- [ ] Mapeamento de ~150 syscalls Linux para nativas.
- [ ] Carregador de ELF adaptado para interpretar o `interp` e carregar `ld‑linux.so`.
- [ ] Testar com binários estáticos (busybox) e dinâmicos simples.
- [ ] Executar um shell Linux real e alguns utilitários.

### Fase 6 – Modos de performance (semanas 21‑24)
*Objetivo: fornecer acesso directo ao hardware para aplicações exigentes.*

- [ ] Syscall `map_physical` (mapeia região de memória física no espaço do processo).
- [ ] Configuração de IOPB por processo.
- [ ] Comandos `/ring0`, `/ring3`, `/perf`.
- [ ] Criar perfis para uma aplicação de desenho (Paint nativo) que usa `/ring0`.

### Fase 7 – Aceleração gráfica e áudio (semanas 25‑32)
*Objetivo: interface gráfica rica e som.*

- [ ] Driver Intel i915 com aceleração 2D (preencher rectângulos, copiar bitmaps).
- [ ] Port do Mesa (modo software inicialmente, depois aceleração via Vulkan/OpenGL).
- [ ] Driver HDA e servidor de áudio simples.
- [ ] Implementar um ambiente de trabalho mínimo (lançador de aplicações, relógio).

### Fase 8 – Virtualização e compatibilidade avançada (semanas 33‑40)
*Objetivo: correr sistemas operativos completos como convidados.*

- [ ] Módulo de hypervisor (VT‑x / AMD‑V) integrado no kernel.
- [ ] Pass‑through de GPU e controladores de disco para VMs.
- [ ] Integração com o shell: `/vm windows.iso`.
- [ ] Finalizar tradutor Windows (Wine) sobre a camada Linux.
- [ ] Suporte a binários macOS (Mach‑O) de forma nativa.

### Fase 9 – Polimento e ferramentas de produtividade (contínuo)
*Objetivo: tornar o sistema usável para trabalho diário.*

- [ ] Editor de texto avançado.
- [ ] Terminal emulado completo (com tabs, splits).
- [ ] Compiladores (GCC/Clang) portados.
- [ ] Gestor de pacotes.
- [ ] Suporte a multi‑monitor.
- [ ] Gestão de energia e suspensão.

---

## 9. Estrutura de directórios (proposta)

```

kora/
├── kernel/            # Código fonte do microkernel
├── servers/           # Servidores de sistema (WindowServer, VFS, NetSrv, AudioSrv)
├── drivers/           # Drivers user‑space (por tipo: gpu, audio, network, storage)
├── libs/              # libc nativa, libmach, libgraphics
├── translators/       # Tradutores de syscalls (linux, wine, macho)
├── apps/              # Aplicações nativas (shell, paint, edit)
├── docs/              # Toda a documentação
├── build/             # Scripts de compilação e imagens
└── iso/               # Estrutura para criação de ISO bootável

```

---

## 10. Notas finais

- A robustez é garantida porque todos os drivers correm fora do kernel – um crash num driver de rede não derruba o sistema.
- O desempenho máximo é alcançado pelos modos `/ring0` e `/perf`, que removem a sobrecarga das syscalls para tarefas críticas.
- A compatibilidade com software existente é obtida sem virtualização total, aproveitando as syscalls nativas como backend comum.

Este documento será revisto e actualizado à medida que o projecto avançar.

Créditos: **kora os equipe**
```