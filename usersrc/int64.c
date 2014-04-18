#include <lupi/int64.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define Int64Metatable "LupiInt64Metatable"

static int hi(lua_State* L) {
	int64* obj = (int64*)luaL_checkudata(L, 1, Int64Metatable);
	uint32 value = *obj >> 32;
	lua_pushinteger(L, value);
	return 1;
}

static int lo(lua_State* L) {
	int64* obj = (int64*)luaL_checkudata(L, 1, Int64Metatable);
	uint32 value = (uint32)*obj;
	lua_pushinteger(L, value);
	return 1;
}

void initInt64Module(lua_State* L) {
	// module env at top of L stack
	luaL_newmetatable(L, Int64Metatable);
	luaL_Reg fns[] = {
		{ "hi", hi },
		{ "lo", lo },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, fns, 0);
	lua_setfield(L, -2, "Int64");
}

void int64_new(lua_State* L, int64 n) {
	int64* obj = (int64*)lua_newuserdata(L, sizeof(int64));
	*obj = n;
	luaL_newmetatable(L, Int64Metatable); // Will already exist, this API name is a bit misleading
	lua_setmetatable(L, -2); // pops Int64Metatable
}
