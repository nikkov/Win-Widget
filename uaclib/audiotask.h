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

#pragma once
#include "targetver.h"
#include <windows.h>
#include <string.h>
#include <tchar.h>
#include <math.h>

#ifdef _ENABLE_TRACE
extern void debugPrintf(const _TCHAR *szFormat, ...);
#endif

#define packetPerTransferDAC	16
#define packetPerTransferADC	16
#define packetPerTransferFb		8

class USBAudioDevice;

typedef void (*FillDataCallback)(void* context, UCHAR *buffer, int& len);

class Mutex 
{ 
protected: 
	CRITICAL_SECTION cs;

public:
	inline void Enter() { EnterCriticalSection(&cs); }
	inline void Leave() { LeaveCriticalSection(&cs); }

	Mutex() { InitializeCriticalSection(&cs); }
	~Mutex() { DeleteCriticalSection(&cs); } 
};

class FeedbackInfo
{
	Mutex m_guard;
	float cur_value;

	float playback_value; // BSB 20121222 added default sample rate value for playback

	float max_value;
	float min_value;
#ifdef _ENABLE_TRACE
	float last_value;
#endif
	float interval;
public:
	FeedbackInfo()
	{
		cur_value = 0.f;
#ifdef _ENABLE_TRACE
		last_value = 0.f;
#endif
		interval = 1.f;
	}

	void SetIntervalValue(float value)
	{
		interval = 1000.f * value; //for output real freq
	}

	void SetDefaultValue(float feedbackValue)
	{
		m_guard.Enter();
		cur_value = feedbackValue;
#ifdef _ENABLE_TRACE
		last_value = feedbackValue;
		playback_value = feedbackValue; // BSB 20121222 added
		debugPrintf("ASIOUAC: Set default value: %f\n", interval * feedbackValue);
#endif
		m_guard.Leave();
	}
	void SetValue(int feedbackValue)
	{
		m_guard.Enter();

		// BSB 20121222 revised to support both 16.16 and 15.17 feedback
		float newValue = (float)feedbackValue / 32768.0f;	// Must divide by 32768.0f when FW uses 16.16 samples / 125µs microframe

		if (newValue > playback_value * 1.5) {				// Must divide by 65536.0f when FW uses 15.17 samples / 125µs microframe
			newValue = newValue / 2;						// Inspired by https://git.kernel.org/?p=linux/kernel/git/tiwai/sound.git;a=blob;f=sound/usb/endpoint.c l 1081
		}

#ifdef _ENABLE_TRACE
		if(newValue != 0.f && fabs(last_value - newValue)/newValue > 0.5f / interval)
			//(int)(10*last_value) != (int)(10*newValue))
//			debugPrintf("ASIOUAC: Set fb value: %f (raw = %d, curVal = %f)\n", interval * newValue, feedbackValue, interval * cur_value);
			debugPrintf("ASIOUAC: Set fb value: %f (raw=%d, cur_value=%f, playback_value=%f)\n", interval * newValue, feedbackValue, interval * cur_value, interval * playback_value);

		last_value = newValue;
#endif
		cur_value = newValue;

		if(max_value == 0.f || max_value < cur_value)
			max_value = cur_value;
		if(min_value == 0.f || min_value > cur_value)
			min_value = cur_value;
		m_guard.Leave();
	}

	float GetValue()
	{
		float retVal;
		m_guard.Enter();
		retVal = cur_value;
		m_guard.Leave();
		return retVal;
	}

	float GetFreqValue()
	{
		float retVal;
		m_guard.Enter();
		retVal = interval * cur_value;
		m_guard.Leave();
		return retVal;
	}

	void ClearStatistics()
	{
		max_value = 0.f;
		min_value = 0.f;
	}

	float GetMaxValue()
	{
		float retVal;
		m_guard.Enter();
		retVal = interval * max_value;
		m_guard.Leave();
		return retVal;
	}

	float GetMinValue()
	{
		float retVal;
		m_guard.Enter();
		retVal = interval * min_value;
		m_guard.Leave();
		return retVal;
	}
};

class TaskThread abstract
{
public:
	enum TaskState
	{
		TaskCreated = 0,
		TaskStarted,
		TaskStopped,
		TaskExit
	};

	virtual bool BeforeStart() = 0;
	virtual bool AfterStop() = 0;
	virtual bool Work(volatile TaskState& exitFlag) = 0;
	virtual _TCHAR* TaskName() = 0;
};

template <class TaskClass> class BaseThread
{
	HANDLE			m_Thread;
	DWORD			m_ThreadID;

	//
	Mutex			m_inWork;

	Mutex			m_taskStateGuard;
	//terminate thread flag
	volatile TaskThread::TaskState	m_taskState;

	static void sThreadFunc(void* lpParameter)
	{
		BaseThread *bTread = (BaseThread *)(lpParameter);
		bTread->ThreadFunc();
	}

	//main thread fucntion
	void ThreadFunc()
	{
		bool retVal = TRUE;
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s. Thread started!\n", m_Task.TaskName());
#endif
		while (m_taskState != TaskThread::TaskExit)
		{
			try
			{
				m_inWork.Enter();
				if(m_taskState == TaskThread::TaskStarted)	// if in working mode call job function
					retVal = m_Task.Work(m_taskState);		// main job function
				m_inWork.Leave();
				if(!retVal)
					break; //????
				if(m_taskState == TaskThread::TaskStopped) // if in not working mode we go sleep and other thread can capture m_inWork mutex
					Sleep(1);
			}
			catch (...)
			{
				//todo
			}
		}
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s. Thread exited!\n", m_Task.TaskName());
#endif
		ExitThread(0);
	}
protected:
	//
	TaskClass		m_Task;

public:
	BaseThread(int nPriority = THREAD_PRIORITY_TIME_CRITICAL) : m_Task(), m_taskState(TaskThread::TaskCreated), m_Thread(INVALID_HANDLE_VALUE)
	{
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: %s. Thread constructor\n", m_Task.TaskName());
#endif
		m_Thread = CreateThread(NULL, 0, LPTHREAD_START_ROUTINE(sThreadFunc), this, CREATE_SUSPENDED, &m_ThreadID);
		if(m_Thread == INVALID_HANDLE_VALUE)
		{
		}
		SetThreadPriority(m_Thread, nPriority);
	}

	virtual ~BaseThread(void)
	{
		if(m_Thread != INVALID_HANDLE_VALUE)
		{
			if(m_taskState == TaskThread::TaskStarted)
				Stop();
			m_taskState = TaskThread::TaskExit;
			bool retVal = (ResumeThread(m_Thread) != -1);

			if(WaitForSingleObject(m_Thread, 1000/*INFINITE*/) == WAIT_TIMEOUT)
				TerminateThread(m_Thread, -1);
			if (m_Thread != INVALID_HANDLE_VALUE)
				CloseHandle(m_Thread);
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: %s. Thread destructor\n", m_Task.TaskName());
#endif
		}
	}


	bool Start()
	{
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s. Try start...\n", m_Task.TaskName());
#endif
		if(m_Thread == INVALID_HANDLE_VALUE)
		{
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: %s. Can't start thread. Invalid handle\n", m_Task.TaskName());
#endif
			return FALSE;
		}
		if(m_taskState == TaskThread::TaskStarted)
		{
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: %s. Can't start thread. Already started\n", m_Task.TaskName());
#endif
			return TRUE;
		}

		bool retVal;

		if(!m_Task.BeforeStart())
			return FALSE;
		m_taskState = TaskThread::TaskStarted;

		retVal = (ResumeThread(m_Thread) != -1);
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s. Start is OK\n", m_Task.TaskName());
#endif
		return retVal;
	}
	
	bool Stop()
	{
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s. Try stop...\n", m_Task.TaskName());
#endif
		if(m_Thread == INVALID_HANDLE_VALUE)
		{
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: %s. Can't stop thread. Invalid handle\n", m_Task.TaskName());
#endif
			return FALSE;
		}

		if(m_taskState == TaskThread::TaskStopped)
		{
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: %s. Can't stop thread. Already stopped\n", m_Task.TaskName());
#endif
			return TRUE;
		}

		bool retVal;
		//signal for exit from forking mode
		m_taskState = TaskThread::TaskStopped;
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s. Wait current job ending ...\n", m_Task.TaskName());
#endif
		//wait current job ending
		m_inWork.Enter();
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s. Wait current job ending successfully\n", m_Task.TaskName());
#endif

		//suspend thread
		retVal = (SuspendThread(m_Thread) != -1);

		m_Task.AfterStop();
		m_inWork.Leave();
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s.Stop is OK\n", m_Task.TaskName());
#endif
		return retVal;
	}

	bool IsStarted()
	{
		return m_taskState == TaskThread::TaskStarted;
	}
};

struct ISOBuffer
{
	PUCHAR          DataBuffer;
	KOVL_HANDLE     OvlHandle;

	KISO_CONTEXT*   IsoContext;
	KISO_PACKET*    IsoPackets;
};

#define MAX_OUTSTANDING_TRANSFERS	6


class AudioTask : public TaskThread
{
	KOVL_POOL_HANDLE    m_OvlPool;
	ULONG               m_DataBufferSize;
	ULONG               m_SubmittedCount;
	ULONG               m_CompletedCount;
	ULONG               m_FrameNumber;
	ULONG				m_LastStartFrame;

	ISOBuffer			m_isoBuffers[MAX_OUTSTANDING_TRANSFERS];
	int					m_outstandingIndex;
	int					m_completedIndex;

	Mutex				m_buffersGuard;

	void SetNextFrameNumber(ISOBuffer* buffer)
	{
		buffer->IsoContext->StartFrame = m_FrameNumber;
		m_FrameNumber += m_channelNumber * m_sampleSize;
	}

	void IsoXferComplete(ISOBuffer* buffer, ULONG transferLength)
	{
		m_CompletedCount++;
		m_LastStartFrame = buffer->IsoContext->StartFrame;
	}


	_TCHAR				m_taskName[64];
	int					m_isoTransferErrorCount;

protected:
	USBAudioDevice*		m_device;

	//Pipe information
	UCHAR				m_pipeId;
	USHORT				m_maximumPacketSize;
	UCHAR				m_interval;

	//sample format
	UCHAR				m_sampleSize;
	UCHAR				m_channelNumber;

	int					m_packetPerTransfer;
	int					m_packetSize;
	float				m_defaultPacketSize;

	bool				m_isStarted;
	int					nextFrameSize;
	int					m_sampleFreq;

	bool AllocBuffers();
	bool FreeBuffers();
	virtual bool InitBuffers(int freq) = 0;
	virtual bool BeforeStartInternal() = 0;
	virtual bool AfterStopInternal() = 0;

	virtual int FillBuffer(ISOBuffer* buffer) = 0;
	virtual bool RWBuffer(ISOBuffer* buffer, int len) = 0;
	virtual void ProcessBuffer(ISOBuffer* buffer) = 0;

#ifdef _ENABLE_TRACE
	virtual void CalcStatistics(int sampleNumbers) {}
	int					m_sampleNumbers;
	DWORD				m_tickCount;
#endif

public:
	AudioTask(int packetPerTransfer, _TCHAR* taskName) : m_device(NULL), 
		m_pipeId(0),
		m_maximumPacketSize(0),
		m_interval(0),
		m_DataBufferSize(0),
		m_SubmittedCount(0),
		m_CompletedCount(0),
		m_FrameNumber(0),
		m_LastStartFrame(0),
		m_packetPerTransfer(packetPerTransfer), 
		m_packetSize(0), 
		m_defaultPacketSize(0), 
		m_outstandingIndex(0),
		m_completedIndex(0),
		m_isStarted(FALSE),
		m_sampleFreq(0),
		m_sampleSize(4), //default sample size in bytes
		m_channelNumber(2),
		m_isoTransferErrorCount(0)
#ifdef _ENABLE_TRACE
		, m_sampleNumbers(0)
		, m_tickCount(0)
#endif
	{
		memset(m_isoBuffers, 0, sizeof(m_isoBuffers));
		_tcscpy_s(m_taskName, taskName);
	}

	virtual ~AudioTask()
	{
		FreeBuffers();
	}

	bool BeforeStart();
	bool AfterStop();
	bool Work(volatile TaskState& taskState);
	_TCHAR* TaskName() 
	{
		return m_taskName;
	}

	void Init(USBAudioDevice *device, UCHAR pipeId, USHORT maximumPacketSize, UCHAR interval, UCHAR channelNumber, UCHAR sampleSize)
	{
		FreeBuffers();
		m_device = device;
		m_pipeId = pipeId;
		m_maximumPacketSize = maximumPacketSize;
		m_interval = interval;
		m_channelNumber = channelNumber;
		m_sampleSize = sampleSize;
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: %s. AudioTask::Init(MaxPacketSize=%d, Interval=%d, ChannelNum=%d, SampleSize=%d)\n", 
			TaskName(), (int)maximumPacketSize, (int)m_interval, (int)m_channelNumber, (int)m_sampleSize);
#endif
	}

	void SetSampleFreq(int freq)
	{
		if(m_sampleFreq != freq)
			InitBuffers(freq);
		m_sampleFreq = freq;
	}

	bool BufferIsAllocated()
	{ return m_isoBuffers[0].DataBuffer != NULL; }
};

class AudioDACTask : public AudioTask
{
	FeedbackInfo*				m_feedbackInfo;
	FillDataCallback			m_readDataCb;
	void*						m_readDataCbContext;
#ifdef _ENABLE_TRACE
	FILE *m_dumpFile;
#endif
protected:
	bool InitBuffers(int freq)
	{
		if(m_isStarted)
			return FALSE;

		m_defaultPacketSize = (float)freq / 8000.f * (1 << (m_interval - 1)); // size in stereo samples in microframe
		m_packetSize = m_channelNumber * m_sampleSize * ((int)m_defaultPacketSize + 1); //size in bytes = (packetSize + 1 extra frame) * size of frame
		if(BufferIsAllocated())
			FreeBuffers();
		AllocBuffers();
		return TRUE;
	}
	bool BeforeStartInternal();
	bool AfterStopInternal();

	virtual int FillBuffer(ISOBuffer* buffer);
	virtual bool RWBuffer(ISOBuffer* buffer, int len);
	virtual void ProcessBuffer(ISOBuffer* buffer);

#ifdef _ENABLE_TRACE
	virtual void CalcStatistics(int sampleNumbers);
#endif

public:
	AudioDACTask() : AudioTask(packetPerTransferDAC, "Audio DAC task"), m_feedbackInfo(NULL), m_readDataCb(NULL), m_readDataCbContext(NULL)
	{
#ifdef _ENABLE_TRACE
		m_dumpFile = fopen("c:\\dac_dump.bin", "wb");
		debugPrintf("Created file c:\\dac_dump.bin");
#endif
	}

	~AudioDACTask()
	{
#ifdef _ENABLE_TRACE
		if(m_dumpFile)
			fclose(m_dumpFile);
#endif
	}

	void SetFeedbackInfo(FeedbackInfo* fb)
	{
		m_feedbackInfo = fb;
	}

	void SetCallback(FillDataCallback readDataCb, void* context)
	{
		m_readDataCbContext = context;
		m_readDataCb = readDataCb;
	}
};

class AudioADCTask : public AudioTask
{
	FeedbackInfo*				m_feedbackInfo;
	FillDataCallback			m_writeDataCb;
	void*						m_writeDataCbContext;
protected:
	bool InitBuffers(int freq)
	{
		if(m_isStarted)
			return FALSE;

		m_defaultPacketSize = (float)freq / 8000.f * (1 << (m_interval - 1)); // size in stereo samples in microframe
		m_packetSize = m_channelNumber * m_sampleSize * ((int)m_defaultPacketSize + 1); //size in bytes = (packetSize + 1 extra frame) * size of frame

		if(BufferIsAllocated())
			FreeBuffers();
		AllocBuffers();
		return TRUE;
	}
	bool BeforeStartInternal();
	bool AfterStopInternal();

	virtual int FillBuffer(ISOBuffer* buffer);
	virtual bool RWBuffer(ISOBuffer* buffer, int len);
	virtual void ProcessBuffer(ISOBuffer* buffer);

#ifdef _ENABLE_TRACE
	virtual void CalcStatistics(int sampleNumbers);
#endif

public:
	AudioADCTask() : AudioTask(packetPerTransferADC, "Audio ADC task"), m_feedbackInfo(NULL), m_writeDataCb(NULL), m_writeDataCbContext(NULL)
	{}

	~AudioADCTask()
	{}

	void SetFeedbackInfo(FeedbackInfo* fb)
	{
		m_feedbackInfo = fb;
	}

	void SetCallback(FillDataCallback writeDataCb, void* context)
	{
		m_writeDataCbContext = context;
		m_writeDataCb = writeDataCb;
	}
};

class AudioFeedbackTask : public AudioTask
{
	FeedbackInfo*	m_feedbackInfo;
protected:
	bool InitBuffers(int freq)
	{
		if(m_isStarted)
			return FALSE;
		if(!BufferIsAllocated())
		{
			m_packetSize = m_maximumPacketSize;
			m_defaultPacketSize = m_maximumPacketSize;
			AllocBuffers();
		}

		return TRUE;
	}
	bool BeforeStartInternal();
	bool AfterStopInternal();

	virtual int FillBuffer(ISOBuffer* buffer);
	virtual bool RWBuffer(ISOBuffer* buffer, int len);
	virtual void ProcessBuffer(ISOBuffer* buffer);
public:
	AudioFeedbackTask() : AudioTask(packetPerTransferFb, "Audio feedback task"), m_feedbackInfo(NULL)
	{}

	~AudioFeedbackTask()
	{}

	void SetFeedbackInfo(FeedbackInfo* fb)
	{
		m_feedbackInfo = fb;
	}
};

class AudioDAC : public BaseThread<AudioDACTask>
{
public:
	AudioDAC()
	{
	}
	void Init(USBAudioDevice* device, FeedbackInfo* fb, UCHAR pipeId, USHORT maximumPacketSize, UCHAR interval, UCHAR channelNumber, UCHAR sampleSize)
	{
		m_Task.Init(device, pipeId, maximumPacketSize, interval, channelNumber, sampleSize);
		m_Task.SetFeedbackInfo(fb);
	}
	void SetSampleFreq(int freq)
	{
		m_Task.SetSampleFreq(freq);
	}
	void SetCallback(FillDataCallback readDataCb, void* context)
	{
		m_Task.SetCallback(readDataCb, context);
	}
};

class AudioADC : public BaseThread<AudioADCTask>
{
public:
	AudioADC()
	{
	}
	void Init(USBAudioDevice* device, FeedbackInfo* fb, UCHAR pipeId, USHORT maximumPacketSize, UCHAR interval, UCHAR channelNumber, UCHAR sampleSize)
	{
		m_Task.Init(device, pipeId, maximumPacketSize, interval, channelNumber, sampleSize);
		m_Task.SetFeedbackInfo(fb);
	}
	void SetSampleFreq(int freq)
	{
		m_Task.SetSampleFreq(freq);
	}
	void SetCallback(FillDataCallback readDataCb, void* context)
	{
		m_Task.SetCallback(readDataCb, context);
	}
};

class AudioFeedback : public BaseThread<AudioFeedbackTask>
{
public:
	AudioFeedback()
	{
	}
	void Init(USBAudioDevice* device, FeedbackInfo* fb, UCHAR pipeId, USHORT maximumPacketSize, UCHAR interval, UCHAR valueSize)
	{
		m_Task.Init(device, pipeId, maximumPacketSize, interval, 1, valueSize);
		//WARNING! ADC & DAC endpoints must have the same values of the intervals for implicit feedback
		m_Task.SetFeedbackInfo(fb);
		m_Task.SetSampleFreq(48000); //set any sample rate only for allocate buffers
	}
};

