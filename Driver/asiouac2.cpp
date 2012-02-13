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
	(c) 1996, Steinberg Soft- und Hardware GmbH
	charlie (May 1996)
*/

#include <stdio.h>
#include <string.h>
#include "asiouac2.h"
#include "AudioTask.h"


void debugPrintf(const char *szFormat, ...)
{
    char str[4096];
    va_list argptr;
    va_start(argptr, szFormat);
    vsprintf(str, szFormat, argptr);
    va_end(argptr);

    OutputDebugString(str);
}



//------------------------------------------------------------------------------------------

// extern
void getNanoSeconds(ASIOTimeStamp *time);

// local

double AsioUAC2s2double (ASIOSamples* samples);

static const double twoRaisedTo32 = 4294967296.;
static const double twoRaisedTo32Reciprocal = 1. / twoRaisedTo32;

//---------------------------------------------------------------------------------------------
double AsioUAC2s2double (ASIOSamples* samples)
{
	double a = (double)(samples->lo);
	if (samples->hi)
		a += (double)(samples->hi) * twoRaisedTo32;
	return a;
}

//------------------------------------------------------------------------------------------
// on windows, we do the COM stuff.

#if WINDOWS
#include "windows.h"
#include "mmsystem.h"


const char driverDescriptionLong[] = "ASIO USB Audio Class 2 Driver";
const char driverDescriptionShort[] = "ASIO UAC2";
const char driverDllName[] = "asiouac2.dll";

// class id
// {CCB9056B-7B74-4677-8BDE-65E42ADB7A6A}
CLSID IID_ASIO_DRIVER = { 0xccb9056b, 0x7b74, 0x4677, { 0x8b, 0xde, 0x65, 0xe4, 0x2a, 0xdb, 0x7a, 0x6a } };

CFactoryTemplate g_Templates[1] = {
    {L"ASIOUAC2", &IID_ASIO_DRIVER, AsioUAC2::CreateInstance} 
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

CUnknown* AsioUAC2::CreateInstance (LPUNKNOWN pUnk, HRESULT *phr)
{
	return (CUnknown*)new AsioUAC2 (pUnk,phr);
};

STDMETHODIMP AsioUAC2::NonDelegatingQueryInterface (REFIID riid, void ** ppv)
{
	if (riid == IID_ASIO_DRIVER)
	{
		return GetInterface (this, ppv);
	}
	return CUnknown::NonDelegatingQueryInterface (riid, ppv);
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//		Register ASIO Driver
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
extern LONG RegisterAsioDriver (CLSID,char *,char *,char *,char *);
extern LONG UnregisterAsioDriver (CLSID,char *,char *);

//
// Server registration, called on REGSVR32.EXE "the dllname.dll"
//
HRESULT _stdcall DllRegisterServer()
{
	LONG	rc;
	char	errstr[128];

	rc = RegisterAsioDriver (IID_ASIO_DRIVER, (char*)driverDllName,(char*) driverDescriptionLong, (char*)driverDescriptionShort, "Apartment");

	if (rc) {
		memset(errstr, 0, sizeof(errstr));
		sprintf(errstr, "Register Server failed ! (%d)", rc);
		MessageBox(0,(LPCTSTR)errstr, (LPCTSTR)driverDescriptionLong, MB_OK);
		return -1;
	}

	return S_OK;
}

//
// Server unregistration
//
HRESULT _stdcall DllUnregisterServer()
{
	LONG	rc;
	char	errstr[128];

	rc = UnregisterAsioDriver (IID_ASIO_DRIVER, (char*)driverDllName, (char*)driverDescriptionLong);

	if (rc) {
		memset(errstr, 0, sizeof(errstr));
		sprintf(errstr,"Unregister Server failed ! (%d)",rc);
		MessageBox(0,(LPCTSTR)errstr, (LPCTSTR)driverDescriptionLong, MB_OK);
		return -1;
	}

	return S_OK;
}

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
AsioUAC2::AsioUAC2 (LPUNKNOWN pUnk, HRESULT *phr)
	: CUnknown("ASIOUAC2", pUnk, phr)

//------------------------------------------------------------------------------------------

#else

// when not on windows, we derive from AsioDriver
AsioUAC2::AsioUAC2 () : AsioDriver ()

#endif
{
	long i;
//---------------- USB related ---------------------//
	deviceList = NULL;
	handle = NULL;
	deviceInfo = NULL;

	outputTask = NULL;
	feedbackTask = NULL;

	memset(&deviceDescriptor, 0, sizeof(USB_DEVICE_DESCRIPTOR));
	memset(&configurationDescriptor, 0, sizeof(USB_CONFIGURATION_DESCRIPTOR));
	memset(&gPipeInfoFeedback, 0, sizeof(WINUSB_PIPE_INFORMATION));
	memset(&gPipeInfoWrite, 0, sizeof(WINUSB_PIPE_INFORMATION));

// ASIO
	//block number by default
	blockFrames = kBlockFrames;

	inputLatency = blockFrames;		// typically
	outputLatency = blockFrames * 2;
	
	// typically blockFrames * 2; try to get 1 by offering direct buffer
	// access, and using asioPostOutput for lower latency
	samplePosition = 0;
	sampleRate = DEFAULT_SAMPLE_RATE;
	milliSeconds = (long)((double)(kBlockFrames * 1000) / sampleRate);
	active = false;
	started = false;
	timeInfoMode = false;
	tcRead = false;
#ifndef NO_INPUTS
	for (i = 0; i < kNumInputs; i++)
	{
		inputBuffers[i] = 0;
		inMap[i] = 0;
	}
#endif
	for (i = 0; i < kNumOutputs; i++)
	{
		outputBuffers[i] = 0;
		outMap[i] = 0;
	}
	callbacks = 0;
#ifndef NO_INPUTS
	activeInputs = 
#endif
	activeOutputs = 0;
	toggle = 0;
	//read position in buffer
	currentBufferPosition = 0;
}

//------------------------------------------------------------------------------------------
AsioUAC2::~AsioUAC2 ()
{
	stop ();
	outputClose ();
	inputClose ();
	disposeBuffers ();

    // Close the device handle
    // if handle is invalid (NULL), has no effect
    UsbK_Free(handle);

    // Free the device list
    // if deviceList is invalid (NULL), has no effect
    LstK_Free(deviceList);
}

//------------------------------------------------------------------------------------------
void AsioUAC2::getDriverName (char *name)
{
	strcpy (name, driverDescriptionLong);
}

//------------------------------------------------------------------------------------------
long AsioUAC2::getDriverVersion ()
{
	return DRIVER_VERSION;
}

//------------------------------------------------------------------------------------------
void AsioUAC2::getErrorMessage (char *string)
{
	strcpy (string, errorMessage);
}

//------------------------------------------------------------------------------------------
ASIOBool AsioUAC2::init (void* sysRef)
{
	KLST_DEVINFO_HANDLE tmpDeviceInfo = NULL;
	ULONG deviceCount = 0;
    UCHAR interfaceIndex = (UCHAR) - 1;
	USB_INTERFACE_DESCRIPTOR interfaceDescriptor;

#ifdef _DEBUG
	debugPrintf("ASIOUAC: ASIO driver init!");
#endif

	sysRef = sysRef;
	if (active)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: ASIO already active!");
#endif
		return true;
	}

	strcpy (errorMessage, "ASIO Driver open Failure!");
//	libusbk initialize
	// Get the device list
	if (!LstK_Init(&deviceList, KLST_FLAG_NONE))
	{
		strcpy (errorMessage, "Error initializing libusbk device list!");
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Error initializing libusbk device list!");
#endif
		return false;
	}

	LstK_Count(deviceList, &deviceCount);
	if (!deviceCount)
	{
		strcpy (errorMessage, "No connected libusbk device!");
#ifdef _DEBUG
		debugPrintf("ASIOUAC: No connected libusbk device!");
#endif
		SetLastError(ERROR_DEVICE_NOT_CONNECTED);
		// If LstK_Init returns TRUE, the list must be freed.
		LstK_Free(deviceList);
		deviceList = NULL;
		return false;
	}

	LstK_MoveReset(deviceList);
    while(LstK_MoveNext(deviceList, &tmpDeviceInfo))
    {
        if (tmpDeviceInfo->Common.Vid == WIDGET_VID &&
                tmpDeviceInfo->Common.Pid == WIDGET_PID)
        {
			if(!_stricmp(tmpDeviceInfo->Service, "libusbK") && tmpDeviceInfo->Connected)
			{
				deviceInfo = tmpDeviceInfo;
				break;
			}
        }
    }
	if (!deviceInfo)
	{
		sprintf(errorMessage, "Device vid/pid %04X/%04X not found!", WIDGET_VID, WIDGET_PID);
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Device vid/pid %04X/%04X not found!", WIDGET_VID, WIDGET_PID);
#endif
		// If LstK_Init returns TRUE, the list must be freed.
		LstK_Free(deviceList);
		deviceList = NULL;
		return false;
	}
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Found device vid/pid %04X/%04X!", WIDGET_VID, WIDGET_PID);
#endif

    // Initialize the device with the "dynamic" Open function
    if (!UsbK_Init(&handle, deviceInfo))
    {
        sprintf(errorMessage, "Usb init failed. ErrorCode: %08Xh",  GetLastError());
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Usb init failed. ErrorCode: %08Xh",  GetLastError());
#endif
		// If LstK_Init returns TRUE, the list must be freed.
		LstK_Free(deviceList);
		deviceList = NULL;
		return false;
    }

//todo: load this values from descriptors
	audioControlInterfaceNum = AUDIO_CTRL_IFACE_NUM;
	clockSourceId = CLOCK_SOURCE_ID;
	outEndpointNum = EP_TRANSFER_OUT;
	feedbackEndpointNum = EP_TRANSFER_FEEDBACK;

	SetCurrentFreq(handle, audioControlInterfaceNum, clockSourceId, (int)sampleRate);
	sampleRate = GetCurrentFreq(handle, audioControlInterfaceNum, clockSourceId);
	if(sampleRate == 0.)
	{
		strcpy(errorMessage, "Error getting samplerate from device!");
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Error getting samplerate from device!");
#endif
		// Close the device handle
		UsbK_Free(handle);
		handle = NULL;
		// If LstK_Init returns TRUE, the list must be freed.
		LstK_Free(deviceList);
		deviceList = NULL;
		return false;
	}
#ifdef _DEBUG
	else
		debugPrintf("ASIOUAC: Set samplerate %d", (int)sampleRate);
#endif

	//found endpoints
    while(gPipeInfoFeedback.PipeId == 0 && gPipeInfoWrite.PipeId == 0 && 
		UsbK_SelectInterface(handle, ++interfaceIndex, TRUE))
    {
        memset(&interfaceDescriptor, 0, sizeof(interfaceDescriptor));
        UCHAR gAltsettingNumber = (UCHAR) - 1;
        while(UsbK_QueryInterfaceSettings(handle, ++gAltsettingNumber, &interfaceDescriptor))
        {
            UCHAR pipeIndex = (UCHAR) - 1;
            while(UsbK_QueryPipe(handle, gAltsettingNumber, ++pipeIndex, &gPipeInfoFeedback))
            {
                if (gPipeInfoFeedback.PipeId != feedbackEndpointNum || gPipeInfoFeedback.PipeType != UsbdPipeTypeIsochronous)
	                memset(&gPipeInfoFeedback, 0, sizeof(gPipeInfoFeedback));
				else
					break;
            }
            pipeIndex = (UCHAR) - 1;
            while(UsbK_QueryPipe(handle, gAltsettingNumber, ++pipeIndex, &gPipeInfoWrite))
            {
                if (gPipeInfoWrite.PipeId != outEndpointNum || gPipeInfoWrite.PipeType != UsbdPipeTypeIsochronous)
	                memset(&gPipeInfoWrite, 0, sizeof(gPipeInfoWrite));
				else
					break;
            }
            if (gPipeInfoFeedback.PipeId && gPipeInfoWrite.PipeId) break;
            memset(&interfaceDescriptor, 0, sizeof(interfaceDescriptor));
        }
    }

    if (!gPipeInfoFeedback.PipeId)
    {
		strcpy(errorMessage, "Input pipe not found!");
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Input pipe not found!");
#endif
        return false;
    }
    if (!gPipeInfoWrite.PipeId)
    {
		strcpy(errorMessage, "Output pipe not found!");
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Output pipe not found!");
#endif
        return false;
    }
	audioControlStreamNum = interfaceDescriptor.bInterfaceNumber;
	audioControlStreamAltNum = interfaceDescriptor.bAlternateSetting;
//	end libusbk initialize

	if (inputOpen ())
	{
		if (outputOpen ())
		{
			active = true;
			return true;
		}
		else
		{
#ifdef _DEBUG
			debugPrintf("ASIOUAC: Output open error!!");
#endif
		}
	}
	else
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Input open error!!");
#endif
	}
	//timerOff ();		// de-activate 'hardware'
	
	StopDevice();

	outputClose ();
	inputClose ();
	return false;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::start ()
{
	if(started)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't start device: already started!");
#endif
		return ASE_OK; //ASE_InvalidMode;
	}

	if (callbacks)
	{
		started = false;
		samplePosition = 0;
		theSystemTime.lo = theSystemTime.hi = 0;
		toggle = 0;
		currentBufferPosition = 0;

#ifdef EMULATION_HARDWARE
		timerOn ();		
		started = true;
		return ASE_OK;
#else
		// activate hardware
		ASIOError retVal = StartDevice();
		if(retVal == ASE_OK)
		{
#ifdef _DEBUG
			debugPrintf("ASIOUAC: Device started successfully!");
#endif
			started = true;
		}

		return retVal;
#endif
	}
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Can't start device: callbacks is NULL!");
#endif
	return ASE_NotPresent;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::stop ()
{
	if(!started)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't stop device: already stoped!");
#endif
		return ASE_OK;//ASE_InvalidMode;
	}

#ifdef EMULATION_HARDWARE
	timerOff ();		
	started = false;
	return ASE_OK;
#else
	// de-activate hardware
	ASIOError retVal = StopDevice();
	if(retVal == ASE_OK)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Device stoped successfully!");
#endif
		started = false;
	}
	return retVal;
#endif
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::getChannels (long *numInputChannels, long *numOutputChannels)
{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: getChannels request");
#endif
#ifndef NO_INPUTS
	*numInputChannels = kNumInputs;
#else
	*numInputChannels = 0;
#endif
	*numOutputChannels = kNumOutputs;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::getLatencies (long *_inputLatency, long *_outputLatency)
{
	*_inputLatency = inputLatency;
	*_outputLatency = outputLatency;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::getBufferSize (long *minSize, long *maxSize,
	long *preferredSize, long *granularity)
{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: getBufferSize request");
#endif
	*minSize = *maxSize = *preferredSize = blockFrames;		// allow this size only
	*granularity = 0;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::canSampleRate (ASIOSampleRate sampleRate)
{
	int iSampleRate = (int)sampleRate;
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Checked samplerate %d", iSampleRate);
#endif
	if (iSampleRate == 44100 
		|| iSampleRate == 48000
		|| iSampleRate == 88200
		|| iSampleRate == 96000
		|| iSampleRate == 192000)		// allow these rates only
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Checked samplerate OK", iSampleRate);
#endif
		return ASE_OK;
	}
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Checked samplerate failed", iSampleRate);
#endif
	return ASE_NoClock;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::getSampleRate (ASIOSampleRate *sampleRate)
{
	*sampleRate = this->sampleRate;
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Return current samplerate %d", (int)*sampleRate);
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::setSampleRate (ASIOSampleRate sampleRate)
{
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Try set samplerate %d", (int)sampleRate);
#endif
	if (sampleRate != 44100. 
		&& sampleRate != 48000.
		&& sampleRate != 88200.
		&& sampleRate != 96000.
		&& sampleRate != 192000.)
		return ASE_NoClock;

	if (sampleRate != this->sampleRate)
	{
		if(SetCurrentFreq(handle, audioControlInterfaceNum, clockSourceId, (int)sampleRate))
		{
			this->sampleRate = sampleRate;
			asioTime.timeInfo.sampleRate = sampleRate;
			asioTime.timeInfo.flags |= kSampleRateChanged;
			milliSeconds = (long)((double)(kBlockFrames * 1000) / this->sampleRate);
			if (callbacks && callbacks->sampleRateDidChange)
				callbacks->sampleRateDidChange (this->sampleRate);

#ifdef _DEBUG
			debugPrintf("ASIOUAC: Samplerate changed to %d", (int)this->sampleRate);
#endif
		}
		else
			return ASE_NoClock;

	}
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::getClockSources (ASIOClockSource *clocks, long *numSources)
{
	// internal
	clocks->index = 0;
	clocks->associatedChannel = -1;
	clocks->associatedGroup = -1;
	clocks->isCurrentSource = ASIOTrue;
	strcpy(clocks->name, "Internal");
	*numSources = 1;
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Get clock source req");
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::setClockSource (long index)
{
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Set clock source by index %d", (int)index);
#endif
	if (!index)
	{
		asioTime.timeInfo.flags |= kClockSourceChanged;
		return ASE_OK;
	}
	return ASE_NotPresent;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::getSamplePosition (ASIOSamples *sPos, ASIOTimeStamp *tStamp)
{
	tStamp->lo = theSystemTime.lo;
	tStamp->hi = theSystemTime.hi;
	if (samplePosition >= twoRaisedTo32)
	{
		sPos->hi = (unsigned long)(samplePosition * twoRaisedTo32Reciprocal);
		sPos->lo = (unsigned long)(samplePosition - (sPos->hi * twoRaisedTo32));
	}
	else
	{
		sPos->hi = 0;
		sPos->lo = (unsigned long)samplePosition;
	}

#ifdef _DEBUG
	//debugPrintf("ASIOUAC: Retrieve sample position %d", (int)samplePosition);
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::getChannelInfo (ASIOChannelInfo *info)
{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: getChannelInfo request. Channel %d", info->channel);
#endif

	if (info->channel < 0 || (info->isInput ? 
#ifndef NO_INPUTS
		info->channel >= kNumInputs 
#else
		info->channel >= 0 
#endif
			: info->channel >= kNumOutputs))
		return ASE_InvalidParameter;

	info->type = ASIOSTInt32LSB; //ASIOSTInt32MSB; //ASIOSTInt32LSB24;//ASIOSTInt32MSB24;
	info->channelGroup = 0;
	info->isActive = ASIOFalse;

	long i;
	if (info->isInput)
	{
#ifndef NO_INPUTS
		for (i = 0; i < activeInputs; i++)
		{
			if (inMap[i] == info->channel)
			{
				info->isActive = ASIOTrue;
				break;
			}
		}
#endif
	}
	else
	{
		for (i = 0; i < activeOutputs; i++)
		{
			if (outMap[i] == info->channel)
			{
				info->isActive = ASIOTrue;
				break;
			}
		}
	}
	sprintf(info->name, "UAC2 %d", info->channel);
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::createBuffers (ASIOBufferInfo *bufferInfos, long numChannels,
	long bufferSize, ASIOCallbacks *callbacks)
{
	ASIOBufferInfo *info = bufferInfos;
	long i;
	bool notEnoughMem = false;

#ifndef NO_INPUTS
	activeInputs = 0;
#endif
	activeOutputs = 0;
	blockFrames = bufferSize;
	for (i = 0; i < numChannels; i++, info++)
	{
		if (info->isInput)
		{
#ifndef NO_INPUTS
			if (info->channelNum < 0 || info->channelNum >= kNumInputs)
				goto error;
			inMap[activeInputs] = info->channelNum;
			inputBuffers[activeInputs] = new int[blockFrames * 2];	// double buffer
			if (inputBuffers[activeInputs])
			{
				info->buffers[0] = inputBuffers[activeInputs];
				info->buffers[1] = inputBuffers[activeInputs] + blockFrames;
			}
			else
			{
				info->buffers[0] = info->buffers[1] = 0;
				notEnoughMem = true;
			}
			activeInputs++;
			if (activeInputs > kNumInputs)
				goto error;
#endif
		}
		else	// output			
		{
			if (info->channelNum < 0 || info->channelNum >= kNumOutputs)
				goto error;
			outMap[activeOutputs] = info->channelNum;
			outputBuffers[activeOutputs] = new int[blockFrames * 2];	// double buffer
			if (outputBuffers[activeOutputs])
			{
				info->buffers[0] = outputBuffers[activeOutputs];
				info->buffers[1] = outputBuffers[activeOutputs] + blockFrames;
			}
			else
			{
				info->buffers[0] = info->buffers[1] = 0;
				notEnoughMem = true;
			}
			activeOutputs++;
			if (activeOutputs > kNumOutputs)
			{
				activeOutputs--;
				disposeBuffers();
				return ASE_InvalidParameter;
			}
		}
	}		
	if (notEnoughMem)
	{
		disposeBuffers();
		return ASE_NoMemory;
	}

	this->callbacks = callbacks;
	if (callbacks->asioMessage (kAsioSupportsTimeInfo, 0, 0, 0))
	{
		timeInfoMode = true;
		asioTime.timeInfo.speed = 1.;
		asioTime.timeInfo.systemTime.hi = asioTime.timeInfo.systemTime.lo = 0;
		asioTime.timeInfo.samplePosition.hi = asioTime.timeInfo.samplePosition.lo = 0;
		asioTime.timeInfo.sampleRate = sampleRate;
		asioTime.timeInfo.flags = kSystemTimeValid | kSamplePositionValid | kSampleRateValid;

		asioTime.timeCode.speed = 1.;
		asioTime.timeCode.timeCodeSamples.lo = asioTime.timeCode.timeCodeSamples.hi = 0;
		asioTime.timeCode.flags = kTcValid | kTcRunning ;
	}
	else
		timeInfoMode = false;

#ifdef _DEBUG
	debugPrintf("ASIOUAC: Create buffers with length %d OK", blockFrames);
#endif
	return ASE_OK;

error:
	disposeBuffers();
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Create buffers with length %d failed!", blockFrames);
#endif
	return ASE_InvalidParameter;
}

//---------------------------------------------------------------------------------------------
ASIOError AsioUAC2::disposeBuffers()
{
	long i;
	
	callbacks = 0;
	stop();
#ifndef NO_INPUTS
	for (i = 0; i < activeInputs; i++)
		delete inputBuffers[i];
	activeInputs = 0;
#endif
	for (i = 0; i < activeOutputs; i++)
		delete outputBuffers[i];
	activeOutputs = 0;
	return ASE_OK;
}

//---------------------------------------------------------------------------------------------
ASIOError AsioUAC2::controlPanel()
{
	return ASE_NotPresent;
}

//---------------------------------------------------------------------------------------------
ASIOError AsioUAC2::future (long selector, void* opt)	// !!! check properties 
{
/*
	ASIOTransportParameters* tp = (ASIOTransportParameters*)opt;
	switch (selector)
	{
		case kAsioEnableTimeCodeRead:	tcRead = true;	return ASE_SUCCESS;
		case kAsioDisableTimeCodeRead:	tcRead = false;	return ASE_SUCCESS;
		case kAsioSetInputMonitor:		return ASE_SUCCESS;	// for testing!!!
		case kAsioCanInputMonitor:		return ASE_SUCCESS;	// for testing!!!
		case kAsioCanTimeInfo:			return ASE_SUCCESS;
		case kAsioCanTimeCode:			return ASE_SUCCESS;
	}
*/
	return ASE_NotPresent;
}

//--------------------------------------------------------------------------------------------------------
// private methods
//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------
// input
//--------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------
bool AsioUAC2::inputOpen ()
{
	return true;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::inputClose ()
{
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::input()
{
#if 0
	long i;
	short *in = 0;

	for (i = 0; i < activeInputs; i++)
	{
		in = inputBuffers[i];
		if (in)
		{
			if (toggle)
				in += blockFrames;
			if ((i & 1) && sawTooth)
				memcpy(in, sawTooth, (unsigned long)(blockFrames * 2));
			else if (sineWave)
				memcpy(in, sineWave, (unsigned long)(blockFrames * 2));
		}
	}
#endif
}

//------------------------------------------------------------------------------------------------------------------
// output
//------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------
bool AsioUAC2::outputOpen()
{

	return true;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::outputClose ()
{
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::output ()
{
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::bufferSwitch ()
{
	if (started && callbacks)
	{
		getNanoSeconds(&theSystemTime);			// latch system time
		input();
		output();
		samplePosition += blockFrames;

		if (timeInfoMode)
			bufferSwitchX ();
		else
			callbacks->bufferSwitch (toggle, ASIOFalse);
		toggle = toggle ? 0 : 1;
#ifdef _DEBUG
		//debugPrintf("ASIOUAC: Buffer switched to %d, samplePosition %d, blockFrames %d", toggle, (int)samplePosition, blockFrames);
#endif
	}
}

//---------------------------------------------------------------------------------------------
// asio2 buffer switch
void AsioUAC2::bufferSwitchX ()
{
	getSamplePosition (&asioTime.timeInfo.samplePosition, &asioTime.timeInfo.systemTime);
	long offset = toggle ? blockFrames : 0;
	if (tcRead)
	{	// Create a fake time code, which is 10 minutes ahead of the card's sample position
		// Please note that for simplicity here time code will wrap after 32 bit are reached
		asioTime.timeCode.timeCodeSamples.lo = asioTime.timeInfo.samplePosition.lo + 600.0 * sampleRate;
		asioTime.timeCode.timeCodeSamples.hi = 0;
	}
	callbacks->bufferSwitchTimeInfo (&asioTime, toggle, ASIOFalse);
	asioTime.timeInfo.flags &= ~(kSampleRateChanged | kClockSourceChanged);
}

//---------------------------------------------------------------------------------------------
ASIOError AsioUAC2::outputReady ()
{
	return ASE_NotPresent;
}


//----------------USB related functions---------------------------------------


ASIOBool AsioUAC2::SendUsbControl(KUSB_HANDLE handle, int interfaceNumber, 
				   int dir, int type, int recipient, int request, int value, int index,
				   unsigned char *buff, int size, ULONG *lengthTransferred)
{
	BOOL retVal = FALSE;
	WINUSB_SETUP_PACKET packet;
	KUSB_SETUP_PACKET* defPkt = (KUSB_SETUP_PACKET*)&packet;

	memset(&packet, 0, sizeof(packet));
	defPkt->BmRequest.Dir	= dir;
	defPkt->BmRequest.Type	= type;
	defPkt->BmRequest.Recipient = recipient;
	defPkt->Request			= request;
	defPkt->Value			= value;
	defPkt->Index			= index;
	defPkt->Length			= 0;

	*lengthTransferred = 0;
    if(UsbK_ClaimInterface(handle, interfaceNumber, FALSE))
	{
		if(UsbK_ControlTransfer(handle, packet, buff, size, lengthTransferred, NULL))
			retVal = TRUE;

		UsbK_ReleaseInterface(handle, interfaceNumber, FALSE);
	}
	return retVal;
}


int AsioUAC2::GetCurrentFreq(KUSB_HANDLE handle, int interfaceNumber, int clockId)
{
	int freq;
	ULONG lengthTransferred = 0;
	if(SendUsbControl(handle, interfaceNumber, BMREQUEST_DIR_DEVICE_TO_HOST, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_INTERFACE, 
		AUDIO_CS_REQUEST_CUR, AUDIO_CS_CONTROL_SAM_FREQ << 8, (clockId << 8) + interfaceNumber,
				   (unsigned char*)&freq, sizeof(freq), &lengthTransferred) && lengthTransferred == 4)
	{
		return freq;
	}
	return 0;
}

ASIOBool AsioUAC2::SetCurrentFreq(KUSB_HANDLE handle, int interfaceNumber, int clockId, int freq)
{
	ULONG lengthTransferred = 0;
	if(SendUsbControl(handle, interfaceNumber, BMREQUEST_DIR_HOST_TO_DEVICE, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_INTERFACE, 
		AUDIO_CS_REQUEST_CUR, AUDIO_CS_CONTROL_SAM_FREQ << 8, (clockId << 8) + interfaceNumber,
				   (unsigned char*)&freq, sizeof(freq), &lengthTransferred) && lengthTransferred == 4)
	{
		return true;
	}
	return false;
}

ASIOError AsioUAC2::StartDevice()
{
	int packetSize = 4 * (int)sampleRate / 1000 / (1 << (gPipeInfoWrite.Interval - 1)) + 8;

	UsbK_ClaimInterface(handle, audioControlStreamNum, FALSE);
	UsbK_SetAltInterface(handle, audioControlStreamNum, FALSE, audioControlStreamAltNum);

	memset(outputBuffers[0], 0, blockFrames * 2 * sizeof(int));
	memset(outputBuffers[1], 0, blockFrames * 2 * sizeof(int));

	feedbackTask = new AudioTask(this, handle, &gPipeInfoFeedback, 16, gPipeInfoFeedback.MaximumPacketSize, gPipeInfoFeedback.MaximumPacketSize, TRUE);
	outputTask = new AudioTask(this, handle, &gPipeInfoWrite, 32, packetSize, (float)sampleRate / 1000.f, FALSE);

	feedbackTask->Start();
	outputTask->Start();

	return ASE_OK;
}

ASIOError AsioUAC2::StopDevice()
{
	if(outputTask)
		outputTask->Stop();

	if(feedbackTask)
		feedbackTask->Stop();

	if(outputTask)
		delete outputTask;
	outputTask = NULL;

	if(feedbackTask)
		delete feedbackTask;
	feedbackTask = NULL;

	UsbK_SetAltInterface(handle, audioControlStreamNum, FALSE, 0);
	UsbK_ReleaseInterface(handle, audioControlStreamNum, FALSE);

	return ASE_OK;
}

struct AudioSample
{
	int left;
	int right;
};

void AsioUAC2::FillData(UCHAR *buffer, int len)
{
	AudioSample *sampleBuff = (AudioSample *)buffer;
	int sampleLength = len / sizeof(AudioSample);
#ifdef _DEBUG
	//debugPrintf("ASIOUAC: Fill data with length %d, currentBufferPosition %d", sampleLength, currentBufferPosition);
#endif

	int *hostBuffer0 = toggle ? outputBuffers[0] + blockFrames : outputBuffers[0];
	int *hostBuffer1 = toggle ? outputBuffers[1] + blockFrames : outputBuffers[1];

	for(int i = 0; i < sampleLength; i++)
	{
		sampleBuff[i].left =  hostBuffer0[currentBufferPosition];
		sampleBuff[i].right = hostBuffer1[currentBufferPosition];

		currentBufferPosition ++;
		if(currentBufferPosition == blockFrames)
		{
			currentBufferPosition = 0;
			bufferSwitch ();
			hostBuffer0 = toggle ? outputBuffers[0] + blockFrames : outputBuffers[0];
			hostBuffer1 = toggle ? outputBuffers[1] + blockFrames : outputBuffers[1];
		}
	}
}
