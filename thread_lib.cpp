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

typedef class context {
public:
    ucontext_t* ucontext_ptr;
    std::queue<context*> exit_queue;
    bool finish;
    char* stack_address;
    context(){
        ucontext_ptr = new ucontext_t;
        finish = false;
        stack_address = nullptr;
    }
    ~context(){
        delete[] stack_address;
        delete ucontext_ptr;
    }
} context;


// ***********
// *   cpu   *
// ***********

class cpu::impl{
public:
	static void interrupt_timer() {
		thread::yield();
	}

	static void ready_queue_push_helper(context* t){
		try {
				cpu::impl::ready_queue.push(t);
			}
		catch (std::bad_alloc& ba) {
				throw ba;
			}
	}

	static std::queue<context*> ready_queue;
	static context main_program;
	static context* current_thread; // current running thread, should be set 
									// before setcontext in cpu_init.
};

std::queue<context*> cpu::impl::ready_queue;
context cpu::impl::main_program;
context* cpu::impl::current_thread;


// ************
// *  thread  *
// ************
class thread::impl {
public:
	impl() {}

	static void release_resource(context* thread_context) {
		// dump exit_queue into ready_queue
		while (!thread_context->exit_queue.empty()) {
			cpu::impl::ready_queue_push_helper(thread_context->exit_queue.front());
		}

		// release heap resources
		delete thread_context;
	}

	static void wrapper_func(thread_startfunc_t func, void* arg, context* context) {
		cpu::interrupt_enable();
		func(arg);
		cpu::interrupt_disable();
		context->finish = true;
		swapcontext(context->ucontext_ptr, cpu::impl::main_program.ucontext_ptr);
	}

	context* thread_context;  // Not destroy with the dtor of thread,
							  // Manually destroy when the cpu_init 
							  // finds that cpu::impl::before_thread is nullptr.
							  // Binded with the life span of ucontext,
							  // not the thread object.
							  // Call static function: thread::impl::release_resource
							  // to release resources.
};

thread::thread(thread_startfunc_t func, void* arg) {
	cpu::interrupt_disable();
	try {
		this->impl_ptr = new impl;
		this->impl_ptr->thread_context = new context;
		this->impl_ptr->thread_context->stack_address = new char[STACK_SIZE];
		this->impl_ptr->thread_context->ucontext_ptr->uc_stack.ss_sp = this->impl_ptr->thread_context->stack_address;
		this->impl_ptr->thread_context->ucontext_ptr->uc_stack.ss_size = STACK_SIZE;
		this->impl_ptr->thread_context->ucontext_ptr->uc_stack.ss_flags = 0;
		this->impl_ptr->thread_context->ucontext_ptr->uc_link = nullptr;
		makecontext(this->impl_ptr->thread_context->ucontext_ptr, (void (*)()) thread::impl::wrapper_func, 3, func, arg, this->impl_ptr->thread_context);
        cpu::impl::ready_queue.push(this->impl_ptr->thread_context);
	}
	catch (std::bad_alloc& ba) {
		throw ba;
	}
	cpu::interrupt_enable();
};

thread::~thread() {
	cpu::interrupt_disable();
	delete this->impl_ptr;
	cpu::interrupt_enable();
};

void thread::join() {
	cpu::interrupt_disable();
	if (this->impl_ptr->thread_context) {
		try {
			this->impl_ptr->thread_context->exit_queue.push(cpu::impl::current_thread);
		}
		catch (std::bad_alloc& ba) {
			throw ba;
		}
		swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program.ucontext_ptr);
	}
	cpu::interrupt_enable();
};

void thread::yield() {
	cpu::interrupt_disable();
	if (!cpu::impl::ready_queue.empty()) {
		cpu::impl::ready_queue_push_helper(cpu::impl::current_thread);
		swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program.ucontext_ptr);
	}
	cpu::interrupt_enable();
};

// ***************
// *  cpu_inint  *
// ***************
void cpu::init(thread_startfunc_t func, void* arg) {
    //initialize the main_program
    context* init_context = new context;
    try {
        getcontext(impl::main_program.ucontext_ptr);

        init_context->stack_address = new char[STACK_SIZE];
        init_context->ucontext_ptr->uc_stack.ss_sp = init_context->stack_address;
        init_context->ucontext_ptr->uc_stack.ss_size = STACK_SIZE;
        init_context->ucontext_ptr->uc_stack.ss_flags = 0;
        init_context->ucontext_ptr->uc_link = nullptr;
        makecontext(init_context->ucontext_ptr, (void (*)()) thread::impl::wrapper_func, 3, func, arg, init_context);
		cpu::impl::ready_queue.push(init_context);
	}
	catch (std::bad_alloc& ba) {
		throw ba;
	}
	impl_ptr = new impl;
	interrupt_vector_table[TIMER] = &impl::interrupt_timer;
	//run the program
	while (!impl::ready_queue.empty()) {
		impl::current_thread = impl::ready_queue.front();
		impl::ready_queue.pop();
		swapcontext(impl::main_program.ucontext_ptr, impl::current_thread->ucontext_ptr);
		if(impl::current_thread->finish){
		    thread::impl::release_resource(impl::current_thread);
		}
	}
	//release cpu resources
	delete impl_ptr;
	cpu::interrupt_enable_suspend();
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
			swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program.ucontext_ptr);
		}
	};

	void impl_unlock() {
		if (func_with_lock.find(cpu::impl::current_thread) == func_with_lock.end()) {
			throw std::runtime_error("unlock without lock");
		}
		func_with_lock.erase(cpu::impl::current_thread);
		status = true;
		if (!lock_queue.empty()) {
			cpu::impl::ready_queue_push_helper(lock_queue.front());
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
			cpu::impl::ready_queue_push_helper(m.impl_ptr->lock_queue.front());
			m.impl_ptr->lock_queue.pop();
			m.impl_ptr->status = false;
		}
		swapcontext(cpu::impl::current_thread->ucontext_ptr, cpu::impl::main_program.ucontext_ptr);
		m.impl_ptr->impl_lock();
	}

	void impl_signal() {
		if (!wait_queue.empty()) {
			cpu::impl::ready_queue_push_helper(wait_queue.front());
		}
	}

	void impl_broadcast() {
		if (!wait_queue.empty()) {
			cpu::impl::ready_queue_push_helper(wait_queue.front());
			wait_queue.pop();
			/*for (unsigned int i = 0; i < wait_queue.size(); i++) {
				cpu::impl::ready_queue_push_helper(wait_queue.front());
				wait_queue.pop();
			}*/
		}
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