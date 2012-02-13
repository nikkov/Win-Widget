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
// Based on samples from ASIO SDK
/*
	Steinberg Audio Stream I/O API
	(c) 1999, Steinberg Soft- und Hardware GmbH
*/


#ifndef _asiosmpl_
#define _asiosmpl_

#include "asiosys.h"

#define DRIVER_VERSION		0x00000001L
#define NO_INPUTS
//#define EMULATION_HARDWARE

enum
{
	kBlockFrames = 512,
#ifndef NO_INPUTS
	kNumInputs = 2,
#endif
	kNumOutputs = 2
};

#define WIDGET_VID			0x16C0
#define WIDGET_PID			0x03E8


#define DEFAULT_SAMPLE_RATE	44100.
#define AUDIO_CTRL_IFACE_NUM	1
#define CLOCK_SOURCE_ID			5
#define EP_TRANSFER_OUT         0x02
#define EP_TRANSFER_FEEDBACK    0x81

#if WINDOWS

#include "rpc.h"
#include "rpcndr.h"
#ifndef COM_NO_WINDOWS_H
#include <windows.h>
#include "ole2.h"
#endif

#include "combase.h"
#include "iasiodrv.h"
#include "libusbk.h"
#include "usb_audio.h"


class AudioTask;

class AsioUAC2 : public IASIO, public CUnknown
{
public:
	AsioUAC2(LPUNKNOWN pUnk, HRESULT *phr);
	~AsioUAC2();

	DECLARE_IUNKNOWN
    //STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {      \
    //    return GetOwner()->QueryInterface(riid,ppv);            \
    //};                                                          \
    //STDMETHODIMP_(ULONG) AddRef() {                             \
    //    return GetOwner()->AddRef();                            \
    //};                                                          \
    //STDMETHODIMP_(ULONG) Release() {                            \
    //    return GetOwner()->Release();                           \
    //};

	// Factory method
	static CUnknown *CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFIID riid,void **ppvObject);
#else

#include "asiodrvr.h"

//---------------------------------------------------------------------------------------------
class AsioUAC2 : public AsioDriver
{
public:
	AsioUAC2 ();
	~AsioUAC2 ();
#endif

	ASIOBool init (void* sysRef);
	void getDriverName (char *name);	// max 32 bytes incl. terminating zero
	long getDriverVersion ();
	void getErrorMessage (char *string);	// max 128 bytes incl.

	ASIOError start ();
	ASIOError stop ();

	ASIOError getChannels (long *numInputChannels, long *numOutputChannels);
	ASIOError getLatencies (long *inputLatency, long *outputLatency);
	ASIOError getBufferSize (long *minSize, long *maxSize,
		long *preferredSize, long *granularity);

	ASIOError canSampleRate (ASIOSampleRate sampleRate);
	ASIOError getSampleRate (ASIOSampleRate *sampleRate);
	ASIOError setSampleRate (ASIOSampleRate sampleRate);
	ASIOError getClockSources (ASIOClockSource *clocks, long *numSources);
	ASIOError setClockSource (long index);

	ASIOError getSamplePosition (ASIOSamples *sPos, ASIOTimeStamp *tStamp);
	ASIOError getChannelInfo (ASIOChannelInfo *info);

	ASIOError createBuffers (ASIOBufferInfo *bufferInfos, long numChannels,
		long bufferSize, ASIOCallbacks *callbacks);
	ASIOError disposeBuffers ();

	ASIOError controlPanel ();
	ASIOError future (long selector, void *opt);
	ASIOError outputReady ();

	void bufferSwitch ();
	long getMilliSeconds () {return milliSeconds;}

	void FillData(UCHAR *buffer, int len);

private:
friend void myTimer();

	bool inputOpen ();
	void inputClose ();
	void input ();

	bool outputOpen ();
	void outputClose ();
	void output ();

#ifdef EMULATION_HARDWARE
	void timerOn ();
	void timerOff ();
#endif
	void bufferSwitchX ();

	double samplePosition;
#ifdef EMULATION_HARDWARE
public:
#endif
	double sampleRate;
#ifdef EMULATION_HARDWARE
private:
#endif
	ASIOCallbacks *callbacks;
	ASIOTime asioTime;
	ASIOTimeStamp theSystemTime;
#ifndef NO_INPUTS
	int *inputBuffers[kNumInputs * 2];
#endif
	int *outputBuffers[kNumOutputs * 2];
#ifndef NO_INPUTS
	long inMap[kNumInputs];
#endif
	long outMap[kNumOutputs];
	long blockFrames;
	long inputLatency;
	long outputLatency;
#ifndef NO_INPUTS
	long activeInputs;
#endif
	long activeOutputs;
	long toggle;
	long milliSeconds;
	bool active, started;
	bool timeInfoMode, tcRead;
	char errorMessage[128];

private:
	KLST_HANDLE deviceList;
	KUSB_HANDLE handle;
	KLST_DEVINFO_HANDLE deviceInfo;
	USB_DEVICE_DESCRIPTOR deviceDescriptor;
	USB_CONFIGURATION_DESCRIPTOR configurationDescriptor;
	WINUSB_PIPE_INFORMATION     gPipeInfoFeedback;
	WINUSB_PIPE_INFORMATION     gPipeInfoWrite;

	int audioControlInterfaceNum;
	int audioControlStreamNum;
	int audioControlStreamAltNum;
	int clockSourceId;
	int outEndpointNum;
	int feedbackEndpointNum;

	int currentBufferPosition;

	ASIOBool SendUsbControl(KUSB_HANDLE handle, int interfaceNumber, 
				   int dir, int type, int recipient, int request, int value, int index,
				   unsigned char *buff, int size, ULONG *lengthTransferred);

	int GetCurrentFreq(KUSB_HANDLE handle, int interfaceNumber, int clockId);
	ASIOBool SetCurrentFreq(KUSB_HANDLE handle, int interfaceNumber, int clockId, int freq);

	ASIOError StartDevice();
	ASIOError StopDevice();

	AudioTask* outputTask;
	AudioTask* feedbackTask;
};

#endif

