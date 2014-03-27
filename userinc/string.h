#ifndef STRING_H
#define STRING_H

#include <stddef.h>

#define memcpy(dest, src, size)	__builtin_memcpy(dest, src, size)
#define memset(dest, val, size) __builtin_memset(dest, val, size)
#define memcmp(s1, s2, size)	__builtin_memcmp(s1, s2, size)

#define strcoll(s1, s2) strcmp(s1, s2)

// Provided by crt.c
char* strcpy(char *to, const char *from);
const char* strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
const char* strpbrk(const char *s1, const char *s2);
int sprintf(char * restrict outstr, const char * restrict fmt, ...);
char* strstr(const char *s, const char *find);
size_t strspn(const char *s1, const char *s2);
void *memchr(const void *s, int c, size_t n);

#endif
