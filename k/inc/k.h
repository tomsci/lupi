#ifndef LUPI_K_H
#define LUPI_K_H

#define LUPI_VERSION_STRING "0.11"

typedef unsigned long PhysAddr;

// I don't think we'll be worrying about huge pages
#define PAGE_SIZE 4096

/*
Limiting to 256 running processes makes the maths quite nice - the ProcessList fits into a page,
and Process overhead is a maximum 1MB (sounds big but it's fixed). There's maybe also some
cacheing tweaks we can do a la Multiple Memory Model on ARM11.
*/
#define MAX_PROCESSES 256

/*
Max threads per process
*/
#define MAX_THREADS 64

void* memset(void* b, byte val, int len);

void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
void hexdump(const char* addr, int len);
void worddump(const char* addr, int len);

typedef struct MemMap {
	int TODO;
} MemMap;


typedef struct Process Process;
typedef struct Thread Thread;

typedef struct Thread {
	//Process* process;
	//Thread* nextInProcess;
	Thread* prevSchedulable;
	Thread* nextSchedulable;
	uint8 state;
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
	MemMap memMap;
	uint32 pid;
	uint8 index;

	uint8 nthreads;
	Thread threads[MAX_THREADS];
//	byte filler[922*4];
} Process;

/*
Each process gets one page of kernel memory for its Process, and nothing else, so everything the
process does kernel side has to be contained within that.
...
Except that page tables take a minimum of one page per 1MB of virtual address space 
(1 page = 256 words each describing a page, 256*4KB = 1MB) plus first-level PTE, so there's no
way we can fit it all in one page.
*/
ASSERT_COMPILE(sizeof(Process) <= PAGE_SIZE);

typedef struct SuperPage {
	int TODO; // I'm sure there's something we need to put in here...
} SuperPage;

ASSERT_COMPILE(sizeof(SuperPage) <= PAGE_SIZE);

//typedef struct PageAllocator {
//
//};

#define TheSuperPage ((SuperPage*)KSuperPageAddress)
#define KSuperPageBlockLength (PAGE_SIZE * (MAX_PROCESSES + 2))
// One for superpage, MAX_PROCESS for the processes, one more for the kernel stack

/*
Process pages directly follow the superpage
*/
#define GetProcess(idx) ((Process*)(KSuperPageAddress + PAGE_SIZE * ((idx)+1)))


inline Thread* firstThreadForProcess(Process* p) {
	return &p->threads[0];
}

inline Process* ProcessForThread(Thread* t) {
	// Threads are always within their process page, so simply mask off and cast
	return (Process*)(((uintptr)t) & ~(PAGE_SIZE - 1));
	
}

inline Thread* currentThread() {
	//TODO!
	return NULL;
}

#endif // LUPI_K_H
