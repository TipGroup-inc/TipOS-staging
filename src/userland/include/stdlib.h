#ifndef STDLIB_H
#define STDLIB_H

int atoi(const char *s);
char *itoa(int n, char *buf);
void *malloc(int n);
void *calloc(int n, int size);
void *realloc(void *p, int n);
void free(void *p);
void *mmap(void *addr, int length, int prot, int flags);
int munmap(void *addr, int length);
void exit(int code);

#endif
