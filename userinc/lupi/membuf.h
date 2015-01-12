#ifndef MEMBUF_H
#define MEMBUF_H

#include <stddef.h>

typedef struct lua_State lua_State;

typedef struct MemBuf {
	void* ptr;
	int len;
} MemBuf;

#define MEMBER_SIZEOF(type, member) sizeof(((type*)0x1000)->member)

/* MBUF_MEMBER_TYPE() and mbuf_declare_member() with non-NULL name should only be used for
 * members which are embedded in the object (or are enums). Pointers get figured out automatically.
 */
#define MBUF_TYPE(t) mbuf_declare_type(L, #t, sizeof(t))
#define MBUF_MEMBER(t, m) mbuf_declare_member(L, #t, #m, offsetof(t, m), MEMBER_SIZEOF(t, m), NULL)
#define MBUF_MEMBER_TYPE(t, m, mt) mbuf_declare_member(L, #t, #m, offsetof(t, m), MEMBER_SIZEOF(t, m), mt)
#define MBUF_MEMBER_BITFIELD(t, m, mt) mbuf_declare_member(L, #t, #m, offsetof(t, m), MEMBER_SIZEOF(t, m), "BITFIELD#" mt)
#define MBUF_ENUM(t, e) mbuf_declare_enum(L, #t, e, #e)
#define MBUF_NEW(type, ptr) mbuf_new(L, ptr, sizeof(type), #type)

/**
Declare a new type of MemBuf which has the given size. Normally this is called
via the `MBUF_TYPE()` macro, for example: `MBUF_TYPE(struct Something)`.

All the `mbuf_declare_*` functions are currently only of use by the klua
debugger, which uses them to decode kernel data structures.
*/
void mbuf_declare_type(lua_State* L, const char* typeName, int size);

/**
Declares that MemBufs of type `typeName` have a member called `memberName` of
given size at the given offset.
*/
void mbuf_declare_member(lua_State* L, const char* typeName, const char* memberName, int offset, int size, const char* memberType);
void mbuf_declare_enum(lua_State* L, const char* typeName, int value, const char* name);

typedef int (*mbuf_getvalue)(lua_State* L, uintptr ptr, int size);

/**
Sets an accessor function. If set, this function is used instead of a raw
pointer dereference to access the memory of all MemBufs. The debugger uses this
to implement a trapped access that throws an error (instead of crashing again)
if invalid memory is accessed. The accessor function must be of the form:

		accessorFn(lua_State* L, uintptr ptr, int size)
*/
void mbuf_set_accessor(lua_State* L, mbuf_getvalue accessorFn);

void mbuf_push_object(lua_State* L, uintptr ptr, int size);

/**
Construct a new MemBuf. `type` can be NULL, or a string previously passed to
[mbuf\_declare\_type()](#mbuf_declare_type). The returned `MemBuf*` is owned by
the Lua runtime - the corresponding Lua userdata is pushed onto the stack.

If `ptr` is `NULL` and `len` is non-zero, a new region of RAM is allocated and
the MemBuf set to point to it. The region is owned by the Lua runtime and will
be freed when the Lua MemBuf userdata is collected.
*/
MemBuf* mbuf_new(lua_State* L, void* ptr, int len, const char* type);

/**
Checks that the value at Lua stack index `idx` is a MemBuf, and returns it.
Otherwise errors.
*/
MemBuf* mbuf_checkbuf(lua_State* L, int idx);
MemBuf* mbuf_checkbuf_type(lua_State* L, int idx, const char* type);

/**
Creates a new MemBuf pointing to the XBM data in the variable `<xbmName>_bits`.
*/
#define mbuf_newXbm(L, xbmName) \
	mbuf_doNewXbm(L, (void*)xbmName ## _bits, sizeof(xbmName ## _bits), xbmName ## _width)

MemBuf* mbuf_doNewXbm(lua_State* L, void* ptr, int len, int xbmWidth);

#endif
