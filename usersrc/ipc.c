#include <stddef.h>
#include <string.h>
#include <lupi/membuf.h>
#include <lupi/runloop.h>
#include <lupi/ipc.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef union {
	char text[4];
	uint32 code;
} FourCC;

ASSERT_COMPILE(sizeof(FourCC) == 4);

typedef struct IpcMessage {
	int length;
	AsyncRequest request; // in server's RunLoop
	AsyncRequest response; // in client's RunLoop
	uint32 data; // Offset from start of page
} IpcMessage;

typedef struct IpcPage {
	int numMessages;
	IpcMessage msgs[1];
	// Array of IpcMessages follows...
} IpcPage;

#define MAX_MESSAGES 64

#define KSharedPagesBase		0x0F000000u
#define KSharedPagesSize		0x00100000u

uintptr exec_newSharedPage();
int exec_connectToServer(uint32 server, void* ipcPage);
int exec_completeIpcRequest(AsyncRequest* ipcRequest, bool toServer);
void exec_requestServerMessage(AsyncRequest* serverRequest);
int exec_createServer(uint32 serverId);

static int getSharedPage(lua_State* L) {
	uintptr ptr = luaL_checkinteger(L, 1);
	if (ptr < KSharedPagesBase || ptr >= KSharedPagesBase + KSharedPagesSize || (ptr & 0xFFF)) {
		return luaL_error(L, "%p is not a shared page address", ptr);
	}
	/*MemBuf* buf =*/ mbuf_new(L, (void*)ptr, KPageSize, NULL);
	return 1;
}

static int newSharedPage(lua_State* L) {
	uintptr page = exec_newSharedPage();
	if (!page) return luaL_error(L, "Couldn't get new shared page");

	/*MemBuf* buf =*/ mbuf_new(L, (void*)page, KPageSize, NULL);
	return 1;
}

static IpcPage* checkPage(lua_State* L, int idx) {
	uintptr page = (uintptr)mbuf_checkbuf(L, idx)->ptr;
	if (page >= KSharedPagesBase && page < KSharedPagesBase + KSharedPagesSize - 1 && (page & 0xFFF) == 0) {
		return (IpcPage*)page;
	} else {
		luaL_error(L, "Bad page %p", (void*)page);
		return NULL;
	}
}

// Do some checks on the consistancy of an IpcMessage
static bool dataValid(IpcMessage* m) {
	if (m->length < 0 || m->length > KPageSize) return false;
	// Message len vaguely sane
	IpcPage* page = (IpcPage*)(((uintptr)m) & (~0xFFF));
	if (page->numMessages < 1 || page->numMessages > MAX_MESSAGES) return false;
	// numMessages sane
	uintptr data = (uintptr)m->data;
	if (data == 0 && m->length == 0) return true; // No payload
	if (data < offsetof(IpcPage, msgs[page->numMessages])) return false;
	// data ptr follows messages
	if (data > KPageSize - m->length) return false;
	// data fits entirely inside page
	return true; // Probably
}

static IpcMessage* checkMessage(lua_State* L, int idx, bool isRequest) {
	uintptr req = (uintptr)runloop_checkRequest(L, idx);
	// The argument is an AsyncRequest which should be the 'request' member of an
	// IpcMessage within a valid IpcPage. Check all of these things.
	if (req >= KSharedPagesBase && req < KSharedPagesBase + KSharedPagesSize) {
		uintptr pageAddr = req & ~0xFFF;
		IpcPage* page = (IpcPage*)pageAddr;
		uintptr messagePtr;
		if (isRequest) messagePtr = req - offsetof(IpcMessage, request);
		else messagePtr = req - offsetof(IpcMessage, response);
		uintptr msgOffset = messagePtr - pageAddr;
		int msgIdx = (msgOffset - offsetof(IpcPage, msgs)) / sizeof(IpcMessage);
		//PRINTL("page=%p numMessages=%d msgIdx=%d", page, page->numMessages, msgIdx);
		uintptr expectedAddr;
		if (isRequest) expectedAddr = (uintptr)&page->msgs[msgIdx].request;
		else expectedAddr = (uintptr)&page->msgs[msgIdx].response;
		if (msgIdx >= 0 && msgIdx < page->numMessages && req == expectedAddr) {
			return (IpcMessage*)messagePtr;
		}
	}
	luaL_error(L, "Bad request %p", req);
	return NULL;
}

static IpcMessage* checkRequestMessage(lua_State* L, int idx) {
	return checkMessage(L, idx, true);
}

static IpcMessage* checkResponseMessage(lua_State* L, int idx) {
	return checkMessage(L, idx, false);
}

//// Server functions ////

static int requestServerMessage(lua_State* L) {
	AsyncRequest* req = runloop_checkRequestPending(L, 1);
	req->flags |= KAsyncFlagAccepted;
	exec_requestServerMessage(req);
	return 0;
}

static int doCreateServer(lua_State* L) {
	const char* serverName = lua_tostring(L, 1);
	FourCC fcc;
	memcpy(fcc.text, serverName, 4);
	int res = exec_createServer(fcc.code);
	if (res < 0) {
		return luaL_error(L, "Error %d creating server", res);
	}
	lua_pushinteger(L, res);
	return 1;
}

static int setupMsgsForClient(lua_State* L) {
	IpcPage* p = checkPage(L, 1);
	const int n = p->numMessages;
	lua_createtable(L, n, 0);
	for (int i = 0; i < n; i++) {
		ASSERTL(p->msgs[i].request.flags == 0, ". Oh no flags are %d", (int)p->msgs[i].request.flags);
		runloop_newIndirectAsyncRequest(L, &p->msgs[i].request);
		lua_rawseti(L, -2, i+1);
	}
	return 1;
}

static int getMsgData(lua_State* L) {
	IpcPage* p = checkPage(L, 1);
	int idx = luaL_checkint(L, 2);
	if (idx >= p->numMessages) {
		return luaL_error(L, "Message index %d out of range", idx);
	}
	IpcMessage* msg = &p->msgs[idx];
	if (!dataValid(msg)) {
		return luaL_error(L, "Bad message %p", msg);
	}
	lua_pushinteger(L, (uintptr)msg->data);
	lua_pushinteger(L, msg->length);
	return 2;
}

static int complete(lua_State* L) {
	IpcMessage* msg = checkRequestMessage(L, 1);
	int result = lua_tointeger(L, 2);
	msg->response.result = result;
	msg->response.flags |= KAsyncFlagCompleted;
	exec_completeIpcRequest(&msg->response, false);
	return 0;
}

//// Client functions ////

static int connectToServer(lua_State* L) {
	const char* serverName = lua_tostring(L, 1);
	FourCC fcc;
	memcpy(fcc.text, serverName, 4);
	IpcPage* page = checkPage(L, 2);
	// Create lua objects for the AsyncRequests in the IpcPage. Since the Lua runtime
	// doesn't own the AsyncRequest objects, you can't directly make a full user data
	// for them, and you can't specify a custom metatable for a light userdata, so instead
	// we create full userdatas containing only a pointer to the real AsyncRequest, and
	// add code to runloop_checkRequest to handle this format.
	lua_createtable(L, page->numMessages, 0);
	for (int i = 0; i < page->numMessages; i++) {
		runloop_newIndirectAsyncRequest(L, &page->msgs[i].response);
		lua_rawseti(L, -2, i+1);
	}
	int serverId = exec_connectToServer(fcc.code, page);
	lua_pushinteger(L, serverId);
	lua_insert(L, -2); // move serverId behind msgs
	return 2;
}

static int doSendMsg(lua_State* L) {
	// args: (msg, cmd, startOfData, len)
	// The lua code passes us in the response message not the request like you might
	// expect, because the response is the only one it knows about. But it doesn't
	// really matter because we deal with it at the level of the IpcMessage which
	// encapsulates both of them.
	IpcMessage* msg = checkResponseMessage(L, 1);
	int cmd = lua_tointeger(L, 2);
	int startOfData = lua_tointeger(L, 3);
	int len = lua_tointeger(L, 4);
	msg->length = len;
	msg->data = startOfData;
	msg->response.flags |= KAsyncFlagAccepted;
	// Normally you'd only set Accepted when completing a request, however IPC is
	// optimised in the kernel because there's no need to context switch, so we might
	// as well set everything up here.
	msg->request.result = cmd;
	msg->request.flags |= KAsyncFlagAccepted | KAsyncFlagCompleted | KAsyncFlagIntResult;
	int err = exec_completeIpcRequest(&msg->request, true);
	lua_pushinteger(L, err);
	return 1;
}

int init_module_ipc(lua_State* L) {
	luaL_Reg modFns[] = {
		// Client functions
		{ "newSharedPage", newSharedPage },
		{ "connectToServer", connectToServer },
		{ "doSendMsg", doSendMsg },

		// Server functions
		{ "doCreateServer", doCreateServer },
		{ "requestServerMessage", requestServerMessage },
		{ "getSharedPage", getSharedPage },
		{ "setupMsgsForClient", setupMsgsForClient },
		{ "getMsgData", getMsgData },
		{ "complete", complete },
		{ NULL, NULL }
	};
	luaL_setfuncs(L, modFns, 0);

	return 0;
}
