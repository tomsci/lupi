// AKA "this file makes me sad"

// All the stuff which I can't persuade the compiler to supply for me, and which lua needs, and
// which are too big to go in the relevant header as an inline

#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>

void* memset(void* ptr, byte val, int len) {
	// TODO a more efficient version
	byte* b = (byte*)ptr;
	byte* end = b + len;
	while (b != end) {
		*b++ = val;
	}
	return ptr;
}

#include <string.h>

// Huge hack of a pow fn, we only handle positive integers
int pow(int a, int b) {
	int result = 1;
	int aa = (int)a;
	int bb = (int)b;
	for (int i = 0; i < bb; i++) {
		result = result * aa;
	}
	return result;
}

const char* strchr(const char *s, int c) {
	for (;; s++) {
		char ch = *s;
		if (ch == c) return s;
		if (ch == 0) return NULL;
	}
}

int strcmp(const char *s1, const char *s2) {
	for(;;) {
		char c1 = *s1++;
		char c2 = *s2++;

		if (c1 == c2) {
			if (!c1) return 0;
			else continue;
		}
		return c1-c2;
	}
}

size_t strlen(const char *s) {
	size_t res = 0;
	while (*s++) res++;
	return res;
}

size_t strnlen(const char *s, size_t maxlen) {
	size_t res = 0;
	while (*s++ && res < maxlen) res++;
	return res;
}

const char* strpbrk(const char *s1, const char *s2) {
	for (; *s1 != 0; s1++) {
		// ugh
		int ch = *s1;
		for (const char* candidate = s2; *candidate != 0; candidate++) {
			if (ch == *candidate) return s1;
		}
	}
	return NULL;
}

#ifdef ARM

int NAKED setjmp(jmp_buf env) {
	asm("ldr r1, .jmpbufMagicVal");
	asm("str r1, [r0], #4");
	// We don't do floating point (yet)
	asm("stmia r0, {r4-r14}");
	asm("mov r0, #0");
	asm("bx lr");

	LABEL_WORD(.jmpbufMagicVal, 0x5CAFF01D);
}

void NAKED longjmp(jmp_buf env, int val) {
	//TODO check jmpbufMagicVal
	asm("add r0, #4");
	asm("ldmia r0, {r4-r14}");
	asm("mov r0, r1");
	asm("bx lr");
}

#elif defined(THUMB2)

int NAKED setjmp(jmp_buf env) {
	asm("ldr r1, .jmpbufMagicVal");
	asm("str r1, [r0], #4");
	asm("mov r2, r13");
	asm("mov r3, r14");
	asm("stmia r0, {r2-r12}");
	asm("mov r0, #0");
	asm("bx lr");

	LABEL_WORD(.jmpbufMagicVal, 0x5CAFF01D);
}

void NAKED longjmp(jmp_buf env, int val) {
	//TODO check jmpbufMagicVal
	asm("add r0, #4");
	asm("ldmia r0, {r2-r12}");
	asm("mov r13, r2");
	asm("mov r14, r3");
	asm("mov r0, r1");
	asm("bx lr");
}

#elif defined(AARCH64)

int NAKED setjmp(jmp_buf env) {
	LOAD_WORD(x1, 0x5CAFF01D);
	asm("mov x2, sp");
	asm("stp x1, x2, [x0], #16");
	asm("stp x19, x20, [x0], #16");
	asm("stp x21, x22, [x0], #16");
	asm("stp x23, x24, [x0], #16");
	asm("stp x25, x26, [x0], #16");
	asm("stp x27, x28, [x0], #16");
	asm("stp x29, x30, [x0], #16");
	asm("mov x0, #0");
	asm("ret");
	// TODO fp regs?
}

void NAKED longjmp(jmp_buf env, int val) {
	//TODO check jmpbufMagicVal
	asm("add x0, x0, #8");
	asm("ldr x2, [x0], #8");
	asm("mov sp, x2");
	asm("ldp x19, x20, [x0], #16");
	asm("ldp x21, x22, [x0], #16");
	asm("ldp x23, x24, [x0], #16");
	asm("ldp x25, x26, [x0], #16");
	asm("ldp x27, x28, [x0], #16");
	asm("ldp x29, x30, [x0], #16");
	asm("mov x0, x1");
	asm("ret");
}

#else

#error "Undefined architecture!"

#endif

static int uintToStr(uint val, char* restrict str, int width, char filler) {
	char buf[10]; // Max size of a 32-bit uint
	int idx = sizeof(buf);
	do {
		uint i = val % 10;
		buf[--idx] = '0' + i;
		val = val / 10;
	} while (val);

	// size of buf string is 10-idx, starting at buf+idx
	int bufLen = sizeof(buf) - idx;
	int padLen = width - bufLen;
	if (padLen > 0) {
		idx -= padLen;
		memset(&buf[idx], filler, padLen);
		bufLen += padLen;
	}
	memcpy(str, &buf[idx], bufLen);
	return bufLen;
}

static int uintToHex(uint val, char* restrict str, int width, char filler, bool caps) {
	char a = caps ? 'A' : 'a';
	char buf[8]; // Max size for a 32-bit uint in hex
	int idx = sizeof(buf);
	do {
		uint digit = val & 0xF;
		if (digit >= 10) {
			buf[--idx] = a + digit - 10;
		} else {
			buf[--idx] = '0' + digit;
		}
		val = val >> 4;
	} while (val);

	// size of buf string is 8-idx, starting at buf+idx
	int bufLen = sizeof(buf) - idx;
	int padLen = width - bufLen;
	if (padLen > 0) {
		idx -= padLen;
		memset(&buf[idx], filler, padLen);
		bufLen += padLen;
	}
	memcpy(str, &buf[idx], bufLen);
	return bufLen;
}


int sprintf(char * restrict outstr, const char * restrict fmt, ...) {
	// I so love implementing formatters and scanners...
	// This supports only enough to keep Lua happy, which keeps me happy
	va_list args;
	va_start(args, fmt);
	char ch;
	char* str = outstr;
	while ((ch = *fmt++)) {
		if (ch == '%') {
			char filler = ' ';
			int width = 0;
			char formatChar = *fmt++;
			if (formatChar == '0') {
				filler = '0';
				formatChar = *fmt++;
			} else if (formatChar == ' ') {
				formatChar = *fmt++;
			}
			if (isdigit(formatChar)) {
				fmt--;
				char* end;
				width = (int)strtol(fmt, &end, 10);
				fmt = end;
				formatChar = *fmt++;
			}
			// %ld is equivalent to %d so we don't care, just ignore
			if (formatChar == 'l') {
				formatChar = *fmt++;
			}

//			// long long on the other hand we should worry about
//			bool longlong = false;
//			if (formatChar == 'L') {
//				longlong = true;
//				formatChar = *fmt++;
//			} else if (formatChar = 'l') {
//				formatChar = *fmt++;
//				if (formatChar == 'l') {
//					longlong = true;
//					formatChar = *fmt++;
//				}
//			}

			switch (formatChar) {
			case 'c':
				*str++ = (char)va_arg(args, int);
				break;
			case 's': {
				const char* argstr = va_arg(args, char*);
				int len = strlen(argstr);
				memcpy(str, argstr, len);
				str += len;
				break;
			}
			case 'i':
			case 'd': {
				int val = va_arg(args, int);
				if (val < 0) {
					*str++ = '-';
					val = -val;
				}
				str += uintToStr((uint)val, str, width, filler);
				break;
			}
			case 'u':
				str += uintToStr(va_arg(args, uint), str, width, filler);
				break;
			case 'x':
				str += uintToHex(va_arg(args, uint), str, width, filler, false);
				break;
			case 'p':
				*str++ = '0';
				*str++ = 'x';
				str += uintToHex((uint)(uintptr)va_arg(args, void*), str, width, filler, true);
				break;
			case 'X':
				str += uintToHex(va_arg(args, uint), str, width, filler, true);
				break;
			case '%':
				// Drop through
			default:
				// TODO
				*str++ = ch;
				*str++ = formatChar;
			}
		} else {
			*str++ = ch;
		}
	}
	va_end(args);
	*str = 0;
	return str - outstr;
}

void lupi_printstring(const char* str);
void exec_putch(char ch);
void uk_print(const char* fmt, va_list args, void (*putch)(char), void (*putstr)(const char*));

static void putchNowWithAddedNewlines(char ch) {
	// exec_putch is more like putbyte
	if (ch == '\n') {
		exec_putch('\r');
	}
	exec_putch(ch);
}

void printf(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	uk_print(fmt, args, putchNowWithAddedNewlines, lupi_printstring);
	va_end(args);
}

// Ok I'm getting bored of this. These are from AOSP
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Find the first occurrence of find in s.
 */
char *
strstr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
		s--;
    }
	return ((char *)s);
}

int
strncmp(const char *s1, const char *s2, size_t n)
{

	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

char *
strcpy(char *to, const char *from)
{
    char *save = to;

    for (; (*to = *from) != '\0'; ++from, ++to);
    return(save);
}

/*
 * Span the string s2 (skip characters that are in s2).
 */
size_t
strspn(const char *s1, const char *s2)
{
    const char *p = s1, *spanp;
    char c, sc;

    /*
     * Skip any characters in s2, excluding the terminating \0.
     */
cont:
    c = *p++;
    for (spanp = s2; (sc = *spanp++) != 0;)
        if (sc == c)
            goto cont;
    return (p - 1 - s1);
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char*  p   = s;
    const unsigned char*  end = p + n;

    for (;;) {
        if (p >= end || p[0] == c) break; p++;
        if (p >= end || p[0] == c) break; p++;
        if (p >= end || p[0] == c) break; p++;
        if (p >= end || p[0] == c) break; p++;
    }
    if (p >= end)
        return NULL;
    else
        return (void*) p;
}
