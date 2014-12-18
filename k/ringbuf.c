#include <k.h>

/// Ring buffer impl. Takes a byte buffer up to 257 bytes in size.
// Uses 2 of those bytes for metadata (so maximum number of bytes you can buffer is 255)
bool ring_empty(byte* ring, int size) {
	byte* got = &ring[size];
	byte* read = &ring[size+1];
	return *got == *read;
}

bool ring_full(byte* ring, int size) {
	byte* got = &ring[size];
	return *got == 0xFF;
}

void ring_push(byte* ring, int size, byte b) {
	// Must have checked for !ring_full()
	byte* got = &ring[size];
	byte* read = &ring[size+1];

	ring[*got] = b;
	*got = *got + 1;
	if (*got == size) *got = 0;
	if (*got == *read) *got = 0xFF; // We're now full
}

byte ring_pop(byte* ring, int size) {
	// Must have checked for !ring_empty()
	byte* got = &ring[size];
	byte* read = &ring[size+1];

	byte result = ring[*read];
	if (*got == 0xFF) *got = *read; // We're no longer full, got a free space at *read
	*read = *read + 1;
	if (*read == size) *read = 0;
	return result;
}

int ring_free(byte* ring, int size) {
	byte got = ring[size];
	byte read = ring[size+1];
	if (got == 0xFF) return 0; // Full
	else if (read >= got) return read - got;
	else return size - (got - read);
}
