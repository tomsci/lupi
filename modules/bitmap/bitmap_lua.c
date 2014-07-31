#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "bitmap.h"

#define BitmapMetatable "LupiBitmapMt"

Bitmap* bitmap_check(lua_State* L, int idx) {
	void* ptr = luaL_checkudata(L, idx, BitmapMetatable);
	return *(Bitmap**)ptr;
}

static int drawRect(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	int x = luaL_checkint(L, 2);
	int y = luaL_checkint(L, 3);
	int w = luaL_checkint(L, 4);
	int h = luaL_checkint(L, 5);
	bitmap_drawRect(b, x, y, w, h);
	return 0;
}

static int drawText(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	int x = luaL_checkint(L, 2);
	int y = luaL_checkint(L, 3);
	const char* text = luaL_checkstring(L, 4);
	bitmap_drawText(b, x, y, text);
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
	lua_pushinteger(L, b->height);
	return 1;
}

static int getWidth(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	lua_pushinteger(L, b->width);
	return 1;
}

static int create(lua_State* L) {
	int w = luaL_checkint(L, 1);
	int h = luaL_checkint(L, 2);
	Bitmap* b = bitmap_create(w, h);
	ASSERTL(b, "Couldn't create bitmap");
	Bitmap** bud = (Bitmap**)lua_newuserdata(L, sizeof(Bitmap*));
	*bud = b;
	luaL_setmetatable(L, BitmapMetatable);
	return 1;
}

static int blit(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	int x = luaL_optint(L, 2, 0);
	int y = luaL_optint(L, 3, 0);
	int w = luaL_optint(L, 4, b->width - x);
	int h = luaL_optint(L, 5, b->height - y);
	bitmap_blitToScreen(b, x, y, w, h);
	return 0;	
}

static int gc(lua_State* L) {
	Bitmap* b = bitmap_check(L, 1);
	bitmap_destroy(b);
	return 0;
}

int init_module_bitmap_init(lua_State* L) {
	luaL_newmetatable(L, BitmapMetatable);
	luaL_Reg fns[] = {
		{ "drawRect", drawRect },
		{ "drawText", drawText },
		{ "getHeight", getHeight },
		{ "getWidth", getWidth },
		{ "setColour", setColour },
		{ "setBackgroundColour", setBackgroundColour},
		{ "getColour", getColour },
		{ "getBackgroundColour", getBackgroundColour},
		{ "create", create },
		{ "blit", blit },
		{ "__gc", gc },
		{ NULL, NULL },
	};
	luaL_setfuncs(L, fns, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setfield(L, -2, "Bitmap");
	return 0;
}
