#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "bitmap.h"

#define BitmapMetatable "LupiBitmapMt"

Bitmap* bitmap_check(lua_State* L, int idx) {
	void* ptr = luaL_checkudata(L, idx, BitmapMetatable);
	return *(Bitmap**)ptr;
}

static int getxy(lua_State* L, int idx, int* x, int* y) {
	if (lua_type(L, idx) == LUA_TTABLE) {
		// Assume arg is a table {x,y}
		lua_rawgeti(L, idx, 1);
		*x = luaL_checkint(L, -1);
		lua_rawgeti(L, idx, 2);
		*y = luaL_checkint(L, -1);
		lua_pop(L, 2);
		return idx + 1;
	} else {
		*x = luaL_checkint(L, idx);
		*y = luaL_checkint(L, idx+1);
		return idx + 2;
	}
}

static int drawRect(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	int x, y;
	int nextArg = getxy(L, 2, &x, &y);
	int w = luaL_checkint(L, nextArg);
	int h = luaL_checkint(L, nextArg+1);
	Rect r = rect_make(x, y, w, h);
	bitmap_drawRect(b, &r);
	// PRINTL("dirtyRect = %d,%d,%dx%d", b->dirtyRect.x, b->dirtyRect.y, b->dirtyRect.w, b->dirtyRect.h);
	return 0;
}

static int drawText(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	const char* text = luaL_checkstring(L, 2);
	int x, y;
	getxy(L, 3, &x, &y);
	bitmap_drawText(b, x, y, text);
	// PRINTL("dirtyRect = %d,%d,%dx%d", b->dirtyRect.x, b->dirtyRect.y, b->dirtyRect.w, b->dirtyRect.h);
	return 0;
}

static int getTextSize(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	Rect r;
	rect_zero(&r);
	lua_len(L, 2);
	bitmap_getTextRect(b, lua_tointeger(L, -1), &r);
	lua_pushinteger(L, r.w);
	lua_pushinteger(L, r.h);
	return 2;
}

void bitmap_getTextRect(Bitmap* b, int numChars, Rect* result);


static int drawLine(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	int x0, y0, x1, y1;
	int nextIdx = getxy(L, 2, &x0, &y0);
	getxy(L, nextIdx, &x1, &y1);
	bitmap_drawLine(b, x0, y0, x1, y1);
	// PRINTL("dirtyRect = %d,%d,%dx%d", b->dirtyRect.x, b->dirtyRect.y, b->dirtyRect.w, b->dirtyRect.h);
	return 0;
}

static int setColour(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	int colour = luaL_checkint(L, 2);
	bitmap_setColour(b, (uint16)colour);
	return 0;
}

static int setBackgroundColour(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	int colour = luaL_checkint(L, 2);
	bitmap_setBackgroundColour(b, (uint16)colour);
	return 0;
}

static int getColour(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	lua_pushinteger(L, bitmap_getColour(b));
	return 1;
}

static int getBackgroundColour(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	lua_pushinteger(L, bitmap_getBackgroundColour(b));
	return 1;
}

static int getHeight(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	lua_pushinteger(L, bitmap_getHeight(b));
	return 1;
}

static int getWidth(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	lua_pushinteger(L, bitmap_getWidth(b));
	return 1;
}

static int create(lua_State* L) {
	int w = luaL_optint(L, 1, 0);
	int h = luaL_optint(L, 2, 0);
	Bitmap* b = bitmap_create(w, h);
	ASSERTL(b, "Couldn't create bitmap");
	Bitmap** bud = (Bitmap**)lua_newuserdata(L, sizeof(Bitmap*));
	*bud = b;
	luaL_setmetatable(L, BitmapMetatable);
	return 1;
}

static int blit(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	if (lua_isnoneornil(L, 2)) {
		bitmap_blitDirtyToScreen(b);
	} else {
		int x = luaL_optint(L, 2, 0);
		int y = luaL_optint(L, 3, 0);
		int w = luaL_optint(L, 4, bitmap_getWidth(b) - x);
		int h = luaL_optint(L, 5, bitmap_getHeight(b) - y);
		Rect r = rect_make(x, y, w, h);
		bitmap_blitToScreen(b, &r);
	}
	return 0;	
}

static int gc(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	bitmap_destroy(b);
	return 0;
}

static int setAutoBlit(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	bitmap_setAutoBlit(b, lua_toboolean(L, 2));
	return 0;
}

int init_module_bitmap_bitmap(lua_State* L) {
	luaL_newmetatable(L, BitmapMetatable);
	luaL_Reg fns[] = {
		{ "drawRect", drawRect },
		{ "drawText", drawText },
		{ "getTextSize", getTextSize },
		{ "drawLine", drawLine },
		{ "getHeight", getHeight },
		{ "getWidth", getWidth },
		{ "setColour", setColour },
		{ "setBackgroundColour", setBackgroundColour},
		{ "getColour", getColour },
		{ "getBackgroundColour", getBackgroundColour},
		{ "create", create },
		{ "blit", blit },
		{ "setAutoBlit", setAutoBlit },
		{ "__gc", gc },
		{ NULL, NULL },
	};
	luaL_setfuncs(L, fns, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setfield(L, -2, "Bitmap");
	return 0;
}
