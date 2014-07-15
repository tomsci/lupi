#ifndef SCHED_H
#define SCHED_H

void exec_threadYield();

static inline void sched_yield() {
	exec_threadYield();
}

#endif
