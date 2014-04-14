#include <lupi/membuf.h>
#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lupi/exec.h>

#define MemBufMetatable "LupiMemBufMetatable"
#define MAX_BUFSIZE 1024*1024

typedef struct MemBuf {
	void* ptr;
	int len;
} MemBuf;

static MemBuf* checkBuf(lua_State* L) {
	return (MemBuf*)luaL_checkudata(L, 1, MemBufMetatable);
}

static void boundsCheck(lua_State* L, MemBuf* buf, int offset, int typeSize) {
	// Assume natural alignment of types
	if (offset & (typeSize - 1)) {
		luaL_error(L, "Unaligned offset %d", offset);
	} else if (offset < 0 || offset > MAX_BUFSIZE || offset + typeSize > buf->len) {
		luaL_error(L, "Offset %d out of bounds", offset);
	}
}

static int getInt(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	int offset = luaL_checkint(L, 2);
	boundsCheck(L, buf, offset, sizeof(int));

	int* val = (int*)((char*)buf->ptr + offset);
	lua_pushinteger(L, *val);
	return 1;
}

static int getByte(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	int offset = luaL_checkint(L, 2);
	boundsCheck(L, buf, offset, sizeof(byte));

	byte* val = (byte*)((byte*)buf->ptr + offset);
	lua_pushinteger(L, *val);
	return 1;
}

static int length(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	lua_pushinteger(L, buf->len);
	return 1;
}

static int address(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	lua_pushinteger(L, (uintptr)buf->ptr);
	return 1;
}

static int sub(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	int offset = luaL_checkint(L, 2);
	int len = luaL_checkint(L, 3);
	boundsCheck(L, buf, offset, 1);
	boundsCheck(L, buf, offset+len-1, 1);
	mbuf_new(L, (char*)buf->ptr + offset, len);
	return 1;
}

static int tostring(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	lua_pushfstring(L, "MemBuf(%p, %d)", buf->ptr, buf->len);
	return 1;
}

void initMbufModule(lua_State* L) {
	// module env at top of L stack
	luaL_newmetatable(L, MemBufMetatable);
	luaL_Reg fns[] = {
		{ "address", address },
		{ "getInt", getInt },
		{ "getByte", getByte },
		{ "length", length },
		{ "sub", sub },
		{ "__tostring", tostring },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, fns, 0);
	lua_pushvalue(L, -1); // Dup MemBufMetatable
	lua_setfield(L, -2, "__index");
	lua_setfield(L, -2, "MemBuf");
}

MemBuf* mbuf_new(lua_State* L, void* ptr, int len) {
	MemBuf* buf = (MemBuf*)lua_newuserdata(L, sizeof(MemBuf));
	buf->ptr = ptr;
	buf->len = len;
	luaL_newmetatable(L, MemBufMetatable); // Will already exist, this API name is a bit misleading
	lua_setmetatable(L, -2);
	return buf;
}
