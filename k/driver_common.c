#include <k.h>
#include <err.h>

int kern_setInputRequest(uintptr userInputRequestPtr) {
	// Arg2 is the InputRequest* aka the AsyncRequest*
	// User side ensures that asyncRequest->result starts off pointing to an
	// integer being the max buf size, which is directly followed by the buf
	// (maxBufSize is always measured in number of samples, not bytes).
	// printk("kern_setInputRequest userInputRequestPtr=%lx\n", userInputRequestPtr);
	if (TheSuperPage->inputRequest.userPtr) {
		return KErrAlreadyExists;
	}
	if (TheSuperPage->inputRequest.thread == 0) {
		TheSuperPage->inputRequest.thread = TheSuperPage->currentThread;
	}
	// Currently, no-one else gets to steal being the input handler
	ASSERT(TheSuperPage->currentThread == TheSuperPage->inputRequest.thread);
	ASSERT_USER_WPTR32(userInputRequestPtr);
	int* maxSamplesPtr = (int*)*(uintptr*)userInputRequestPtr;
	ASSERT_USER_WPTR32(maxSamplesPtr);
	TheSuperPage->inputRequestBufferSize = *maxSamplesPtr;
	TheSuperPage->inputRequestBuffer = (uintptr)maxSamplesPtr + sizeof(int);
	ASSERT_USER_WPTR8(TheSuperPage->inputRequestBuffer + TheSuperPage->inputRequestBufferSize * 3*sizeof(uint32) - 1);
	TheSuperPage->inputRequest.userPtr = userInputRequestPtr;
	// printk("kern_setInputRequest done\n");
	return 0;
}
