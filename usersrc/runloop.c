#include <lupi/runloop.h>
#include <lupi/ipc.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

extern int exec_waitForAnyRequest();

#define AsyncRequestMetatable "LupiAsyncRequestMt"

static AsyncRequest* checkRequest(lua_State* L, int idx) {
	return (AsyncRequest*)luaL_checkudata(L, idx, AsyncRequestMetatable);
}

AsyncRequest* checkRequestPending(lua_State* L, int idx) {
	AsyncRequest* req = checkRequest(L, idx);
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

int newAsyncRequest(lua_State* L) {
	AsyncRequest* req = (AsyncRequest*)lua_newuserdata(L, sizeof(AsyncRequest));
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

static int wfar(lua_State* L) {
	int numRequests = exec_waitForAnyRequest();
	lua_pushinteger(L, numRequests);
	return 1;
}

static int getResult(lua_State* L) {
	AsyncRequest* req = checkRequest(L, 1);
	if (req->flags & KAsyncFlagCompleted) {
		//ASSERT(req->flags & KAsyncFlagIntResult, req->flags); // Don't support any other types of completion yet
		lua_pushinteger(L, req->result);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int setResult(lua_State* L) {
	AsyncRequest* req = checkRequest(L, 1);
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
	/*AsyncRequest* req =*/ checkRequest(L, 1);
	lua_getuservalue(L, 1);
	return 1;
}

static int setPending(lua_State* L) {
	AsyncRequest* req = checkRequest(L, 1);
	if (req->flags & KAsyncFlagPending) {
		return luaL_error(L, "Request is already pending");
	}
	else if (req->flags & KAsyncFlagCompleted) {
		return luaL_error(L, "Request is awaiting completion and cannot be reused yet");
	}
	req->flags |= KAsyncFlagPending;
	return 0;
}

static int clearFlags(lua_State* L) {
	AsyncRequest* req = checkRequest(L, 1);
	req->flags = 0;
	return 0;
}

int init_module_runloop(lua_State* L) {
	lua_newtable(L);
	luaL_Reg runloopFns[] = {
		{ "newAsyncRequest", newAsyncRequest },
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
		{ "clearFlags", clearFlags },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, reqFns, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setfield(L, -2, "AsyncRequest");
	return 0;
}
