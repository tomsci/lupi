#include <lua.h>
#include <lauxlib.h>
#include <lupi/runloop.h>
#include <lupi/membuf.h>
#include <lupi/exec.h>
#include <lupi/ipc.h>

int exec_driverConnect(uint32 driverId);
int exec_driverCmd(uint32 driverHandle, uint32 arg1, uint32 arg2);

typedef struct InputRequest {
	AsyncRequest asyncRequest; // must be first
	uint32 driverHandle;
	int maxSamples; // data must come immediately after
	int data[1]; // Extends beyond struct, size maxSamples*3.
} InputRequest;

static int inputRequestFn(lua_State* L) {
	InputRequest* req = (InputRequest*)runloop_checkRequestPending(L, 1);
	req->asyncRequest.flags |= KAsyncFlagAccepted;
	req->asyncRequest.result = (uintptr)&req->maxSamples;
	exec_driverCmd(req->driverHandle, KExecDriverTftInputRequest, (uint32)req);
	return 0;
}

static int newInputRequest(lua_State* L) {
	// 1 = runloop, 2 = maxSamples
	int maxSamples = luaL_optint(L, 2, 128);
	lua_settop(L, 1);
	lua_createtable(L, 0, 1); // members
	lua_pushcfunction(L, inputRequestFn);
	lua_setfield(L, -2, "requestFn");
	int dataSize = maxSamples * 3 * sizeof(int);
	int extraSize = dataSize + offsetof(InputRequest, data) - sizeof(AsyncRequest);
	lua_pushinteger(L, extraSize);
	// 1 = runloop, 2 = members, 3 = extraSize
	runloop_newAsyncRequest(L);
	InputRequest* req = (InputRequest*)runloop_checkRequest(L, -1);
	req->maxSamples = maxSamples;
	req->driverHandle = exec_driverConnect(FOURCC("pTFT"));

	mbuf_new(L, req->data, dataSize, NULL);
	lua_setfield(L, -2, "data");
	return 1;
}


int init_module_input_input(lua_State* L) {
	luaL_Reg fns[] = {
		{ "newInputRequest", newInputRequest },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, fns, 0);
	return 0;
}
