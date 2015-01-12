#ifndef LUPI_K_H
#define LUPI_K_H

#ifndef LUPI_STD_H
#include <std.h>
#endif

#define KPageSize 4096
#define KPageShift 12

#define LUPI_VERSION_STRING "Geoff 0.24"

#ifdef HAVE_MMU
#define MAX_PROCESSES 256
#else
#define MAX_PROCESSES 1
#endif

/*
Max threads per process
Hmm, the "one page per process" limit turns out to be somewhat limiting...
*/
#define MAX_THREADS 48

#define MAX_SERVERS 32

#define MAX_PROCESS_NAME 31

#define THREAD_TIMESLICE 25 // milliseconds

void zeroPage(void* addr);
void zeroPages(void* addr, int num);
void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
void hexdump(const char* addr, int len);
void worddump(const void* addr, int len);
NORETURN NAKED assertionFail(int nextras, const char* file, int line, const char* condition, ...);
NORETURN hang();
NORETURN reboot();
uint32 GET32(uint32 addr);
void PUT32(uint32 addr, uint32 val);
void PUT8(uint32 addr, byte val);

#define NUMVARARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))
#define KRegisterNotSaved 0xA11FADE5

#define ASSERT(cond, args...) \
	if (unlikely(!(cond))) { \
		assertionFail(NUMVARARGS(args), __FILE__, __LINE__, #cond, ## args); \
	}
#define kabort() assertionFail(0, __FILE__, __LINE__, "(kabort)")
#define kabort1(arg) assertionFail(1, __FILE__, __LINE__, "(kabort)", arg)

#define ASSERT_USER(x, writeable, align) \
	ASSERT(!(x & align) && \
		((!writeable && x >= KKernelCodeBase && x < KKernelCodeBase + KKernelCodesize) \
		|| (x >= KUserHeapBase && x < KUserMemLimit)), x)

// Must be readable user memory (ie user heap or const data)
#define ASSERT_USER_PTR8(x) ASSERT_USER((uintptr)(x), false, 0)
#define ASSERT_USER_PTR16(x) ASSERT_USER((uintptr)(x), false, 1)
#define ASSERT_USER_PTR32(x) ASSERT_USER((uintptr)(x), false, 3)

// Must be writeable user memory
#define ASSERT_USER_WPTR8(x) ASSERT_USER((uintptr)(x), true, 0)
#define ASSERT_USER_WPTR16(x) ASSERT_USER((uintptr)(x), true, 1)
#define ASSERT_USER_WPTR32(x) ASSERT_USER((uintptr)(x), true, 3)

#define FOURCC(str) ((str[0]<<24)|(str[1]<<16)|(str[2]<<8)|(str[3]))
#define IS_POW2(val) ((val & (val-1)) == 0)

typedef struct Process Process;
typedef struct Thread Thread;
typedef struct PageAllocator PageAllocator;

#if defined(ARMV7_M)
// 0-7 are r4-r11, 8 is r13
#define KSavedR13 8
#define NUM_SAVED_REGS 9
#elif defined(ARM)
// 0-15 are r0-r15, 16 is PSR
#define KSavedR13 13
#define NUM_SAVED_REGS 17
#endif

typedef struct Thread {
	Thread* prev;
	Thread* next;
	uint8 index;
	uint8 state;
	uint8 timeslice;
	uint8 completedRequests;
	int exitReason; // Also holds blockedReason if state is EBlockedFromSvc
	uint32 savedRegisters[NUM_SAVED_REGS];
} Thread;

typedef enum ThreadState {
	EReady = 0,
	EBlockedFromSvc = 1, // Reason is exitReason
	EDying = 2, // Thread has executed its last but hasn't yet been cleaned up by threadExit_dfc
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
NOIGNORE uint32 atomic_set(uint32* ptr, uint32 val);
NOIGNORE bool atomic_cas(uint32* ptr, uint32 expectedVal, uint32 newVal);
#ifndef LP64
NOIGNORE static inline Thread* atomic_set_thread(Thread** ptr, Thread* val);
static inline Thread* atomic_set_thread(Thread** ptr, Thread* val) {
	return (Thread*)atomic_set((uint32*)ptr, (uint32)val);
}
NOIGNORE static inline uintptr atomic_set_uptr(uintptr* ptr, uintptr val);
static inline uintptr atomic_set_uptr(uintptr* ptr, uintptr val) {
	return (uintptr)atomic_set((uint32*)ptr, (uint32)val);
}

#endif

uint8 atomic_inc8(uint8* ptr);
NOIGNORE uint8 atomic_set8(uint8* ptr, uint8 val);
NOIGNORE bool atomic_cas8(uint8* ptr, uint8 expectedVal, uint8 newVal);
static inline bool atomic_setbool(bool* ptr, bool val) {
	return (bool)atomic_set8((uint8*)ptr, val);
}

/*
This structure is one page in size (maximum), and is always page-aligned.
Unless LUPI_NO_SECTION0 is defined, in which case there's only one and it's
packed into the SuperPage.
*/
typedef struct Process {
	uint32 pid;
#ifdef HAVE_MMU
	uintptr pdePhysicalAddress;
#endif
	uint8 numThreads;
	char name[MAX_PROCESS_NAME];
	// The LSB of heapLimit will always be zero, meaning heapLimit can also act
	// as the null-terminator for name.
	uintptr heapLimit;

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

typedef enum Flag {
	NeedToSendTouchUp = 0,
	TrapAbort = 1,
	ExceptionOccurred = 2, // only used in kluadebugger
} Flag;

typedef struct SuperPage {
	uint32 totalRam;
	uint32 boardRev;
	int screenWidth;
	int screenHeight;
	int bootMode;
	uint32 nextPid;
	uint64 uptime; // in ms
	Process* currentProcess;
	Thread* currentThread;
	int numValidProcessPages;
	Thread* blockedUartReceiveIrqHandler;
	Thread* readyList;
	uint32 flags;
	uint8 screenFormat;
	uint8 numDfcsPending;
	bool marvin;
	uint8 uartDroppedChars; // Access only with atomic_*
	KAsyncRequest uartRequest;
	KAsyncRequest timerRequest;
	uint64 timerCompletionTime;
	uint32 crashRegisters[17];
	uint32 crashFar;
	byte uartBuf[68];
	Server servers[MAX_SERVERS];
	bool quiet; // Suppress printks
	byte spare1;
#ifdef ARM
	bool rescheduleNeededOnSvcExit;
	byte svcPsrMode; // settable so we don't accidentally enable interrupts when crashed
#endif
#ifdef ARMV7_M
	bool rescheduleNeededOnPendSvExit;
	byte spare2;
#endif
	Dfc dfcs[MAX_DFCS];
	Driver drivers[MAX_DRIVERS];
	KAsyncRequest inputRequest;
	uintptr inputRequestBuffer;
	int inputRequestBufferSize; // in samples
	uint32 inputRequestBufferPos; // in samples
	uint32 buttonStates;

#ifndef HAVE_MMU
	uintptr crashedHeapLimit;
#endif
#ifdef TIMER_DEBUG
	uint64 lastRescheduleTime;
	uint64 lastPendSvTime;
	uint64 lastSvcTime;
	uint32 lastSvc;
#endif

	uint32 audioAddr;
	uint32 audioEnd;
	uint32 audioLoopLen;
	uint8* audioBufPtr;
	uint8* audioBufNewestDataEnd;

#ifdef ARM
	// DFCs implemented using PendSV rather than a Thread in ARMv7-M
	Thread dfcThread;
#endif
#ifdef LUPI_NO_SECTION0
	// We compact some other data structures into the superpage when we're
	// on a mem-constrained platform
	// uint8 pageAllocatorMem[pageAllocator_size(KRamSize >> KPageShift)];
	Process mainProcess;
	// User BSS follows (but is not explicitly included in the struct definition)
#endif
} SuperPage;

#ifdef LUPI_NO_SECTION0
// Must be enough space to jam the BSS data in
ASSERT_COMPILE(sizeof(SuperPage) <= KPageSize - KSuperPageUsrRegionSize);
#else
ASSERT_COMPILE(sizeof(SuperPage) <= KPageSize);
#endif

#define TheSuperPage ((SuperPage*)KSuperPageAddress)

#ifdef LUPI_NO_SECTION0
//#define Al ((PageAllocator*)TheSuperPage->pageAllocatorMem)
#define GetProcess(idx) (&TheSuperPage->mainProcess)
#define indexForProcess(p) (0)
#else
#define Al ((PageAllocator*)KPageAllocatorAddr)
#define GetProcess(idx) ((Process*)(KProcessesSection + ((idx) << KPageShift)))
#define indexForProcess(p) ((int)((((uintptr)(p)) >> KPageShift) & 0xFF))
#endif


#ifdef ARM

// NOTE: don't change these without also updating the asm in svc()
#define svcStackOffset(threadIdx) (threadIdx << USER_STACK_AREA_SHIFT)
#define svcStackBase(threadIdx) (KUserStacksBase + svcStackOffset(threadIdx))
#define userStackBase(threadIdx) (svcStackBase(threadIdx) + 2*KPageSize)
#define userStackForThread(t) userStackBase(t->index)

#elif defined(ARMV7_M)

#define userStackForThread(t) (KUserMemLimit - (((t)->index + 1) << USER_STACK_AREA_SHIFT))

#endif


static inline Thread* firstThreadForProcess(Process* p) {
	return &p->threads[0];
}

static inline Process* processForThread(Thread* t) {
#ifdef LUPI_NO_SECTION0
	return GetProcess(0);
#else
	if ((uintptr)t < KProcessesSection) {
		// Special case. The DFC thread doesn't have an associated process
		return NULL;
	}
	// Otherwise, threads are within their process page, so simply mask off and cast
	return (Process*)(((uintptr)t) & ~(KPageSize - 1));
#endif
}

static inline Process* processForServer(Server* s) {
	return processForThread(s->serverRequest.thread);
}

NOIGNORE int process_new(const char* name, Process** resultProcess);
NORETURN process_start(Process* p);
NOIGNORE bool process_grow_heap(Process* p, int incr);
int process_reset(Thread* t, const char* name);
NOIGNORE int thread_new(Process* p, uintptr context, Thread** resultThread);
void thread_setState(Thread* t, enum ThreadState s);
NORETURN thread_exit(Thread* t, int reason);
void thread_requestSignal(KAsyncRequest* request);
void thread_requestComplete(KAsyncRequest* request, int result);
void thread_setBlockedReason(Thread* t, ThreadBlockedReason reason);
void thread_enqueueBefore(Thread* t, Thread* before);
void thread_dequeue(Thread* t, Thread** head);
void thread_yield(Thread* t);
void thread_writeSvcResult(Thread* t, uintptr result);

int kern_disableInterrupts();
void kern_enableInterrupts();
void kern_restoreInterrupts(int mask);
void kern_sleep(int ms);
NORETURN reschedule();
void saveCurrentRegistersForThread(void* savedRegisters);
void dfc_queue(DfcFn fn, uintptr arg1, uintptr arg2, uintptr arg3);
void dfc_requestComplete(KAsyncRequest* request, int result);
bool irq_checkDfcs();
int kern_setInputRequest(uintptr userInputRequestPtr);

NOIGNORE uintptr ipc_mapNewSharedPageInCurrentProcess();
NOIGNORE int ipc_connectToServer(uint32 id, uintptr sharedPage);
NOIGNORE int ipc_createServer(uint32 id, Thread* thread);
NOIGNORE void ipc_processExited(PageAllocator* pa, Process* p);
void ipc_requestServerMsg(Thread* serverThread, uintptr serverRequest);
NOIGNORE int ipc_completeRequest(uintptr request, bool toServer);

void ring_push(byte* ring, int size, byte b);
byte ring_pop(byte* ring, int size);
bool ring_empty(byte* ring, int size);
bool ring_full(byte* ring, int size);
int ring_free(byte* ring, int size);

static inline void kern_setFlag(Flag f, bool val) {
	if (val) TheSuperPage->flags |= 1 << f;
	else TheSuperPage->flags &= ~(1 << f);
}

static inline bool kern_getFlag(Flag f) {
	return TheSuperPage->flags & (1 << f);
}

#endif // LUPI_K_H
