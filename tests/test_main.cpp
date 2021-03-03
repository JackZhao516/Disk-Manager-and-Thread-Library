#include "thread.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

int num_thread = 0;
int max_disk_queue = 0;
int count = 0;
int currentPos = 0;
int started = 0;
std::vector<std::string> argv_local{"", "", "disk.in0" , "disk.in1" , 
"disk.in2" , "disk.in3" , "disk.in4" , "disk.in5" };

mutex mutex1;
cv cv1;
std::vector<std::vector<int>> inputData;
std::vector<std::pair<int, int>> diskQueue;
std::vector<int> current;

void readData() {
	num_thread = 2;
	count = 1;
	max_disk_queue = 3;
	inputData.resize(num_thread - 1);
	diskQueue.reserve(3);
	current.resize(num_thread - 1, -1);
	//argv_local = argv;
}

void issue(void* i) {
	mutex1.lock();
	std::ifstream file;
	file.open(argv_local[*((int*)i) + 2]);
	//cout << *(int*)i << endl;
	//cout << file.is_open() << endl;
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

void create_file(std::string filename, int num1, int num2) {
	std::ofstream outfile(filename);
	outfile << num1 << std::endl;
	outfile << num2 << std::endl;
	outfile.close();
}

int main() {
	create_file("disk.in0", 785, 53);
	create_file("disk.in1", 350, 914);
	create_file("disk.in2", 827, 567);
	create_file("disk.in3", 302, 230);
	create_file("disk.in4", 631, 11);
	readData();
	cpu::boot(1, (thread_startfunc_t)service, (void*)100, false, false, 0);
}