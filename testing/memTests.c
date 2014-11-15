#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lupi/uluaHeap.h>

void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);
void ulua_setupGlobals(lua_State* L);

static const char* modules[] = {
	"membuf",
	"int64",
	"oo",
	"misc",
	"runloop", // Requires misc
	"interpreter", // Requires runloop
	"input.input", // Requires runloop, membuf
	"bitmap.bitmap",
	"tetris.tetris", // Requires bitmap, runloop, input
};

static int test_mem(lua_State* L) {
	printk("test_mem\n");

	// We tear down the existing heap and lua env entirely
	Heap* h = (Heap*)0x20070000;
	uluaHeap_reset(h);
	L = lua_newstate(uluaHeap_allocFn, h);
	luaL_openlibs(L);

	HeapStats stats;
	uluaHeap_stats(h, &stats);
	printk("Overheads of runtime: %d\n", stats.alloced);

	// This module itself is fairly minimal
	newLuaStateForModule("test.memTests", L);
	ulua_setupGlobals(L);
	lua_call(L, 1, 1);
	lua_gc(L, LUA_GCCOLLECT, 0);
	uluaHeap_stats(h, &stats);
	const int fixedOverhead = stats.alloced;
	printk("Fixed overheads: %d\n", fixedOverhead);

	for (int i = 0; i < sizeof(modules) / sizeof(char*); i++) {
		uluaHeap_reset(h);
		L = lua_newstate(uluaHeap_allocFn, h);
		luaL_openlibs(L);
		newLuaStateForModule(modules[i], L);
		lua_call(L, 1, 1);
		lua_gc(L, LUA_GCCOLLECT, 0);
		uluaHeap_stats(h, &stats);
		printk("Cost of module %s: %d\n", modules[i], stats.alloced - fixedOverhead);
	}

	// No way to safely return from this
	for(;;) {}
	return 0;
}

int init_module_test_memTests(lua_State* L) {
	lua_pushcfunction(L, test_mem);
	lua_setfield(L, -2, "test_mem");
	return 0;
}