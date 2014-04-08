// Support fns for lua when it's being run in the kernel
// This file is considered a user source, but it also has access to kernel headers
#include <stddef.h>
#define LUPI_STDDEF_H // Stop kernel redefining std stuff
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <k.h>

void putbyte(byte b);
byte getch();

#ifdef HOSTED
//#define USE_HOST_MALLOC_FOR_LUA
#endif

#ifndef USE_HOST_MALLOC_FOR_LUA

// Dumbest allocator in the world. Doesn't reclaim memory, just returns sucessively growing
// pointers. We shall see if it's good enough....

typedef struct Heap {
	int nallocs;
	int nfrees;
	uintptr top;
	long alloced;
} Heap;

#define GetHeap() ((Heap*)KLuaHeapBase)
#define align(ptr) ((((uintptr)(ptr)) + 0x7) & ~0x7)

#endif // USE_HOST_MALLOC_FOR_LUA

#ifndef HOSTED

void hang() __attribute__((noreturn));

void abort() {
	printk("abort called. Arse!\n");
	hang();
}

// Only here to satisfy linkage of luaL_newstate (which we don't use)
void* realloc(void* ptr, size_t len) {
	printk("realloc bork!\n");
	hang();
}

void free(void *ptr) {
	printk("free bork!\n");
	hang();
}

void* malloc(size_t len) {
	return realloc(NULL, len);
}

#endif // HOSTED

#ifndef USE_HOST_MALLOC_FOR_LUA
void heapReset() {
	Heap* h = GetHeap();
	h->nallocs = 0;
	h->nfrees = 0;
	h->top = align(h+1);
	h->alloced = 0;
	//printk("Heap reset top = %p\n", (void*)h->top);
}

void* lua_alloc_fn(void *ud, void *ptr, size_t osize, size_t nsize) {
	//printk("lua_alloc_fn from %p\n", __builtin_return_address(0));
	Heap* h = (Heap*)ud;
	if (nsize == 0) {
		// free
		//printk("Freeing %p len %lu\n", ptr, osize);
		return NULL;
	}

	if (ptr && nsize <= osize) {
		return ptr;
	}

	void* result = (void*)h->top;
	h->top = align(h->top + nsize);
	// Don't bother checking - let the MMU fault us
	/*
	if (h->top > KLuaHeapBase + 1*1024*1024) {
		printk("No mem! nallocs=%d\n", h->nallocs);
		abort();
	}
	*/
	h->nallocs++;
	h->alloced += nsize;
	if (ptr) {
		// Remember to copy in the reallocd mem!
		memcpy(result, ptr, osize);
	}
	//printk("realloc returning %p for len=%d\n", (void*)result, (int)nsize);
	return result;
}
#endif // USE_HOST_MALLOC_FOR_LUA

void goDoLuaStuff() {
	//printk("%s\n", lua_ident);

	//printk("About to newstate\n");

#ifdef USE_HOST_MALLOC_FOR_LUA
	lua_State* L = luaL_newstate();
#else
	heapReset();
	lua_State* L = lua_newstate(lua_alloc_fn, GetHeap());
#endif
	luaL_openlibs(L);
	const char* prog = "print('hello from lua!')\n";
	//printk("About to loadstring\n");
	int ret = luaL_loadstring(L, prog);
	if (ret != LUA_OK) {
		printk("err from luaL_loadstring! %d\n", ret);
		return;
	}
	//printk("About to execute lua\n");
	ret = lua_pcall(L, 0, 0, 0);
	printk("goDoLuaStuff Done.\n");
	if (ret != LUA_OK) {
		printk("Err %d: %s\n", ret, lua_tostring(L, -1));
	}
}

void interactiveLuaPrompt() {
#ifdef USE_HOST_MALLOC_FOR_LUA
	lua_State* L = luaL_newstate();
#else
	heapReset();
	lua_State* L = lua_newstate(lua_alloc_fn, GetHeap());
#endif
	luaL_openlibs(L);
	printk("lua> ");

	char line[256];
	int lpos = 0;
	for(;;) {
		char ch = getch();
		if (ch == '\r') {
			printk("\n");
			line[lpos] = 0;
			int ret = luaL_loadstring(L, line);
			if (ret != LUA_OK) {
				printk("Error: %s\n", lua_tostring(L, -1));
				lua_pop(L, 1);
			} else {
				ret = lua_pcall(L, 0, 0, 0);
				if (ret != LUA_OK) {
					printk("Error: %s\n", lua_tostring(L, -1));
					lua_pop(L, 1);
				}
			}
			lpos = 0;
			printk("lua> ");
		} else if (ch == 8) {
			// Backspace
			if (lpos > 0) {
				lpos--;
				putbyte(ch);
				putbyte(' ');
				putbyte(ch);
			}
		} else if (lpos < sizeof(line)-1) {
			line[lpos++] = ch;
			printk("%c", (int)ch);
		}
	}
}

const char* getLuaModule(const char* name, int* length);

static int putch_lua(lua_State* L) {
	int ch = lua_tointeger(L, 1);
	putbyte((byte)ch);
	return 0;
}

static int getch_lua(lua_State* L) {
	lua_pushinteger(L, getch());
	return 1;
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

static int getModuleFn_lua(lua_State* L) {
	const char* modName = lua_tostring(L, 1);
	int len;
	const char* mod = getLuaModule(modName, &len);
	struct ModuleInfo readerInfo = { mod, len };
	int ret = lua_load(L, readerFn, &readerInfo, modName, NULL);
	if (ret) {
		// Error
		lua_pushnil(L);
	}
	return 1;
}

// A variant of interactiveLuaPrompt that lets us write the actual intepreter loop as a lua module
void runLuaIntepreterModule() {
#ifdef USE_HOST_MALLOC_FOR_LUA
	lua_State* L = luaL_newstate();
#else
	heapReset();
	lua_State* L = lua_newstate(lua_alloc_fn, GetHeap());
#endif
	luaL_openlibs(L);

	int n;
	const char* mod = getLuaModule("interpreter", &n);
	if (!mod) {
		printk("No lua interpreter module!\n");
		abort();
	}
	int ret = luaL_loadstring(L, mod);
	if (ret) {
		printk("Error %d loading interpreter module!\n%s\n", ret, lua_tostring(L, lua_gettop(L)));
		abort();
	}
	lua_call(L, 0, 0);
	lua_pushcfunction(L, putch_lua);
	lua_setglobal(L, "putch");
	lua_pushcfunction(L, getch_lua);
	lua_setglobal(L, "getch");
	lua_pushcfunction(L, getModuleFn_lua);
	lua_setglobal(L, "getModuleFn");
	lua_getglobal(L, "main");
	lua_call(L, 0, 0);
	// Shouldn't return
	abort();
}
