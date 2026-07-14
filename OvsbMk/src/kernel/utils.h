#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, int n);
int strlen(const char *s);
char *strcpy(char *d, const char *s);
char *strncpy(char *d, const char *s, int n);
void *memset(void *s, int c, int n);
#endif
