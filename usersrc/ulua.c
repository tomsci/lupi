#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lupi/exec.h>

void abort() {
	for(;;) {} //TODO
}

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

void newProcessEntryPoint(const char* moduleName, const char* module, int moduleSize) {

	//uint32 superPage = *(uint32*)0xF802E000; // This should fail with far=F802E000
	//*(int*)(0xBAD) = superPage; // This definitely does

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	struct ModuleInfo readerInfo = { module, moduleSize };
	int ret = lua_load(L, readerFn, &readerInfo, moduleName, NULL);
	if (ret != LUA_OK) {
		lupi_printstring("Error loading lua module: ");
		lupi_printstring(lua_tostring(L, 1));
		abort();
	}
	lua_call(L, 0, 0);

	// I'm sure a whole bunch of stuff will need setting up here...
	// TODO need a better way of managing the serial port...
	lua_pushcfunction(L, putch_lua);
	lua_setglobal(L, "putch");
	lua_pushcfunction(L, getch_lua);
	lua_setglobal(L, "getch");

	lua_getglobal(L, "main");
	lua_call(L, 0, 0);
	abort(); // Shouldn't reach here

}

