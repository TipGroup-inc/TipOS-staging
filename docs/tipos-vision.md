/* moe moe kyun <3 */
# TipOS — Visão do Sistema

## 1. Arquitetura Geral

```
 ┌──────────────────────────────────────┐
 │         Userland (Aplicativos)       │  ← Jogos, ferramentas, TUI
 │    ┌────────┐ ┌────────┐ ┌────────┐  │
 │    │ bash   │ │  ls    │ │ jogo X │  │
 │    └────────┘ └────────┘ └────────┘  │
 ├──────────────────────────────────────┤
 │   Ovsb.OS (camada de sistema)        │  ← Shell, comandos, init
 ├──────────────────────────────────────┤
 │   Kernel OvsbMkM (Bugsappetit.inc)    │  ← Ring 0, drivers, syscalls
 └──────────────────────────────────────┘
```

- Apps rodam em **userland** (espaço de usuário)
- Nenhum app tem acesso direto ao kernel (ring 0)
- Tudo que não precisa de hardware profundo fica no userland
- Modelo idêntico a qualquer SO moderno (Linux, Windows, macOS)

## 2. Separação de Partições

```
┌──────────────┬──────────────────────────┐
│  Partição 1  │     Partição 2           │
│  kernel.elf  │  Apps, libs, config      │
│  bootloader  │  (Ovsb.OS + userland)    │
└──────────────┴──────────────────────────┘
```

- **Partição do kernel**: bootável, contém kernel ELF + GRUB
- **Partição de usuário/apps**: sistema de arquivos convencional (FAT32/ext2)
- Kernel monta a partição de usuário durante o boot
- Separação física entre o que é sistema e o que é aplicativo

## 3. Boot Splash

Em vez de log técnico, o boot exibe uma tela de carregamento.

### Opção Simples (recomendada para começo)
Três bolinhas que acendem sequencialmente, estilo:
```
○ ○ ○   →   ● ○ ○   →   ● ● ○   →   ● ● ●   →   boot completo
```
Cada bolinha acende por ~1 segundo, da esquerda para a direita.
Fundo preto. Simples, eficaz, fácil de implementar no VGA.

### Opção Elaborada (futuro)
- Splash screen com arte (logotipo, aquário com "Loading")
- Ou tela de diagnóstico tipo Linux boot (mostra serviços subindo)
- Ou uma animação mais rica quando o framebuffer estiver pronto

## 4. Sistema Terminal

- Sistema de arquivos convencional (FAT32 ou similar)
- Terminal interativo como interface principal (TUI)
- Comandos padrão: ls, cat, echo, clear, help, shutdown
- Editores de texto simples
- Sem dependência de GUI para funcionamento básico

## 5. Relação com o Kernel

| Camada | Responsabilidade | Exemplos |
|--------|-----------------|----------|
| **OvsbMkM** (Bugsappetit.inc) | Ring 0, hardware, syscalls | Boot, IDT, PIC, ATA, FAT32, memoria |
| **Ovsb.OS** (Bugsappetit.inc) | Shell, comandos, init | ls, cat, touch, rm, edit |
| **TipOS** (nossa) | Userland, apps, visão do sistema | TUI, jogos, ferramentas, dock |

O kernel e o Ovsb.OS são do Bugsappetit.inc. O TipOS é a camada superior que
define a experiência do usuário, os aplicativos e a filosofia do sistema.

## 6. Licença

- Kernel OvsbMkM e Ovsb.OS: licença do Bugsappetit.inc (perguntar)
- TipOS userland e apps: MIT
- Camada Dock: MIT (clean room, sem código GPL)
