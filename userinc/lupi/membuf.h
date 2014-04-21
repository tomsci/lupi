#ifndef MEMBUF_H
#define MEMBUF_H

#include <stddef.h>

typedef struct lua_State lua_State;
typedef struct MemBuf MemBuf;

void initMbufModule(lua_State* L);

#define MEMBER_SIZEOF(type, member) sizeof(((type*)0x1000)->member)

/* MBUF_MEMBER_TYPE() and mbuf_declare_member() with non-NULL name should only be used for
 * members which are embedded in the object (or are enums). Pointers get figured out automatically.
 */
#define MBUF_TYPE(t) mbuf_declare_type(L, #t, sizeof(t))
#define MBUF_MEMBER(t, m) mbuf_declare_member(L, #t, #m, offsetof(t, m), MEMBER_SIZEOF(t, m), NULL)
#define MBUF_MEMBER_TYPE(t, m, mt) mbuf_declare_member(L, #t, #m, offsetof(t, m), MEMBER_SIZEOF(t, m), mt)
#define MBUF_ENUM(t, e) mbuf_declare_enum(L, #t, e, #e)
#define MBUF_NEW(type, ptr) mbuf_new(L, ptr, sizeof(type), #type)

void mbuf_declare_type(lua_State* L, const char* typeName, int size);
void mbuf_declare_member(lua_State* L, const char* typeName, const char* memberName, int offset, int size, const char* memberType);
void mbuf_declare_enum(lua_State* L, const char* typeName, int value, const char* name);
MemBuf* mbuf_new(lua_State* L, void* ptr, int len, const char* type);

typedef int (*mbuf_getvalue)(lua_State* L, uintptr ptr, int size);
void mbuf_set_accessor(lua_State* L, mbuf_getvalue accessptr);

void mbuf_get_object(lua_State* L, uintptr ptr, int size);

#endif
