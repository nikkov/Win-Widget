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
/*
# Copyright (c) 2011 Travis Robinson <libusbdotnet@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
# 	  
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL TRAVIS LEE ROBINSON 
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
# THE POSSIBILITY OF SUCH DAMAGE. 
#
*/

// Read and write endpoints classes
// Contains parts from LibUsbK examples by Travis Lee Robinson (http://libusb-win32.sourceforge.net/libusbKv3/)

#pragma once
#include <crtdbg.h>

#include "primitives.h"
#include "libusbk.h"

class FeedbackInfo
{
	mutex_internals m_guard;
	float cur_value;
public:
	FeedbackInfo()
	{
		cur_value = 0.f;
	}
	void SetValue(int feedbackValue)
	{
		m_guard.enter();
		cur_value = (float)feedbackValue / 16384.0f;
		m_guard.leave();
	}

	float GetValue()
	{
		float retVal;
		m_guard.enter();
		retVal = cur_value;
		m_guard.leave();
		return retVal;
	}
};

#define MAX_OUTSTANDING_TRANSFERS	6

typedef void (*FillDataCallback)(void* context, UCHAR *buffer, int& len);
typedef void (*InitDataFunction)();

struct AudioSample4
{
	int left;
	int right;
};

struct ThreeByteSample
{
	char sample[3];
	ThreeByteSample(int val = 0)
	{
		sample[0] = (val >> 16) & 0xff;
		sample[1] = (val >> 0) & 0xff;
		sample[2] = (val >> 8) & 0xff;
	}

	operator int()const
	{
		return (sample[0]) + (sample[1] << 8) + (sample[2] << 16);
	}
};

struct AudioSample3
{
	ThreeByteSample left;
	ThreeByteSample right;
};

class AudioTask :
	public SimpleWorker
{
	// one buffer element
	struct ISOBuffer
	{
		PUCHAR          DataBuffer;
		KOVL_HANDLE     OvlHandle;

		KISO_CONTEXT*   IsoContext;
		KISO_PACKET*    IsoPackets;
	};

	KOVL_POOL_HANDLE    m_OvlPool;
	ULONG               m_DataBufferSize;
	ULONG               m_SubmittedCount;
	ULONG               m_CompletedCount;
	ULONG               m_FrameNumber;
	ULONG				m_LastStartFrame;

	ISOBuffer			m_isoBuffers[MAX_OUTSTANDING_TRANSFERS];
	int					m_outstandingIndex;
	int					m_completedIndex;

	int					m_sampleSize;

	void SetNextFrameNumber(ISOBuffer* buffer)
	{
		buffer->IsoContext->StartFrame = m_FrameNumber;
		m_FrameNumber++;
	}

	void IsoXferComplete(ISOBuffer* buffer, ULONG transferLength)
	{
		m_CompletedCount++;
		m_LastStartFrame = buffer->IsoContext->StartFrame;
	}


	FillDataCallback	m_fillDataFunc;
	InitDataFunction	m_iniDataFunc;

protected:
	// main function
	virtual bool DoWork();

public:
	AudioTask(KUSB_HANDLE hdl, WINUSB_PIPE_INFORMATION* pipeInfo, int ppt, int ps, float defPacketSize, BOOL isRead, int sampleSize);
	~AudioTask(void);

	virtual void Start();
	virtual void Stop();

private:
	KUSB_HANDLE handle;
	WINUSB_PIPE_INFORMATION* gPipeInfo;

	int				m_packetPerTransfer;
	int				m_packetSize;
	float			m_defaultPacketSize;
	
	DWORD			m_errorCode;
	BOOL			m_isReadTask;
};
