#ifndef LUPI_STDDEF_H
#define LUPI_STDDEF_H

typedef signed char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef unsigned char byte;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned int uint;
typedef unsigned long long uint64;

typedef unsigned long ulong;
typedef unsigned long uintptr;

typedef _Bool bool;
#define true  1
#define false 0

#define NULL ((void*)0)

#define asm __asm
#define WORD(x) asm(".word %a0" : : "i" (x))
#define LABEL_WORD(label, x) asm(#label ":"); WORD(x)

#define ATTRIBUTE_PRINTF(str, check)	__attribute__((format(printf, str, check)))
#define NAKED							__attribute__((naked))
#define NOINLINE						__attribute__((noinline))

#define ASSERT_COMPILE(x) extern int __compiler_assert(int[(x)?1:-1])

typedef __builtin_va_list va_list;
#define va_start(ap, param)		__builtin_va_start(ap, param)
#define va_end(ap)				__builtin_va_end(ap)
#define va_arg(ap, type)		__builtin_va_arg(ap, type)
#define _VA_LIST

#define likely(x)				__builtin_expect(!!(x), 1)
#define unlikely(x)				__builtin_expect(!!(x), 0)

#define memcpy(dest, src, size)	__builtin_memcpy(dest, src, size)

#endif
