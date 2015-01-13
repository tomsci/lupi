#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#ifndef MALLOC_AVAILABLE

#include <lupi/uluaHeap.h>

#define LMEM() (lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0))

lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);
void ulua_openLibs(lua_State* L);
void ulua_setupGlobals(lua_State* L);

static const char* modules[] = {
	"membuf",
	"membuf.types",
	"membuf.print",
	"int64",
	"oo",
	"misc",
	"runloop", // Requires misc
	"interpreter", // Requires runloop
	"input.input", // Requires runloop, membuf
	"bitmap.bitmap",
	"bitmap.transform",
	"tetris.tetris", // Requires bitmap, bitmap.transform, runloop, input
};

static int test_mem(lua_State* L) {
	printf("test_mem " LUA_RELEASE "\n");

	// We tear down the existing heap and lua env entirely
	Heap* h = (Heap*)0x20070000;
	uluaHeap_reset(h);
	HeapStats stats;

	L = lua_newstate(uluaHeap_allocFn, h);
	uluaHeap_stats(h, &stats);
	int newStateCost = stats.alloced;

	printf("lua_newstate: %d (%d)\n", newStateCost, LMEM());
	ulua_openLibs(L);

	uluaHeap_stats(h, &stats);
	printf("ulua_openLibs: %d (%d)\n", stats.alloced - newStateCost, LMEM() - newStateCost);

	newLuaStateForModule("test.emptyModule", L);
	ulua_setupGlobals(L);
	lua_call(L, 1, 1);
	lua_gc(L, LUA_GCCOLLECT, 0);
	uluaHeap_stats(h, &stats);
	const int fixedOverhead = stats.alloced;
	printf("Baseline mem usage: %d\n", fixedOverhead);

	// Check multiple require doesn't cost extra mem
	/*
	int requireMem[3];
	for (int i = 0; i < 3; i++) {
		lua_getglobal(L, "require");
		lua_pushstring(L, "test.memTests");
		lua_call(L, 1, 0);
		lua_gc(L, LUA_GCCOLLECT, 0);
		uluaHeap_stats(h, &stats);
		requireMem[i] = stats.alloced;
	}
	printf("Multiple requires: %d, %d, %d\n", requireMem[0], requireMem[1], requireMem[2]);
	*/

	for (int i = 0; i < sizeof(modules) / sizeof(char*); i++) {
		uluaHeap_reset(h);
		L = lua_newstate(uluaHeap_allocFn, h);
		luaL_openlibs(L);
		newLuaStateForModule(modules[i], L);
		ulua_setupGlobals(L);
		lua_call(L, 1, 1);
		lua_gc(L, LUA_GCCOLLECT, 0);
		uluaHeap_stats(h, &stats);
		printf("Module %s: %d (%d)\n", modules[i], stats.alloced - fixedOverhead, LMEM() - fixedOverhead);
	}
	// No way to safely return from this
	for(;;) {}
	return 0;
}
#endif

int init_module_test_memTests(lua_State* L) {
#ifndef MALLOC_AVAILABLE
	lua_pushcfunction(L, test_mem);
	lua_setfield(L, -2, "test_mem");
#endif
	return 0;
}
