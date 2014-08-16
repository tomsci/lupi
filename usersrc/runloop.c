#include <lupi/runloop.h>
#include <lupi/ipc.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

extern int exec_waitForAnyRequest();

#define AsyncRequestMetatable "LupiAsyncRequestMt"

AsyncRequest* runloop_checkRequest(lua_State* L, int idx) {
	void* ptr = luaL_checkudata(L, idx, AsyncRequestMetatable);
	if (lua_rawlen(L, idx) == sizeof(void*)) {
		// It's a double indirect - used for AsyncRequests in IpcPages
		return *(AsyncRequest**)ptr;
	} else {
		return (AsyncRequest*)ptr;
	}
}

AsyncRequest* runloop_checkRequestPending(lua_State* L, int idx) {
	AsyncRequest* req = runloop_checkRequest(L, idx);
	if (!(req->flags & KAsyncFlagPending)) {
		luaL_error(L, "AsyncRequest must be pending");
	}
	if (req->flags & KAsyncFlagAccepted) {
		luaL_error(L, "AsyncRequest is already in use by kernel");
	}
	if (req->flags & KAsyncFlagCompleted) {
		luaL_error(L, "AsyncRequest is waiting to be handled");
	}
	return req;
}

int runloop_newAsyncRequest(lua_State* L) {
	int extraSize = luaL_optint(L, 3, 0);
	AsyncRequest* req = (AsyncRequest*)lua_newuserdata(L, sizeof(AsyncRequest) + extraSize);
	req->flags = 0;
	luaL_setmetatable(L, AsyncRequestMetatable);
	// If a table with completionFn etc specified was passed to the fn, use that for the uservalue
	// otherwise create a new table and use that
	if (lua_isnoneornil(L, 2)) {
		lua_newtable(L);
	} else {
		luaL_checktype(L, 2, LUA_TTABLE);
		lua_pushvalue(L, 2);
	}
	lua_setuservalue(L, -2);
	return 1;
}

void runloop_newIndirectAsyncRequest(lua_State* L, AsyncRequest* req) {
	AsyncRequest** obj = (AsyncRequest**)lua_newuserdata(L, sizeof(AsyncRequest*));
	*obj = req;
	luaL_setmetatable(L, AsyncRequestMetatable);
	// Setup uservalue
	lua_newtable(L);
	lua_setuservalue(L, -2);
}

static int wfar(lua_State* L) {
	int numRequests = exec_waitForAnyRequest();
	lua_pushinteger(L, numRequests);
	return 1;
}

static int getResult(lua_State* L) {
	AsyncRequest* req = runloop_checkRequest(L, 1);
	if (req->flags & KAsyncFlagCompleted) {
		//ASSERT(req->flags & KAsyncFlagIntResult, req->flags); // Don't support any other types of completion yet
		lua_pushinteger(L, req->result);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int setResult(lua_State* L) {
	AsyncRequest* req = runloop_checkRequest(L, 1);
	if (lua_isnoneornil(L, 2)) {
		req->flags = req->flags & ~(KAsyncFlagCompleted);
	} else if (req->flags & KAsyncFlagCompleted) {
		return luaL_error(L, "Async request has already been completed");
	} else if (lua_isnumber(L, 2)) {
		req->result = lua_tointeger(L, 2);
		req->flags = KAsyncFlagCompleted | KAsyncFlagIntResult;
	} else {
		return luaL_error(L, "Unknown result type");
	}
	return 0;
}

static int getMembers(lua_State* L) {
	/*AsyncRequest* req =*/ runloop_checkRequest(L, 1);
	lua_getuservalue(L, 1);
	return 1;
}

static int tostring(lua_State* L) {
	AsyncRequest* req = runloop_checkRequest(L, 1);
	const char* completed = (req->flags & KAsyncFlagCompleted) ? " completed" : "";
	const char* pending = (req->flags & KAsyncFlagPending) ? " pending" : "";
	const char* accepted = (req->flags & KAsyncFlagAccepted) ? " accepted" : "";
	lua_pushfstring(L, "AsyncRequest %p result=%d%s%s%s",
		req, req->result, pending, accepted, completed);
	return 1;
}

static int setPending(lua_State* L) {
	AsyncRequest* req = runloop_checkRequest(L, 1);
	if (req->flags & KAsyncFlagPending) {
		return luaL_error(L, "Request is already pending");
	}
	else if (req->flags & KAsyncFlagCompleted) {
		return luaL_error(L, "Request is awaiting completion and cannot be reused yet");
	}
	req->flags |= KAsyncFlagPending;
	return 0;
}

static int isFree(lua_State* L) {
	AsyncRequest* req = runloop_checkRequest(L, 1);
	lua_pushboolean(L, !(req->flags & KAsyncFlagAccepted));
	return 1;
}

static int clearFlags(lua_State* L) {
	AsyncRequest* req = runloop_checkRequest(L, 1);
	req->flags = 0;
	return 0;
}

int init_module_runloop(lua_State* L) {
	lua_newtable(L);
	luaL_Reg runloopFns[] = {
		{ "newAsyncRequest", runloop_newAsyncRequest },
		{ "waitForAnyRequest", wfar },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, runloopFns, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setfield(L, -2, "RunLoop");

	luaL_newmetatable(L, AsyncRequestMetatable);
	luaL_Reg reqFns[] = {
		{ "getMembers", getMembers },
		{ "getResult", getResult },
		{ "setResult", setResult },
		{ "setPending", setPending },
		{ "isFree", isFree },
		{ "clearFlags", clearFlags },
		{ "__tostring", tostring },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, reqFns, 0);
	lua_setfield(L, -2, "AsyncRequest");
	return 0;
}
