#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

void abort() {
	for(;;) {} //TODO
}

struct ModuleInfo {
	const char* module;
	int size;
};

static const char* readerFn(lua_State* L, void* data, size_t* size) {
	struct ModuleInfo* info = (struct ModuleInfo*)data;
	*size = info->size;
	return info->module;
}

void newProcessEntryPoint(const char* moduleName, const char* module, int moduleSize) {

	uint32 superPage = *(uint32*)0xF802E000; // This should fail with far=F802E000
	*(int*)(0xBAD) = superPage; // This definitely does

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	struct ModuleInfo readerInfo = { module, moduleSize };
	lua_load(L, readerFn, &readerInfo, moduleName, NULL);
	// I'm sure a whole bunch of stuff will need setting up here...
	lua_call(L, 0, 0);
	lua_getglobal(L, "main");
	lua_call(L, 0, 0);
	abort(); // Shouldn't reach here

}

