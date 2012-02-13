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
#include "asiouac2.h"

#define MAX_OUTSTANDING_TRANSFERS   6

extern void debugPrintf(const char *szFormat, ...);

struct AudioSample
{
	int left;
	int right;
};


FeedbackInfo globalFeedbackInfo;

AudioTask::AudioTask(AsioUAC2 *asioDrv, KUSB_HANDLE hdl, WINUSB_PIPE_INFORMATION* pipeInfo, int ppt, int ps, float defPacketSize, BOOL isRead) : 
	asioDriver(asioDrv), handle(hdl), gPipeInfo(pipeInfo), 
		packetPerTransfer(ppt), packetSize(ps), defaultPacketSize(defPacketSize), isReadTask(isRead)
{
	memset(&gXfers, 0, sizeof(gXfers));
    gXfers.DataBufferSize = packetPerTransfer * packetSize;
}

AudioTask::~AudioTask(void)
{
}

void AudioTask::DL_APPEND(PMY_ISO_BUFFER_EL &head, PMY_ISO_BUFFER_EL &add)
{
	if (head) 
	{
		add->prev = head->prev;
		head->prev->next = add;
		head->prev = add;      
		add->next = NULL;        
	} else 
	{      
		head=add;
		head->prev = head;
		head->next = NULL;
	}                       
}


void AudioTask::DL_DELETE(PMY_ISO_BUFFER_EL &head,PMY_ISO_BUFFER_EL &del)
{                      
	if (del->prev == del)
	{
		head = NULL;
	}
	else
		if (del == head)
		{
			if(del->next)
				del->next->prev = del->prev;
			head = del->next;
		}
		else
		{
			del->prev->next = del->next;
			if (del->next)
			{
				del->next->prev = del->prev;
			}
			else
			{
				head->prev = del->prev;
			}
		}
}

/*
Reports isochronous packet information.
*/
void AudioTask::IsoXferComplete(PMY_ISO_XFERS myXfers, PMY_ISO_BUFFER_EL myBufferEL, ULONG transferLength)
{
	myXfers->CompletedCount++;
	myXfers->LastStartFrame = myBufferEL->IsoContext->StartFrame;
}


void AudioTask::Start()
{
	if(IsWork())
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't start AudioTask thread: already started");
#endif
		return;
	}

    BOOL r = OvlK_Init(&gXfers.OvlPool, handle, MAX_OUTSTANDING_TRANSFERS, KOVL_POOL_FLAG_NONE);
	if(!r)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't start AudioTask thread: OvlK_Init failed");
#endif
		return;
	}

    for (int pos = 0; pos < MAX_OUTSTANDING_TRANSFERS; pos++)
    {
        PMY_ISO_BUFFER_EL bufferEL = (PMY_ISO_BUFFER_EL)malloc(sizeof(MY_ISO_BUFFER_EL));
        memset(bufferEL, 0, sizeof(*bufferEL));
		
        bufferEL->DataBuffer = (PUCHAR)malloc(gXfers.DataBufferSize);
        memset(bufferEL->DataBuffer, 0xAA, gXfers.DataBufferSize);

        IsoK_Init(&bufferEL->IsoContext, packetPerTransfer, 0);
        IsoK_SetPackets(bufferEL->IsoContext, packetSize);

        //bufferEL->IsoContext->Flags = KISO_FLAG_SET_START_FRAME;

        bufferEL->IsoPackets = bufferEL->IsoContext->IsoPackets;
        OvlK_Acquire(&bufferEL->OvlHandle, gXfers.OvlPool);

        DL_APPEND(gXfers.BufferList, bufferEL);
        DL_APPEND(gXfers.Completed, bufferEL);
    }
    // Reset the pipe.
    UsbK_ResetPipe(handle, (UCHAR)gPipeInfo->PipeId);

	nextFrameSize = packetSize;

	UCHAR policyValue = 1;
	if(!isReadTask)
	{
		UsbK_SetPipePolicy(handle, (UCHAR)gPipeInfo->PipeId, ISO_ALWAYS_START_ASAP, 1, &policyValue);
	}
	else
	{
		globalFeedbackInfo.SetValue(0);
		// Set a start frame (not used) see KISO_FLAG_SET_START_FRAME.
		//UsbK_GetCurrentFrameNumber(handle, &gXfers.FrameNumber);
		//gXfers.FrameNumber += ISO_PACKETS_PER_XFER * 2;
		//gXfers.FrameNumber -= gXfers.FrameNumber % ISO_PACKETS_PER_XFER;
	}
	errorCode = ERROR_SUCCESS;

#ifdef _DEBUG
		debugPrintf("ASIOUAC: Start AudioTask thread");
#endif
	Sleep(100);
	SimpleWorker::Start();
}

void AudioTask::Stop()
{
	if(!IsWork())
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't stop AudioTask thread: already stopped");
#endif
		return;
	}
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Stop AudioTask thread");
#endif

	SimpleWorker::Stop();

	UsbK_AbortPipe(handle, gPipeInfo->PipeId);

    //  Cancel all transfers left outstanding.
    while(gXfers.Outstanding)
    {
        PMY_ISO_BUFFER_EL nextBufferEL = gXfers.Outstanding;
        ULONG transferred;

        OvlK_WaitOrCancel(nextBufferEL->OvlHandle, 0, &transferred);
        DL_DELETE(gXfers.Outstanding, nextBufferEL);
    }

    //  Free the iso buffer resources.
    while(gXfers.BufferList)
    {
        PMY_ISO_BUFFER_EL nextBufferEL = gXfers.BufferList;
        DL_DELETE(gXfers.BufferList, nextBufferEL);

        OvlK_Release(nextBufferEL->OvlHandle);
        IsoK_Free(nextBufferEL->IsoContext);
        free(nextBufferEL->DataBuffer);
        free(nextBufferEL);
    }

    // Free the overlapped pool.
    OvlK_Free(gXfers.OvlPool);

#ifdef _DEBUG
	debugPrintf("ASIOUAC: AudioTask buffer free OK!");
#endif
}

bool AudioTask::DoWork()
{
	try
	{
		BOOL r;
		PMY_ISO_BUFFER_EL nextXfer;
		ULONG transferred;
		float nextOffSet = 0;
		int dataLength = 0;

		while(errorCode == ERROR_SUCCESS &&	gXfers.Completed && !m_exitFlag)
		{
			if(nextFrameSize == packetSize)
				nextFrameSize -= 8;
			else
				nextFrameSize += 8;

			nextXfer = gXfers.Completed;

			if(!isReadTask)
			{
				float cur_feedback = 2.0f * globalFeedbackInfo.GetValue(); //value in samples
				if(cur_feedback > (float)packetSize)
					cur_feedback = (float)packetSize;
				nextOffSet = 0;
				if(cur_feedback > 0)
				{
					int packetIndex;
					for (packetIndex = 0; packetIndex < nextXfer->IsoContext->NumberOfPackets; packetIndex++)
					{
						nextXfer->IsoContext->IsoPackets[packetIndex].Offset = (int)nextOffSet;
						nextOffSet += cur_feedback;
						// trunk offset to integer number of stereo sample
						nextOffSet += 4.f;
						nextOffSet = (float)((int)nextOffSet - ((int)nextOffSet % 8));
					}
					dataLength = (int)nextOffSet;
					//dataLength = (int)nextOffSet + 4;
					//dataLength = dataLength - (dataLength % 8);

					asioDriver->FillData(nextXfer->DataBuffer, dataLength);
#ifdef _DEBUG
					//debugPrintf("ASIOUAC: Transfer: feedback val = %.1f, send %.1f samples, transfer length=%d",  cur_feedback, (float)dataLength/8.f, dataLength);
#endif
				}
				else
				{
					dataLength = (int)(defaultPacketSize * packetPerTransfer);
					memset(nextXfer->DataBuffer, 0, dataLength);
				}
			}

			DL_DELETE(gXfers.Completed, nextXfer);
			DL_APPEND(gXfers.Outstanding, nextXfer);
			OvlK_ReUse(nextXfer->OvlHandle);

			SetNextFrameNumber(&gXfers, nextXfer);

			if(isReadTask)
				r = UsbK_IsoReadPipe(
						  handle,
						  gPipeInfo->PipeId,
						  nextXfer->DataBuffer,
						  gXfers.DataBufferSize,
						  (LPOVERLAPPED)nextXfer->OvlHandle,
						  nextXfer->IsoContext);
			else
				r = UsbK_IsoWritePipe(
						  handle,
						  gPipeInfo->PipeId,
						  nextXfer->DataBuffer,
						  dataLength, //packetPerTransfer * nextFrameSize,
						  (LPOVERLAPPED)nextXfer->OvlHandle,
						  nextXfer->IsoContext);

			errorCode = GetLastError();
			if (errorCode != ERROR_IO_PENDING) 
			{
#ifdef _DEBUG
			if(isReadTask)
				debugPrintf("ASIOUAC: IsoReadPipe failed. ErrorCode: %08Xh",  errorCode);
			else
				debugPrintf("ASIOUAC: IsoWritePipe failed. ErrorCode: %08Xh",  errorCode);
#endif
				return FALSE;
			}
			errorCode = ERROR_SUCCESS;
		}

		nextXfer = gXfers.Outstanding;
		if (!nextXfer || m_exitFlag) 
		{
#ifdef _DEBUG
			debugPrintf("ASIOUAC: No more packets!");
#endif
			return TRUE;
		}

		r = OvlK_Wait(nextXfer->OvlHandle, 100, KOVL_WAIT_FLAG_NONE, &transferred);
		if (!r) 
		{
#ifdef _DEBUG
			if(isReadTask)
				debugPrintf("ASIOUAC: OvlK_Wait failed while read. ErrorCode: %08Xh",  GetLastError());
			else
				debugPrintf("ASIOUAC: OvlK_Wait failed while write. ErrorCode: %08Xh",  GetLastError());
#endif

			//errorCode = GetLastError();
			//printf("OvlK_Wait failed. ErrorCode: %08Xh\n",  errorCode);
			//return FALSE;
		}
		DL_DELETE(gXfers.Outstanding, nextXfer);
		DL_APPEND(gXfers.Completed, nextXfer);

		IsoXferComplete(&gXfers, nextXfer, transferred);
		if(isReadTask)
		{
			KISO_PACKET isoPacket = nextXfer->IsoPackets[nextXfer->IsoContext->NumberOfPackets - 1];
			if (isoPacket.Length > 1)
			{
				int feedback = *((int*)(nextXfer->DataBuffer + isoPacket.Offset));
				globalFeedbackInfo.SetValue(feedback);
				//printf("New feedback value: %d\n",  feedback);
			}
		}
	}
	catch (...)
	{
#ifdef _DEBUG
			debugPrintf("ASIOUAC: Catch exception in DeviceManager::DoWork()!");
#endif
	}
	return errorCode == ERROR_SUCCESS;
}

