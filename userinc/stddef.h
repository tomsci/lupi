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

typedef unsigned long size_t;
typedef long ssize_t;
typedef long ptrdiff_t;
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

#ifdef AARCH64
// Clang asm too stupid to synthesize single instruction MOVZs from compatible
// MOV declarations, so need a generalised version. Also is too stupid to load
// from a .word
#define LOAD_WORD(reg, word) \
	asm("MOVZ " #reg ", %0" : : "i" ((word) & 0xFFFF)); \
	asm("MOVK " #reg ", %0, LSL #16" : : "i" (((uintptr)(word)) >> 16))
#else
#define WORD(x) asm(".word %a0" : : "i" (x))
#define LABEL_WORD(label, x) asm(#label ":"); WORD(x)
#endif

#define ATTRIBUTE_PRINTF(str, check)	__attribute__((format(printf, str, check)))
#define NAKED							__attribute__((naked))
#define NORETURN						void __attribute__((noreturn))
#define NOIGNORE						__attribute__((warn_unused_result))

#define ASSERT_COMPILE(x) extern int __compiler_assert(int[(x)?1:-1])
#define ASSERTL(cond, args...) \
	do { if (!(cond)) { luaL_error(L, "Assertion failure: " #cond args); } } while(0)

#define PRINTL(args...) \
	do { \
		int printPos = lua_gettop(L) + 1; \
		/* Make sure to call lua_pushfstring before perturbing the stack */ \
		/* in case args uses negative indexes */ \
		lua_pushfstring(L, args); \
		lua_getglobal(L, "print"); \
		lua_insert(L, printPos); \
		lua_call(L, 1, 0); \
	} while(0)
#define FOURCC(str) ((str[0]<<24)|(str[1]<<16)|(str[2]<<8)|(str[3]))

typedef __builtin_va_list va_list;
#define _VA_LIST
#define va_start(ap, param)		__builtin_va_start(ap, param)
#define va_end(ap)				__builtin_va_end(ap)
#define va_arg(ap, type)		__builtin_va_arg(ap, type)

#define likely(x)				__builtin_expect(!!(x), 1)
#define unlikely(x)				__builtin_expect(!!(x), 0)

#define offsetof(type, member)	__builtin_offsetof(type, member)

#define min(x,y)				((x) < (y) ? (x) : (y))
#define max(x,y)				((x) > (y) ? (x) : (y))

#define KPageSize 4096

#endif
