#ifndef CTYPE_H
#define CTYPE_H

#define isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')
#define isdigit(c) ((c) >= '0' && (c) <= '9')
#define isalpha(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define isalnum(c) (isalpha(c) || isdigit(c))
#define isprint(c) ((c) >= ' ' && (c) <= '~')
#define islower(c) ((c) >= 'a' && (c) <= 'z')
#define isupper(c) ((c) >= 'A' && (c) <= 'Z')
#define tolower(c) (isupper(c) ? (c) + 32 : (c))
#define toupper(c) (islower(c) ? (c) - 32 : (c))

#endif
