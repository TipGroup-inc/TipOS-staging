#include "utils.h"

int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a - *b; }
int strncmp(const char *a, const char *b, int n) { for (int i = 0; i < n; i++) { if (a[i] != b[i]) return a[i] - b[i]; if (!a[i]) return 0; } return 0; }
int strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)); return r; }
char *strncpy(char *d, const char *s, int n) { char *r = d; while (n-- > 0 && (*d++ = *s++)); while (n-- > 0) *d++ = 0; return r; }
void *memset(void *s, int c, int n) { for (int i = 0; i < n; i++) ((uint8_t*)s)[i] = c; return s; }
