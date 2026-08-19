/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: stdio.h ~ funcoes anotadas: 0
 */
/* ~*~ stdio.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef STDIO_H
#define STDIO_H
#include <stdarg.h>

// ~~ stdio.h ~~ Header de E/S padrão da libc do TipOS~
// Tudo via syscall int 0x80 (simples e direto~)

// File descriptors padrão do UNIX (todo mundo respeita~)
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// Constantes de origem pro lseek (posicionamento no arquivo~)
#define SEEK_SET 0  // início do arquivo
#define SEEK_CUR 1  // posição atual
#define SEEK_END 2  // final do arquivo

// ~~ FILE ~~
// Estrutura simples: só o file descriptor por enquanto.
// Sem buffer, sem mutex, sem frescura~ (libc minimalista, sabe como é~)
typedef struct { int fd; } FILE;

extern FILE __stdin_file;
extern FILE __stdout_file;
extern FILE __stderr_file;

#define stdin  (&__stdin_file)
#define stdout (&__stdout_file)
#define stderr (&__stderr_file)

// ── Operações de arquivo (syscall) ─────────────────────────
int open(const char *path, int flags);   // Abre arquivo (retorna fd ou -1)
int close(int fd);                       // Fecha fd (não esquece, senão vaza~)
int read(int fd, void *buf, int count);  // Lê `count` bytes pro buffer
int write(int fd, const void *buf, int count); // Escreve `count` bytes do buffer
int lseek(int fd, int offset, int whence); // Reposiciona offset da leitura/escrita
int unlink(const char *path);            // Remove arquivo (não vai pra lixeira~)
int mkdir(const char *path);             // Cria diretório (vazio, obviamente~)
int rmdir(const char *path);             // Remove diretório (só vazio, chato~)

// ── E/S de caracteres ──────────────────────────────────────
void putchar(char c);                    // Escreve um caractere no stdout
void puts(const char *s);               // Escreve string + \n no stdout
int printf(const char *fmt, ...);       // Printf formatado (sim, tem %d, %s, etc~)
int vsnprintf(char *buf, int n, const char *fmt, va_list ap); // Formata em buffer (seguro~)
int sprintf(char *buf, const char *fmt, ...);  // Formata em buffer (perigoso~)
int sscanf(const char *s, const char *fmt, ...);  // Parseia string formatada
char getchar(void);                      // Lê um caractere do stdin (bloqueante~)
char *gets(char *buf);                   // Lê linha do stdin (sabidão~)
char *fgets(char *buf, int n, FILE *f);  // Lê linha de um FILE (limitado~)

// ── E/S arquivada (FILE*) ──────────────────────────────────
FILE *fopen(const char *path, const char *mode);  // Abre arquivo (retorna FILE*)
int fclose(FILE *f);                      // Fecha arquivo (libera FILE*)
int fread(void *buf, int size, int count, FILE *f);  // Lê blocos do arquivo
int fwrite(const void *buf, int size, int count, FILE *f);  // Escreve blocos
int fputs(const char *s, FILE *f);        // Escreve string no arquivo
int fputc(int c, FILE *f);                // Escreve caractere no arquivo
int fprintf(FILE *f, const char *fmt, ...);  // Printf formatado em arquivo

// ── Utilitários ────────────────────────────────────────────
int kbhit(void);                         // Verifica se tem tecla disponível

#endif




/* ♥ stdio.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
