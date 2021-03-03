#include <iostream>
#include <cstdlib>
#include "thread.h"

using std::cout;
using std::endl;

// Error handling: deadlock
// a thread trying to acquire a locked mutex

int g = 0;

mutex mutex1, mutex2;

int child_done = 0;		// global variable; shared between the two threads

void child(void* a)
{
	char* message = (char*)a;
	mutex2.lock();
	cout << "mutex2 lock called by child" << endl;
	mutex1.lock();
	cout << "mutex1 lock called by child" << endl;
	thread::yield();
	mutex1.unlock();
	mutex2.unlock();
}

void parent(void* a)
{
	intptr_t arg = (intptr_t)a;
	thread t1((thread_startfunc_t)child, (void*) "test message");
	mutex1.lock();
	thread::yield();
	cout << "mutex1 lock called by parent" << endl;
	mutex2.lock();
	cout << "mutex2 lock called by parent" << endl;
	mutex2.unlock();
	mutex1.unlock();
}

int main()
{
	cpu::boot(1, (thread_startfunc_t)parent, (void*)100, false, false, 0);
}