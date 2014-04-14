#ifndef LUPI_K_H
#define LUPI_K_H

#include <std.h>

#define LUPI_VERSION_STRING "LuPi 0.15"

typedef unsigned long PhysAddr;

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

#define MAX_PROCESS_NAME 32

// I'll be generous
#define USER_STACK_SIZE (16*1024)

void zeroPage(void* addr);
void zeroPages(void* addr, int num);
void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
void hexdump(const char* addr, int len);
void worddump(const void* addr, int len);
void dumpRegisters(uint32* regs, uint32 pc);
void hang();

#define ASSERT(cond) if (unlikely(!(cond))) { printk("assert %s at line %d\n", #cond, __LINE__); hang(); }

#define IS_POW2(val) ((val & (val-1)) == 0)

typedef struct Process Process;
typedef struct Thread Thread;

typedef struct Thread {
	//Thread* prevSchedulable;
	Thread* nextSchedulable;
	uint8 index;
	uint8 state;
	uint8 pad[6];
	uint32 savedRegisters[16];
	uint32 spsr;
} Thread;

enum ThreadState {
	EReady = 0,
	EBlocked = 1,
	// ???
};


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

typedef struct SuperPage {
	uint32 nextPid;
	Process* currentProcess;
	Thread* currentThread;
	int numValidProcessPages;
	Thread* blockedUartReceiveIrqHandler;
	uint64 uptime; // in ms
	bool marvin;
} SuperPage;

ASSERT_COMPILE(sizeof(SuperPage) <= KPageSize);

#define TheSuperPage ((SuperPage*)KSuperPageAddress)
#define Al ((PageAllocator*)KPageAllocatorAddr)

#define GetProcess(idx) ((Process*)(KProcessesSection + ((idx) << KPageShift)))
#define indexForProcess(p) ((int)((((uintptr)(p)) >> KPageShift) & 0xFF))

static inline Thread* firstThreadForProcess(Process* p) {
	return &p->threads[0];
}

static inline Process* processForThread(Thread* t) {
	// Threads are always within their process page, so simply mask off and cast
	return (Process*)(((uintptr)t) & ~(KPageSize - 1));
	
}

Process* process_new(const char* name);
void process_start(Process* p);
bool process_grow_heap(Process* p, int incr);

NORETURN reschedule(Thread* t);
void saveUserModeRegistersForCurrentThread(void* savedRegisters);

#endif // LUPI_K_H
