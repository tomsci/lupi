#include <lua.h>
#include <lauxlib.h>

#include <lupi/runloop.h>
#include <lupi/int64.h>

void exec_setTimer(AsyncRequest* request, uint64* time);

static int setNewTimerCallback(lua_State* L) {
	AsyncRequest* req = runloop_checkRequestPending(L, 1);
	uint64 time = (uint64)int64_check(L, 2);
	PRINTL("[timerserver] setNewTimerCallback %d", (int)time);
	exec_setTimer(req, &time);
	return 0;
}

int init_module_timerserver_server(lua_State* L) {
	luaL_Reg fns[] = {
		{ "setNewTimerCallback", setNewTimerCallback },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, fns, 0);
	return 0;
}
