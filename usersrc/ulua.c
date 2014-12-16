#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lupi/ipc.h>
#include <lupi/runloop.h>
#include <lupi/int64.h>
#include <lupi/exec.h>
#ifndef MALLOC_AVAILABLE
#include <lupi/uluaHeap.h>
#endif

// #define MEM_DEBUG

#ifdef MEM_DEBUG

#include <stdlib.h>

inline int getLuaMem(lua_State* L) {
	int mem = lua_gc(L, LUA_GCCOUNT, 0) * 1024;
	mem += lua_gc(L, LUA_GCCOUNTB, 0);
	return mem;
}
#define PRINT_MEM_STATS(fmt) PRINTL(fmt, getLuaMem(L)); memStats_lua(L)

#else

#define PRINT_MEM_STATS(fmt)

#endif // MEM_DEBUG

void exec_putch(char ch);
int exec_getch();
int exec_createProcess(const char* name);
int exec_getUptime();
void exec_getch_async(AsyncRequest* request);
NORETURN exec_abort();
void exec_reboot();
int exec_getInt(ExecGettableValue val);
void exec_threadYield();
int exec_threadCreate(void* newThreadState);
void exec_threadExit(int reason);
int exec_driverConnect(uint32 driverId);
int exec_driverCmd(uint32 driverHandle, uint32 arg1, uint32 arg2);
void hang();

uint32 user_ProcessPid;
char user_ProcessName[32];

NORETURN abort() {
	exec_abort();
}

#ifndef DEBUG_CUSTOM_ENTRY_POINT

static int putch_lua(lua_State* L) {
	int ch = lua_tointeger(L, 1);
	exec_putch((byte)ch);
	return 0;
}

static int getch_lua(lua_State* L) {
	lua_pushinteger(L, exec_getch());
	return 1;
}

static int getch_async(lua_State* L) {
	AsyncRequest* req = runloop_checkRequestPending(L, 1);
	req->flags |= KAsyncFlagAccepted;
	exec_getch_async(req);
	return 0;
}

static int getProcessName(lua_State* L) {
	lua_pushstring(L, user_ProcessName);
	return 1;
}

static int createProcess(lua_State* L) {
	const char* name = lua_tostring(L, 1);
	int pid = exec_createProcess(name);
	if (pid < 0) {
		return luaL_error(L, "Error %d creating process", pid);
	}
	lua_pushinteger(L, pid);
	return 1;
}

static int getUptime(lua_State* L) {
	uint64 t = exec_getUptime();
	int64_new(L, (int64)t);
	return 1;
}

static int crash(lua_State* L) {
	*(int*)(0xBAD) = 0xDEADBAD;
	return 0;
}

static int reboot_lua(lua_State* L) {
	exec_reboot();
	return 0;
}

static int printf_lua(lua_State* L) {
	lua_getglobal(L, "string");
	lua_getfield(L, -1, "format");
	lua_insert(L, 1);
	lua_pop(L, 1); // string
	lua_call(L, lua_gettop(L) - 1, 1); // call string.format(...)
	lua_getglobal(L, "print");
	lua_insert(L, 1);
	lua_call(L, lua_gettop(L) - 1, 1); // call print(formattedString)
	return 0;
}

// Must match enum ExecGettableValue
static const char* KGetIntEnums[] = {
	"TotalRam",
	"BootMode",
	"ScreenWidth",
	"ScreenHeight",
	"ScreenFormat",
	NULL // Must be last
};

static int getInt(lua_State* L) {
	int type = lua_type(L, 1);
	int arg;
	if (type == LUA_TSTRING) {
		arg = luaL_checkoption(L, 1, NULL, KGetIntEnums);
	} else {
		arg = luaL_checkinteger(L, 1);
	}
	int result = exec_getInt(arg);
	lua_pushinteger(L, result);
	return 1;
}

static int yield_lua(lua_State* L) {
	exec_threadYield();
	return 0;
}

static int panicFn(lua_State* L) {
	const char* str = lua_tostring(L, lua_gettop(L));
	lupi_printstring("\nLua panic:\n");
	lupi_printstring(str);
	// If we've got here, there's nothing we can really do which won't go
	// recursive except hang.
	hang();
	return 0;
}

static int driverConnect_lua(lua_State* L) {
	size_t codeLen;
	const char* code = luaL_checklstring(L, 1, &codeLen);
	ASSERTL(codeLen == 4, "Driver code must be 4 characters");
	int ret = exec_driverConnect(FOURCC(code));
	if (ret < 0) {
		return luaL_error(L, "Driver connection to '%s' failed with %d", code, ret);
	}
	lua_pushinteger(L, ret);
	return 1;
}

static int driverCmd_lua(lua_State* L) {
	uint32 handle = (uint32)luaL_checkinteger(L, 1);
	int arg1 = luaL_checkint(L, 2);
	int arg2 = luaL_optint(L, 3, 0);
	int ret = exec_driverCmd(handle, arg1, arg2);
	lua_pushinteger(L, ret);
	return 1;
}

static int threadCreate_lua(lua_State* L);
void ulua_setupGlobals(lua_State* L);

lua_State* newLuaStateForModule(const char* moduleName, lua_State* L);
int traceback_lua(lua_State* L);
int memStats_lua(lua_State* L);

#define SET_INT(L, name, val) lua_pushinteger(L, val); lua_setfield(L, -2, name);

int newProcessEntryPoint() {

	//uint32 superPage = *(uint32*)0xF802E000; // This should fail with far=F802E000
	//*(int*)(0xBAD) = superPage; // This definitely does

	const char* moduleName = user_ProcessName;
#ifdef MALLOC_AVAILABLE
	lua_State* L = newLuaStateForModule(moduleName, NULL);
	lua_atpanic(L, panicFn);
#else
	Heap* h = uluaHeap_init();
	lua_State* L = lua_newstate(uluaHeap_allocFn, h);
	uluaHeap_setLuaState(h, L);
	luaL_openlibs(L);
	lua_atpanic(L, panicFn);
	newLuaStateForModule(moduleName, L);
#endif

	PRINT_MEM_STATS("Lua mem usage after module init %d B");
	ulua_setupGlobals(L);

	lua_call(L, 1, 1); // Loads module, _ENV is now on top
	lua_getfield(L, -1, "main");
	if (!lua_isfunction(L, -1)) {
		luaL_error(L, "Module %s does not have a main function!", moduleName);
	}
	PRINT_MEM_STATS("Lua mem usage after main() fn %d B");

	lua_call(L, 0, 1);
	return lua_tointeger(L, -1);
}

void ulua_setupGlobals(lua_State* L) {
	// I'm sure a whole bunch of stuff will need setting up here...
	// TODO need a better way of managing the serial port...
	static const luaL_Reg globals[] = {
		{ "putch", putch_lua },
		{ "getch", getch_lua },
		{ "crash", crash },
		{ "reboot", reboot_lua },
		{ "printf", printf_lua },
		{ NULL, NULL }
	};
	static const luaL_Reg lupi_funcs[] = {
		{ "getProcessName", getProcessName },
		{ "createProcess", createProcess },
		{ "createThread", threadCreate_lua },
		{ "getUptime", getUptime },
		{ "getInt", getInt },
		{ "yield", yield_lua },
		{ "getch_async", getch_async },
		{ "memStats", memStats_lua },
		{ "driverConnect", driverConnect_lua },
		{ "driverCmd", driverCmd_lua },
		{ NULL, NULL }
	};
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	luaL_setfuncs(L, globals, 0);
	lua_pop(L, 1); // pops globals
	lua_newtable(L);
	luaL_setfuncs(L, lupi_funcs, 0);
	lua_setglobal(L, "lupi");

	// The debug table is evil and must be excised, except for debug.traceback
	// which is dead handy
	lua_newtable(L);
	const int newDebugTable = lua_gettop(L);
	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_setfield(L, newDebugTable, "traceback");
	lua_settop(L, newDebugTable);
	lua_setglobal(L, "debug");
}

static void copyArg(lua_State* oldL, int arg, lua_State* newL);

static int threadCreate_lua(lua_State* L) {
	// Create a new state sharing the same malloc but nothing else and pass it
	// to the thread create - it will be passed back to newThreadEntryPoint

	// Args are a single function (which must have no upvalues) and any number
	// of arguments, which will be deep copied into the new Lua state
	luaL_checktype(L, 1, LUA_TFUNCTION);

	lua_State* newL = newLuaStateForModule(NULL, NULL);

	const int n = lua_gettop(L);
	for (int i = 1; i <= n; i++) {
		copyArg(L, i, newL);
	}
	exec_threadCreate(newL);
	return 0;
}

static int dumpToBuf(lua_State* oldL, const void* p, size_t sz, void* ud) {
	luaL_addlstring((luaL_Buffer*)ud, (const char*)p, sz);
	return 0;
}

static void copyArg(lua_State* oldL, int arg, lua_State* newL) {
	arg = lua_absindex(oldL, arg);
	int type = lua_type(oldL, arg);
	switch (type) {
		case LUA_TNIL:
			lua_pushnil(newL);
			break;
		case LUA_TNUMBER: {
			lua_Number num = lua_tonumber(oldL, arg);
			lua_pushnumber(newL, num);
			break;
		}
		case LUA_TBOOLEAN: {
			int b = lua_toboolean(oldL, arg);
			lua_pushboolean(newL, b);
			break;
		}
		case LUA_TSTRING: {
			size_t len;
			const char* str = lua_tolstring(oldL, arg, &len);
			lua_pushlstring(newL, str, len);
			break;
		}
		case LUA_TTABLE: {
			if (lua_getmetatable(oldL, arg)) {
				luaL_error(oldL, "Copying tables with metatables is not supported");
			}
			lua_newtable(newL);
			int newTblIdx = lua_absindex(newL, -1);
			lua_pushnil(oldL);  /* first key */
			while (lua_next(oldL, arg) != 0) {
				const int key = -2;
				const int val = -1;
				copyArg(oldL, key, newL);
				copyArg(oldL, val, newL);
				lua_settable(newL, newTblIdx);
				/* removes 'value'; keeps 'key' for next iteration */
				lua_pop(oldL, 1);
			}
			break;
		}
		case LUA_TFUNCTION: {
			// We expect exactly one upvalue, being _ENV
			const char* upvalueName = lua_getupvalue(oldL, 1, 2);
			if (upvalueName) {
				luaL_error(oldL, "function passed to threadCreate cannot have any upvalues (except _ENV), like '%s'", upvalueName);
			}
			// Functions we serialise
			luaL_Buffer buf;
			luaL_buffinit(newL, &buf); // Use the new stack for buf
			lua_pushvalue(oldL, arg); // Get fn at top
			lua_dump(oldL, dumpToBuf, &buf);
			luaL_pushresult(&buf);
			lua_pop(oldL, 1); // Remove fn
			size_t len;
			const char* ptr = lua_tolstring(newL, -1, &len);
			int err = luaL_loadbuffer(newL, ptr, len, "thread");
			if (err) {
				lua_pop(newL, 1);
				luaL_error(oldL, "Error %d loading function into new thread", err);
				return;
			}
			lua_remove(newL, -2); // buf (since function is now on top)
			break;
		}
		case LUA_TLIGHTUSERDATA: {
			// I guess these are ok...?
			void* ptr = lua_touserdata(oldL, arg);
			lua_pushlightuserdata(newL, ptr);
			break;
		}
		default:
			luaL_error(oldL, "Type '%s' not supported by copyArg", lua_typename(oldL, type));
	}
}

void newThreadEntryPoint(lua_State* L) {
	ulua_setupGlobals(L);
	// First 2 items on stack are _ENV and the fn
	int nargs = lua_gettop(L) - 2;
	lua_pushcfunction(L, traceback_lua);
	lua_insert(L, 1); // stack pos 1 is now the trace fn
	int err = lua_pcall(L, nargs, 0, 1);
	if (err) {
		PRINTL("Error: %s", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
	lua_close(L);
	exec_threadExit(0);
}

#endif // DEBUG_CUSTOM_ENTRY_POINT
