#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lupi/exec.h>
void exec_putch(uint ch);
int exec_getch();
int exec_createProcess(const char* name);
int exec_getUptime();

uint32 user_ProcessPid;
char user_ProcessName[32];

void abort() {
	for(;;) {} //TODO
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

static int getProcessName(lua_State* L) {
	lua_pushstring(L, user_ProcessName);
	return 1;
}

static int createProcess(lua_State* L) {
	const char* name = lua_tostring(L, 1);
	/*int err =*/ exec_createProcess(name);
	return 0; // TODO some error handling
}

static int getUptime(lua_State* L) {
	uint64 t = exec_getUptime();
	lua_pushinteger(L, t); // Hmm we should support 64-bit in Lua code...
	return 1;
}

lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);

void newProcessEntryPoint() {

	//uint32 superPage = *(uint32*)0xF802E000; // This should fail with far=F802E000
	//*(int*)(0xBAD) = superPage; // This definitely does

	const char* moduleName = user_ProcessName;
	lua_State* L = newLuaStateForModule(moduleName, NULL);

	// I'm sure a whole bunch of stuff will need setting up here...
	// TODO need a better way of managing the serial port...
	static const luaL_Reg globals[] = {
		{ "putch", putch_lua },
		{ "getch", getch_lua },
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
	lua_call(L, 0, 0);
	abort(); // Shouldn't reach here
}
