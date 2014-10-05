#ifndef KIPC_H
#define KIPC_H

#ifndef LUPI_NO_IPC

#include <k.h>
#include <mmu.h>

#define MAX_SHARED_PAGES 256

#define indexForUserSharedPage(userAddr) (((userAddr) >> KPageShift) & 0xFF)
#define indexForServer(server) (((uintptr)(server) - (uintptr)&TheSuperPage->servers[0])/sizeof(Server))
#define userAddressForSharedPage(idx) (KSharedPagesBase + (idx << KPageShift))

#ifndef LUPI_SINGLE_PROCESS

#define KSharedPageMappingServerIsSet (0x80)
#define KSharedPageMappingPageOwned (0x40)

/*
Since the TTBCR setup means the bottom of the kernel PDE is never even used,
We have 256 spare uint32s which we will steal for holding the shared pages mappings
*/
inline static uint32* sharedPagePtrForIndex(int idx) {
	return ((uint32*)KKernelPdeBase) + idx;
}

inline static setSharedPageMapping(int idx, Server* server, Process* owner) {
	// Bottom 2 bits must remain zero so as to be a legal no-access PDE
	uint32 mapping = 0;
	if (owner) {
		mapping = (indexForProcess(owner) << 8) | KSharedPageMappingPageOwned;
	}
	if (server) {
		mapping |= KSharedPageMappingServerIsSet | (indexForServer(server) << 16);
	}
	((uint32*)KKernelPdeBase)[idx] = mapping;
}

inline static Server* serverForSharedPage(int idx) {
	uint32 mapping = *sharedPagePtrForIndex(idx);
	if (mapping & KSharedPageMappingServerIsSet) {
		return &TheSuperPage->servers[(mapping & 0xFF0000) >> 16];
	} else {
		return NULL;
	}
}

inline static Process* ownerForSharedPage(int idx) {
	uint32 mapping = *sharedPagePtrForIndex(idx);
	if (mapping & KSharedPageMappingPageOwned) {
		int procIdx = (mapping >> 8) & 0xFF;
		return GetProcess(procIdx);
	} else {
		return NULL;
	}
}

#endif // LUPI_SINGLE_PROCESS

#endif // LUPI_NO_IPC

#endif // KIPC_H
