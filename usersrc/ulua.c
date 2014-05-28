#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lupi/ipc.h>
#include <lupi/runloop.h>
#include <lupi/int64.h>

void exec_putch(uint ch);
int exec_getch();
int exec_createProcess(const char* name);
int exec_getUptime();
void exec_getch_async(AsyncRequest* request);
void exec_abort();

uint32 user_ProcessPid;
char user_ProcessName[32];

void abort() {
	exec_abort();
}

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
	AsyncRequest* req = checkRequestPending(L, 1);
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

lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);

int newProcessEntryPoint() {

	//uint32 superPage = *(uint32*)0xF802E000; // This should fail with far=F802E000
	//*(int*)(0xBAD) = superPage; // This definitely does

	const char* moduleName = user_ProcessName;
	lua_State* L = newLuaStateForModule(moduleName, NULL);

	// I'm sure a whole bunch of stuff will need setting up here...
	// TODO need a better way of managing the serial port...
	static const luaL_Reg globals[] = {
		{ "putch", putch_lua },
		{ "getch", getch_lua },
		{ "getch_async", getch_async },
		{ "crash", crash },
		{ NULL, NULL }
	};
	static const luaL_Reg lupi_funcs[] = {
		{ "getProcessName", getProcessName },
		{ "createProcess", createProcess },
		{ "getUptime", getUptime },
		{ NULL, NULL }
	};
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	luaL_setfuncs(L, globals, 0);
	lua_newtable(L);
	luaL_setfuncs(L, lupi_funcs, 0);
	lua_setfield(L, -2, "lupi");

	lua_pop(L, 1);

	lua_getfield(L, -1, "main");
	if (!lua_isfunction(L, -1)) {
		luaL_error(L, "Module %s does not have a main function!", moduleName);
	}
	lua_call(L, 0, 1);
	return lua_tointeger(L, -1);
}
