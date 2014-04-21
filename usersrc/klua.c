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
#include <lupi/int64.h>

void putbyte(byte b);
byte getch();

#ifdef HOSTED
//#define USE_HOST_MALLOC_FOR_LUA
#endif

#if !defined(HOSTED) && !defined(ULUA_PRESENT)

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

#endif // HOSTED && !ULUA_PRESENT

#ifndef USE_HOST_MALLOC_FOR_LUA

void klua_heapReset();
void* klua_alloc_fn(void *ud, void *ptr, size_t osize, size_t nsize);

#endif // USE_HOST_MALLOC_FOR_LUA

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
static void WeveCrashedSetupDebuggingStuff(lua_State* L);

static int lua_newMemBuf(lua_State* L) {
	uint32 ptr = lua_tointeger(L, 1);
	int len = lua_tointeger(L, 2);
	mbuf_new(L, (void*)ptr, len, NULL);
	return 1;
}

// A variant of interactiveLuaPrompt that lets us write the actual intepreter loop as a lua module
void runLuaIntepreterModule(uintptr heapBase) {
#ifdef USE_HOST_MALLOC_FOR_LUA
	lua_State* L = luaL_newstate();
#else
	klua_heapReset(heapBase);
	lua_State* L = lua_newstate(klua_alloc_fn, (void*)heapBase);
#endif
	L = newLuaStateForModule("interpreter", L);
	const int interpreterIdx = lua_gettop(L);
	// the interpreter module is now at top of L stack
	if (!L) abort();

	lua_atpanic(L, panicFn);
	lua_pushcfunction(L, putch_lua);
	lua_setglobal(L, "putch");
	lua_pushcfunction(L, getch_lua);
	lua_setglobal(L, "getch");
	lua_pushstring(L, "klua> ");
	lua_setfield(L, interpreterIdx, "prompt");

#ifdef KLUA_DEBUGGER
	// klua debugger support
	if (TheSuperPage->marvin) {
		// We've crashed, set up debugging stuff
		WeveCrashedSetupDebuggingStuff(L);
	}
#endif

	lua_settop(L, interpreterIdx);
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

#ifndef HOSTED

extern uint32 GET32(uint32 ptr);
extern byte GET8(uint32 ptr);

static int memBufGetMem(lua_State* L, uintptr ptr, int size) {
	TheSuperPage->trapAbort = true;
	int result;
	if (size == 1) {
		result = GET8(ptr);
	} else {
		result = GET32(ptr);
	}
	TheSuperPage->trapAbort = false;
	if (TheSuperPage->exception) {
		TheSuperPage->exception = false;
		return luaL_error(L, "Abort at %p", (void*)ptr);
	}
	return result;
}

#endif // HOSTED

#ifdef KLUA_DEBUGGER

static int GetProcess_lua(lua_State* L) {
	int idx = luaL_checkint(L, 1);
	if (idx >= TheSuperPage->numValidProcessPages) {
		return luaL_error(L, "process index %d out of range", idx);
	}
	Process* p = GetProcess(idx);
	mbuf_get_object(L, (uintptr)p, sizeof(Process));
	return 1;
}

static void WeveCrashedSetupDebuggingStuff(lua_State* L) {
	lua_getglobal(L, "print");
	lua_setglobal(L, "p");

	lua_getfield(L, -1, "require");
	lua_pushstring(L, "membuf");
	lua_call(L, 1, 0);

	lua_pushcfunction(L, lua_newMemBuf);
	lua_setglobal(L, "mem");

#ifndef HOSTED
	mbuf_set_accessor(L, memBufGetMem); // Handles aborts
#endif

	mbuf_declare_type(L, "regset", sizeof(uint32)*17);
	mbuf_declare_member(L, "regset", "r0", 0, 4, NULL);
	mbuf_declare_member(L, "regset", "r1", 4, 4, NULL);
	mbuf_declare_member(L, "regset", "r2", 8, 4, NULL);
	mbuf_declare_member(L, "regset", "r3", 12, 4, NULL);
	mbuf_declare_member(L, "regset", "r4", 16, 4, NULL);
	mbuf_declare_member(L, "regset", "r5", 20, 4, NULL);
	mbuf_declare_member(L, "regset", "r6", 24, 4, NULL);
	mbuf_declare_member(L, "regset", "r7", 28, 4, NULL);
	mbuf_declare_member(L, "regset", "r8", 32, 4, NULL);
	mbuf_declare_member(L, "regset", "r9", 36, 4, NULL);
	mbuf_declare_member(L, "regset", "r10", 40, 4, NULL);
	mbuf_declare_member(L, "regset", "r11", 44, 4, NULL);
	mbuf_declare_member(L, "regset", "r12", 48, 4, NULL);
	mbuf_declare_member(L, "regset", "r13", 52, 4, NULL);
	mbuf_declare_member(L, "regset", "r14", 56, 4, NULL);
	mbuf_declare_member(L, "regset", "r15", 60, 4, NULL);
	mbuf_declare_member(L, "regset", "cpsr", 64, 4, NULL);

	MBUF_TYPE(SuperPage);
	MBUF_MEMBER(SuperPage, nextPid);
	MBUF_MEMBER(SuperPage, currentProcess);
	MBUF_MEMBER(SuperPage, currentThread);
	MBUF_MEMBER(SuperPage, numValidProcessPages);
	MBUF_MEMBER(SuperPage, blockedUartReceiveIrqHandler);
	MBUF_MEMBER(SuperPage, readyList);
	MBUF_MEMBER(SuperPage, uptime);
	MBUF_MEMBER(SuperPage, marvin);
	MBUF_MEMBER(SuperPage, trapAbort);
	MBUF_MEMBER(SuperPage, exception);
	MBUF_MEMBER_TYPE(SuperPage, crashRegisters, "regset");

	MBUF_NEW(SuperPage, TheSuperPage);
	lua_pushvalue(L, -1);
	lua_setglobal(L, "TheSuperPage");
	lua_setglobal(L, "sp");

	MBUF_TYPE(ThreadState);
	MBUF_ENUM(ThreadState, EReady);
	MBUF_ENUM(ThreadState, EBlocked);
	MBUF_ENUM(ThreadState, EDead);

	MBUF_TYPE(Thread);
	MBUF_MEMBER(Thread, prev);
	MBUF_MEMBER(Thread, next);
	MBUF_MEMBER(Thread, index);
	MBUF_MEMBER_TYPE(Thread, state, "ThreadState");
	MBUF_MEMBER(Thread, timeslice);
	MBUF_MEMBER_TYPE(Thread, savedRegisters, "regset");

	MBUF_TYPE(Process);
	MBUF_MEMBER(Process, pid);
	MBUF_MEMBER(Process, pdePhysicalAddress);
	MBUF_MEMBER(Process, heapLimit);
	MBUF_MEMBER_TYPE(Process, name, "char[]");
	MBUF_MEMBER(Process, numThreads);
	mbuf_declare_member(L, "Process", "firstThread", offsetof(Process, threads), sizeof(Thread), "Thread");

	for (int i = 0; i < TheSuperPage->numValidProcessPages; i++) {
		MBUF_NEW(Process, GetProcess(i));
		lua_pop(L, 1);
	}
	lua_pushcfunction(L, GetProcess_lua);
	lua_setglobal(L, "GetProcess");
}

#endif // KLUA_DEBUGGER
