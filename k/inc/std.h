#ifndef LUPI_STD_H
#define LUPI_STD_H

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

#define INT64_MAX 0x7FFFFFFFFFFFFFFFLL
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL

typedef unsigned long ulong;

#ifdef __LP64__
typedef unsigned long uintptr;
#else
// Be compatible with definition of uint32
typedef unsigned int uintptr;
#endif

typedef _Bool bool;
#define true  ((bool)1)
#define false ((bool)0)

#define NULL ((void*)0)

#define asm __asm
#define ASM_JFDI __asm __volatile
#ifdef AARCH64
#define WORD(x) asm(".word %c0" : : "i" (x))
#define ADDRESS(x) asm(".quad %c0" : : "i" (x))
#define LABEL_ADDRESS(label, x) asm(#label ":"); ADDRESS(x)
#else
#define WORD(x) asm(".word %a0" : : "i" (x))
#define LABEL_ADDRESS(label, x) asm(#label ":"); WORD(x)
#endif
#define LABEL_WORD(label, x) asm(#label ":"); WORD(x)


#define ATTRIBUTE_PRINTF(str, check)	__attribute__((format(printf, str, check)))
#define NAKED							__attribute__((naked))
#define NOINLINE						__attribute__((noinline))
#define NORETURN						void __attribute__((noreturn))
#define NOIGNORE						__attribute__((warn_unused_result))
#define USED  							__attribute__((used))

#define ASSERT_COMPILE(x) extern int __compiler_assert(int[(x)?1:-1])

typedef __builtin_va_list va_list;
#define va_start(ap, param)		__builtin_va_start(ap, param)
#define va_end(ap)				__builtin_va_end(ap)
#define va_arg(ap, type)		__builtin_va_arg(ap, type)
#define _VA_LIST

#define likely(x)				__builtin_expect(!!(x), 1)
#define unlikely(x)				__builtin_expect(!!(x), 0)

#define memcpy(dest, src, size)	__builtin_memcpy(dest, src, size)

#define offsetof(type, member)	__builtin_offsetof(type, member)

#define min(x,y)				((x) < (y) ? (x) : (y))
#define max(x,y)				((x) > (y) ? (x) : (y))

#endif
