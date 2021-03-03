#include <iostream>
#include <cstdlib>
#include "thread.h"

using std::cout;
using std::endl;

// Error handling: Misuse of thread functions

int g = 0;
int h = 0;

mutex mutex1, mutex2;
cv cv1;

void loop(void* a)
{
	char* id = (char*)a;
	int i;

	mutex1.lock();
	cout << "loop called with id " << id << endl;

	for (i = 0; i < 5; i++, g++) {
		cout << id << ":\t" << i << "\t" << g << endl;
		mutex1.unlock();
		thread::yield();
		mutex1.lock();
	}
	// deliberately releasing a lock it is not holding
	// runtime error should be throwed
	mutex2.unlock();
	cout << id << ":\t" << i << "\t" << g << endl;
	mutex1.unlock();
}

void loop_two(void* a)
{
	char* id = (char*)a;
	int i;

	mutex2.lock();
	cout << "loop called with id " << id << endl;

	for (i = 0; i < 5; i++, h++) {
		cout << id << ":\t" << i << "\t" << h << endl;
		mutex2.unlock();
		thread::yield();
		mutex2.lock();
	}
	// deliberately releasing a lock which the parent thread is holding
	// runtime error should be throwed
	try {
		mutex1.unlock();
	}
	catch (std::runtime_error &e) {
		cout << "runtime/n";
	}
	cout << id << ":\t" << i << "\t" << g << endl;
	mutex2.unlock();
}

void parent(void* a)
{
	intptr_t arg = (intptr_t)a;

	mutex1.lock();
	cout << "parent called with arg " << arg << endl;

	thread t1((thread_startfunc_t)loop, (void*)"child thread");
	thread t2((thread_startfunc_t)loop_two, (void*)"child thread two");

	loop((void*)"parent thread");
	mutex1.unlock();
}

int main()
{
	cpu::boot(1, (thread_startfunc_t)parent, (void*)100, false, false, 0);
}
