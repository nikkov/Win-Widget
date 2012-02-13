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
#include "primitives.h"
#include "libusbk.h"


class AsioUAC2;

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

class AudioTask :
	public SimpleWorker
{
	//один элемент-буфер
	typedef struct _MY_ISO_BUFFER_EL
	{
		PUCHAR          DataBuffer;
		KOVL_HANDLE     OvlHandle;

		KISO_CONTEXT*   IsoContext;
		KISO_PACKET*    IsoPackets;

		struct _MY_ISO_BUFFER_EL* prev;
		struct _MY_ISO_BUFFER_EL* next;

	} MY_ISO_BUFFER_EL, *PMY_ISO_BUFFER_EL;

	//список буферов
	typedef struct _MY_ISO_XFERS
	{
		KOVL_POOL_HANDLE    OvlPool;
		PMY_ISO_BUFFER_EL   BufferList;

		ULONG               DataBufferSize;

		PMY_ISO_BUFFER_EL   Outstanding;
		PMY_ISO_BUFFER_EL   Completed;

		ULONG               SubmittedCount;
		ULONG               CompletedCount;

		ULONG               FrameNumber;

		ULONG LastStartFrame;

	} MY_ISO_XFERS, *PMY_ISO_XFERS;


	//! Adds an element to the end of a linked list.
	/*!
	* \param head
	* First element of the list.
	*
	* \param add
	* Element to add.
	*/
	void DL_APPEND(PMY_ISO_BUFFER_EL &head, PMY_ISO_BUFFER_EL &add);

	//! Removes an element from a linked list.
	/*!
	*
	* \param head
	* First element of the list.
	*
	* \param del
	* Element to remove.
	*
	* \attention
	* \c DL_DELETE does not free or de-allocate memory.
	* It "de-links" the element specified by \c del from the list.
	*/
	void DL_DELETE(PMY_ISO_BUFFER_EL &head,PMY_ISO_BUFFER_EL &del);

	VOID SetNextFrameNumber(PMY_ISO_XFERS myXfers, PMY_ISO_BUFFER_EL myBufferEL)
	{
		myBufferEL->IsoContext->StartFrame = myXfers->FrameNumber;
		myXfers->FrameNumber = myXfers->FrameNumber + (8);
	}

	/*
	Reports isochronous packet information.
	*/
	void IsoXferComplete(PMY_ISO_XFERS myXfers, PMY_ISO_BUFFER_EL myBufferEL, ULONG transferLength);
	void FillData(UCHAR *buffer, int len);


protected:
	// основная рабочая функция потока
	virtual bool DoWork();

public:
	AudioTask(AsioUAC2 *asioDrv, KUSB_HANDLE hdl, WINUSB_PIPE_INFORMATION* pipeInfo, int ppt, int ps, float defPacketSize, BOOL isRead);
	~AudioTask(void);

	virtual void Start();
	virtual void Stop();

private:
	KUSB_HANDLE handle;
	MY_ISO_XFERS gXfers;
	WINUSB_PIPE_INFORMATION* gPipeInfo;
	int packetPerTransfer;
	int packetSize;
	float defaultPacketSize;
	DWORD errorCode;
	BOOL isReadTask;

	int nextFrameSize;

	AsioUAC2* asioDriver;
};
