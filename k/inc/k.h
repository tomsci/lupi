#ifndef LUPI_K_H
#define LUPI_K_H

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

// I'll be generous
#define USER_STACK_SIZE (16*1024)

void zeroPage(void* addr);
void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
void hexdump(const char* addr, int len);
void worddump(const char* addr, int len);
void hang();

#define ASSERT(cond) if (unlikely(!(cond))) { printk("assert %s at line %d\n", #cond, __LINE__); hang(); }

#define IS_POW2(val) ((val & (val-1)) == 0)

typedef struct Process Process;
typedef struct Thread Thread;

typedef struct Thread {
	Thread* prevSchedulable;
	Thread* nextSchedulable;
	uint8 index;
	uint8 state;
	uint8 pad[6];
	uint32 savedRegisters[16];
} Thread;

enum ThreadState {
	EReady = 0,
	EBlockedMutex = 1,
	// ???
};


/*
Note this page is also mapped read-only into the process's user-side address space, because
ISLAGIATT.
*/
typedef struct Process {
	uint32 pid;
	uintptr pdePhysicalAddress;
	uintptr heapLimit;

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
} SuperPage;

ASSERT_COMPILE(sizeof(SuperPage) <= KPageSize);

#define TheSuperPage ((SuperPage*)KSuperPageAddress)
#define Al ((PageAllocator*)KPageAllocatorAddr)

#define GetProcess(idx) ((Process*)(KProcessesSection + ((idx) << KPageShift)))
#define indexForProcess(p) ((((uintptr)(p)) >> KPageShift) & 0xFF)

static inline Thread* firstThreadForProcess(Process* p) {
	return &p->threads[0];
}

static inline Process* processForThread(Thread* t) {
	// Threads are always within their process page, so simply mask off and cast
	return (Process*)(((uintptr)t) & ~(KPageSize - 1));
	
}

void process_start(const char* moduleName, const char* module, int moduleSize, uint32 sp);
bool process_init(Process* p);
bool process_grow_heap(Process* p, int incr);

#endif // LUPI_K_H
