#include <kipc.h>
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
