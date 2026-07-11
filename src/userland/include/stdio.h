#ifndef STDIO_H
#define STDIO_H
#include <stdarg.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct { int fd; } FILE;

extern FILE __stdin_file;
extern FILE __stdout_file;
extern FILE __stderr_file;

#define stdin  (&__stdin_file)
#define stdout (&__stdout_file)
#define stderr (&__stderr_file)

int open(const char *path, int flags);
int close(int fd);
int read(int fd, void *buf, int count);
int write(int fd, const void *buf, int count);
int lseek(int fd, int offset, int whence);
int unlink(const char *path);
int mkdir(const char *path);
int rmdir(const char *path);

void putchar(char c);
void puts(const char *s);
int printf(const char *fmt, ...);
int vsnprintf(char *buf, int n, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int sscanf(const char *s, const char *fmt, ...);
char getchar(void);
char *gets(char *buf);
char *fgets(char *buf, int n, FILE *f);

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
int fread(void *buf, int size, int count, FILE *f);
int fwrite(const void *buf, int size, int count, FILE *f);
int fputs(const char *s, FILE *f);
int fputc(int c, FILE *f);
int fprintf(FILE *f, const char *fmt, ...);
int kbhit(void);

#endif
