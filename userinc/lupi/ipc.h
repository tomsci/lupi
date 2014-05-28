#ifndef IPC_H
#define IPC_H

#define KAsyncFlagPending   1 // Is added to the RunLoop pendingRequests
#define KAsyncFlagAccepted	2 // Has been passed to the kernel
#define KAsyncFlagCompleted 4 // Has been completed by the kernel
#define KAsyncFlagIntResult 8

#endif
