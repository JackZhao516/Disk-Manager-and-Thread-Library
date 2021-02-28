#include <iostream>
#include <queue>
#include <unordered_set>
#include <stdexcept>
#include <ucontext.h>
#include "thread.h"
#include "cpu.h"

// ************
// *  README  *
// ************
// context is the wrapper class of ucontext_t, stores all the info about a 
// running ucontext_t. All the data structures in this project should store
// the context*, rather thant the raw ucontext_t*. The only exception should
// only be the main_program variable in cpu::impl, since it is used for uc_link,
// and a raw ucontext_t* is sufficient.


typedef struct context {
	ucontext_t* ucontext_ptr = nullptr;
	std::queue<context*> exit_queue;
	thread* running_thread = nullptr; // bind thread with ucontext
} context;


// ***********
// *   cpu   *
// ***********

class cpu::impl{
public:
	static void interrupt_timer() {
		thread::yield();
	}

	static std::queue<context*> ready_queue;
	static ucontext_t* main_program;
	static context* current_thread; // current running thread, should be set 
									// before setcontext in cpu_init.

	static context* before_thread; // If cpu::impl::current_thread* is not nullptr,
							// it was returned from yield, do not release resource
							// for this context*. Otherwise, it was automatically
							// returned from uc_link, release resource for context*.
							// Remember to set back to nullptr before you setcontext
							// to a new thread.
	static bool init;
};

std::queue<context*> cpu::impl::ready_queue;
ucontext_t* cpu::impl::main_program;
context* cpu::impl::current_thread = nullptr;
context* cpu::impl::before_thread = nullptr;
bool cpu::impl::init = false;


// ************
// *  thread  *
// ************
class thread::impl {
public:
	impl() {
		thread_context = new context;
	}

	void func_wrapper(thread_startfunc_t t) {
		cpu::interrupt_enable();
		
	}

	static void release_resource(context* thread_context) {
		// for thread obj with finished ucontext
		if (thread_context->running_thread)
			thread_context->running_thread->impl_ptr->finished = true;

		// dump exit_queue into ready_queue
		while (!thread_context->exit_queue.empty()) {
			try {
				cpu::impl::ready_queue.push(thread_context->exit_queue.front());
			}
			catch (std::bad_alloc& ba) {
				throw ba;
			}
		}

		// release heap resources
		delete[] (char*)thread_context->ucontext_ptr->uc_stack.ss_sp;
		delete thread_context;
	}

	context* thread_context;  // Not destroy with the dtor of thread,
							  // Manually destroy when the cpu_init 
							  // finds that cpu::impl::before_thread is nullptr.
							  // Binded with the life span of ucontext,
							  // not the thread object.
							  // Call static function: thread::impl::release_resource
							  // to release resources.
	bool finished = false;
};


thread::thread(thread_startfunc_t t, void* arg) {
	cpu::interrupt_disable();
	try {
		this->impl_ptr = new impl;
		this->impl_ptr->thread_context->running_thread = this;
		this->impl_ptr->thread_context->ucontext_ptr = new ucontext_t;
		char* stack = new char[STACK_SIZE];
		this->impl_ptr->thread_context->ucontext_ptr->uc_stack.ss_sp = stack;
		this->impl_ptr->thread_context->ucontext_ptr->uc_stack.ss_size = STACK_SIZE;
		this->impl_ptr->thread_context->ucontext_ptr->uc_stack.ss_flags = 0;
		this->impl_ptr->thread_context->ucontext_ptr->uc_link = cpu::impl::main_program;
		makecontext(this->impl_ptr->thread_context->ucontext_ptr, (void (*)()) t, 1, arg);
	}
	catch (std::bad_alloc& ba) {
		throw ba;
	}
	
	try {
		cpu::impl::ready_queue.push(this->impl_ptr->thread_context);
	}
	catch (std::bad_alloc& ba) {
		throw ba;
	}
	
	cpu::interrupt_enable();
};

thread::~thread() {
	cpu::interrupt_disable();
	this->impl_ptr->thread_context->running_thread = nullptr;
	delete this->impl_ptr;
	cpu::interrupt_enable();
};

void thread::join() {
	cpu::interrupt_disable();
	if (!this->impl_ptr->finished) {
		try {
			this->impl_ptr->thread_context->exit_queue.push(cpu::impl::current_thread);
		}
		catch (std::bad_alloc& ba) {
			throw ba;
		}
		swapcontext(cpu::impl::current_thread->ucontext_ptr, this->impl_ptr->thread_context->ucontext_ptr);
	}
	cpu::interrupt_enable();
};

void thread::yield() {
	cpu::interrupt_disable();
	if (!cpu::impl::ready_queue.empty()) {
		try {
			cpu::impl::ready_queue.push(cpu::impl::current_thread);
		}
		catch (std::bad_alloc& ba) {
			throw ba;
		}

		cpu::impl::before_thread = cpu::impl::current_thread;
		swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program);
	}
	cpu::interrupt_enable();
};

// ***************
// *  cpu_inint  *
// ***************
void cpu::init(thread_startfunc_t func, void* v) {
    //initialize the main_program
    try {
        impl::main_program = new ucontext_t;
        char* stack = new char[STACK_SIZE];
        impl::main_program->uc_stack.ss_sp = stack;
        impl::main_program->uc_stack.ss_size = STACK_SIZE;
        impl::main_program->uc_stack.ss_flags = 0;
        impl::main_program->uc_link = nullptr;
        getcontext(impl::main_program);
    }
    catch (std::bad_alloc& ba) {
        throw ba;
    }
    interrupt_enable();
	try {
		thread(func, v);
	}
	catch (std::bad_alloc& ba) {
		throw ba;
	}
	impl_ptr = new impl;
	interrupt_vector_table[TIMER] = &impl::interrupt_timer;
	while (!impl::ready_queue.empty()) {
		impl::current_thread = impl::ready_queue.front();
		impl::ready_queue.pop();
		swapcontext(impl::main_program, impl::current_thread->ucontext_ptr);
		if (!impl::before_thread)
			thread::impl::release_resource(impl::before_thread);
		impl::before_thread = nullptr;
	}
	printf("All CPUs suspended.  Exiting.");
}

// ***********
// *  mutex  *
// ***********
class mutex::impl {
public:
	bool status;
	std::queue<context*> lock_queue;
	std::unordered_set<context*> func_with_lock;
	impl() {
		status = true;
	}

	void impl_lock() {
		try {
			func_with_lock.insert(cpu::impl::current_thread);
		}
		catch (std::bad_alloc& ba) {
			throw ba;
		}
		if (status) {
			status = false;
		}
		else {
			try {
				lock_queue.push(cpu::impl::current_thread);
			}
			catch (std::bad_alloc& ba) {
				throw ba;
			}
			swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program);
		}
	};

	void impl_unlock() {
		if (func_with_lock.find(cpu::impl::current_thread) == func_with_lock.end()) {
			throw std::runtime_error("unlock without lock");
		}
		func_with_lock.erase(cpu::impl::current_thread);
		status = true;
		if (!lock_queue.empty()) {
			try {
				cpu::impl::ready_queue.push(lock_queue.front());
			}
			catch (std::bad_alloc& ba) {
				throw ba;
			}
			
			lock_queue.pop();
			status = false;
		}
	}
};

mutex::mutex() {
	cpu::interrupt_disable();
	try {
		this->impl_ptr = new impl;
	}
	catch (std::bad_alloc& ba) {
		throw ba;
	}
	cpu::interrupt_enable();
}

mutex::~mutex() {
	cpu::interrupt_disable();
	delete impl_ptr;
	cpu::interrupt_enable();
}

void mutex::lock() {
	cpu::interrupt_disable();
	impl_ptr->impl_lock();
	cpu::interrupt_enable();
}

void mutex::unlock() {
	cpu::interrupt_disable();
	impl_ptr->impl_unlock();
	cpu::interrupt_enable();
}


// ********
// *  cv  *
// ********
class cv::impl {
private:
	std::queue<context*> wait_queue;
public:
	void impl_wait(mutex& m) {
		try {
			wait_queue.push(cpu::impl::current_thread);
		}
		catch (std::bad_alloc& ba) {
			throw ba;
		}
		
		//copy the unlock function without interrupt control
		if (m.impl_ptr->func_with_lock.find(cpu::impl::current_thread) == m.impl_ptr->func_with_lock.end()) {
			throw std::runtime_error("unlock without lock");
		}
		m.impl_ptr->func_with_lock.erase(cpu::impl::current_thread);
		m.impl_ptr->status = true;
		if (!m.impl_ptr->lock_queue.empty()) {
			try {
				cpu::impl::ready_queue.push(m.impl_ptr->lock_queue.front());
			}
			catch (std::bad_alloc& ba) {
				throw ba;
			}
			m.impl_ptr->lock_queue.pop();
			m.impl_ptr->status = false;
		}
		swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program);
	}

	void impl_signal() {
		if (!wait_queue.empty()) {
			try {
				cpu::impl::ready_queue.push(wait_queue.front());
			}
			catch (std::bad_alloc& ba) {
				throw ba;
			}	
		}
		swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program);
	}

	void impl_broadcast() {
		if (!wait_queue.empty()) {
			for (unsigned int i = 0; i < wait_queue.size(); i++) {
				try {
					cpu::impl::ready_queue.push(wait_queue.front());
				}
				catch (std::bad_alloc& ba) {
					throw ba;
				}
				
				wait_queue.pop();
			}
		}
		swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program);
	}
};

cv::cv() {
	cpu::interrupt_disable();
	impl_ptr = new impl;
	cpu::interrupt_enable();
}

cv::~cv() {
	cpu::interrupt_disable();
	delete impl_ptr;
	cpu::interrupt_enable();
}

void cv::broadcast() {
	cpu::interrupt_disable();
	impl_ptr->impl_broadcast();
	cpu::interrupt_enable();
}

void cv::signal() {
	cpu::interrupt_disable();
	impl_ptr->impl_signal();
	cpu::interrupt_enable();
}

void cv::wait(mutex& m) {
	cpu::interrupt_disable();
	impl_ptr->impl_wait(m);
	cpu::interrupt_enable();
}