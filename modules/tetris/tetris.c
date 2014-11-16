#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "../bitmap/bitmap.h"
#include <lupi/membuf.h>

#include "tetris.xbm"

int init_module_tetris_tetris(lua_State* L) {
	mbuf_newXbm(L, tetris);
	lua_setfield(L, 1, "xbm");
	return 0;
}
