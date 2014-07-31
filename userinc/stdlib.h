#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void* malloc(size_t);
void free(void*);
void* realloc(void*, size_t);

static inline int abs(int i) {
	return i < 0 ? -i : i;
}

static inline char* getenv(const char* name) {
	return NULL;
}

// Provided by crt.c
long strtol(const char *restrict str, char **restrict endptr, int base);

// Provided by ulua.c (or klua.c or the hosted environment)
void abort() __attribute__((noreturn));

#endif
