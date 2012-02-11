#ifndef __INCLUDE_PRIMITIVES_H_
#define __INCLUDE_PRIMITIVES_H_

#include <windows.h>
//#include "log/logfunction.h"

//Описание примитивов синхронизации и потоков
//оригинал - библиотека SAL by K.A. Knizhnik

#define task_proc WINAPI
typedef unsigned timeout_t; // timeout in milliseconds

//Класс синхронизации для сигнализации
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

//Внутренний класс синхронизации для обеспечения исключения доступа к критичным данным из разных потоков
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


class guard_internals 
{ 
protected: 
	mutex_internals* m_mutex;

public:
	guard_internals(mutex_internals* mutex) { m_mutex = mutex; if(m_mutex) m_mutex->enter(); }
	~guard_internals() { if(m_mutex) m_mutex->leave(); } 
};


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
			//console::error("CreateThread failed with error code=%d\n", GetLastError());
			return NULL;
		}
		SetThreadPriority(h, THREAD_PRIORITY_LOWEST + 
			(THREAD_PRIORITY_HIGHEST - THREAD_PRIORITY_LOWEST) 
			* (pri - pri_background) 
			/ (pri_realtime - pri_background));
		ResumeThread(h);
		CloseHandle(h);
		return threadid;
	}

	static void  exit()
	{
		ExitThread(0);
	} 
	//
	// Current task will sleep during specified period
	//
	static void  sleep(timeout_t msec)
	{ 
		Sleep(msec);
	}
	//
	// Get current task
	//
	static task* current()
	{ 
		return (task*)GetCurrentThreadId();
	}
}; 


class SimpleWorker
{
public:
	SimpleWorker() : m_task(NULL), m_exitFlag(false) {}
	virtual ~SimpleWorker() { Stop(); }

	// Запуск потока
	virtual void Start()
	{
		m_task = task::create(process_function, (void*)this, task::pri_realtime);
	}
	// Остановка потока
	virtual void Stop()
	{
		if(m_task)
		{
			m_exitFlag = true;
			// Подождем завершения потока
			m_guard_thread.enter();
			m_guard_thread.leave();

			//m_task->exit();	
			m_task = NULL;
		}
	}
	bool IsWork()
	{
		return m_task != NULL;
	}
protected:
	// основная рабочая функция потока
	virtual bool DoWork() = 0;
	// мьютех для ограничения преждевременного выхода из потока
	mutex_internals m_guard_thread;

	// мьютех для ограничения одновременного доступа к данным
	mutex_internals m_guard;
	// флаг сигнализирующий о необходимости завершения потока
	volatile int	m_exitFlag;

	// рабочая функция потока
	virtual void Work()
	{
		m_guard_thread.enter();
		while (!m_exitFlag)
		{
			try
			{
				//вход в критическую секцию для ограничения одновременного доступа к критическим данным объекта
				m_guard.enter();
				//Работа в производном классе
				bool retVal = DoWork();
				m_guard.leave();
				//Если производный класс просигнализировал, то выйти
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