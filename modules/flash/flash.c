#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lupi/membuf.h>
#include <lupi/exec.h>

int exec_driverCmd(uint32 driverHandle, uint32 arg1, uint32 arg2);

static int doReadTest(lua_State* L) {
	int handle = luaL_checkint(L, 1);
	uintptr addr = luaL_checkint(L, 2);

	// Create a new membuf that owns its memory
	MemBuf* membuf = mbuf_new(L, NULL, 256, NULL);

	uintptr args[] = { (uintptr)membuf->ptr, addr, membuf->len };
	exec_driverCmd(handle, KExecDriverFlashRead, (uintptr)args);
	return 1; // the membuf
}

static const uint8 testData[] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
};

static int doWriteTest(lua_State* L) {
	int handle = luaL_checkint(L, 1);
	uintptr addr = luaL_checkint(L, 2);
	uintptr args[] = { (uintptr)testData, addr, sizeof(testData) };
	exec_driverCmd(handle, KExecDriverFlashWrite, (uintptr)args);
	return 0;
}

// #define INCLUDE_TETRISDATA

#ifdef INCLUDE_TETRISDATA

#include "../tetris/tetrisa.pcm.chunk0.c"

static int writePageFromData(lua_State* L) {
	int handle = luaL_checkint(L, 1);
	int offset = luaL_checkint(L, 2);
	const uint8* dataPtr = &data[offset];
	uint32 destAddr = dataStart + offset;
	int dataLen = min(256, sizeof(data) - offset);
	uintptr args[] = { (uintptr)dataPtr, destAddr, dataLen };
	exec_driverCmd(handle, KExecDriverFlashWrite, (uintptr)args);
	bool moreData = (offset + dataLen < sizeof(data));
	lua_pushboolean(L, moreData);
	return 1;
}
#endif

int init_module_flash_flash(lua_State* L) {
	luaL_Reg fns[] = {
		{ "doReadTest", doReadTest },
		{ "doWriteTest", doWriteTest },
#ifdef INCLUDE_TETRISDATA
		{ "writePageFromData", writePageFromData },
#endif
		{ NULL, NULL }
	};
	luaL_setfuncs(L, fns, 0);
	return 0;
}
