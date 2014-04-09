#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lupi/exec.h>

void abort() {
	for(;;) {} //TODO
}

void exec_putch(uint ch);
int exec_getch();

static int putch_lua(lua_State* L) {
	int ch = lua_tointeger(L, 1);
	exec_putch((byte)ch);
	return 0;
}

static int getch_lua(lua_State* L) {
	lua_pushinteger(L, exec_getch());
	return 1;
}

lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);

void newProcessEntryPoint(const char* moduleName) {

	//uint32 superPage = *(uint32*)0xF802E000; // This should fail with far=F802E000
	//*(int*)(0xBAD) = superPage; // This definitely does

	lua_State* L = newLuaStateForModule(moduleName, NULL);

	// I'm sure a whole bunch of stuff will need setting up here...
	// TODO need a better way of managing the serial port...
	lua_pushcfunction(L, putch_lua);
	lua_setglobal(L, "putch");
	lua_pushcfunction(L, getch_lua);
	lua_setglobal(L, "getch");

	lua_getfield(L, -1, "main");
	lua_call(L, 0, 0);
	abort(); // Shouldn't reach here
}
