#include <lupi/int64.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define Int64Metatable "LupiInt64Metatable"

static char moduleLoaded;

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

static int rawval(lua_State* L) {
	int64* obj = (int64*)luaL_checkudata(L, 1, Int64Metatable);
	lua_pushlstring(L, (const char*)obj, sizeof(*obj));
	return 1;
}

int init_module_int64(lua_State* L) {
	// module env at top of L stack
	luaL_newmetatable(L, Int64Metatable);
	luaL_Reg fns[] = {
		{ "hi", hi },
		{ "lo", lo },
		{ "rawval", rawval },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, fns, 0);
	lua_setfield(L, -2, "Int64");
	moduleLoaded = true;
	return 0;
}

void int64_new(lua_State* L, int64 n) {
	int64* obj = (int64*)lua_newuserdata(L, sizeof(int64));
	*obj = n;
	// Make sure this function works even if the module hasn't been officially loaded yet
	if (!moduleLoaded) {
		lua_getglobal(L, "require");
		lua_pushliteral(L, "int64");
		lua_call(L, 1, 0);
	}
	luaL_setmetatable(L, Int64Metatable);
}
