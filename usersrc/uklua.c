#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lupi/membuf.h>
#include <lupi/int64.h>
#include <lupi/runloop.h>

const char* getLuaModule(const char* moduleName, int* modSize);

struct ModuleInfo {
	const char* module;
	int size;
};

static const char* readerFn(lua_State* L, void* data, size_t* size) {
	struct ModuleInfo* info = (struct ModuleInfo*)data;
	if (info->size == 0) return NULL;
	*size = info->size;
	info->size = 0;
	return info->module;
}

static int requireFn(lua_State* L) {
	// upvalue 1 is the _ENV of where we want to put the result
	// arg 1 is the module name
	lua_settop(L, 1);
	lua_getglobal(L, "require");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	// moduleName = 1, module = 2
	lua_pushvalue(L, 2); // Copy module
	lua_insert(L, 1); // And move it to the bottom
	// And set _ENV.modName = moduleEnv
	lua_settable(L, lua_upvalueindex(1));
	return 1; // The copy of module we moved to the bottom
}

static bool lcmp(lua_State* L, int idx, const char* val) {
	lua_pushstring(L, val);
	bool result = lua_compare(L, idx, -1, LUA_OPEQ);
	lua_pop(L, 1);
	return result;
}

static int loaderFn(lua_State* L) {
	// Custom loader that gives every module a separate _ENV, and makes sure that
	// package.loaded[modName] will always be set to that _ENV unless the module returns a table.
	// This isn't a major deviation from the Lua spec and seems the more sensible behaviour.
	// Rather than having:
	//     x = require("moduleName")
	// making x equal <true> unless the module remembers to put "return _ENV" at the end.

	// Arg 1 is modName
	// Arg 2 is whatever the searcher returned (which we don't bother with)
	// upvalue 1 is the module fn itself

	//TODO clean this up
	bool isMbuf = lcmp(L, 1, "membuf");
	bool isInt64 = lcmp(L, 1, "int64");
	bool isRunloop = lcmp(L, 1, "runloop");

	lua_newtable(L); // The _ENV
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_getfield(L, LUA_REGISTRYINDEX, "LupiGlobalMetatable");
	lua_setmetatable(L, -3); // setmetatable(_ENV, LupiGlobalMetatable)
	// moduleFn at -1, _ENV at -2
	lua_pushvalue(L, -2); // dups _ENV
	lua_setupvalue(L, -2, 1); // Pops _ENV

	lua_pushvalue(L, -2); // Another _ENV
	if (isMbuf) {
		initMbufModule(L);
	} else if (isInt64) {
		initInt64Module(L);
	} else if (isRunloop) {
		initRunloopModule(L);
	}

	lua_pushcclosure(L, requireFn, 1); // upvalue 1 for requireFn is _ENV (pops _ENV)
	lua_setfield(L, -3, "require"); // pops the closure

	lua_call(L, 0, 0);
	// TODO check what call returns rather than always returning the _ENV
	return 1;
}

static int getLuaModule_searcherFn(lua_State* L) {
	const char* moduleName = lua_tostring(L, 1);
	int moduleSize;
	// Slightly crazy we can call a (nominally kernel) function from user-side, but we can
	// so we do, since it doesn't access any private kernel memory
	const char* module = getLuaModule(moduleName, &moduleSize);
	if (!module) {
		lua_pushfstring(L, "\n\tno compiled module " LUA_QS, moduleName);
		return 1;
	}
	struct ModuleInfo readerInfo = { module, moduleSize };
	int ret = lua_load(L, readerFn, &readerInfo, moduleName, NULL);
	if (ret != LUA_OK) {
		lua_pushfstring(L, "\n\tError loading compiled module: %s", lua_tostring(L, -1));
		return 1;
	}
	// Ok now the module function is on the top of the stack
	// We make it an upvalue of loaderFn and return that
	lua_pushcclosure(L, loaderFn, 1);
	return 1;
}

// Returns with env of the module at the top of L's stack
lua_State* newLuaStateForModule(const char* moduleName, lua_State* L) {

	if (L == NULL) L = luaL_newstate();
	luaL_openlibs(L);

	// Replace package.searchers with one which works for us
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "searchers");
	// The first searcher can stay (the preload one) but 2, 3 and 4 gotta go
	lua_pushnil(L); lua_rawseti(L, -2, 2);
	lua_pushnil(L); lua_rawseti(L, -2, 3);
	lua_pushnil(L); lua_rawseti(L, -2, 4);

	lua_pushcfunction(L, getLuaModule_searcherFn);
	lua_rawseti(L, -2, 2);
	lua_pop(L, 1); // package

	// Setup a metatable for module envs to allow access to globals but not set em
	//		REGISTRY["LupiGlobalMetatable"] = {}
	//		REGISTRY["LupiGlobalMetatable"].__index = _G
	lua_createtable(L, 0, 1);
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	lua_setfield(L, -2, "__index");
	lua_setfield(L, LUA_REGISTRYINDEX, "LupiGlobalMetatable");

	// Since we've just added support for require, we might as well use that to load the
	// main module for the process and save duplicating code. Note we bootstrap with global
	// require here to get us into module context. Every subsequent call to require from inside
	// the module will go via moduleFn above.
	lua_getglobal(L, "require");
	lua_pushstring(L, moduleName);
	lua_call(L, 1, 1); // the module's env will be on the top of the stack
	return L;
}
