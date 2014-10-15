#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lupi/ipc.h>
#include <lupi/runloop.h>
#include <lupi/int64.h>
#include <lupi/exec.h>

// #define MEM_DEBUG

#ifdef MEM_DEBUG

void malloc_stats();
#include <stdlib.h>

inline int getLuaMem(lua_State* L) {
	int mem = lua_gc(L, LUA_GCCOUNT, 0) * 1024;
	mem += lua_gc(L, LUA_GCCOUNTB, 0);
	return mem;
}
#define PRINT_MEM_STATS(fmt) PRINTL(fmt, getLuaMem(L)); malloc_stats()

#else

#define PRINT_MEM_STATS(fmt)

#endif // MEM_DEBUG

void exec_putch(uint ch);
int exec_getch();
int exec_createProcess(const char* name);
int exec_getUptime();
void exec_getch_async(AsyncRequest* request);
NORETURN exec_abort();
void exec_reboot();
int exec_getInt(ExecGettableValue val);
void exec_threadYield();

uint32 user_ProcessPid;
char user_ProcessName[32];

NORETURN abort() {
	exec_abort();
}

#ifndef DEBUG_CUSTOM_ENTRY_POINT

static int putch_lua(lua_State* L) {
	int ch = lua_tointeger(L, 1);
	exec_putch((byte)ch);
	return 0;
}

static int getch_lua(lua_State* L) {
	lua_pushinteger(L, exec_getch());
	return 1;
}

static int getch_async(lua_State* L) {
	AsyncRequest* req = runloop_checkRequestPending(L, 1);
	req->flags |= KAsyncFlagAccepted;
	exec_getch_async(req);
	return 0;
}

static int getProcessName(lua_State* L) {
	lua_pushstring(L, user_ProcessName);
	return 1;
}

static int createProcess(lua_State* L) {
	const char* name = lua_tostring(L, 1);
	int pid = exec_createProcess(name);
	if (pid < 0) {
		return luaL_error(L, "Error %d creating process", pid);
	}
	lua_pushinteger(L, pid);
	return 1;
}

static int getUptime(lua_State* L) {
	uint64 t = exec_getUptime();
	int64_new(L, (int64)t);
	return 1;
}

static int crash(lua_State* L) {
	*(int*)(0xBAD) = 0xDEADBAD;
	return 0;
}

static int reboot_lua(lua_State* L) {
	exec_reboot();
	return 0;
}

static int getInt(lua_State* L) {
	int result = exec_getInt(lua_tointeger(L, 1));
	lua_pushinteger(L, result);
	return 1;
}

static int yield_lua(lua_State* L) {
	exec_threadYield();
	return 0;
}

lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);

#define SET_INT(L, name, val) lua_pushinteger(L, val); lua_setfield(L, -2, name);

int newProcessEntryPoint() {

	//uint32 superPage = *(uint32*)0xF802E000; // This should fail with far=F802E000
	//*(int*)(0xBAD) = superPage; // This definitely does

	const char* moduleName = user_ProcessName;
	lua_State* L = newLuaStateForModule(moduleName, NULL);

	PRINT_MEM_STATS("Lua mem usage after module init %d B");

	// I'm sure a whole bunch of stuff will need setting up here...
	// TODO need a better way of managing the serial port...
	static const luaL_Reg globals[] = {
		{ "putch", putch_lua },
		{ "getch", getch_lua },
		{ "crash", crash },
		{ "reboot", reboot_lua },
		{ NULL, NULL }
	};
	static const luaL_Reg lupi_funcs[] = {
		{ "getProcessName", getProcessName },
		{ "createProcess", createProcess },
		{ "getUptime", getUptime },
		{ "getInt", getInt },
		{ "yield", yield_lua },
		{ "getch_async", getch_async },
		{ NULL, NULL }
	};
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	luaL_setfuncs(L, globals, 0);
	lua_pop(L, 1); // pops globals
	lua_newtable(L);
	luaL_setfuncs(L, lupi_funcs, 0);
	SET_INT(L, "TotalRam", EValTotalRam);
	SET_INT(L, "BootMode", EValBootMode);
	SET_INT(L, "ScreenWidth", EValScreenWidth);
	SET_INT(L, "ScreenHeight", EValScreenHeight);
	lua_setglobal(L, "lupi");

	// The debug table is evil and must be excised, except for debug.traceback
	// which is dead handy
	lua_newtable(L);
	const int newDebugTable = lua_gettop(L);
	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_setfield(L, newDebugTable, "traceback");
	lua_settop(L, newDebugTable);
	lua_setglobal(L, "debug");

	lua_call(L, 1, 1); // Loads module, _ENV is now on top
	lua_getfield(L, -1, "main");
	if (!lua_isfunction(L, -1)) {
		luaL_error(L, "Module %s does not have a main function!", moduleName);
	}
	PRINT_MEM_STATS("Lua mem usage after main() fn %d B");

	lua_call(L, 0, 1);
	return lua_tointeger(L, -1);
}
#endif
