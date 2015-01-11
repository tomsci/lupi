#ifndef SETJMP_H
#define SETJMP_H

#ifndef HOSTED

#define _JBLEN 12
typedef long jmp_buf[_JBLEN];

int setjmp(jmp_buf);
void longjmp(jmp_buf, int) __attribute__((noreturn));

#endif

#endif
