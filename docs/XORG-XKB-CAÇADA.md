# XORG NO TIPOS — A CRÔNICA COMPLETA DA CAÇADA AO TECLADO

> moe moe kyun <3 — documento histórico-técnico da sessão em que o
> Xorg 21.1.13 finalmente **carregou o keymap e subiu como servidor**
> no TipOS, lendo mouse e criando pipes X11~
>
> Branch: `fs-terminal/67-ext2-clean-zig`
> Commits-chave: `48e7ccc` (readv+XKM), `f561888` (demand paging)
> Data: 22 de agosto de 2026

---

## 1. Resumo executivo

O Xorg 21.1.13 (musl static-PIE) travava desde sempre em:

```
XKB: Failed to compile keymap
Keyboard initialization failed.
Fatal server error:
```

**Causa raiz dupla:**

1. **`SYS_readv` (Linux syscall 19) não existia no kernel.** O musl faz
   *toda* leitura de stdio via `readv()` com 2 iovecs (buffer do usuário
   + buffer interno do FILE). Sem handler, `default: ret = -1`, e TODO
   `fread`/`fgets` retornava EOF fantasma — silenciosamente.

2. **O fluxo de compilação de keymap do Xorg depende de `Popen`**
   (fork + pipe + exec de `/bin/sh -c xkbcomp ...`) — infraestrutura de
   processo que o TipOS ainda não tem completa (issue #72).

**Solução em duas frentes:**

- Kernel: implementar `readv` (arquivos via VFS + pipes).
- Binário do Xorg: bypass cirúrgico da compilação — um `.xkm`
  pré-compilado no host é carregado diretamente pelo `LoadXKM`.

**Resultado:** keymap carregado (`missing=0x0`), teclado inicializado,
servidor vivo criando pipes X11 e lendo `/dev/input/mice`.

---

## 2. A cadeia de bloqueio original

```
Xorg inicia
  └─ InitKeyboard → XkbInitDevice
       └─ XkbCompileKeymapFromString(keymap_string)
            └─ RunXkbComp()
                 ├─ escreve texto do keymap em <outdir>/server-N.xkm
                 ├─ Popen("xkbcomp ...")     ← fork+exec+pipe+sh!
                 │    └─ ❌ SEM fork/exec/sh no TipOS
                 └─ retorna path do .xkm compilado
                      └─ LoadXKM(path)
                           └─ fopen + XkmReadFile ← ❌ readv faltando
```

Dois muros em sequência. A decisão de engenharia: **pular o muro 1**
(bypass binário) e **remover o muro 2** (implementar readv).

---

## 3. Diagnóstico passo a passo (a caçada)

### 3.1 Descoberta da dependência de fork

- `nm xorg` revelou `Popen`, `RunXkbComp`, `LoadXKM`.
- Disassembly de `XkbCompileKeymapFromString` (0x131FEF):
  ```
  call RunXkbComp → call LoadXKM
  ```
- `strings` achou `"/bin/sh"` + formato do comando:
  `"\"%s/xkbcomp\" -w %d \"-R%s\" -xkm \"%s\" -em1 %s ..."`
- Conclusão: sem fork real (#72), compilar era impossível →
  **bypass**: pré-compilar no host.

### 3.2 O xkbcomp corrompido

O `bin/xkbcomp` no disco começava com zeros (`file`: "data").
Causa: link do Makefile quebrado ("read-only segment has dynamic
relocations"). Fix: relink manual:

```bash
musl-gcc -static -o xkbcomp2 *.o \
    prefix/lib/libxkbfile.a prefix/lib/libX11.a
```

### 3.3 Geração do keymap pré-compilado

Sem `keymap/` nas trees modernas do xkeyboard-config, sintetizamos
o bloco completo:

```
// us.map
xkb_keymap {
    xkb_keycodes  { include "evdev" };
    xkb_types     { include "complete" };
    xkb_compat    { include "complete" };
    xkb_symbols   { include "pc(pc105)+us" };
    xkb_geometry  { include "pc(pc105)" };
};
```

```bash
xkbcomp -xkm -I/usr/share/X11/xkb us.map default.xkm
# → 11684 bytes, magic 0f 6d 6b 78 ✓
```

Validação extra: roundtrip `xkbcomp ours.xkm roundtrip.xkb` OK, e
md5 idêntico entre geradores (host Debian vs nosso musl-build).

### 3.4 Descoberta do LoadXKM path builder

Primeiro teste abriu `/share/X11/xkb/compiled//k.xkm.xkm` — o
`XkbDDXOpenConfigFile` monta `<base>/compiled/<name>.xkm`. Logo:
nome retornado deve ser `"def"` e o arquivo vai para
`share/X11/xkb/compiled/def.xkm`.

### 3.5 O segfault que não era do arquivo

Um tester host (`XkmReadFile(f, want, need, &xkb)`) segfaultava —
**ordem dos args invertida**: a assinatura real é
`XkmReadFile(file, need, want, &xkb)`. Com a ordem certa, o guest
passou a dar diagnóstico limpo em vez de crash.

---

## 4. Os patches binários no XORG (4 séries)

Aplicados via script Python sobre `hw/xfree86/Xorg` recém-linkado
(os offsets mudam entre builds — sempre rederivar de `nm`/`objdump`!).

| # | Alvo | Patch | Motivo |
|---|------|-------|--------|
| 1 | FBDevPreInit: byte do `jne` pós `xf86LoadSubModule("shadow")` | `75 → EB` (jmp) | módulo shadow não existe no build estático; falha abortava PreInit |
| 2 | Entrada de `RunXkbComp` | `movabs rax,&"def"; ret` | pula Popen/fork; retorna nome do keymap pré-feito |
| 3 | rodata `"/bin/sh\0"` (8 bytes) | `"def\0\0\0\0\0"` | string sacrificada vira o nome (não é mais usada) |
| 4 | **TODOS** os `call free` do retorno (achar via objdump: `e8` cujo alvo == símbolo `free`, dentro do range xkb) | `0f 1f 44 00 00` (nopl ×5) | o retorno aponta pra **rodata** — free() nela = a_crash() |

⚠️ **Armadilha clássica:** esquecer o patch #1 ao regenerar o binário
faz o fbdev falhar em PreInit e o crash volta disfarçado
(`free(0x71501460)` com meta-tags zeradas). Sempre conferir os 4!

Script de referência (resumo):

```python
sh_off = data.find(b'/bin/sh\x00')
data[sh_off:sh_off+8] = b'def\x00\x00\x00\x00\x00'
sh_va = 0x40000000 + sh_off
f[entry_off:] = b'\x48\xb8' + struct.pack('<Q', sh_va) + b'\xc3'
# NOP cada 'call free' cujo alvo == símbolo free (rel32 = alvo-off-5)
```

---

## 5. Kernel: o bug do readv (o vilão silencioso)

### Sintoma
`LoadXKM` abria o arquivo, mas `XkmGetCARD32` lia `0x0` com `nRead=0`.
Nenhum `[rdFAIL]`, nenhum sinal — fread retornava vazio.

### Causa
musl `__stdio_read()`:

```c
struct iovec iov[2] = {
    { .iov_base = buf,    .iov_len = len - !!f->buf_size },
    { .iov_base = f->buf, .iov_len = f->buf_size }        // 1024
};
syscall(SYS_readv, f->fd, iov, 2);   // ← SEMPRE readv!
```

Linux readv = 19. Tabela Zig não mapeava → identity → C não tinha
case 19 → `-1` → musl setava F_ERR → EOF fantasma para sempre.

### Fix (syscall.c, case 19)
- Arquivos (`type==0`): loop pelos iovecs chamando `vfs_read_at`,
  avançando `fds[fd].pos`, parando em short-read.
- Pipes (`type==3`): consumo sequencial do ring buffer.
- Limite iovcnt ≤ 16.

**Lição histórica:** isso explica leituras "que abriam mas não
funcionavam" em todo o passado do projeto. Qualquer programa musl
usando stdio dependia disso.

---

## 6. Kernel: demand paging no PF handler

Com stdio vivo, o mallocng/musl passou a escrever em regiões antes do
wire cobrir tudo → PF de escrita em página não-presente coberta por
região válida do vm_map.

### Fix (idt.c, dentro do branch PF do idt_handler)

```c
/* user fault + vm_map_covers(me->vm_map, cr2): */
anda pml4/pdpt/pd/pt criando intermediários com mmap_user;
pt[ti] = frame_zerado_do_mmap_user | 0x07;
invlpg(cr2);
return;   /* retry — página viva! */
```

- `mmap_user` já devolve zerado (anon POSIX).
- Proteção contra ponteiro selvagem: só fixa se
  `vm_map_covers(me->vm_map, cr2)` (nova export em vm_map.c).
- Contadores: `g_pf_diag`, `g_pf_fix_count` ([DMND] prints).

### Resultado
PF único em 0x76022DA8 resolvido na hora; execução seguiu para:
pipes X11 criados, `/dev/input/mice` aberto/lido, framebuffer com
renderização ativa (screendump 1280×800).

---

## 7. Outros fixes colaterais da sessão

### 7.1 fd tables por processo (pré-requisito do fork #72)
- Pool estático em BSS: `fd_tables[MAX_PROC]` (1 por slot PCB) —
  heap era corrompido por overflows de user stacks vizinhas.
- Macro `#define fds fdtab->e` preservou os ~148 usos existentes.
- `fds_bind_current()` no início de toda syscall; dup no fork;
  close ≥3 no execve/exit.

### 7.2 SYS_fork_real (214) + execve ELF (208)
- `proc_fork`: walker de page tables (huge→512×4KB, máscara bits
  51:12!), frame iretq completo do filho com RAX=0, vm_map copiado.
- execve: VFS+ELF static-pie, troca de CR3, kframe reescrito.

### 7.3 vm_munmap
- Libera frames do objeto (vazava 1 frame por ciclo mmap/munmap).
- `invlpg` em TODAS as páginas do range (antes: 1 só!).

### 7.4 pipe()/pipe2()/socketpair
- `alloc_fd()` duplo marcava `used` DEPOIS dos dois allocs →
  rfd==wfd==3! Agora marca entre os allocs.

### 7.5 FS_BASE do switch.asm (BUG LATENTE HISTÓRICO!)
- `FS_BASE equ 0xA8 → 0xB8`: desde a entrada de vm_map/vmspace no
  PCB, o wrmsr carregava o **ponteiro vm_map como TLS do musl**.

### 7.6 child_frame_for (fork)
- Máscara de PA correta (`0x000FFFFFFFFFF000`) — bits NX/software do
  PTE viravam endereços non-canonical → #GP.

---

## 8. Infraestrutura de debug criada

| Print | Significado |
|-------|-------------|
| `[TOCDIAG] magic/nRead` | XkmReadTOC: header XKM |
| `[FI-DIAG] fread/ferror/num_toc/present` | file_info do XKM |
| `[XKMDIAG] missing/xkbRtrn` | resultado do XkmReadFile |
| `[RV]` / `[RVLEG]` | readv: iovecs e retorno por perna |
| `[vfsrd]` | vfs_read_at de paths *.xkm* |
| `[RDW]` | janela de reads pós-open do keymap |
| `[OPENDEF]` | registro do fd do def.xkm |
| `[W]` / `[dw]` | writes (pipe / geral) |
| `[CLR]` / `[wt]` | unmaps / criações de tabela no wire |
| `[UNM-TEXT]` / `[RM-TEXT]` | desmapeamento da região de código |
| `[PFIX]` / `[DMND]` | demand paging: elegibilidade e fixes |
| `[fdtab]` / `[bind]` | alocação/binding de fd tables |
| `[fork]` / `[nkf]` / `[fkf]` | fork: etapas e frames |

Post-mortem via QEMU monitor (`xp`) funciona porque o handler de EXC
termina em `cli;hlt` e o kernel é identity-mapped — dá pra ler PCBs,
PDs, PTs e heap direto do halt. ⚠️ Confundir VA de usuário com PA no
xp dá "Cannot access memory" falso (nosso caso: frame 0xAA1000 é
identity `0xAA1000`, não `0xAA100000`!).

Reconstruir o Xorg com diag: patch em
`src/xorg-server-21.1.13/xkb/{ddxLoad.c,xkmread.c}` +
`rm xkb/xkmread.o && make -C xkb && make -C hw/xfree86`, reaplicar
os 4 patches binários (offsets mudam!).

---

## 9. Estado atual

### Funcionando ✅
- Boot até shell, HELLO/TTEST de demonstração
- exec de ELFs static-pie via cmd_exec (VFS/ext2)
- Xorg: config lido, fbdev PreInit ok, pixman, fontes escaneadas,
  sockets Unix, **keymap carregado**, **teclado inicializado**,
  pipes X11, mouse aberto/lido, renderização no framebuffer
- fork/execve/readv/pipe/dup2/waitpid (base do Popen)
- Demand paging para regiões anônimas cobertas

### Pendente 🔜
1. **`clone` (56)**: Xorg pede threads → triplo-fault no fim do boot
   do servidor (última syscall antes da morte: clone + epoll_pwait)
2. PFs residuais durante churn de heap (demand paging está
   mascarando; investigar wire parcial/eventual source do overrun)
3. Limpeza dos ~15 prints de debug espalhados
4. Issue #72 (fork completo c/ COW opcional) segue aberta
5. Log file `/var/log/Xorg.0.log` não persiste writes (fd4)

---

## 10. Como reproduzir o estado atual

```bash
cd ~/repo/TipOS-staging && make kernel && make iso
cp /tmp/tipos/xkm/default.xkm  /tmp/ext2root/share/X11/xkb/compiled/def.xkm
cp /tmp/tipos/xorg.diag        /tmp/ext2root/bin/XORG        # c/ 4 patches
bash /tmp/tipos/runtrace.sh                                  # qemu -d int -s
# no monitor 5556: sendkey "exec /bin/XORG -nolock -config xorg.cfg"
# esperado: [XKMDIAG] missing=0x0  + pipes + mice (sem Fatal!)
```

Arquivos-chave fora do repo:
- `/tmp/tipos/xorg.xkmpatch` — binário com patches (regenerável)
- `/tmp/tipos/xkm/default.xkm` — keymap compilado (11684 B)
- `/tmp/tipos/run{fork,tracе,xorg7}.sh` — runners de teste
- `/tmp/tipos/shbuild/{sh,forktest,fork2,fork3,xkmtest}` — userland

---

*"Um readv a mais, e o império do X se ergueu."*
— alguém do squad kernel, às 22h de uma sexta-feira 💛
