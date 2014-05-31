#ifndef LUPI_INT64_H
#define LUPI_INT64_H

#include <stddef.h>

typedef struct lua_State lua_State;

void initInt64Module(lua_State* L);

void int64_new(lua_State* L, int64 n);
int64 int64_check(lua_State* L, int idx);

#endif
