#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "../bitmap/bitmap.h"

#include "bapple.xbm"

int bapple(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	int screenx = luaL_optint(L, 2, 0);
	int screeny = luaL_optint(L, 3, 0);
	int x = luaL_optint(L, 4, 0);
	int y = luaL_optint(L, 5, 0);
	int w = luaL_optint(L, 6, bapple_width);
	int h = luaL_optint(L, 7, bapple_height);
	Rect r = rect_make(x, y, w, h);
	bitmap_drawXbm(b, screenx, screeny, &r, bapple);
	return 0;
}

int init_module_bapple_bapple(lua_State* L) {
	lua_pushcfunction(L, bapple);
	lua_setfield(L, 1, "bapple");
	return 0;
}
