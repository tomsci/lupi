// Support fns for lua when it's being run in the kernel
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>

// This file is considered a user source, so it doesn't have access to any of the kernel headers
// (except for the platform -include file)
// Therefore any kernel functions (like printk) must be forward-declared rather than #included
void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
void putbyte(byte b);
byte getch();

#ifdef HOSTED
//#define USE_HOST_MALLOC_FOR_LUA
#endif

#ifndef USE_HOST_MALLOC_FOR_LUA

// TODO this needs updating once we enable the MMU
#define KLuaHeap (KPhysicalRamBase + 0x00200000)

// Dumbest allocator in the world. Doesn't reclaim memory, just returns sucessively growing
// pointers. We shall see if it's good enough....

typedef struct Heap {
	int nallocs;
	int nfrees;
	uintptr top;
	long alloced;
} Heap;

#define GetHeap() ((Heap*)KLuaHeap)
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

	h->nallocs++;
	void* result = (void*)h->top;
	h->top = align(h->top + nsize);
	if (h->top > KPhysicalRamBase + KPhysicalRamSize) {
		printk("No mem!\n");
		abort();
	}
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
		} else {
			line[lpos++] = ch;
			printk("%c", (int)ch);
		}
	}
}



