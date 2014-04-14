#include <stddef.h>
#include <string.h>

// Dumbest allocator in the world. Doesn't reclaim memory, just returns sucessively growing
// pointers. We shall see if it's good enough....

typedef struct Heap {
	int nallocs;
	int nfrees;
	uintptr top;
	long alloced;
} Heap;

#define align(ptr) ((((uintptr)(ptr)) + 0x7) & ~0x7)

void klua_heapReset(uintptr hptr) {
	Heap* h = (Heap*)hptr;
	h->nallocs = 0;
	h->nfrees = 0;
	h->top = align(h+1);
	h->alloced = 0;
	//printk("Heap reset top = %p\n", (void*)h->top);
}

void* klua_alloc_fn(void *ud, void *ptr, size_t osize, size_t nsize) {
	//printk("lua_alloc_fn from %p\n", __builtin_return_address(0));
	Heap* h = (Heap*)ud;
	if (nsize == 0) {
		// free
		//printk("Freeing %p len %lu\n", ptr, osize);
		return NULL;
	}

	if (ptr && nsize <= osize) {
		return ptr;
	}

	void* result = (void*)h->top;
	h->top = align(h->top + nsize);
	// Don't bother checking - let the MMU fault us
	/*
	 if (h->top > KLuaHeapBase + 1*1024*1024) {
	 printk("No mem! nallocs=%d\n", h->nallocs);
	 abort();
	 }
	 */
	h->nallocs++;
	h->alloced += nsize;
	if (ptr) {
		// Remember to copy in the reallocd mem!
		memcpy(result, ptr, osize);
	}
	//printk("realloc returning %p for len=%d\n", (void*)result, (int)nsize);
	return result;
}
