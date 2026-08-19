<!-- moe moe kyun <3 -->
# Tutorial: fazendo programas pro TipOS

Este tutorial cobre como escrever, compilar e rodar programas no
TipOS usando a libc própria (MIT).

## Índice

1. [Hello World](#1-hello-world)
2. [Compilando](#2-compilando)
3. [Instalando no disco](#3-instalando-no-disco)
4. [Rodando no QEMU](#4-rodando-no-qemu)
5. [API da libc](#5-api-da-libc)
6. [Teclado — caracteres e teclas especiais](#6-teclado)
7. [Escrevendo para o VGA](#7-escrevendo-para-o-vga)
8. [Arquivos](#8-arquivos)
9. [Dicas e limitações](#9-dicas-e-limitacoes)

---

## 1. Hello World

Crie `src/userland/progs/hello.c`:

```c
#include <stdio.h>

int main(int argc, char **argv) {
    printf("Hello, TipOS!\n");
    return 0;
}
```

O `return 0` chama `exit(0)` via `_start` (em `crt0.c`).

## 2. Compilando

Adicione `hello` no Makefile:

```makefile
# src/userland/Makefile
PROGS = graphy hello
```

O Makefile compila cada programa em `progs/` com:
- `-ffreestanding -nostdlib -static -fno-PIC -mno-red-zone`
- `-nostartfiles -O1 -I include`
- Linker script `libc/link.ld` (entry `_start`, código em `0x2000000`)
- `objcopy -O binary -j .text` extrai o segmento `.text`
- `macho_pack.py` empacota como Mach-O 64-bit minimal

Execute:

```bash
make -C src/userland install
```

Isso gera `build/userland/hello.macho` e copia para o disco.
O nome no FAT32 fica em maiúsculo (`HELLO`).

## 3. Instalando no disco

O `make install` copia os `.macho` para o diretório `/BIN/`
do `disk.img` usando `mcopy`:

```bash
mcopy -o -i ../../disk.img build/userland/hello.macho ::/BIN/HELLO
```

Verifique:

```bash
mdir -i disk.img ::/BIN
```

## 4. Rodando no QEMU

```bash
make run-curses
```

No shell `MkM>`, digite:

```
HELLO
```

(O shell busca automaticamente em `/BIN/`.)

Ou explicitamente:

```
exec /BIN/HELLO
```

## 5. API da libc

### stdio.h

```c
int open(const char *path, int flags);
int close(int fd);
int read(int fd, void *buf, int count);
int write(int fd, const void *buf, int count);
int lseek(int fd, int offset, int whence);
int unlink(const char *path);
int mkdir(const char *path);
int rmdir(const char *path);
int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int kbhit(void);            // 1 se tecla disponível, 0 senão

void putchar(char c);
void puts(const char *s);
int printf(const char *fmt, ...);
int vsnprintf(char *buf, int n, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int sscanf(const char *s, const char *fmt, ...);
char getchar(void);         // blocking
char *gets(char *buf);
char *fgets(char *buf, int n, FILE *f);

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
int fread(void *buf, int size, int count, FILE *f);
int fwrite(const void *buf, int size, int count, FILE *f);
int fputs(const char *s, FILE *f);
int fputc(int c, FILE *f);
int fprintf(FILE *f, const char *fmt, ...);
```

**Nota**: `FILE` é só um wrapper em volta do fd.
`fopen("r")` = `open()` com `O_RDONLY`, `"w"` = `O_WRONLY|O_CREAT`.

### stdlib.h

```c
void *malloc(size_t size);   // bump allocator (nunca libera)
void free(void *p);          // noop
int atoi(const char *s);
void exit(int code);         // syscall 1
long strtol(const char *s, char **end, int base);
```

### string.h

```c
void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strtok(char *s, const char *delim);
```

### ctype.h

```c
int isdigit(int c);
int isspace(int c);
int isalpha(int c);
int isalnum(int c);
int isxdigit(int c);
int isupper(int c);
int islower(int c);
int toupper(int c);
int tolower(int c);
```

### sys/stat.h

```c
struct stat {
    unsigned int st_size;
    unsigned int st_mode;    // 1 = diretório, 0 = arquivo
};

int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
```

## 6. Teclado

`getchar()` bloqueia até uma tecla ser pressionada.
`kbhit()` retorna 1 se há tecla disponível (non-blocking).

Teclas estendidas (setas, F-keys, etc.) chegam como **sequências
VT100** de múltiplos bytes, começando com `\x1b` (ESC).

Exemplo de parser (usado pelo `graphy`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { K_UP=300, K_DOWN, K_LEFT, K_RIGHT,
       K_HOME, K_END, K_PGUP, K_PGDN, K_INS, K_DEL,
       K_F1, K_F2, K_F3, K_F4, K_F5, K_F6,
       K_F7, K_F8, K_F9, K_F10, K_F11, K_F12 };

int rd_k(void) {
    char c = getchar();
    if (c != '\x1b') return (unsigned char)c;

    for (int wait = 0; wait < 50000; wait++) {
        if (kbhit()) {
            char s[8];
            int n = 1;
            s[0] = '\x1b';
            while (n < 7) {
                if (!kbhit()) break;
                s[n++] = getchar();
                if (s[n-1] == '~') break;
                if (n >= 2 && s[1] == '[' && s[n-1] >= 0x40 && s[n-1] <= 0x7E) break;
                if (n >= 2 && s[1] == 'O' && s[n-1] >= 0x40 && s[n-1] <= 0x7E) break;
            }
            if (n == 2) {
                if (s[1] == 'H') return K_HOME;
                if (s[1] == 'F') return K_END;
                if (s[1] >= 'A' && s[1] <= 'D') return K_UP + (s[1] - 'A');
            }
            if (n >= 3 && s[1] == '[') {
                if (s[2] >= 'A' && s[2] <= 'D') return K_UP + (s[2] - 'A');
                if (s[2] == 'H') return K_HOME;
                if (s[2] == 'F') return K_END;
                if (s[2] == '2' && s[3] == '~') return K_INS;
                if (s[2] == '3' && s[3] == '~') return K_DEL;
                if (s[2] == '5' && s[3] == '~') return K_PGUP;
                if (s[2] == '6' && s[3] == '~') return K_PGDN;
            }
            // F-keys, etc (ver graphy.c completo)
            return 0;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    printf("Pressione setas, Home, End, etc\n");
    while (1) {
        int k = rd_k();
        if (k == 'X' - 64) break; // ^X
        if (k == K_UP) printf("UP\n");
        else if (k == K_DOWN) printf("DOWN\n");
        else if (k == K_LEFT) printf("LEFT\n");
        else if (k == K_RIGHT) printf("RIGHT\n");
        else if (k < 256 && k >= 32) printf("'%c' (%d)\n", k, k);
        else printf("key=%d\n", k);
    }
    return 0;
}
```

### Sequências VT100 suportadas

| Tecla        | Sequência     |
|--------------|---------------|
| ↑            | `\x1b[A`      |
| ↓            | `\x1b[B`      |
| ←            | `\x1b[D`      |
| →            | `\x1b[C`      |
| Home         | `\x1b[H`      |
| End          | `\x1b[F`      |
| PgUp         | `\x1b[5~`     |
| PgDn         | `\x1b[6~`     |
| Insert       | `\x1b[2~`     |
| Delete       | `\x1b[3~`     |
| F1           | `\x1bOP`      |
| F2           | `\x1bOQ`      |
| F3           | `\x1bOR`      |
| F4           | `\x1bOS`      |
| F5           | `\x1b[15~`    |
| F6           | `\x1b[17~`    |
| F7           | `\x1b[18~`    |
| F8           | `\x1b[19~`    |
| F9           | `\x1b[20~`    |
| F10          | `\x1b[21~`    |
| F11          | `\x1b[23~`    |
| F12          | `\x1b[24~`    |

## 7. Escrevendo para o VGA

O VGA é um terminal de 80x25 caracteres. `write(1, ...)` ou `printf`
escreve no buffer VGA (`0xB8000`).

### Sequências ANSI suportadas

O driver VGA do kernel interpreta sequências de escape ANSI/VT100
básicas, permitindo posicionamento de cursor, cores e
limpeza de tela:

| Sequência         | Efeito                        |
|-------------------|-------------------------------|
| `\x1b[H`          | Cursor home (0,0)             |
| `\x1b[<r>;<c>H`   | Posiciona cursor (linha,col)  |
| `\x1b[2J`         | Limpa tela                    |
| `\x1b[K`          | Limpa até fim da linha        |
| `\x1b[7m`         | Video reverso                 |
| `\x1b[m`          | Reseta atributos              |
| `\x1b[?25l`       | Esconde cursor                |
| `\x1b[?25h`       | Mostra cursor                 |
| `\x1b[A`          | Seta p/ cima                  |
| `\x1b[B`          | Seta p/ baixo                 |
| `\x1b[C`          | Seta p/ direita               |
| `\x1b[D`          | Seta p/ esquerda              |

Exemplo — barra de status com video reverso:

```c
write(1, "\x1b[7m", 4);      // liga reverse
write(1, "--- status ---", 14);
write(1, "\x1b[m\n", 4);     // desliga reverse
```

Exemplo — centralizar texto:

```c
void center(int row, const char *s) {
    int len = strlen(s);
    int col = (80 - len) / 2;
    char esc[16];
    int n = sprintf(esc, "\x1b[%d;%dH", row + 1, col + 1);
    write(1, esc, n);
    write(1, s, len);
}
```

## 8. Arquivos

O sistema de arquivos é FAT32. A libc fornece acesso via
`fopen`/`fread`/`fwrite`/`fclose` ou `open`/`read`/`write`/`close`.

Exemplo:

```c
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    // Escrever
    FILE *f = fopen("/BIN/HELLO.TXT", "w");
    if (f) {
        fputs("Hello, file!\n", f);
        fclose(f);
    }

    // Ler
    f = fopen("HELLO.TXT", "r");  // caminho relativo ao CWD
    if (f) {
        char buf[256];
        int n = fread(buf, 1, 255, f);
        buf[n] = 0;
        printf("Lido: %s\n", buf);
        fclose(f);
    }
    return 0;
}
```

**Notas**:
- `fopen` converte nomes para 8.3 (maiúsculo, sem espaços)
- Caminhos absolutos começam com `/` (ex: `/BIN/HELLO`)
- Caminhos relativos são resolvidos no diretório atual do shell
- Não há suporte a path traversal (`..` ou multi-componente)
- O shell tipicamente começa em `/`

## 9. Dicas e limitações

- **Stack**: programas compartilham a stack do kernel (16KB).
  Evite grandes arrays locais; use `static` ou `malloc`.
- **malloc**: bump allocator simples, nunca libera memória.
  Útil para alocações pequenas.
- **BSS**: é zerado pelo `crt0.c` na inicialização.
  Variáveis globais não inicializadas começam em 0.
- **Exit**: `return 0` da `main` ou `exit(0)` chamam `SYS_exit(1)`.
- **Sem proteção de memória**: kernel e userland rodam em ring 0.
  Um programa mal comportado pode derrubar o sistema.
- **Nomes FAT32**: 8.3 maiúsculo. `Hello.c` → `HELLO_C`.
  `mcopy` já converte, o `name_to_83()` da libc também.

---

Veja o código do `graphy.c` para um exemplo completo de editor TUI
com scroll, line numbers, copy/paste textual, busca e atalhos.
