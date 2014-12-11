// Support fns for lua when it's being run in the kernel
// This file is considered a user source, but it also has access to kernel headers
#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <k.h>
#if defined(ARM)
#include <arm.h>
#elif defined(ARMV7_M)
#include <armv7-m.h>
#endif

#ifndef MALLOC_AVAILABLE
#include <lupi/uluaHeap.h>
#endif

// #define MEM_DEBUG

#ifdef MEM_DEBUG

void malloc_stats();

inline int getLuaMem(lua_State* L) {
	int mem = lua_gc(L, LUA_GCCOUNT, 0) * 1024;
	mem += lua_gc(L, LUA_GCCOUNTB, 0);
	return mem;
}
#define PRINT_MEM_STATS(args...) printk(args, getLuaMem(L)); malloc_stats()

#else

#define PRINT_MEM_STATS(args...)

#endif // MEM_DEBUG

void putbyte(byte b);
byte getch();

#ifdef HOSTED
//#define USE_HOST_MALLOC_FOR_LUA
#endif

#if !defined(HOSTED) && !defined(ULUA_PRESENT)

NORETURN hang();

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

static int reboot_lua(lua_State* L) {
	reboot();
}

#endif // HOSTED && !ULUA_PRESENT

#ifndef USE_HOST_MALLOC_FOR_LUA

void klua_heapReset(uintptr hptr);
void* klua_alloc_fn(void *ud, void *ptr, size_t osize, size_t nsize);

#endif // USE_HOST_MALLOC_FOR_LUA

static int panicFn(lua_State* L) {
	const char* str = lua_tostring(L, lua_gettop(L));
	printk("\nLua panic: %s\n", str);
	// If we've got here, there's nothing we can really do which won't go
	// recursive except hang.
	hang();
	return 0;
}

#ifndef ULUA_PRESENT

void goDoLuaStuff() {
	//printk("%s\n", lua_ident);

	//printk("About to newstate\n");

#ifdef USE_HOST_MALLOC_FOR_LUA
	lua_State* L = luaL_newstate();
#else
	klua_heapReset(KLuaHeapBase);
	lua_State* L = lua_newstate(klua_alloc_fn, (void*)KLuaHeapBase);
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
	klua_heapReset(KLuaHeapBase);
	lua_State* L = lua_newstate(klua_alloc_fn, (void*)KLuaHeapBase);
#endif
	luaL_openlibs(L);
	lua_atpanic(L, panicFn);
	lua_pushcfunction(L, reboot_lua);
	lua_setglobal(L, "reboot");
	printk("klua> ");

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
			printk("klua> ");
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

#endif // ULUA_PRESENT

#ifdef KLUA_MODULES

static int putch_lua(lua_State* L) {
	int ch = lua_tointeger(L, 1);
	putbyte((byte)ch);
	return 0;
}

static int getch_lua(lua_State* L) {
	lua_pushinteger(L, getch());
	return 1;
}

lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);

#endif // KLUA_MODULES

#ifdef KLUA_MODULES

static lua_State* initModule(uintptr heapBase, const char* module) {
#if defined(USE_HOST_MALLOC_FOR_LUA)
	lua_State* L = luaL_newstate();
#elif !defined(HAVE_MMU)
	// We need to nuke the malloc state because it may be in an inconsistant
	// state due to the fact that we've crashed
	memset((void*)KUserBss, 0, KUserBssSize);

	// Don't have enough RAM to avoid stomping over user heap
	if (TheSuperPage->crashedHeapLimit == 0) {
		TheSuperPage->crashedHeapLimit = TheSuperPage->currentProcess->heapLimit;
	}
	TheSuperPage->currentProcess->heapLimit = KUserHeapBase;

	// Now we can either to malloc or uluaHeap
#ifdef LUPI_USE_MALLOC_FOR_KLUA
	lua_State* L = luaL_newstate();
#else
	Heap* h = uluaHeap_init();
	uluaHeap_disableDebugPrints(h); // Generally not helpful when wanting to use the debugger
	lua_State* L = lua_newstate(uluaHeap_allocFn, h);
#endif

#else
	// We have an MMU, just use klua heap
	klua_heapReset(heapBase);
	lua_State* L = lua_newstate(klua_alloc_fn, (void*)heapBase);
#endif

	PRINT_MEM_STATS("Mem used after klua newstate = %d B\n");
	lua_atpanic(L, panicFn);
	// Don't use luaL_openlibs, that opens extra modules we don't need and it
	// saves a smidgeon of RAM to pick and choose
	static const luaL_Reg libsWeDontHate[] = {
		{"_G", luaopen_base},
		{LUA_LOADLIBNAME, luaopen_package},
		{LUA_TABLIBNAME, luaopen_table},
		{LUA_STRLIBNAME, luaopen_string},
		{LUA_BITLIBNAME, luaopen_bit32},
		{NULL, NULL}
	};
	for (const luaL_Reg* lib = libsWeDontHate; lib->func; lib++) {
		luaL_requiref(L, lib->name, lib->func, 1);
		lua_pop(L, 1);  /* remove lib */
	}
	newLuaStateForModule(module, L);
	lua_call(L, 1, 1);
	PRINT_MEM_STATS("Mem used after module '%s' init = %d B\n", module);
	// the interpreter module is now at top of L stack

	lua_pushcfunction(L, putch_lua);
	lua_setglobal(L, "putch");
	lua_pushcfunction(L, getch_lua);
	lua_setglobal(L, "getch");
	return L;
}

// A variant of interactiveLuaPrompt that lets us write the actual interpreter loop as a lua module
void klua_runInterpreterModule() {
#ifndef KLuaHeapBase
	const uintptr heapBase = KLuaDebuggerSectionHeap;
#else
	const uintptr heapBase = KLuaHeapBase;
#endif
	lua_State* L = initModule(heapBase, "interpreter");
	lua_pushliteral(L, "klua> ");
	lua_setfield(L, -2, "prompt");

#ifdef KLUA_DEBUGGER
	// klua debugger support
	if (TheSuperPage->marvin) {
		// Pull in the debugger module
		PRINT_MEM_STATS("Mem usage pre require kluadebugger = %d B\n");
		lua_getfield(L, -1, "require");
		lua_pushliteral(L, "kluadebugger");
		lua_call(L, 1, 0);
		lua_gc(L, LUA_GCCOLLECT, 0);
	}
#endif

	PRINT_MEM_STATS("Mem usage pre main = %d B\n");
	lua_getfield(L, -1, "main");
	lua_call(L, 0, 0);
	// Shouldn't return
	ASSERT(false);
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

void klua_runInterpreter() {
	klua_runInterpreterModule();
}

#else // KLUA_MODULES

void klua_runInterpreter() {
	interactiveLuaPrompt();
}

#endif // KLUA_MODULES

#ifdef KLUA_DEBUGGER

void NAKED switchToKluaDebuggerMode() {
#ifdef ARM
	// for the klua debugger, use a custom stack and run in system mode.
	// This allows us access to all memory, but also means we can still do SVCs without
	// corrupting registers. This is required because Lua is still in user config so expects
	// to be able to do an SVC to print, for example, and System is the only mode that allows
	// this combination
	asm("MOV r1, r14");
	ModeSwitch(KPsrModeSystem | KPsrIrqDisable | KPsrFiqDisable);
	asm("LDR r13, .stackBase");
	asm("BX r1");
	LABEL_WORD(.stackBase, KLuaDebuggerStackBase + 0x1000);
#elif defined(ARMV7_M)
	asm("MOV r0, #0");
	asm("MSR CONTROL, r0"); // Thread mode privileged (NPRIV=0)
	asm("ISB");

	// And now drop to thread mode (using main handler stack)
	// We can do this even if there were nested exceptions because we have set
	// CCR_NONBASETHRDENA
	RFE_TO_MAIN(lr);
#endif
}

#endif // KLUA_DEBUGGER
