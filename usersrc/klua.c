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
#include <lupi/membuf.h>

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

static int putch_lua(lua_State* L) {
	int ch = lua_tointeger(L, 1);
	putbyte((byte)ch);
	return 0;
}

static int getch_lua(lua_State* L) {
	lua_pushinteger(L, getch());
	return 1;
}

static int panicFn(lua_State* L) {
	const char* str = lua_tostring(L, lua_gettop(L));
	printk("\nLua panic: %s\n", str);
	return 0;
}

lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);

static int lua_newMemBuf(lua_State* L) {
	uint32 ptr = lua_tointeger(L, 1);
	int len = lua_tointeger(L, 2);
	mbuf_new(L, (void*)ptr, len);
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
	L = newLuaStateForModule("interpreter", L);
	// the interpreter module is now at top of L stack
	if (!L) abort();

	lua_atpanic(L, panicFn);
	lua_pushcfunction(L, putch_lua);
	lua_setglobal(L, "putch");
	lua_pushcfunction(L, getch_lua);
	lua_setglobal(L, "getch");
	lua_pushstring(L, "klua> ");
	lua_setfield(L, -2, "prompt");

	// klua debugger support
	lua_getglobal(L, "print");
	lua_setglobal(L, "p");

	lua_getfield(L, -1, "require");
	lua_pushstring(L, "membuf");
	lua_call(L, 1, 0);

	lua_pushcfunction(L, lua_newMemBuf);
	lua_setglobal(L, "mem");

	lua_getfield(L, -1, "main");
	lua_call(L, 0, 0);
	// Shouldn't return
	abort();
}

/*
int klua_dump_reader(const char* name, lua_Reader reader, void* readData, lua_Writer writer, void* writeData) {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	int ret = lua_load(L, reader, readData, name, NULL);
	if (ret) return ret;
	ret = lua_dump(L, writer, writeData);
	lua_close(L);
	return ret;
}
*/
