#ifndef STDLIB_H
#define STDLIB_H

int atoi(const char *s);
char *itoa(int n, char *buf);
void *malloc(int n);
void free(void *p);
void exit(int code);

#endif
