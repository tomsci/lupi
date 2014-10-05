#include <stddef.h>

// static NAKED void doAlignedMemCpy(void* aDest, const void* src, int size) {


// }


// Dumb impl
void* memcpy(void* aDest, const void* src, int size) {
	if (((uintptr)aDest & 3) == 0 && (((uintptr)src) & 3) == 0) {
		// Simple aligned case
		const int n = size >> 2;
		const uint32* ptr = (uint32*)src;
		const uint32* end = ptr + n;
		uint32* dest = (uint32*)aDest;
		while (ptr != end) {
			*dest++ = *ptr++;
		}
		size = size & 3; // Remainder
		if (!size) return aDest;
		src = end;
		aDest = dest;
	}

	// Screw it, be inefficient
	const int n = size;
	const uint8* ptr = (uint8*)src;
	const uint8* end = ptr + n;
	uint8* dest = (uint8*)aDest;
	while (ptr != end) {
		*dest++ = *ptr++;
	}
	return aDest;
}
