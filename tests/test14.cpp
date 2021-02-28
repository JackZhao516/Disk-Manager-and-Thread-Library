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
int thread3_done = 0;
int thread4_done = 0;
int thread5_done = 0;

thread* t_1;
thread* t_2;
thread* t_3;
thread* t_4;
thread* t_5;

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
	t_2->join(); // thread joining itself
	t_3->join();
	t_4->join();
	t_5->join();
	mu1.unlock();
}

void thread3(void* a)
{
	mu1.lock();
	cout << "thead 3 called " << endl;
	cout << "setting thread3_done = 1" << endl;
	thread3_done = 1;
	t_4->join();
	t_5->join();
	mu1.unlock();
}

void thread4(void* a)
{
	mu1.lock();
	cout << "thead 4 called " << endl;
	cout << "setting thread4_done = 1" << endl;
	thread4_done = 1;
	t_5->join();
	mu1.unlock();
}

void thread5(void* a)
{
	mu1.lock();
	cout << "thead 5 called " << endl;
	cout << "setting thread5_done = 1" << endl;
	thread5_done = 1;
	mu1.unlock();
}

void parentThread(void* a)
{
	intptr_t arg = (intptr_t)a;
	mu1.lock();
	cout << "parent called with arg " << arg << endl;
	thread t1((thread_startfunc_t)thread1, (void*) "thread1 created");
	thread t2((thread_startfunc_t)thread2, (void*) "thread2 created");
	thread t3((thread_startfunc_t)thread3, (void*) "thread3 created");
	thread t4((thread_startfunc_t)thread4, (void*) "thread4 created");
	thread t5((thread_startfunc_t)thread5, (void*) "thread5 created");
	t_1 = &t1;
	t_2 = &t2;
	t_3 = &t3;
	t_4 = &t4;
	t_5 = &t5;

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