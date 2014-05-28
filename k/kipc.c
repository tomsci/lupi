#include <kipc.h>
#include <err.h>
#include <pageAllocator.h>

uintptr ipc_mapNewSharedPageInCurrentProcess() {
	// First find an unused page
	int idx = -1;
	uint32* sharedPageMappings = sharedPagePtrForIndex(0);
	for (int i = 0; i < MAX_SHARED_PAGES; i++) {
		if (!sharedPageMappings[i]) {
			idx = i;
			break;
		}
	}
	if (idx == -1) return 0;

	// Now map it. Since all involved processes get the same address for a given shared
	// page, we don't need any additional means of referencing the page, and we don't
	// even need to track its physical address.
	uint32 userPtr = userAddressForSharedPage(idx);
	Process* owner = TheSuperPage->currentProcess;
	bool ok = mmu_newSharedPage(Al, owner, userPtr);
	if (!ok) {
		return 0;
	}
	setSharedPageMapping(idx, NULL, owner);
	mmu_finishedUpdatingPageTables();
	zeroPage((void*)userPtr);
	return userPtr;
}

// returns server idx or err
int ipc_createServer(uint32 id, Thread* thread) {
	// Find a free Server slot
	int idx = -1;
	Server* ss = TheSuperPage->servers;
	for (int i = 0; i < MAX_SERVERS; i++) {
		if (ss[i].id == id) {
			return KErrAlreadyExists;
		} else if (ss[i].serverRequest.thread == NULL) {
			idx = i;
		}
	}
	if (idx == -1) return KErrNotFound;
	Server* s = &ss[idx];
	s->id = id;
	s->serverRequest.thread = thread;
	s->serverRequest.userPtr = 0;
	s->blockedClientList = NULL;

	// Done.
	return 0;
}

static int sharedPageIsValid(uintptr sharedPage) {
	// Check sharedPage does in fact belong to the current process
	int sharedPageIdx = indexForUserSharedPage(sharedPage);
	if (ownerForSharedPage(sharedPageIdx) != TheSuperPage->currentProcess) {
		return KErrBadHandle;
	}
	return sharedPageIdx;
}

void ipc_requestServerMsg(Thread* serverThread, uintptr serverRequest) {
	// Find the server
	Server* s = NULL;
	Server* ss = TheSuperPage->servers;
	for (int i = 0; i < MAX_SERVERS; i++) {
		if (ss[i].serverRequest.thread == serverThread) {
			s = &ss[i];
			break;
		}
	}
	ASSERT(s, (uintptr)serverThread);
	ASSERT(s->serverRequest.userPtr == 0, (uintptr)s);
	s->serverRequest.userPtr = serverRequest;

	if (s->blockedClientList) {
		Thread* t = s->blockedClientList->prev; // The oldest blocked thread
		if (t->exitReason == EBlockedInServerConnect) {
			// The fact that the server's requesting another message means it's
			// done with this one. Bit odd mechanism this, maybe we should make
			// it explicit. For a start that would allow us to actually return
			// a result from the connect
			thread_dequeue(t, &s->blockedClientList);
			uintptr sharedPage = t->savedRegisters[2];
			t->savedRegisters[1] = 0;
			t->savedRegisters[0] = sharedPage;
			thread_setState(t, EReady);
		} else {
			// The server can now handle this connection
			t->exitReason = EBlockedInServerConnect;
			uintptr sharedPage = t->savedRegisters[2];
			thread_requestComplete(&s->serverRequest, sharedPage);
		}
	}
}

int ipc_connectToServer(uint32 id, uintptr sharedPage) {
	//printk("ipc_connectToServer %p\n", (void*)sharedPage);
	Process* src = TheSuperPage->currentProcess;
	int sharedPageIdx = sharedPageIsValid(sharedPage);
	if (sharedPage & 0xFFF) return KErrBadHandle;
	if (sharedPageIdx < 0) {
		return sharedPageIdx;
	}

	// Now find the server
	Server* ss = TheSuperPage->servers;
	Server* s = NULL;
	for (int i = 0; i < MAX_SERVERS; i++) {
		if (ss[i].id == id) {
			s = &ss[i];
			break;
		}
	}
	if (!s) return KErrNotFound;
	// Update mapping for the sharedPage (set the server ptr)
	setSharedPageMapping(sharedPageIdx, s, src);

	// map this page into the server too
	bool ok = mmu_sharePage(Al, src, processForServer(s), sharedPage);
	if (!ok) return KErrNoMemory;

	// And now, we tell the server, blocking the client until the server acknowledges
	// (handleSvc will already have saved the client's user state)
	Thread* client = TheSuperPage->currentThread;
	thread_setState(client, EBlockedFromSvc);
	thread_enqueueBefore(client, s->blockedClientList);
	if (!s->blockedClientList) s->blockedClientList = client;
	client->savedRegisters[2] = sharedPage; // Stash this somewhere we're not using right now

	if (s->serverRequest.userPtr) {
		thread_setBlockedReason(client, EBlockedInServerConnect);
		thread_requestComplete(&s->serverRequest, sharedPage);
	} else {
		thread_setBlockedReason(client, EBlockedWaitingForServerConnect);
	}
	// We'll get serviced (eventually) from ipc_requestServerMsg
	reschedule();
	//return 0;
}

void ipc_processExited(PageAllocator* pa, Process* p) {
	// Check for shared pages
	for (int i = 0; i < MAX_SHARED_PAGES; i++) {
		Server* server = serverForSharedPage(i);
		Process* owner = ownerForSharedPage(i);
		bool changed = false;
		if (owner == p) {
			owner = NULL;
			changed = true;
		} else if (server && processForServer(server) == p) {
			server = NULL;
			changed = true;
		}
		if (changed) {
			if (!server && !owner) {
				// Nobody is using the page any more, free it
				mmu_unmapPagesInProcess(pa, p, userAddressForSharedPage(i), 1);
			}
			setSharedPageMapping(i, server, owner);
		}
	}
}

int ipc_completeRequest(uintptr request, bool toServer) {
	int sharedPageIdx = sharedPageIsValid(request);
	if (sharedPageIdx < 0) return sharedPageIdx;
	Server* s = serverForSharedPage(sharedPageIdx);
	// TODO validate that request is in fact an IpcMessage.request?
	Thread* recipient;
	if (toServer) recipient = s->serverRequest.thread;
	else recipient = &ownerForSharedPage(sharedPageIdx)->threads[0]; // TODO support non-main threads
	ASSERT(recipient, request, (uintptr)s);
	KAsyncRequest req = { .thread = recipient, .userPtr = request };
	// User-side handles writing the result, we jsut have to signal
	thread_requestSignal(&req);
	return 0;
}
