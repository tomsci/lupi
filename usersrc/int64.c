#include <lupi/int64.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <limits.h>

#define Int64Metatable "LupiInt64Metatable"

static char moduleLoaded;

int64 int64_check(lua_State* L, int idx) {
	return *(int64*)luaL_checkudata(L, idx, Int64Metatable);
}

static int hi(lua_State* L) {
	int64 obj = int64_check(L, 1);
	uint32 value = obj >> 32;
	lua_pushinteger(L, value);
	return 1;
}

static int lo(lua_State* L) {
	int64 obj = int64_check(L, 1);
	uint32 value = (uint32)obj;
	lua_pushinteger(L, value);
	return 1;
}

static int rawval(lua_State* L) {
	int64 obj = int64_check(L, 1);
	lua_pushlstring(L, (const char*)&obj, sizeof(obj));
	return 1;
}

static void getLR(lua_State* L, int64* left, int64* right) {
	int isNum = 0;
	*left = lua_tonumberx(L, 1, &isNum);
	if (!isNum) *left = int64_check(L, 1);
	*right = lua_tonumberx(L, 2, &isNum);
	if (!isNum) *right = int64_check(L, 2);
}

#define GETLR(L) int64 left, right; getLR(L, &left, &right);

static int add(lua_State* L) { GETLR(L); int64_new(L, left + right); return 1; }
static int sub(lua_State* L) { GETLR(L); int64_new(L, left - right); return 1; }
static int mul(lua_State* L) { GETLR(L); int64_new(L, left * right); return 1; }
static int div(lua_State* L) { GETLR(L); int64_new(L, left / right); return 1; }
static int mod(lua_State* L) { GETLR(L); int64_new(L, left % right); return 1; }
static int lt(lua_State* L) { GETLR(L); lua_pushboolean(L, left < right); return 1; }
static int le(lua_State* L) { GETLR(L); lua_pushboolean(L, left <= right); return 1; }

static int unm(lua_State* L) {
	int isNum = 0;
	int64 left = lua_tonumberx(L, 1, &isNum);
	if (!isNum) left = int64_check(L, 1);
	int64_new(L, -left);
	return 1;
}

static int concat(lua_State* L) {
	// Bit weird we have to define this even though we already provide a
	// __tostring()
	luaL_tolstring(L, 1, NULL);
	luaL_tolstring(L, 2, NULL);
	lua_concat(L, 2);
	return 1;
}

int init_module_int64(lua_State* L) {
	// module env at top of L stack
	luaL_newmetatable(L, Int64Metatable);
	luaL_Reg fns[] = {
		{ "hi", hi },
		{ "lo", lo },
		{ "rawval", rawval },
		{ "__add", add },
		{ "__sub", sub },
		{ "__mul", mul },
		{ "__div", div },
		{ "__mod", mod },
		{ "__lt", lt },
		{ "__le", le },
		{ "__unm", unm },
		{ "__concat", concat },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, fns, 0);
	lua_setfield(L, -2, "Int64");
	moduleLoaded = true;
	int64_new(L, LLONG_MAX);
	lua_setfield(L, -2, "MAX");
	int64_new(L, LLONG_MIN);
	lua_setfield(L, -2, "MIN");

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
