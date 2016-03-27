#include <stddef.h>
#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lupi/membuf.h>
#include <lupi/int64.h>
#include <lupi/runloop.h>
#include <lupi/module.h>

static const char* readerFn(lua_State* L, void* data, size_t* size) {
	const LuaModule** module = (const LuaModule**)data;
	if (*module == 0) return NULL;
	*size = (*module)->size;
	const char* result = (*module)->data;
	*module = NULL;
	return result;
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

int traceback_lua(lua_State* L) {
	const char *msg = lua_tostring(L, 1);
	if (msg) {
		luaL_traceback(L, L, msg, 1);
	} else if (!lua_isnoneornil(L, 1)) {  /* is there an error object? */
		if (!luaL_callmeta(L, 1, "__tostring")) {  /* try its 'tostring' metamethod */
			lua_pushliteral(L, "(no error message)");
		}
	}
	return 1;
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
	// upvalue 1 is the nativeInit function (or nil)
	// upvalue 2 is the module fn itself

	lua_newtable(L); // The _ENV
	luaL_setmetatable(L, "LupiGlobalMetatable");
	lua_pushvalue(L, lua_upvalueindex(2));
	// moduleFn at -1, _ENV at -2
	lua_pushvalue(L, -2); // dups _ENV
	lua_setupvalue(L, -2, 1); // Pops _ENV

	lua_pushvalue(L, -2); // Another _ENV

	if (lua_isfunction(L, lua_upvalueindex(1))) {
		// Call the nativeInit function if there is one
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_pushvalue(L, -2); // _ENV
		lua_call(L, 1, 0);
	}

	lua_pushcclosure(L, requireFn, 1); // upvalue 1 for requireFn is _ENV (pops _ENV)
	lua_setfield(L, -3, "require"); // pops the closure

	lua_call(L, 0, 0);
	// TODO check what call returns rather than always returning the _ENV
	return 1;
}

const char KNoCompiledModuleFmtString[] = "\n\tno compiled module " LUA_QS;

static const LuaModule* getLuaModuleForName(lua_State* L) {
	// arg at index 1 assumed to be moduleName
	const char* moduleName = lua_tostring(L, 1);
	const LuaModule* module = getLuaModule(moduleName);
	if (module) return module;
	// Otherwise...
	lua_pushfstring(L, KNoCompiledModuleFmtString, moduleName);
	// Try <moduleName>.<moduleName>
	lua_pushvalue(L, 1);
	lua_pushliteral(L, ".");
	lua_pushvalue(L, 1);
	lua_concat(L, 3);
	const char* module_module = lua_tostring(L, -1);
	module = getLuaModule(module_module);
	if (module) {
		lua_pop(L, 2); // module_module, error string
		return module;
	}
	// otherwise...
	lua_pushfstring(L, KNoCompiledModuleFmtString, module_module);
	lua_remove(L, -2); // module_module
	lua_concat(L, 2);
	// Finally, try <moduleName>.init
	lua_pushvalue(L, 1);
	lua_pushliteral(L, ".init");
	lua_concat(L, 2);
	const char* module_init = lua_tostring(L, -1);
	module = getLuaModule(module_init);
	if (module) {
		lua_pop(L, 2); // module_init, error string
		return module;
	}
	// otherwise...
	lua_pushfstring(L, KNoCompiledModuleFmtString, module_init);
	lua_remove(L, -2); // module_init
	lua_concat(L, 2);
	return NULL;
}

static int getLuaModule_searcherFn(lua_State* L) {
	const LuaModule* module = getLuaModuleForName(L);
	if (!module) {
		return 1;
	}
	if (module->nativeInit) {
		lua_pushcfunction(L, module->nativeInit);
	} else {
		lua_pushnil(L);
	}
	int ret = lua_load(L, readerFn, &module, module->name, NULL);
	if (ret != LUA_OK) {
		lua_pushfstring(L, "\n\tError loading compiled module %s: %s", module->name, lua_tostring(L, -1));
		return 1;
	}
	// Ok now the module function is on the top of the stack
	// We make it and the native init fn upvalues of loaderFn and return that
	lua_pushcclosure(L, loaderFn, 2);
	return 1;
}

// Returns with env of the module at the top of L's stack
/**
Creates a new `lua_State` and require()s `moduleName`. Does not actually call
require, rather it sets up the stack all ready for the call. This is so the
caller can make any needed changes to the environment before loading the first
bit of code.

`L` can be NULL in which case a new state is constructed using `luaL_newstate()`.
You may pass in an already-constructed `lua_State`, if for example you need to
use a custom allocator.

`moduleName` can be NULL in which case no module will be loaded, and what will
be returned will be an empty environment with nothing on the stack.

Usage:

	lua_State* L = newLuaStateForModule("init", NULL);
	// Do anything you need to L
	lua_call(L, 1, 1);
	// The module's _ENV is now on the top of the stack
*/
lua_State* newLuaStateForModule(const char* moduleName, lua_State* L) {

	if (L == NULL) {
		L = luaL_newstate();
		luaL_openlibs(L);
	}

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
	luaL_newmetatable(L, "LupiGlobalMetatable");
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	lua_setfield(L, -2, "__index"); // __index = _G
	lua_pop(L, 1);

	// Since we've just added support for require, we might as well use that to load the
	// main module for the process and save duplicating code. Note we bootstrap with global
	// require here to get us into module context. Every subsequent call to require from inside
	// the module will go via moduleFn above.
	if (moduleName) {
		lua_getglobal(L, "require");
		lua_pushstring(L, moduleName);
	}
	return L;
}

// No better place for these
#ifdef MALLOC_AVAILABLE

struct stats {
	lua_State* L;
	int allocCount;
	int freeCellCount;
	int used;
	int slabs[8];
	int wasted;
};

void malloc_stats();

void mallocInfoCallback(void* start, void* end, size_t used, void* ctxt) {
	if ((end - start) + 4 < used) {
		printf("WTF used %d start=%p end=%p\n", (int)used, start, end);
		return;
	}
	struct stats* s = (struct stats*)ctxt;
	if (used) s->allocCount++;
	else s->freeCellCount++;
	s->used += used;

	if (used && used <= 8) s->slabs[0]++;
	else if (used <= 12) s->slabs[1]++;
	else if (used <= 16) s->slabs[2]++;
	else if (used <= 20) s->slabs[3]++;
	else if (used <= 24) s->slabs[4]++;
	else if (used <= 28) s->slabs[5]++;
	else if (used <= 32) s->slabs[6]++;
	else if (used <= 36) s->slabs[7]++;
	else {
		printf("alloc %d\n", (int)used);
	}
	// The +4 is because end-start is always smaller than used, I don't really
	// know why. I assume the start ptr is being incorrectly adjusted
	int wasted = end - start + 4 - used;
	if (used && wasted) {
		s->wasted += wasted;
	}
}

void malloc_inspect_all(void(*handler)(void*, void *, size_t, void*), void* arg);
void malloc_stats();

int memStats_lua(lua_State* L) {
	lua_gc(L, LUA_GCCOLLECT, 0);
	struct stats s = {0};
	s.L = L;
	malloc_inspect_all(mallocInfoCallback, &s);
	printf("alloc count = %d\n", s.allocCount);
	printf("free count = %d\n", s.freeCellCount);
	printf("wasted bytes = %d\n", s.wasted);
	printf("used bytes = %d\n", s.used);
	for (int i = 0; i < 8; i++) {
		printf("Num <=%d bytes: %d\n", 8+4*i, s.slabs[i]);
	}
	malloc_stats();
	return 0;
}
#endif // MALLOC_AVAILABLE
