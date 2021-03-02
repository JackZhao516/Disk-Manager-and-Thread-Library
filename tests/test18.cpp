#include <iostream>
#include <cstdlib>
#include "thread.h"

using std::cout;
using std::endl;

// Error handling: deadlock
// a thread trying to acquire a locked mutex

int g = 0;

mutex rwLock, mu1;
cv waitingReaders, waitingWriters;

int numReaders = 0;
int numWriters = 0; // global variable; shared between the two threads


void readerStart() {
	rwLock.lock();
	while (numWriters > 0) {
		waitingReaders.wait(rwLock);
	}
	numReaders++;
	rwLock.unlock();
}

void readerFinish() {
	rwLock.lock();
	numReaders--;
	if (numReaders == 0) {
		waitingWriters.signal();
	}
	rwLock.unlock();
}

void writerStart() {
	rwLock.lock();
	while (numReaders > 0 || numWriters > 0) {
		waitingWriters.wait(rwLock);
	}
	numWriters++;
	rwLock.unlock();
}

void writerFinish() {
	rwLock.lock();
	numWriters--;
	waitingReaders.broadcast();
	waitingWriters.signal();
	rwLock.unlock();
}

void reader() {
	readerStart();
	mu1.lock();
	cout << "current g value is: " << g << endl;
	cout << "num reader is: " << numReaders << endl;
	mu1.unlock();
	readerFinish();
}

void writer() {
	writerStart();
	mu1.lock();
	g++;
	cout << "changed g value is: " << g << endl;
	cout << "num writer is: " << numWriters<< endl;
	mu1.unlock();
	writerFinish();
}


void parent(void* a)
{

	thread r1((thread_startfunc_t)reader, (void*) "test message");
	thread r2((thread_startfunc_t)reader, (void*) "test message");
	thread w1((thread_startfunc_t)writer, (void*) "test message");
	thread r3((thread_startfunc_t)reader, (void*) "test message");
	thread w2((thread_startfunc_t)writer, (void*) "test message");
	thread r4((thread_startfunc_t)reader, (void*) "test message");
	thread w3((thread_startfunc_t)writer, (void*) "test message");
	thread r5((thread_startfunc_t)reader, (void*) "test message");
	thread w4((thread_startfunc_t)writer, (void*) "test message");
	thread r6((thread_startfunc_t)reader, (void*) "test message");
	thread w5((thread_startfunc_t)writer, (void*) "test message");
	
}

int main()
{
	cpu::boot(1, (thread_startfunc_t)parent, (void*)100, false, false, 0);
}