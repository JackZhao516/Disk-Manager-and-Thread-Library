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


thread* t_1;
thread* t_2;


void thread1(void* a)
{
	mu1.lock();
	cout << "thead 1 called " << endl;
	cout << "setting thread1_done = 1" << endl;
	thread1_done = 1;
	t_2->join();
	mu1.unlock();
}

void thread2(void* a)
{
	mu1.lock();
	cout << "thead 2 called " << endl;
	cout << "setting thread2_done = 1" << endl;
	thread2_done = 1;
	t_1->join();
	mu1.unlock();
}


void parentThread(void* a)
{
	intptr_t arg = (intptr_t)a;
	mu1.lock();
	cout << "parent called with arg " << arg << endl;
	thread t1((thread_startfunc_t)thread1, (void*) "thread1 created");
	thread t2((thread_startfunc_t)thread2, (void*) "thread2 created");
	t_1 = &t1;
	t_2 = &t2;

	thread1((void*)"thread 1");
	thread::yield();
	thread2((void*)"thread 1");
	thread::yield();

	cout << "parent finished" << endl;
	mu1.unlock();
}

int main()
{
	cpu::boot(1, (thread_startfunc_t)parentThread, (void*)100, false, false, 0);
}