#ifndef LUPI_K_H
#define LUPI_K_H

#ifndef LUPI_STD_H
#include <std.h>
#endif

#define LUPI_VERSION_STRING "LuPi 0.19"

/*
Limiting to 256 running processes makes the maths quite nice - the ProcessList fits into a page,
and Process overhead is a maximum 1MB (sounds big but it's fixed). There's maybe also some
cacheing tweaks we can do a la Multiple Memory Model on ARM11.
*/
#define MAX_PROCESSES 256

/*
Max threads per process
Hmm, the "one page per process" limit turns out to be somewhat limiting...
*/
#define MAX_THREADS 48

#define MAX_SERVERS 32

#define MAX_PROCESS_NAME 32

#define THREAD_TIMESLICE 25 // milliseconds

void zeroPage(void* addr);
void zeroPages(void* addr, int num);
void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
void hexdump(const char* addr, int len);
void worddump(const void* addr, int len);
void dumpRegisters(uint32* regs, uint32 pc, uint32 dataAbortFar);
NORETURN kabort4(uint32 r0, uint32 r1, uint32 r2, uint32 r3);
NORETURN hang();
NORETURN reboot();

#define KRegisterNotSaved 0xA11FADE5
#define kabort() kabort4(KRegisterNotSaved, KRegisterNotSaved, KRegisterNotSaved, KRegisterNotSaved)
#define argn(i, nargs, args) (i < nargs ? args[i] : KRegisterNotSaved)
#define kabort1(arg) kabort4(arg, KRegisterNotSaved, KRegisterNotSaved, KRegisterNotSaved)
#define kabortn(n, args) kabort4(argn(0, n, args), argn(1, n, args), argn(2, n, args), argn(3, n, args))

#define ASSERT(cond, args...) \
	if (unlikely(!(cond))) { \
		printk("ASSERTION FAILURE %s at %s:%d\n", #cond, __FILE__, __LINE__); \
		uint32 argsArray[] = {args}; \
		kabortn(sizeof(argsArray)/sizeof(uint32), argsArray); \
	}

#define NUMVARARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))
#define FOURCC(str) ((str[0]<<24)|(str[1]<<16)|(str[2]<<8)|(str[3]))
#define IS_POW2(val) ((val & (val-1)) == 0)

typedef struct Process Process;
typedef struct Thread Thread;
typedef struct PageAllocator PageAllocator;

typedef struct Thread {
	Thread* prev;
	Thread* next;
	uint8 index;
	uint8 state;
	uint8 timeslice;
	uint8 completedRequests;
	int exitReason; // Also holds blockedReason if state is EBlockedFromSvc
	uint32 savedRegisters[17];
} Thread;

typedef enum ThreadState {
	EReady = 0,
	EBlockedFromSvc = 1, // Reason is exitReason
	EDying = 2, // Thread has executed its last but hasn't yet been cleaned up by dfc_threadExit
	EDead = 3, // Stacks have been freed
	EWaitForRequest = 4,
} ThreadState;

typedef enum ThreadBlockedReason {
	EBlockedOnGetch = 1,
	EBlockedWaitingForServerConnect = 2,
	EBlockedInServerConnect = 3,
	EBlockedWaitingForDfcs = 4, // Special reason only used by the DFC thread
} ThreadBlockedReason;

uint32 atomic_inc(uint32* ptr);
uint32 atomic_set(uint32* ptr, uint32 val);
bool atomic_cas(uint32* ptr, uint32 expectedVal, uint32 newVal);
#ifndef LP64
static inline Thread* atomic_set_thread(Thread** ptr, Thread* val) {
	return (Thread*)atomic_set((uint32*)ptr, (uint32)val);
}
static inline uintptr atomic_set_uptr(uintptr* ptr, uintptr val) {
	return (uintptr)atomic_set((uint32*)ptr, (uint32)val);
}

#endif

uint8 atomic_inc8(uint8* ptr);
uint8 atomic_set8(uint8* ptr, uint8 val);
bool atomic_cas8(uint8* ptr, uint8 expectedVal, uint8 newVal);
static inline bool atomic_setbool(bool* ptr, bool val) {
	return (bool)atomic_set8((uint8*)ptr, val);
}

/*
This structure is one page in size (maximum), and is always page-aligned.
*/
typedef struct Process {
	uint32 pid;
	uintptr pdePhysicalAddress;
	uintptr heapLimit;
	char name[MAX_PROCESS_NAME];

	uint8 numThreads;
	Thread threads[MAX_THREADS];
} Process;

/*
Each process gets one page of kernel memory for its Process, and nothing else, so everything the
process does kernel side has to be contained within that.
...
Except that page tables take a minimum of one page per 1MB of virtual address space 
(1 page = 256 words each describing a page, 256*4KB = 1MB) plus first-level PTE, so there's no
way we can fit it all in one page.
*/
ASSERT_COMPILE(sizeof(Process) <= KPageSize);

typedef struct KAsyncRequest {
	Thread* thread;
	uintptr userPtr;
} KAsyncRequest;

typedef struct Server {
	uint32 id; // a fourcc
	KAsyncRequest serverRequest;
	Thread* blockedClientList;
} Server;

typedef void (*DfcFn)(uintptr arg1, uintptr arg2, uintptr arg3);

typedef struct Dfc {
	DfcFn fn;
	uintptr args[3];
} Dfc;

#define MAX_DFCS 4

typedef struct Driver Driver;
// Let's see if CaseyM's favourite syntax makes me retch
#define DRIVER_FN(fn) int fn(Driver* driver, uintptr arg1, uintptr arg2)
typedef DRIVER_FN((*DriverExecFn));
#define MAX_DRIVERS 4
void kern_registerDriver(uint32 id, DriverExecFn fn);

struct Driver {
	uint32 id; // a fourcc
	DriverExecFn execFn;
};

typedef struct SuperPage {
	uint32 totalRam;
	uint32 boardRev;
	int bootMode;
	uint32 nextPid;
	Process* currentProcess;
	Thread* currentThread;
	int numValidProcessPages;
	Thread* blockedUartReceiveIrqHandler;
	Thread* readyList;
	uint64 uptime; // in ms
	bool marvin;
	bool trapAbort;
	bool exception; // only used in kdebugger mode
	uint8 uartDroppedChars; // Access only with atomic_*
	KAsyncRequest uartRequest;
	KAsyncRequest timerRequest;
	uint64 timerCompletionTime;
	uint32 crashRegisters[17];
	uint32 crashFar;
	byte uartBuf[66];
	Server servers[MAX_SERVERS];
	bool rescheduleNeededOnSvcExit;
	byte svcPsrMode; // settable so we don't accidentally enable interrupts when crashed
	uint32 numDfcsPending;
	Thread dfcThread;
	Dfc dfcs[MAX_DFCS];
	Driver drivers[MAX_DRIVERS];
} SuperPage;

ASSERT_COMPILE(sizeof(SuperPage) <= KPageSize);

#define TheSuperPage ((SuperPage*)KSuperPageAddress)
#define Al ((PageAllocator*)KPageAllocatorAddr)

#define GetProcess(idx) ((Process*)(KProcessesSection + ((idx) << KPageShift)))
#define indexForProcess(p) ((int)((((uintptr)(p)) >> KPageShift) & 0xFF))

// NOTE: don't change these without also updating the asm in svc()
#define svcStackOffset(threadIdx) (threadIdx << USER_STACK_AREA_SHIFT)
#define svcStackBase(threadIdx) (KUserStacksBase + svcStackOffset(threadIdx))
#define userStackBase(threadIdx) (svcStackBase(threadIdx) + 2*KPageSize)
#define userStackForThread(t) userStackBase(t->index)

static inline Thread* firstThreadForProcess(Process* p) {
	return &p->threads[0];
}

static inline Process* processForThread(Thread* t) {
	if ((uintptr)t < KProcessesSection) {
		// Special case. The DFC thread doesn't have an associated process
		return NULL;
	}
	// Otherwise, threads are within their process page, so simply mask off and cast
	return (Process*)(((uintptr)t) & ~(KPageSize - 1));
}

static inline Process* processForServer(Server* s) {
	return processForThread(s->serverRequest.thread);
}

int process_new(const char* name, Process** resultProcess);
NORETURN process_start(Process* p);
bool process_grow_heap(Process* p, int incr);
void thread_setState(Thread* t, enum ThreadState s);
NORETURN thread_exit(Thread* t, int reason);
void thread_requestSignal(KAsyncRequest* request);
void thread_requestComplete(KAsyncRequest* request, int result);
void thread_setBlockedReason(Thread* t, ThreadBlockedReason reason);
void thread_enqueueBefore(Thread* t, Thread* before);
void thread_dequeue(Thread* t, Thread** head);
void thread_yield(Thread* t);

int kern_disableInterrupts();
void kern_enableInterrupts();
void kern_restoreInterrupts(int mask);
void kern_sleep(int ms);
NORETURN reschedule();
void saveCurrentRegistersForThread(void* savedRegisters);
void dfc_queue(DfcFn fn, uintptr arg1, uintptr arg2, uintptr arg3);
void dfc_requestComplete(KAsyncRequest* request, int result);
void irq_checkDfcs(void* savedRegisters);

uintptr ipc_mapNewSharedPageInCurrentProcess();
int ipc_connectToServer(uint32 id, uintptr sharedPage);
int ipc_createServer(uint32 id, Thread* thread);
void ipc_processExited(PageAllocator* pa, Process* p);
void ipc_requestServerMsg(Thread* serverThread, uintptr serverRequest);
int ipc_completeRequest(uintptr request, bool toServer);

#endif // LUPI_K_H
