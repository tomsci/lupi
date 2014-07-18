#include <lupi/membuf.h>
#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lupi/exec.h>
#include <lupi/int64.h>

#define MemBufMetatable "LupiMemBufMt"

/**
The maximum permitted size of a MemBuf.
*/
#define MAX_BUFSIZE 1024*1024

MemBuf* mbuf_checkbuf(lua_State* L, int idx) {
	return mbuf_checkbuf_type(L, idx, NULL);
}

static MemBuf* checkBuf(lua_State* L) {
	return mbuf_checkbuf(L, 1);
}

static void pushMemBuf(lua_State* L) {
	luaL_getmetatable(L, MemBufMetatable);
}

MemBuf* mbuf_checkbuf_type(lua_State* L, int idx, const char* type) {
	MemBuf* buf = (MemBuf*)luaL_checkudata(L, idx, MemBufMetatable);
	if (type) {
		ASSERTL(idx > 0, "mbuf_checkbuf_type must be called with an absolute index");
		pushMemBuf(L);
		lua_getfield(L, -1, "checkType");
		lua_pushvalue(L, idx);
		lua_pushstring(L, type);
		lua_call(L, 2, 0);
	}
	return buf;
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

	pushMemBuf(L);
	lua_getfield(L, -1, "_accessfn");
	void* accessFn = lua_touserdata(L, -1);
	lua_pop(L, 2);
	uintptr ptr = (uintptr)buf->ptr + offset;
	int val;
	if (accessFn) {
		val = ((mbuf_getvalue)accessFn)(L, ptr, sizeof(int));
	} else {
		val = *(int*)ptr;
	}
	lua_pushinteger(L, val);
	return 1;
}

static int setInt(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	int offset = luaL_checkint(L, 2);
	boundsCheck(L, buf, offset, sizeof(int));
	int val = luaL_checkint(L, 3);
	uintptr ptr = (uintptr)buf->ptr + offset;
	// We don't support using an accessfn for setInt, just set directly
	*(int*)ptr = val;
	return 0;
}

static int getByte(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	int offset = luaL_checkint(L, 2);
	boundsCheck(L, buf, offset, sizeof(byte));

	pushMemBuf(L);
	lua_getfield(L, -1, "_accessfn");
	void* accessFn = lua_touserdata(L, -1);
	lua_pop(L, 2);
	uintptr ptr = (uintptr)buf->ptr + offset;
	byte val;
	if (accessFn) {
		val = ((mbuf_getvalue)accessFn)(L, ptr, 1);
	} else {
		val = *(byte*)ptr;
	}
	lua_pushinteger(L, val);
	return 1;
}

static int getInt64(lua_State* L) {
	MemBuf* buf = checkBuf(L);
	int offset = luaL_checkint(L, 2);
	boundsCheck(L, buf, offset, sizeof(int64));

	pushMemBuf(L);
	lua_getfield(L, -1, "_accessfn");
	void* accessFn = lua_touserdata(L, -1);
	lua_pop(L, 2);
	uintptr ptr = (uintptr)buf->ptr + offset;
	int64 val;
	if (accessFn) {
		val = ((mbuf_getvalue)accessFn)(L, ptr, 8);
	} else {
		val = *(int64*)ptr;
	}
	int64_new(L, val);
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
	const char* type = luaL_optstring(L, 4, NULL);
	mbuf_new(L, (char*)buf->ptr + offset, len, type);
	return 1;
}

static int getType(lua_State* L) {
	checkBuf(L);
	lua_getuservalue(L, -1);
	return 1;
}

int init_module_membuf(lua_State* L) {
	// module env at top of L stack
	luaL_newmetatable(L, MemBufMetatable);
	luaL_Reg fns[] = {
		{ "getAddress", address },
		{ "getInt", getInt },
		{ "getInt64", getInt64 },
		{ "getByte", getByte },
		{ "getLength", length },
		{ "sub", sub },
		{ "getType", getType },
		{ "setInt", setInt },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, fns, 0);
	lua_pushinteger(L, sizeof(void*));
	lua_setfield(L, -2, "_PTR_SIZE");
	lua_setfield(L, -2, "MemBuf");
	return 0;
}

//#define CALL(L, nargs, nret) if (lua_pcall(L, nargs, nret, 0) != 0) { lua_getglobal(L, "print"); lua_insert(L, -2); lua_call(L, 1, 0); }
#define CALL(L, nargs, nret) lua_call(L, nargs, nret)

MemBuf* mbuf_new(lua_State* L, void* ptr, int len, const char* type) {
	MemBuf* buf = (MemBuf*)lua_newuserdata(L, sizeof(MemBuf));
	luaL_setmetatable(L, MemBufMetatable);
	buf->ptr = ptr;
	buf->len = len;
	int bufIdx = lua_gettop(L);
	pushMemBuf(L);
	if (type) {
		lua_getfield(L, -1, "_types");
		lua_getfield(L, -1, type);
		lua_setuservalue(L, bufIdx);
		lua_pop(L, 1); // _types
	}
	lua_getfield(L, -1, "_newObject");
	lua_pushvalue(L, bufIdx);
	CALL(L, 1, 0);
	lua_pop(L, 1); // The last MemBufMetatable
	// Returns with the new buf on the Lua stack
	return buf;
}

void mbuf_declare_type(lua_State* L, const char* typeName, int size) {
	pushMemBuf(L);
	lua_getfield(L, -1, "_declareType");
	lua_pushstring(L, typeName);
	lua_pushinteger(L, size);
	CALL(L, 2, 0);
	lua_pop(L, 1);
}

void mbuf_declare_member(lua_State* L, const char* typeName, const char* memberName, int offset, int size, const char* memberType) {
	pushMemBuf(L);
	lua_getfield(L, -1, "_declareMember");
	lua_pushstring(L, typeName);
	lua_pushstring(L, memberName);
	lua_pushinteger(L, offset);
	lua_pushinteger(L, size);
	lua_pushstring(L, memberType);
	CALL(L, 5, 0);
	lua_pop(L, 1);
}

void mbuf_set_accessor(lua_State* L, mbuf_getvalue accessptr) {
	pushMemBuf(L);
	lua_pushlightuserdata(L, (void*)accessptr);
	lua_setfield(L, -2, "_accessfn");
	lua_pop(L, 1); // MT
}

void mbuf_declare_enum(lua_State* L, const char* typeName, int value, const char* name) {
	pushMemBuf(L);
	lua_getfield(L, -1, "_declareValue");
	lua_pushstring(L, typeName);
	lua_pushinteger(L, value);
	lua_pushstring(L, name);
	CALL(L, 3, 0);
	lua_pop(L, 1);
}

void mbuf_push_object(lua_State* L, uintptr ptr, int size) {
	pushMemBuf(L);
	lua_getfield(L, -1, "_objects");
	lua_pushinteger(L, ptr);
	lua_gettable(L, -2);
	lua_insert(L, -3);
	lua_pop(L, 2);
}
