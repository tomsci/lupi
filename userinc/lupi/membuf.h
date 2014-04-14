#ifndef MEMBUF_H
#define MEMBUF_H

typedef struct lua_State lua_State;
typedef struct MemBuf MemBuf;

void initMbufModule(lua_State* L);
MemBuf* mbuf_new(lua_State* L, void* ptr, int len);

#endif
