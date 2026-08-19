# OvsbOS ELF e binarios nativos

OvsbOS possui dois caminhos de execucao em Ring 3:

1. **ELF64** para binarios Linux estaticos compatveis.
2. **Mach-O 64-bit** para aplicacoes nativas empacotadas pelo Ovsb SDK.

## ELF64

O loader esta em `OvsbMk/kernel/elf64.zig` e e usado por `exec`, `spawn` e
`spawn_shared` quando os quatro primeiros bytes sao `0x7fELF`.

### Fluxo de carregamento

```text
FAT32 -> shell/syscall -> valida ELF64 -> cria PML4 filha
       -> mapeia PT_LOAD em paginas de 2 MiB
       -> copia segmentos e zera BSS
       -> monta stack Linux/auxv
       -> marca o PCB como linux_abi
       -> iretq para CS=0x1B, SS=0x23
```

O loader valida arquitetura x86_64, tipo `ET_EXEC`/`ET_DYN`, tabela de
program headers e limites dos segmentos. A PML4 do filho recebe as paginas de
codigo e dados com bit de usuario antes da entrada em Ring 3.

### ABI Linux

`OvsbMk/kernel/syscall_linux.zig` traduz somente processos com
`pcb.linux_abi=1`. Isso permite rodar ELF Linux sem alterar a ABI nativa do
OvsbOS. O stack inicial recebe `argc`, `argv`, `envp` e entradas auxiliares
como `AT_RANDOM`, `AT_PAGESZ`, `AT_PHDR`, `AT_PHENT` e `AT_PHNUM`.

## Mach-O nativo

Aplicacoes nativas sao carregadas por `OvsbMk/kernel/mach_o.c` e empacotadas
por `src/userland/tools/macho_pack.py`.

O pacote contem um header Mach-O 64-bit minimo, um segmento `__TEXT` e o ponto
de entrada. O loader copia o segmento para o endereco virtual definido pelo
formato e cria uma stack de usuario. O processo usa os numeros nativos da
tabela em [SYSCALLS.md](SYSCALLS.md), sem a traducao Linux.

### Build de uma aplicacao

```text
C -> ELF temporario -> objcopy .text -> macho_pack.py -> FAT32:/BIN/APP
```

O Makefile de `src/userland` executa esse fluxo para os programas listados em
`PROGS`. O nome instalado no FAT32 e normalizado para maiusculas no formato
8.3.

## Ring 3

O frame de entrada usa:

| Campo | Valor | Significado |
|---|---:|---|
| `CS` | `0x1B` | codigo de usuario, RPL 3 |
| `SS` | `0x23` | dados/pilha de usuario, RPL 3 |
| `RFLAGS` | `0x202` | interrupcoes habilitadas |

O kernel usa `TSS.RSP0` para receber syscalls e interrupcoes sem entregar sua
stack ao processo. Comunicacao com o kernel ocorre por `int 0x80`, nunca por
chamada direta de funcao de kernel a partir do userland.
