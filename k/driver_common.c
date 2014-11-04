#include <k.h>
#include <err.h>

int kern_setInputRequest(uintptr userInputRequestPtr) {
	// Arg2 is the InputRequest* aka the AsyncRequest*
	// User side ensures that asyncRequest->result starts off pointing to an
	// integer being the max buf size, which is directly followed by the buf
	// (maxBufSize is always measured in number of samples, not bytes).
	if (TheSuperPage->inputRequest.userPtr) {
		return KErrAlreadyExists;
	}
	if (TheSuperPage->inputRequest.thread == 0) {
		TheSuperPage->inputRequest.thread = TheSuperPage->currentThread;
	}
	// Currently, no-one else gets to steal being the input handler
	ASSERT(TheSuperPage->currentThread == TheSuperPage->inputRequest.thread);
	ASSERT_USER_PTR32(userInputRequestPtr);
	int* maxSamples = (int*)*(uintptr*)userInputRequestPtr;
	ASSERT_USER_PTR32(maxSamples);
	TheSuperPage->inputRequestBufferSize = *maxSamples;
	TheSuperPage->inputRequestBuffer = (uintptr)maxSamples + sizeof(int);
	ASSERT_USER_PTR8(TheSuperPage->inputRequestBuffer + TheSuperPage->inputRequestBufferSize * 2*sizeof(uint32) - 1);
	TheSuperPage->inputRequest.userPtr = userInputRequestPtr;
	return 0;
}