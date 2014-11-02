#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <k.h>
#include <mmu.h>
#if defined(ARM)
#include <arm.h>
#elif defined(ARMV7_M)
#include <armv7-m.h>
#endif
#include <lupi/membuf.h>

void malloc_stats();

// #define MEM_DEBUG

#ifdef MEM_DEBUG
static inline int getLuaMem(lua_State* L) {
	int mem = lua_gc(L, LUA_GCCOUNT, 0) * 1024;
	mem += lua_gc(L, LUA_GCCOUNTB, 0);
	return mem;
}
#define PRINT_MEM_STATS(args...) printk(args, getLuaMem(L)); malloc_stats()
#else
#define PRINT_MEM_STATS(args...)
#endif

#ifdef HAVE_MMU
#include <pageAllocator.h>

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
#endif // HAVE_MMU

static int printMallocStats_lua(lua_State* L) {
	malloc_stats();
	return 0;
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

#if !defined(HOSTED)

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

#endif // !defined(HOSTED)

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

static int reboot_lua(lua_State* L) {
	reboot();
	return 0;
}

int init_module_kluadebugger(lua_State* L) {
	PRINT_MEM_STATS("Mem usage pre init_module_kluadebugger = %d B\n");

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

#ifdef ARMV7_M
	mbuf_declare_type(L, "threadregset", sizeof(uint32)*9);
	mbuf_declare_member(L, "threadregset", "r4", 0, 4, NULL);
	mbuf_declare_member(L, "threadregset", "r5", 4, 4, NULL);
	mbuf_declare_member(L, "threadregset", "r6", 8, 4, NULL);
	mbuf_declare_member(L, "threadregset", "r7", 12, 4, NULL);
	mbuf_declare_member(L, "threadregset", "r8", 16, 4, NULL);
	mbuf_declare_member(L, "threadregset", "r9", 20, 4, NULL);
	mbuf_declare_member(L, "threadregset", "r10", 24, 4, NULL);
	mbuf_declare_member(L, "threadregset", "r11", 28, 4, NULL);
	mbuf_declare_member(L, "threadregset", "r13", 32, 4, NULL);
#endif

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
	MBUF_MEMBER(SuperPage, uptime);
	MBUF_MEMBER(SuperPage, currentProcess);
	MBUF_MEMBER(SuperPage, currentThread);
	MBUF_MEMBER(SuperPage, numValidProcessPages);
	MBUF_MEMBER(SuperPage, blockedUartReceiveIrqHandler);
	MBUF_MEMBER(SuperPage, readyList);
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
#ifdef ARM
	MBUF_MEMBER(SuperPage, svcPsrMode);
#endif
	MBUF_MEMBER(SuperPage, screenFormat);
	MBUF_MEMBER(SuperPage, numDfcsPending);
	MBUF_MEMBER(SuperPage, needToSendTouchUp);
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
#ifdef ARMV7_M
	MBUF_MEMBER_TYPE(Thread, savedRegisters, "threadregset");
#else
	MBUF_MEMBER_TYPE(Thread, savedRegisters, "regset");
#endif

#ifdef ARM
	MBUF_MEMBER_TYPE(SuperPage, dfcThread, "Thread");
#endif

#ifndef HAVE_MMU
	MBUF_MEMBER(SuperPage, crashedHeapLimit);
#endif

	MBUF_NEW(SuperPage, TheSuperPage);
	lua_setglobal(L, "TheSuperPage");

	MBUF_TYPE(Process);
	MBUF_MEMBER(Process, pid);
	MBUF_MEMBER(Process, pdePhysicalAddress);
	MBUF_MEMBER(Process, heapLimit);
	MBUF_MEMBER_TYPE(Process, name, "char[]");
	MBUF_MEMBER(Process, numThreads);
	//mbuf_declare_member(L, "Process", "firstThread", offsetof(Process, threads), sizeof(Thread), "Thread");

	for (int i = 0; i < TheSuperPage->numValidProcessPages; i++) {
		Process* p = GetProcess(i);
		MBUF_NEW(Process, p);
		lua_pop(L, 1);
		for (int j = 0; j < p->numThreads; j++) {
			MBUF_NEW(Thread, &p->threads[j]);
			lua_pop(L, 1);
		}
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

#ifdef HAVE_MMU
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
	DECLARE_FN(L, pageStats_getCounts, "pageStats_getCounts");
#endif

	DECLARE_FN(L, GetProcess_lua, "GetProcess");
	DECLARE_FN(L, switch_process_lua, "switch_process");
	DECLARE_FN(L, processForThread_lua, "processForThread");
	DECLARE_FN(L, printMallocStats_lua, "printMallocStats");

	// Force out of line copies of a few inline fns so that they're callable
	// via the symbol table should we want to
	FORCE_OUTOFLINE_COPY(firstThreadForProcess);
	FORCE_OUTOFLINE_COPY(processForThread);
	FORCE_OUTOFLINE_COPY(processForServer);
#ifdef ARM
	FORCE_OUTOFLINE_COPY(getFAR);
	FORCE_OUTOFLINE_COPY(getDFSR);
	FORCE_OUTOFLINE_COPY(getIFSR);
#endif

	EXPORT_INT(L, USER_STACK_SIZE);
	EXPORT_INT(L, USER_STACK_AREA_SHIFT);
	EXPORT_INT(L, KPageSize);
	EXPORT_INT(L, MAX_PROCESSES);
	EXPORT_INT(L, MAX_THREADS);
	EXPORT_INT(L, MAX_PROCESS_NAME);
	EXPORT_INT(L, MAX_DFCS);
	EXPORT_INT(L, THREAD_TIMESLICE);
#ifdef ARM
	EXPORT_INT(L, KKernelStackBase);
	EXPORT_INT(L, KKernelStackSize);
	EXPORT_INT(L, KUserStacksBase);
#endif
#ifdef ARMV7_M
	EXPORT_INT(L, KHandlerStackBase);
#endif
	EXPORT_INT(L, KUserHeapBase);
#ifdef HAVE_MMU
	EXPORT_INT(L, (uintptr)Al);
#endif

#ifdef ARMV7_M
#define MBUF_DECLARE_SCB_REG(name, reg) mbuf_declare_member(L, "SystemControlBlock", name, (reg)-KSystemControlSpace, sizeof(uint32), NULL)
	mbuf_declare_type(L, "SystemControlBlock", 4096); // Size if give or take
	MBUF_DECLARE_SCB_REG("icsr", SCB_ICSR);
	MBUF_DECLARE_SCB_REG("vtor", SCB_VTOR);
	MBUF_DECLARE_SCB_REG("shpr1", SCB_SHPR1);
	MBUF_DECLARE_SCB_REG("shpr2", SCB_SHPR2);
	MBUF_DECLARE_SCB_REG("shpr3", SCB_SHPR3);
	MBUF_DECLARE_SCB_REG("shcsr", SCB_SHCSR);
	MBUF_DECLARE_SCB_REG("cfsr", SCB_CFSR);
	MBUF_DECLARE_SCB_REG("hfsr", SCB_HFSR);
	MBUF_DECLARE_SCB_REG("mmar", SCB_MMAR);
	MBUF_DECLARE_SCB_REG("bfar", SCB_BFAR);
	MBUF_DECLARE_SCB_REG("ctrl", SYSTICK_CTRL);
	MBUF_DECLARE_SCB_REG("load", SYSTICK_LOAD);
	MBUF_DECLARE_SCB_REG("val", SYSTICK_VAL);
	MBUF_DECLARE_SCB_REG("calib", SYSTICK_CALIB);

	mbuf_new(L, (void*)KSystemControlSpace, 4096, "SystemControlBlock");
	lua_setglobal(L, "scb");

	MBUF_TYPE(ExceptionStackFrame);
	MBUF_MEMBER(ExceptionStackFrame, r0);
	MBUF_MEMBER(ExceptionStackFrame, r1);
	MBUF_MEMBER(ExceptionStackFrame, r2);
	MBUF_MEMBER(ExceptionStackFrame, r3);
	MBUF_MEMBER(ExceptionStackFrame, r12);
	MBUF_MEMBER(ExceptionStackFrame, lr);
	MBUF_MEMBER(ExceptionStackFrame, returnAddress);
	MBUF_MEMBER(ExceptionStackFrame, psr);

	FORCE_OUTOFLINE_COPY(getThreadExceptionStackFrame);
#endif

	if (TheSuperPage->totalRam < 256*1024) {
		lua_pushboolean(L, 1);
		lua_setglobal(L, "OhGodNoRam");
	}

	PRINT_MEM_STATS("Mem usage post init_module_kluadebugger = %d B\n");

	return 0;
}
