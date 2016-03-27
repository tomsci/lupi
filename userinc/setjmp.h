#ifndef SETJMP_H
#define SETJMP_H

#ifndef HOSTED

#ifdef A64
#define _JBLEN 14
#else
#define _JBLEN 12
#endif

typedef long jmp_buf[_JBLEN];

int setjmp(jmp_buf);
void longjmp(jmp_buf, int) __attribute__((noreturn));

#endif

#endif
