// Support fns for lua when it's being run in the kernel
// This file is considered a user source, but it also has access to kernel headers
#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <k.h>
#include <pageAllocator.h>
#include <mmu.h>
#if defined(ARM)
#include <arm.h>
#elif defined(ARMV7_M)
#include <armv7-m.h>
#endif
#include <lupi/membuf.h>
#include <lupi/int64.h>

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

#endif // HOSTED && !ULUA_PRESENT

#ifndef USE_HOST_MALLOC_FOR_LUA

void klua_heapReset(uintptr hptr);
void* klua_alloc_fn(void *ud, void *ptr, size_t osize, size_t nsize);

#endif // USE_HOST_MALLOC_FOR_LUA

static int panicFn(lua_State* L) {
	const char* str = lua_tostring(L, lua_gettop(L));
	printk("\nLua panic: %s\n", str);
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

#ifdef KLUA_DEBUGGER
static void WeveCrashedSetupDebuggingStuff(lua_State* L);

static int lua_newMemBuf(lua_State* L) {
	uint32 ptr = lua_tointeger(L, 1);
	int len = lua_tointeger(L, 2);
	const char* type = lua_tostring(L, 3);
	mbuf_new(L, (void*)ptr, len, type);
	return 1;
}

static int lua_getObj(lua_State* L) {
	uint32 ptr = lua_tointeger(L, 1);
	int len = lua_tointeger(L, 2);
	mbuf_push_object(L, ptr, len);
	return 1;
}
#endif

#ifdef KLUA_MODULES

static lua_State* initModule(uintptr heapBase, const char* module) {
#ifdef USE_HOST_MALLOC_FOR_LUA
	lua_State* L = luaL_newstate();
#else
	klua_heapReset(heapBase);
	lua_State* L = lua_newstate(klua_alloc_fn, (void*)heapBase);
#endif
	lua_atpanic(L, panicFn);
	newLuaStateForModule(module, L);
	lua_call(L, 1, 1);
	// the interpreter module is now at top of L stack
	ASSERT(L != NULL);

	lua_pushcfunction(L, putch_lua);
	lua_setglobal(L, "putch");
	lua_pushcfunction(L, getch_lua);
	lua_setglobal(L, "getch");
	return L;
}

// A variant of interactiveLuaPrompt that lets us write the actual intepreter loop as a lua module
void klua_runInterpreterModule() {
	lua_State* L = initModule(KLuaHeapBase, "interpreter");
	lua_pushliteral(L, "klua> ");
	lua_setfield(L, -2, "prompt");

#ifdef KLUA_DEBUGGER
	// klua debugger support
	if (TheSuperPage->marvin) {
		// We've crashed, set up debugging stuff
		WeveCrashedSetupDebuggingStuff(L);
	}
#endif

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

#if !defined(HOSTED) && defined(KLUA_DEBUGGER)

extern uint32 GET32(uint32 ptr);
extern byte GET8(uint32 ptr);

static int memBufGetMem(lua_State* L, uintptr ptr, int size) {
	//printk("Setting trapAbort...\n");
	TheSuperPage->trapAbort = true;
	int result;
	if (size == 1) {
		result = GET8(ptr);
	} else {
		result = GET32(ptr);
	}
	TheSuperPage->trapAbort = false;
	//printk("Exited trapAbort exception = %d\n", (int)TheSuperPage->exception);
	if (TheSuperPage->exception) {
		TheSuperPage->exception = false;
		return luaL_error(L, "Abort accessing memory at address %p", (void*)ptr);
	}
	return result;
}

#endif // !defined(HOSTED) && defined(KLUA_DEBUGGER)

void klua_runInterpreter() {
	klua_runInterpreterModule();
}

#else // KLUA_MODULES

void klua_runInterpreter() {
	interactiveLuaPrompt();
}

#endif // KLUA_MODULES

#ifdef KLUA_DEBUGGER

void NAKED switchToKluaDebuggerMode(uintptr sp) {
	// for the klua debugger, use a custom stack and run in system mode.
	// This allows us access to all memory, but also means we can still do SVCs without
	// corrupting registers. This is required because Lua is still in user config so expects
	// to be able to do an SVC to print, for example, and System is the only mode that allows
	// this combination
	asm("MOV r1, r14");
	ModeSwitch(KPsrModeSystem | KPsrIrqDisable | KPsrFiqDisable);
	asm("MOV r13, r0");
	asm("BX r1");
}

#define EXPORT_INT(L, val) lua_pushunsigned(L, val); lua_setglobal(L, #val)
#define DECLARE_FN(L, fn, name) \
	lua_pushcfunction(L, fn); \
	lua_setglobal(L, name)
#define FORCE_OUTOFLINE_COPY(fn) GET32((uintptr)&fn)

static int GetProcess_lua(lua_State* L) {
	int idx = luaL_checkint(L, 1);
	if (idx >= TheSuperPage->numValidProcessPages) {
		return luaL_error(L, "process index %d out of range", idx);
	}
	Process* p = GetProcess(idx);
	mbuf_push_object(L, (uintptr)p, sizeof(Process));
	return 1;
}

static int pageStats_getCounts(lua_State* L) {
	int count[KPageNumberOfTypes];
	for (int i = 0; i < KPageNumberOfTypes; i++) count[i] = 0;
	PageAllocator* al = Al;
	const int n = al->numPages;
	for (int i = 0; i < n; i++) {
		int type = al->pageInfo[i];
		if (type < KPageNumberOfTypes) {
			count[type]++;
		} else {
			printk("Unknown page type %d for page %d!\n", type, i);
		}
	}

	lua_newtable(L);
	for (int i = 0; i < KPageNumberOfTypes; i++) {
		lua_pushinteger(L, count[i]);
		lua_rawseti(L, -2, i);
	}
	return 1;
}

static int switch_process_lua(lua_State* L) {
	Process* p;
	if (lua_isnumber(L, 1)) {
		p = GetProcess(lua_tointeger(L, 1));
	} else {
		uintptr ptr = (uintptr)mbuf_checkbuf_type(L, 1, "Process")->ptr;
		ASSERTL((ptr & 0xFFF) == 0);
		ASSERTL((ptr - (uintptr)GetProcess(0)) >> KPageShift < MAX_PROCESSES);
		p = (Process*)ptr;
	}
	switch_process(p);
	return 0;
}

static int reboot_lua(lua_State* L) {
	reboot();
	return 0;
}

static int processForThread_lua(lua_State* L) {
	Thread* t = (Thread*)mbuf_checkbuf_type(L, 1, "Thread")->ptr;
	mbuf_push_object(L, (uintptr)processForThread(t), sizeof(Thread));
	return 1;
}

typedef uint32 (*GenericFunction)(uint32 arg1, uint32 arg2, uint32 arg3);

static int executeFn(lua_State* L) {
	uintptr fn = luaL_checkunsigned(L, 1);
	uint32 arg1 = luaL_optunsigned(L, 2, 0);
	uint32 arg2 = luaL_optunsigned(L, 3, 0);
	uint32 arg3 = luaL_optunsigned(L, 4, 0);

	// Casting to GenericFunction should mean we can execute the function
	// with a reasonable degree of success thanks to AAPCS. We don't have too
	// many functions (if any) whose calling sequence isn't simply to stuff the
	// args into r0-r3
	uint32 ret = ((GenericFunction)fn)(arg1, arg2, arg3);
	lua_pushunsigned(L, ret);
	return 1;
}

static void WeveCrashedSetupDebuggingStuff(lua_State* L) {
	// Interpreter module at top of stack
	lua_getfield(L, -1, "require");
	lua_pushliteral(L, "membuf");
	lua_call(L, 1, 0);

	DECLARE_FN(L, lua_newMemBuf, "newmem");
	DECLARE_FN(L, lua_getObj, "mem");
	DECLARE_FN(L, reboot_lua, "reboot");
	DECLARE_FN(L, executeFn, "executeFn");

#ifndef HOSTED
	mbuf_set_accessor(L, memBufGetMem); // Handles aborts
#endif

	// Things embedded in structs (as opposed to being pointers) must be declared before the
	// things they are embedded in.
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

	MBUF_TYPE(KAsyncRequest);
	MBUF_MEMBER(KAsyncRequest, thread);
	MBUF_MEMBER(KAsyncRequest, userPtr);

	MBUF_TYPE(Server);
	MBUF_MEMBER_TYPE(Server, id, "char[]"); // It's a uint32 kernel-side but that's actually a union to a char[4]
	MBUF_MEMBER_TYPE(Server, serverRequest, "KAsyncRequest");
	MBUF_MEMBER(Server, blockedClientList);

	MBUF_TYPE(Dfc);
	MBUF_MEMBER(Dfc, fn);
	MBUF_MEMBER(Dfc, args[0]);
	MBUF_MEMBER(Dfc, args[1]);
	MBUF_MEMBER(Dfc, args[2]);

	MBUF_TYPE(Driver);
	MBUF_MEMBER_TYPE(Driver, id, "char[]");
	MBUF_MEMBER(Driver, execFn);

	MBUF_TYPE(SuperPage);
	MBUF_MEMBER(SuperPage, totalRam);
	MBUF_MEMBER(SuperPage, boardRev);
	MBUF_MEMBER(SuperPage, bootMode);
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
	MBUF_MEMBER(SuperPage, uartDroppedChars);
	MBUF_MEMBER_TYPE(SuperPage, uartRequest, "KAsyncRequest");
	MBUF_MEMBER_TYPE(SuperPage, timerRequest, "KAsyncRequest");
	MBUF_MEMBER(SuperPage, timerCompletionTime);
	MBUF_MEMBER_TYPE(SuperPage, crashRegisters, "regset");
	// TODO handle arrays...
	// Servers, for implementation reasons, fill the servers array from the end backwards
	mbuf_declare_member(L, "SuperPage", "firstServer", offsetof(SuperPage, servers[MAX_SERVERS-1]), sizeof(Server), "Server");
	MBUF_MEMBER(SuperPage, rescheduleNeededOnSvcExit);
	MBUF_MEMBER(SuperPage, svcPsrMode);
	MBUF_MEMBER(SuperPage, numDfcsPending);
	// dfcThread has to be declared after Thread
	MBUF_MEMBER_TYPE(SuperPage, inputRequest, "KAsyncRequest");
	MBUF_MEMBER(SuperPage, inputRequestBuffer);
	MBUF_MEMBER(SuperPage, inputRequestBufferSize);

	MBUF_TYPE(ThreadState);
	MBUF_ENUM(ThreadState, EReady);
	MBUF_ENUM(ThreadState, EBlockedFromSvc);
	MBUF_ENUM(ThreadState, EDying);
	MBUF_ENUM(ThreadState, EDead);
	MBUF_ENUM(ThreadState, EWaitForRequest);

	MBUF_TYPE(Thread);
	MBUF_MEMBER(Thread, prev);
	MBUF_MEMBER(Thread, next);
	MBUF_MEMBER(Thread, index);
	MBUF_MEMBER_TYPE(Thread, state, "ThreadState");
	MBUF_MEMBER(Thread, timeslice);
	MBUF_MEMBER(Thread, exitReason);
	MBUF_MEMBER_TYPE(Thread, savedRegisters, "regset");

	MBUF_MEMBER_TYPE(SuperPage, dfcThread, "Thread");

	MBUF_NEW(SuperPage, TheSuperPage);
	lua_setglobal(L, "TheSuperPage");

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
	for (int i = 0; i < MAX_DRIVERS; i++) {
		if (TheSuperPage->drivers[i].id) {
			MBUF_NEW(Driver, &TheSuperPage->drivers[i]);
			lua_pop(L, 1);
		}
	}

	MBUF_TYPE(Server);
	MBUF_MEMBER_TYPE(Server, id, "char[]");
	MBUF_MEMBER_TYPE(Server, serverRequest, "KAsyncRequest");

	MBUF_TYPE(PageAllocator);
	MBUF_MEMBER(PageAllocator, numPages);
	MBUF_MEMBER(PageAllocator, firstFreePage);
	MBUF_NEW(PageAllocator, Al);
	lua_setglobal(L, "Al");

	mbuf_declare_type(L, "PageType", 1);
	MBUF_ENUM(PageType, KPageFree);
	MBUF_ENUM(PageType, KPageSect0);
	MBUF_ENUM(PageType, KPageAllocator);
	MBUF_ENUM(PageType, KPageProcess);
	MBUF_ENUM(PageType, KPageUserPde);
	MBUF_ENUM(PageType, KPageUserPt);
	MBUF_ENUM(PageType, KPageUser);
	MBUF_ENUM(PageType, KPageKluaHeap);
	MBUF_ENUM(PageType, KPageKernPtForProcPts);
	MBUF_ENUM(PageType, KPageSharedPage);
	MBUF_ENUM(PageType, KPageThreadSvcStack);

	DECLARE_FN(L, GetProcess_lua, "GetProcess");
	DECLARE_FN(L, pageStats_getCounts, "pageStats_getCounts");
	DECLARE_FN(L, switch_process_lua, "switch_process");
	DECLARE_FN(L, processForThread_lua, "processForThread");

	// Force out of line copies of a few inline fns so that they're callable
	// via the symbol table should we want to
	FORCE_OUTOFLINE_COPY(firstThreadForProcess);
	FORCE_OUTOFLINE_COPY(processForThread);
	FORCE_OUTOFLINE_COPY(processForServer);
	FORCE_OUTOFLINE_COPY(getFAR);
	FORCE_OUTOFLINE_COPY(getDFSR);
	FORCE_OUTOFLINE_COPY(getIFSR);

	EXPORT_INT(L, USER_STACK_SIZE);
	EXPORT_INT(L, USER_STACK_AREA_SHIFT);
	EXPORT_INT(L, KPageSize);
	EXPORT_INT(L, MAX_PROCESSES);
	EXPORT_INT(L, MAX_THREADS);
	EXPORT_INT(L, MAX_PROCESS_NAME);
	EXPORT_INT(L, MAX_DFCS);
	EXPORT_INT(L, THREAD_TIMESLICE);
	EXPORT_INT(L, KKernelStackBase);
	EXPORT_INT(L, KKernelStackSize);
	EXPORT_INT(L, KUserHeapBase);
	EXPORT_INT(L, KUserStacksBase);
	EXPORT_INT(L, KPageAllocatorAddr);

	lua_getfield(L, -1, "require");
	lua_pushliteral(L, "kluadebugger");
	lua_call(L, 1, 0);

}

#endif // KLUA_DEBUGGER
