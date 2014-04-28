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

#define DRIVER_VERSION		0x00000002L
#define SAMPLE_RATE_INITIAL 100

enum
{
	USBAudioClassUnknown = 0,
	USBAudioClass1,
	USBAudioClass2
};



#if WINDOWS

#include "rpc.h"
#include "rpcndr.h"
#ifndef COM_NO_WINDOWS_H
#include <windows.h>
#include "ole2.h"
#endif

#include "combase.h"
#include "iasiodrv.h"
#include "USBAudioDevice.h"


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

	void DeviceNotify(int reason);

	void bufferSwitch ();
	long getMilliSeconds () {return milliSeconds;}

	template <typename T_SRC, typename T_DST> void FillInputData(UCHAR *buffer, int& len);
	template <typename T_SRC, typename T_DST> void FillOutputData(UCHAR *buffer, int& len);
	
	static void sFillOutputData3(void* context, UCHAR *buffer, int& len);
	static void sFillInputData3(void* context, UCHAR *buffer, int& len);
	static void sFillOutputData4(void* context, UCHAR *buffer, int& len);
	static void sFillInputData4(void* context, UCHAR *buffer, int& len);

	static void sDeviceNotify(void* context, int reason);

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

	int m_NumInputs;
	int m_NumOutputs;
	int m_inputSampleSize;
	int m_outputSampleSize;
	char **inputBuffers;
	char **outputBuffers;
	long *inMap;
	long *outMap;

	long blockFrames;
	long inputLatency;
	long outputLatency;
	long activeInputs;
	long activeOutputs;
	long toggle;
	long milliSeconds;
	bool active, started;
	bool timeInfoMode, tcRead;
	char errorMessage[128];

private:
	int USBAudioClass;

	USBAudioDevice *m_device;

	ASIOError StartDevice();
	ASIOError StopDevice();
	int currentOutBufferPosition;
	int currentInBufferPosition;

	volatile bool	m_StopInProgress;
	HANDLE	m_AsioSyncEvent;
	HANDLE	m_BufferSwitchEvent;
};

#endif

