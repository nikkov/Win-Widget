#pragma once
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

protected:
	// основная рабочая функция потока
	virtual bool DoWork();

public:
	AudioTask(KUSB_HANDLE hdl, WINUSB_PIPE_INFORMATION* pipeInfo, int ppt, int ps, float defPacketSize, BOOL isRead);
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
};
