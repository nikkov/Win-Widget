/*!
#
# Win-Widget. Windows related software for Audio-Widget/SDR-Widget (http://code.google.com/p/sdr-widget/)
# Copyright (C) 2012 Nikolay Kovbasa
#
# Permission to copy, use, modify, sell and distribute this software 
# is granted provided this copyright notice appears in all copies. 
# This software is provided "as is" without express or implied
# warranty, and with no claim as to its suitability for any purpose.
#
#----------------------------------------------------------------------------
# Contact: nikkov@gmail.com
#----------------------------------------------------------------------------
*/
#ifndef __INCLUDE_PRIMITIVES_H_
#define __INCLUDE_PRIMITIVES_H_

#include <windows.h>

// System Abstraction Layer based on SAL by K.A. Knizhnik (http://www.garret.ru/sal.html)

#define task_proc WINAPI

// simple synchronization object
class event_internals 
{ 
private: 
	HANDLE h;

public:
	void wait() { WaitForSingleObject(h, INFINITE); }

	bool wait_with_timeout(DWORD msec) { return WaitForSingleObject(h, msec) == WAIT_OBJECT_0; }

	void signal() { SetEvent(h); }
	void reset() { ResetEvent(h); }

	event_internals(bool signaled = false) { h = CreateEvent(NULL, true, signaled, NULL); }
	~event_internals() { CloseHandle(h); }
}; 

// basic synchronization primitive
class mutex_internals 
{ 
protected: 
	CRITICAL_SECTION cs;

public:
	void enter() { EnterCriticalSection(&cs); }
	void leave() { LeaveCriticalSection(&cs); }

	mutex_internals() { InitializeCriticalSection(&cs); }
	~mutex_internals() { DeleteCriticalSection(&cs); } 
};

// protecting block of code from concurrent access
class guard_internals 
{ 
protected: 
	mutex_internals* m_mutex;

public:
	guard_internals(mutex_internals* mutex) { m_mutex = mutex; if(m_mutex) m_mutex->enter(); }
	~guard_internals() { if(m_mutex) m_mutex->leave(); } 
};

// simple task
class task
{ 
public: 
	typedef void (task_proc *fptr)(void* arg); 
	enum priority 
	{ 
		pri_background, 
		pri_low, 
		pri_normal, 
		pri_high, 
		pri_realtime 
	};
	enum 
	{ 
		min_stack    = 8*1024, 
		small_stack  = 16*1024, 
		normal_stack = 64*1024,
		big_stack    = 256*1024,
		huge_stack   = 1024*1024
	}; 
	//
	// Create new task. Pointer to task object returned by this function
	// can be used only for thread identification.
	//
	static task* create(fptr f, void* arg = NULL, priority pri = pri_normal, 
		size_t stack_size = normal_stack)
	{ 
		task* threadid;
		HANDLE h = CreateThread(NULL, stack_size, LPTHREAD_START_ROUTINE(f), arg, CREATE_SUSPENDED, (DWORD*)&threadid);
		if (h == NULL) 
		{ 
			return NULL;
		}
		SetThreadPriority(h, THREAD_PRIORITY_LOWEST + (THREAD_PRIORITY_HIGHEST - THREAD_PRIORITY_LOWEST) * (pri - pri_background) / (pri_realtime - pri_background));
		ResumeThread(h);
		CloseHandle(h);
		return threadid;
	}

	static void  exit()
	{
		ExitThread(0);
	} 
	//
	// Get current task
	//
	static task* current()
	{ 
		return (task*)GetCurrentThreadId();
	}
}; 

// simple task with some useful functions
class SimpleWorker
{
public:
	SimpleWorker() : m_task(NULL), m_exitFlag(false) {}
	virtual ~SimpleWorker() { Stop(); }

	// Start task
	virtual void Start()
	{
		m_task = task::create(process_function, (void*)this, task::pri_realtime);
	}
	// Stop task
	virtual void Stop()
	{
		if(m_task)
		{
			m_exitFlag = true;
			// waiting task ending
			m_guard_thread.enter();
			m_guard_thread.leave();

			m_task = NULL;
		}
	}
	bool IsWork()
	{
		return m_task != NULL;
	}
protected:
	// main task function
	virtual bool DoWork() = 0;
	// sync object for task
	mutex_internals m_guard_thread;

	// sync object for concurrent data access
	mutex_internals m_guard;
	// need exit task flag
	volatile int	m_exitFlag;

	// function task
	virtual void Work()
	{
		m_guard_thread.enter();
		while (!m_exitFlag)
		{
			try
			{
				// input to main function
				m_guard.enter();
				// work
				bool retVal = DoWork();
				// exit from main function
				m_guard.leave();
				// DoWork failed - exit
				if(!retVal)
					break;
			}
			catch (...)
			{
				exit(-1);
			}
		}
		m_guard_thread.leave();
		task::exit();
	}
private:
	static void task_proc process_function(void* arg)
	{
		SimpleWorker* worker = (SimpleWorker*)arg;
		if(worker)
			worker->Work();
	}
	task*			m_task;
};

#endif //__INCLUDE_PRIMITIVES_H_