#include <iostream>
#include <cstdlib>
#include "thread.h"

using std::cout;
using std::endl;

// Error handling: deadlock
// a thread trying to join with itself

int g = 0;

mutex mutex1;

void parent(void* a)
{
	thread* arg = (thread*)a; // the ptr to parent thread obj
	mutex1.lock();
	cout << "mutex1 lock called by parent" << endl;
	arg->join();
	mutex1.unlock();
}

int main()
{
	cpu::boot(1, (thread_startfunc_t)parent, (void*)(&parent), false, false, 0);
}