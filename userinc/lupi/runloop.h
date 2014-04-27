#ifndef LUPI_RUNLOOP_H
#define LUPI_RUNLOOP_H

#include <stddef.h>

typedef struct lua_State lua_State;

typedef struct AsyncRequest {
	uintptr result;
	uint32 flags;
} AsyncRequest;

void initRunloopModule(lua_State* L);

AsyncRequest* checkRequestPending(lua_State* L, int idx);

#endif
