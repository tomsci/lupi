#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "../bitmap/bitmap.h"
#include <lupi/membuf.h>

#include "bapple.xbm"

int init_module_bapple_bapple(lua_State* L) {
	mbuf_newXbm(L, bapple);
	lua_setfield(L, 1, "xbm");

	return 0;
}
