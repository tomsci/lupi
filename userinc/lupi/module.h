#ifndef LUPI_MODULE_H
#define LUPI_MODULE_H

typedef struct lua_State lua_State;
typedef int (*lua_CFunction) (lua_State *L);

typedef struct LuaModule {
	const char* name;
	lua_CFunction nativeInit;
	const char* data;
	int size;
} LuaModule;

const LuaModule* getLuaModule(const char* moduleName);

#endif
