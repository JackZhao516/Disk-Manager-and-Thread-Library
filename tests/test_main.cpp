#include "thread.h"
#include <vector>
#include <fstream>
#include <iostream>

int num_thread = 0;
int max_disk_queue = 0;
int count = 0;
int currentPos = 0;
int started = 0;
char** argv_local;
mutex mutex1;
cv cv1;
std::vector<std::vector<int>> inputData;
std::vector<std::pair<int, int>> diskQueue;
std::vector<int> current;

void readData(int argc, char** argv) {
	num_thread = argc - 1;
	count = num_thread - 1;
	max_disk_queue = std::atoi(argv[1]);
	inputData.resize(num_thread - 1);
	diskQueue.reserve(std::atoi(argv[1]));
	current.resize(num_thread - 1, -1);
	argv_local = argv;
}

void issue(void* i) {
	mutex1.lock();
	std::ifstream file;
	file.open(argv_local[*((int*)i) + 2]);
	int data = -1;

	while (file >> data) {
		inputData[*((int*)i)].push_back(data);
	}
	if (inputData[*((int*)i)].size() == 0)	count--;

	started++;
	cv1.broadcast();
	mutex1.unlock();

	mutex1.lock();
	size_t s = 0;
	int* index = (int*)i;

	while (s != (inputData[*index].size())) {
		while ((int)diskQueue.size() == std::min(max_disk_queue, std::min(count, num_thread - 1)) || current[*index] != ((int)s - 1)) {
			cv1.wait(mutex1);
		}
		int tmp = inputData[*index][s];
		diskQueue.push_back({ *index, tmp });
		//print_request(*index, tmp);
		++s;
		cv1.broadcast();
		cv1.wait(mutex1);
	}

	mutex1.unlock();
	file.close();
}


void service(void* s) {
	std::vector<thread*> threads;
	for (int i = 0; i < (num_thread - 1); ++i) {
		int* tmp = new int(i);
		threads.push_back(new thread((thread_startfunc_t)issue, (void*)tmp));
	}

	mutex1.lock();
	while (started != (num_thread - 1)) {
		cv1.wait(mutex1);
	}
	mutex1.unlock();


	mutex1.lock();
	while (count != 0) {

		while (int(diskQueue.size()) < std::min(max_disk_queue, std::min(count, num_thread - 1))) {
			cv1.wait(mutex1);
		}
		int dis = 1000;
		std::pair<int, int> best = { -1,-1 };
		int bestIndex = -1;
		for (int i = 0; i < int(diskQueue.size()); ++i) {
			if (abs(diskQueue[i].second - currentPos) < dis) {
				best = diskQueue[i];
				bestIndex = i;
				dis = abs(diskQueue[i].second - currentPos);
			}
		}

		currentPos = best.second;
		++current[best.first];
		//print_service(best.first, best.second);
		diskQueue.erase(diskQueue.begin() + bestIndex);

		if (current[best.first] == ((int)inputData[best.first].size() - 1)) {
			count--;
		}

		if ((int)diskQueue.size() < count) {
			cv1.broadcast();
		}
	}

	mutex1.unlock();


	for (size_t i = 0; i < threads.size(); ++i) {
		(*(threads[i])).join();
		delete threads[i];
	}

}

int main(int argc, char** argv) {
	readData(argc, argv);
	cpu::boot(1, (thread_startfunc_t)service, (void*)100, false, false, 0);
}