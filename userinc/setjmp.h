#ifndef SETJMP_H
#define SETJMP_H

#ifndef HOSTED

#define _JBLEN 64 // I have no idea...
typedef long jmp_buf[_JBLEN];

int _setjmp(jmp_buf);
void _longjmp(jmp_buf, int) __attribute__((noreturn));

#endif

#endif
