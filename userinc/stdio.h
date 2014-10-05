#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>

#define EOF (-1)
#define	BUFSIZ 1024

typedef struct FILE FILE;

#define fopen(f, mode) (NULL)
#define fdopen(fd, mode) (NULL)
#define freopen(f, mode, stream) (NULL)
static inline int fclose(FILE* f) { return 0; }
#define fflush(f) (EOF)
#define feof(f) (0)
#define fread(ptr, size, nitems, stream) (-1)
#define fwrite(ptr, size, nitems, stream) (-1)
#define fgetc(f) (-1)
#define getc(f) (-1)
#define fgets(str, size, stream) (NULL)
#define ferror(f) (1)
#define stdin (0)

#endif
