#include <iostream>
#include <cstdlib>
#include "thread.h"

using std::cout;
using std::endl;

// Resource exhaustion. The process may exhaust a resource 
// needed by the OS. In this project, the main resource used 
// by the OS is memory. If the thread library is unable to 
// service a request due to lack of memory, it should throw 
// a std::bad_alloc exception.

int g = 0;

mutex mutex1;
cv cv1;

void loop(void* a)
{
	char* id = (char*)a;
	int i;
	new int* [10000];
	mutex1.lock();
	cout << "loop called with id " << id << endl;

	for (i = 0; i < 5; i++, g++) {
		cout << id << ":\t" << i << "\t" << g << endl;
		mutex1.unlock();
		thread::yield();
		mutex1.lock();
	}
	cout << id << ":\t" << i << "\t" << g << endl;
	mutex1.unlock();
}

void parent(void* a)
{
	intptr_t arg = (intptr_t)a;

	mutex1.lock();
	cout << "parent called with arg " << arg << endl;

	try {
		while (true) {
			thread((thread_startfunc_t)loop, (void*)"child thread");
		}
		loop((void*)"parent thread");
	}
	catch (std::bad_alloc& ba) {
		cout << "bad alloc\n";
	}
	mutex1.unlock();
}

int main()
{
	cpu::boot(1, (thread_startfunc_t)parent, (void*)100, false, false, 0);
}
