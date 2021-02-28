#include <iostream>
#include <cstdlib>
#include "thread.h"

using std::cout;
using std::endl;

mutex mu1;
cv cv1;

// test join

int thread1_done = 0;
int thread2_done = 0;
int thread3_done = 0;		// global variable; shared between the two threads

void thread1(void* a)
{
	thread* t2 = (thread*)a;
	mu1.lock();
	t2->join(); // block thread1 and switch to thread2
	cout << "setting thread1_done = 1" << endl;
	thread1_done = 1;
	cv1.signal();
	mu1.unlock();
}

void thread2(void* a)
{
	thread* t3 = (thread*)a;
	mu1.lock();
	t3->join(); // block thread1 and switch to thread2
	cout << "setting thread2_done = 1" << endl;
	thread2_done = 1;
	mu1.unlock();
}

void thread3(void* a)
{
	mu1.lock();
	cout << "setting thread3_done = 1" << endl;
	thread3_done = 1;
	mu1.unlock();
}

void parentThread(void* a)
{
	intptr_t arg = (intptr_t)a;
	mu1.lock();
	cout << "parent called with arg " << arg << endl;
	thread t3((thread_startfunc_t)thread3, (void*) "thread3 created");
	thread t2((thread_startfunc_t)thread2, (void*)(&t3));
	thread t1((thread_startfunc_t)thread1, (void*)(&t2));
	t3.join(); // block parent thread and switch to thread3

	while (!thread1_done || !thread2_done || !thread3_done) {
		cout << "parent waiting for child to run\n";
		cv1.wait(mu1);
	}
	cout << "parent finished" << endl;
	mu1.unlock();
}

int main()
{
	cpu::boot(1, (thread_startfunc_t)parentThread, (void*)100, false, false, 0);
}