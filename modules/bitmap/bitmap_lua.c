#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lupi/membuf.h>
#include <lupi/exec.h>
#include "bitmap.h"

#define BitmapMetatable "LupiBitmapMt"

int exec_getInt(ExecGettableValue val);

Bitmap* bitmap_check(lua_State* L, int idx) {
	void* ptr = luaL_checkudata(L, idx, BitmapMetatable);
	return (Bitmap*)ptr;
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
	Rect r = rect_make(0, 0, bitmap_getWidth(b), bitmap_getHeight(b));
	rect_invert(&r, &b->transform);
	lua_pushinteger(L, r.h);
	return 1;
}

static int getWidth(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	Rect r = rect_make(0, 0, bitmap_getWidth(b), bitmap_getHeight(b));
	rect_invert(&r, &b->transform);
	lua_pushinteger(L, r.w);
	return 1;
}

static int getRawHeight(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	Rect r = rect_make(0, 0, bitmap_getWidth(b), bitmap_getHeight(b));
	lua_pushinteger(L, r.h);
	return 1;
}

static int getRawWidth(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	Rect r = rect_make(0, 0, bitmap_getWidth(b), bitmap_getHeight(b));
	lua_pushinteger(L, r.w);
	return 1;
}

static int create(lua_State* L) {
	int w = luaL_optint(L, 1, 0);
	int h = luaL_optint(L, 2, 0);
	if (!w) w = exec_getInt(EValScreenWidth);
	if (!h) h = exec_getInt(EValScreenHeight);
	int sz = bitmap_getAllocSize(w, h);
	void* mem = lua_newuserdata(L, sz);
	/*Bitmap* b =*/ bitmap_construct(mem, w, h);
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

static int setAutoBlit(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	bitmap_setAutoBlit(b, lua_toboolean(L, 2));
	return 0;
}

static int drawXbm(lua_State* L) {
	// bmp, xbmMemBuf, x, y, [, xbmx, xbmy, w, h]
	Bitmap* b = bitmap_check(L, 1);
	MemBuf* xbm = mbuf_checkbuf_type(L, 2, NULL);
	int x = luaL_checkint(L, 3);
	int y = luaL_checkint(L, 4);
	lua_getuservalue(L, 2);
	int xbmWidth = 0;
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "xbmwidth");
		xbmWidth = lua_tointeger(L, -1);
	}
	ASSERTL(xbmWidth != 0, "XBM MemBufs must have a uservalue with an xbmwidth member");
	lua_pop(L, 2); // uservalue
	int xbmHeight = xbm->len / ((xbmWidth + 7) >> 3);

	Rect r = rect_make(0, 0, xbmWidth, xbmHeight);
	if (!lua_isnoneornil(L, 5)) {
		r.x = luaL_checkint(L, 5);
		r.y = luaL_checkint(L, 6);
		r.w = luaL_checkint(L, 7);
		r.h = luaL_checkint(L, 8);
	}
	bitmap_drawXbmData(b, x, y, &r, xbm->ptr, xbmWidth);
	return 0;
}

static int setTransform(lua_State* L) {
	Bitmap* bmp = bitmap_check(L, 1);
	if (lua_isnoneornil(L, 2)) {
		bitmap_setTransform(bmp, NULL);
		return 0;
	}
	int a = luaL_checkint(L, 2);
	int b = luaL_checkint(L, 3);
	int c = luaL_checkint(L, 4);
	int d = luaL_checkint(L, 5);
	int tx = luaL_checkint(L, 6);
	int ty = luaL_checkint(L, 7);

	AffineTransform t = { .a = a, .b = b, .c = c, .d = d, .tx = tx, .ty = ty };
	bitmap_setTransform(bmp, &t);
	return 0;
}

static int getTransform(lua_State* L) {
	Bitmap* bmp = bitmap_check(L, 1);
	lua_pushinteger(L, bmp->transform.a);
	lua_pushinteger(L, bmp->transform.b);
	lua_pushinteger(L, bmp->transform.c);
	lua_pushinteger(L, bmp->transform.d);
	lua_pushinteger(L, bmp->transform.tx);
	lua_pushinteger(L, bmp->transform.ty);
	return 6;
}

int init_module_bitmap_bitmap(lua_State* L) {
	luaL_newmetatable(L, BitmapMetatable);
	luaL_Reg fns[] = {
		{ "drawRect", drawRect },
		{ "drawText", drawText },
		{ "getTextSize", getTextSize },
		{ "drawLine", drawLine },
		{ "drawXbm", drawXbm },
		{ "height", getHeight },
		{ "width", getWidth },
		{ "rawHeight", getRawHeight },
		{ "rawWidth", getRawWidth },
		{ "setColour", setColour },
		{ "setBackgroundColour", setBackgroundColour},
		{ "getColour", getColour },
		{ "getBackgroundColour", getBackgroundColour},
		{ "create", create },
		{ "blit", blit },
		{ "setAutoBlit", setAutoBlit },
		{ "setTransform", setTransform },
		{ "getTransform", getTransform },
		{ NULL, NULL },
	};
	luaL_setfuncs(L, fns, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setfield(L, -2, "Bitmap");
	return 0;
}
