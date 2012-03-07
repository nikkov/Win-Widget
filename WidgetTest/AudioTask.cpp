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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "AudioTask.h"

extern void debugPrintf(const char *szFormat, ...);

AudioSample3 dummybuffer3[48];
AudioSample4 dummybuffer4[48];

DWORD globalReadBuffer = 0;
DWORD globalPacketCounter = 0;

void FillBuffer4()
{
	memset(dummybuffer4, 0, 48*sizeof(AudioSample4));
	for(int i = 0; i < 48; i++)
	{
		dummybuffer4[i].left = (int)(0x1FFFFF*sin(2.0*3.14159265358979323846*(double)i/48.));
		dummybuffer4[i].right = dummybuffer4[i].left;

		dummybuffer4[i].left = dummybuffer4[i].left << 8;
		dummybuffer4[i].right = dummybuffer4[i].right << 8;
	}
	globalReadBuffer = 0;
}

void FillData4(void*, UCHAR *buffer, int& len)
{
	AudioSample4 *sampleBuff = (AudioSample4 *)buffer;
	int sampleLength = len / sizeof(AudioSample4);

	for(int i = 0; i < sampleLength; i++)
	{
		sampleBuff[i].left =  dummybuffer4[globalReadBuffer].left;
		sampleBuff[i].right = dummybuffer4[globalReadBuffer].right;
		globalReadBuffer++;
		if(globalReadBuffer >= 48)
			globalReadBuffer = 0;
	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;
}


void FillBuffer3()
{
	memset(dummybuffer3, 0, 48*sizeof(AudioSample3));
	for(int i = 0; i < 48; i++)
	{
		int val = (int)(0x1FFFFF*sin(2.0*3.14159265358979323846*(double)i/48.));
		dummybuffer3[i].left = val;
		dummybuffer3[i].right = dummybuffer3[i].left;

		dummybuffer3[i].left = dummybuffer3[i].left << 8;
		dummybuffer3[i].right = dummybuffer3[i].right << 8;
	}
	globalReadBuffer = 0;
}

void FillData3(void*, UCHAR *buffer, int& len)
{
	AudioSample3 *sampleBuff = (AudioSample3 *)buffer;
	int sampleLength = len / sizeof(AudioSample3);

	for(int i = 0; i < sampleLength; i++)
	{
		sampleBuff[i].left =  dummybuffer3[globalReadBuffer].left;
		sampleBuff[i].right = dummybuffer3[globalReadBuffer].right;
		globalReadBuffer++;
		if(globalReadBuffer >= 48)
			globalReadBuffer = 0;
	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;
}

FeedbackInfo globalFeedbackInfo;

#define NEXT_INDEX(x)		((x + 1) % (sizeof(m_isoBuffers) / sizeof(ISOBuffer)))

AudioTask::AudioTask(KUSB_HANDLE hdl, WINUSB_PIPE_INFORMATION* pipeInfo, int ppt, int ps, float defPacketSize, BOOL isRead, int sampleSize) : 
	handle(hdl), gPipeInfo(pipeInfo), 
		m_packetPerTransfer(ppt), 
		m_packetSize(ps), 
		m_defaultPacketSize(defPacketSize), 
		m_isReadTask(isRead),

		m_SubmittedCount(0),
		m_CompletedCount(0),
		m_FrameNumber(0),
		m_LastStartFrame(0),
		m_outstandingIndex(0),
		m_completedIndex(0),
		m_sampleSize(sampleSize)

{
	m_DataBufferSize = m_packetPerTransfer * m_packetSize;
	if(m_sampleSize == 3)
	{
		m_fillDataFunc = FillData3;
		m_iniDataFunc = FillBuffer3;
	}
	else
		if(m_sampleSize == 4)
		{
			m_fillDataFunc = FillData4;
			m_iniDataFunc = FillBuffer4;
		}
}

AudioTask::~AudioTask(void)
{
}

void AudioTask::Start()
{
	if(IsWork())
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't start AudioTask thread: already started\n");
#endif
		return;
	}

    BOOL r = OvlK_Init(&m_OvlPool, handle, MAX_OUTSTANDING_TRANSFERS, KOVL_POOL_FLAG_NONE);
	if(!r)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't start AudioTask thread: OvlK_Init failed\n");
#endif
		return;
	}

	m_SubmittedCount = 0;
	m_CompletedCount = 0;
	m_FrameNumber = 0;
	m_LastStartFrame = 0;

	for(int i = 0; i < sizeof(m_isoBuffers) / sizeof(ISOBuffer); i++)
	{
        ISOBuffer* bufferEL = m_isoBuffers + i;
        bufferEL->DataBuffer = new UCHAR[m_DataBufferSize];
        memset(bufferEL->DataBuffer, 0xAA, m_DataBufferSize);
        IsoK_Init(&bufferEL->IsoContext, m_packetPerTransfer, 0);
        IsoK_SetPackets(bufferEL->IsoContext, m_packetSize);
        bufferEL->IsoPackets = bufferEL->IsoContext->IsoPackets;
        OvlK_Acquire(&bufferEL->OvlHandle, m_OvlPool);
	}
	m_outstandingIndex = 0;
	m_completedIndex = 0;


    // Reset the pipe.
    UsbK_ResetPipe(handle, (UCHAR)gPipeInfo->PipeId);

	UCHAR policyValue = 1;
	if(!m_isReadTask)
	{
		UsbK_SetPipePolicy(handle, (UCHAR)gPipeInfo->PipeId, ISO_ALWAYS_START_ASAP, 1, &policyValue);
		if(m_iniDataFunc)
			m_iniDataFunc();
	}
	else
	{
		globalFeedbackInfo.SetValue(0);
		// Set a start frame (not used) see KISO_FLAG_SET_START_FRAME.
		//Usb.GetCurrentFrameNumber(handle, &gXfers.FrameNumber);
		//gXfers.FrameNumber += ISO_PACKETS_PER_XFER * 2;
		//gXfers.FrameNumber -= gXfers.FrameNumber % ISO_PACKETS_PER_XFER;
	}
	m_errorCode = ERROR_SUCCESS;

#ifdef _DEBUG
	debugPrintf("ASIOUAC: Start AudioTask thread\n");
#endif
	Sleep(100);
	SimpleWorker::Start();
}

void AudioTask::Stop()
{
	if(!IsWork())
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't stop AudioTask thread: already stopped\n");
#endif
		return;
	}
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Stop AudioTask thread\n");
#endif
	SimpleWorker::Stop();

	UsbK_AbortPipe(handle, gPipeInfo->PipeId);

    //  Cancel all transfers left outstanding.
    while(m_completedIndex != m_outstandingIndex)
    {
        ISOBuffer* nextBufferEL = m_isoBuffers + m_completedIndex;
        ULONG transferred;
		OvlK_WaitOrCancel(nextBufferEL->OvlHandle, 0, &transferred);
		m_completedIndex = NEXT_INDEX(m_completedIndex);
    }
	for(int i = 0; i < sizeof(m_isoBuffers) / sizeof(ISOBuffer); i++)
	{
		//  Free the iso buffer resources.
		ISOBuffer* bufferEL = m_isoBuffers + i;
		OvlK_Release(bufferEL->OvlHandle);
        IsoK_Free(bufferEL->IsoContext);
		bufferEL->IsoContext = NULL;
        delete bufferEL->DataBuffer;
		bufferEL->DataBuffer = NULL;
	}
	m_outstandingIndex = 0;
	m_completedIndex = 0;

    OvlK_Free(m_OvlPool);
}

bool AudioTask::DoWork()
{
	try
	{
		BOOL r;
		ISOBuffer* nextXfer;
		ULONG transferred;
		float nextOffSet = 0;
		int dataLength = 0;
		float cur_feedback = 0;

		while(m_errorCode == ERROR_SUCCESS &&	NEXT_INDEX(m_outstandingIndex) != m_completedIndex)
		{
			nextXfer = m_isoBuffers + m_outstandingIndex;;

			if(!m_isReadTask)
			{
				cur_feedback = 2.0f * globalFeedbackInfo.GetValue(); //value in samples
				if(cur_feedback > m_packetSize)
					cur_feedback = (float)m_packetSize;
				nextOffSet = 0;
				if(cur_feedback > 0)
				{
					int packetIndex;
					for (packetIndex = 0; packetIndex < nextXfer->IsoContext->NumberOfPackets; packetIndex++)
					{
						nextXfer->IsoContext->IsoPackets[packetIndex].Offset = (int)nextOffSet;
						nextOffSet += cur_feedback;
						// trunk offset to integer number of stereo sample
						nextOffSet += (float)m_sampleSize;
						nextOffSet = (float)((int)nextOffSet - ((int)nextOffSet % (2 * m_sampleSize)));
					}
					dataLength = (int)nextOffSet;
					if(m_fillDataFunc)
					{
						m_fillDataFunc(NULL, nextXfer->DataBuffer, dataLength);
					}
				}
				else
				{
					dataLength = (int)(m_defaultPacketSize * (float)m_packetPerTransfer);
/*
					if(isUAC1)
					{
						FillData3(nextXfer->DataBuffer, dataLength);
						printf("Transfer w/o: feedback send %.1f samples, transfer length=%d\r",  (float)dataLength/6.f, dataLength);
					}
					else
					{
						FillData4(nextXfer->DataBuffer, dataLength);
						printf("Transfer: w/o feedback send %.1f samples, transfer length=%d\r",  (float)dataLength/8.f, dataLength);
					}
*/
					memset(nextXfer->DataBuffer, 0, dataLength);
				}
			}

			m_outstandingIndex = NEXT_INDEX(m_outstandingIndex);
			OvlK_ReUse(nextXfer->OvlHandle);

			SetNextFrameNumber(nextXfer);

			if(m_isReadTask)
				r = UsbK_IsoReadPipe(
						  handle,
						  gPipeInfo->PipeId,
						  nextXfer->DataBuffer,
						  m_DataBufferSize,
						  (LPOVERLAPPED)nextXfer->OvlHandle,
						  nextXfer->IsoContext);
			else
			{
				r = UsbK_IsoWritePipe(
						  handle,
						  gPipeInfo->PipeId,
						  nextXfer->DataBuffer,
						  dataLength,
						  (LPOVERLAPPED)nextXfer->OvlHandle,
						  nextXfer->IsoContext);
				//printf("Transfer transfer length=%d, result=%d\n", dataLength, (int)r);
			}

			m_errorCode = GetLastError();
			if (m_errorCode != ERROR_IO_PENDING) 
			{
				printf("IsoReadPipe/IsoWritePipe failed. ErrorCode: %08Xh\n",  m_errorCode);
				return FALSE;
			}
			m_errorCode = ERROR_SUCCESS;
		}

		nextXfer = m_isoBuffers + m_completedIndex;
		r = OvlK_Wait(nextXfer->OvlHandle, 1000, KOVL_WAIT_FLAG_NONE, &transferred);
		if (!r) 
		{
#ifdef _DEBUG
			if(m_isReadTask)
				debugPrintf("ASIOUAC: OvlK_Wait failed while read. ErrorCode: %08Xh\n",  GetLastError());
			else
				debugPrintf("ASIOUAC: OvlK_Wait failed while write. ErrorCode: %08Xh\n",  GetLastError());
#endif
/*
			m_errorCode = GetLastError();
			printf("OvlK_Wait failed. ErrorCode: %08Xh\n",  m_errorCode);
			return FALSE;
*/
		}
		IsoXferComplete(nextXfer, transferred);
		IsoXferComplete(nextXfer, transferred);
		m_completedIndex = NEXT_INDEX(m_completedIndex);

		if(m_isReadTask)
		{
			KISO_PACKET isoPacket = nextXfer->IsoPackets[nextXfer->IsoContext->NumberOfPackets - 1];
			if (isoPacket.Length > 1)
			{
				int feedback = 0;
				if(m_sampleSize == 3)
					feedback = (nextXfer->DataBuffer[isoPacket.Offset]) + (nextXfer->DataBuffer[isoPacket.Offset + 1] << 8) + (nextXfer->DataBuffer[isoPacket.Offset + 2] << 16);
				else
					if(m_sampleSize == 3)
						feedback = *((int*)(nextXfer->DataBuffer + isoPacket.Offset));
				globalFeedbackInfo.SetValue(feedback);
				//printf("New feedback value: %f\n",  globalFeedbackInfo.GetValue());
			}
		}
	}
	catch (...)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Catch exception in DeviceManager::DoWork()!\n");
#endif
	}
	return m_errorCode == ERROR_SUCCESS;
}

