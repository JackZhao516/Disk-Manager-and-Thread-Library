#include <iostream>
#include <cstdlib>
#include "thread.h"

using std::cout;
using std::endl;

const int MAX = 3;

mutex mu1;
cv waitingConsumers, waitingProducers;

// test broadcast

int numCokes = 0;		// global variable

void consumer1(void* a)
{
	mu1.lock();
	while (numCokes == 0) {
		cout << "consumer1 wait" << endl;
		waitingConsumers.wait(mu1);
	}
	// take coke out of machine
	numCokes--;
	cout << "consumer1 decremented coke num to " << numCokes << endl;
	if (numCokes == 0) {
		cout << "producer signaled by consumer1" << endl;
		waitingProducers.signal();
	}
	mu1.unlock();
}

void consumer2(void* a)
{
	mu1.lock();
	while (numCokes == 0) {
		cout << "consumer2 wait" << endl;
		waitingConsumers.wait(mu1);
	}
	// take coke out of machine
	numCokes--;
	cout << "consumer2 decremented coke num to " << numCokes << endl;
	if (numCokes == 0) {
		cout << "producer signaled by consumer2" << endl;
		waitingProducers.signal();
	}
	mu1.unlock();
}

void consumer3(void* a)
{
	mu1.lock();
	while (numCokes == 0) {
		cout << "consumer3 wait" << endl;
		waitingConsumers.wait(mu1);
	}
	// take coke out of machine
	numCokes--;
	cout << "consumer3 decremented coke num to " << numCokes << endl;
	if (numCokes == 0) {
		cout << "producer signaled by consumer3" << endl;
		waitingProducers.signal();
	}
	mu1.unlock();
}

void producer(void* a)
{
	mu1.lock();
	thread c1((thread_startfunc_t)consumer1, (void*) "consumer1 created");
	thread c2((thread_startfunc_t)consumer2, (void*) "consumer2 created");
	thread c3((thread_startfunc_t)consumer3, (void*) "consumer3 created");
	while (numCokes == MAX) {
		cout << "producer wait called" << endl;
		waitingProducers.wait(mu1);
	}
	// add coke to machine
	cout << "original num coke: " << numCokes;
	numCokes = MAX;
	cout << " incremented to MAX=3" << endl;
	if (numCokes == MAX) {
		waitingConsumers.broadcast();
	}
	cout << "producer thread finished" << endl;
	mu1.unlock();
}

int main()
{
	cpu::boot(1, (thread_startfunc_t)producer, (void*)100, false, false, 0);
}