# OvsbOS Syscalls

## Convencao nativa

Aplicacoes Mach-O nativas chamam `int 0x80` em Ring 3:

| Registro | Funcao |
|---|---|
| `RAX` | numero da syscall |
| `RDI` | argumento 1 |
| `RSI` | argumento 2 |
| `RDX` | argumento 3 |
| `RCX` | argumento 4 |
| `RAX` | valor de retorno |

O gate esta no vetor `0x80` com DPL 3. O kernel valida o processo, executa o
handler em Ring 0 e retorna com `iretq`.

## Tabela nativa

A tabela abaixo acompanha `OvsbMk/kernel/syscall.h` e o `switch` de
`OvsbMk/kernel/syscall.c`.

| Numero | Nome | Argumentos | Estado |
|---:|---|---|---|
| 1 | `exit` | `code` | funcional |
| 3 | `read` | `fd, buffer, count` | funcional |
| 4 | `write` | `fd, buffer, count` | funcional |
| 5 | `open` | `path, flags, mode` | funcional |
| 6 | `close` | `fd` | funcional |
| 7 | `poll` | `fds, count, timeout` | parcial |
| 10 | `unlink` | `path` | funcional |
| 20 | `getpid` | nenhum | stub |
| 22 | `pipe` | `fds` | parcial |
| 23 | `select` | descritores | parcial |
| 24 | `getuid` | nenhum | stub |
| 25 | `geteuid` | nenhum | stub |
| 33 | `access` | `path, mode` | parcial |
| 32 | `dup` | `fd` | parcial |
| 47 | `getgid` | nenhum | stub |
| 48 | `getegid` | nenhum | stub |
| 54 | `ioctl` | `fd, request, arg` | parcial |
| 63 | `uname` | `buffer` | parcial |
| 72 | `fcntl` | `fd, command, arg` | parcial |
| 73 | `munmap` | `addr, length` | funcional |
| 74 | `mprotect` | `addr, length, prot` | parcial |
| 91 | `dup2` | `oldfd, newfd` | parcial |
| 92 | `recvmsg` | `fd, message, flags` | parcial |
| 96 | `futex` | Linux-compatible | parcial |
| 98 | `getrusage` | `who, usage` | parcial |
| 100 | `times` | `buffer` | parcial |
| 103 | `tgkill` | `pid, tid, signal` | parcial |
| 110 | `getppid` | nenhum | stub |
| 116 | `gettimeofday` | `timeval` | funcional |
| 121 | `getpgid` | `pid` | stub |
| 134 | `sigaction` | `signal, action, old` | stub |
| 136 | `mkdir2` | `path, mode` | funcional |
| 137 | `rmdir2` | `path` | funcional |
| 158 | `arch_prctl` | `code, address` | parcial |
| 173 | `sigreturn` | nenhum | stub |
| 188 | `stat` | `path, statbuf` | funcional |
| 189 | `fstat` | `fd, statbuf` | funcional |
| 197 | `mmap` | `addr, length, prot, flags` | funcional |
| 198 | `kbhit` | nenhum | funcional |
| 199 | `lstat` | `path, statbuf` | funcional |
| 200 | `disp_get_fb` | `addr, width, height, pitch` | funcional |
| 201 | `disp_flush` | `backbuffer` | funcional |
| 202 | `mouse_read` | `dx, dy, buttons` | funcional |
| 203 | `kb_mod` | nenhum | funcional |
| 204 | `lseek` | `fd, offset, whence` | funcional |
| 205 | `disp_flush_rect` | `backbuffer, xy, wh` | funcional |
| 207 | `readdir` | `path, entries, max` | funcional |
| 208 | `execve` | `path, argv, envp` | parcial |
| 209 | `shell_cmd` | `command, output, size` | funcional |
| 210 | `spawn` | `path` | parcial |
| 211 | `spawn_shared` | `path` | funcional |
| 212 | `exit_group` | `code` | compatibilidade ELF |
| 228 | `clock_gettime` | `clock, timespec` | parcial |
| 231 | `brk` | `address` | stub |
| 234 | `nanosleep` | `request, remain` | parcial |
| 257 | `openat` | `dirfd, path, flags, mode` | parcial |
| 258 | `mkdirat` | `dirfd, path, mode` | parcial |
| 262 | `newfstatat` | `dirfd, path, statbuf, flags` | parcial |
| 263 | `unlinkat` | `dirfd, path, flags` | parcial |
| 264 | `renameat` | diretorios e paths | parcial |
| 267 | `readlinkat` | `dirfd, path, buffer, size` | parcial |
| 288 | `accept4` | `fd, address, length, flags` | parcial |
| 292 | `dup3` | `oldfd, newfd, flags` | parcial |
| 293 | `pipe2` | `fds, flags` | parcial |
| 318 | `getrandom` | `buffer, length, flags` | parcial |
| 334 | `rseq` | estrutura Linux | stub |
| 400 | `umask` | `mask` | stub |
| 401 | `sysinfo` | `buffer` | parcial |
| 402-407 | filesystem `*at` | paths e metadados | parcial |
| 408 | `shutdown` | `fd, how` | parcial |

## Display e entrada

`disp_get_fb` retorna o LFB mapeado, largura, altura e pitch. O compositor
escreve em um backbuffer de userland e chama `disp_flush`; o kernel copia o
conteudo para o framebuffer VESA. `mouse_read` retorna movimento acumulado e
os tres botoes PS/2. `spawn_shared` e usado para processos que compartilham
paginas de janela.

## Compatibilidade ELF Linux

Processos ELF recebem a marca Linux no PCB. Somente eles passam pela tabela de
traducao em `OvsbMk/kernel/syscall_linux.zig`; processos Mach-O usam os numeros
nativos sem traducao. Exemplos: Linux `read=0` vira nativa `3`, `write=1`
vira `4`, `exit=60` vira `1` e `exit_group=231` vira `212`.
