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

#define _DeviceInterfaceGUID "{09e4c63c-ce0f-168c-1862-06410a764a35}"

extern void debugPrintf(const char *szFormat, ...);

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
	: CUnknown("ASIOUAC2", pUnk, phr), callbacks(NULL), inputBuffers(NULL), outputBuffers(NULL), inMap(NULL), outMap(NULL), m_device(NULL)


//------------------------------------------------------------------------------------------

#else

// when not on windows, we derive from AsioDriver
AsioUAC2::AsioUAC2 () : AsioDriver (), callbacks(NULL), inputBuffers(NULL), outputBuffers(NULL), inMap(NULL), outMap(NULL), m_device(NULL)

#endif
{
// ASIO
	//block number by default
	blockFrames = 32;

	inputLatency = blockFrames;		// typically
	outputLatency = blockFrames * 2;
	
	// typically blockFrames * 2; try to get 1 by offering direct buffer
	// access, and using asioPostOutput for lower latency
	samplePosition = 0;
	sampleRate = 48000;
	milliSeconds = (long)((double)(blockFrames * 1000) / sampleRate);
	active = false;
	started = false;
	timeInfoMode = false;
	tcRead = false;

	m_NumInputs = m_NumOutputs = 0;
	activeInputs = activeOutputs = 0;
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
	delete m_device;
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
	m_device = new USBAudioDevice(TRUE);
	m_device->InitDevice();

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
	//error
	if(m_device)
		m_device->Stop();

	delete m_device;
	m_device = NULL;
	
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
	if(!m_device)
		return ASE_HWMalfunction;

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
		ASIOError retVal = m_device->Start() ? ASE_OK : ASE_HWMalfunction;
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
	if(!m_device)
		return ASE_HWMalfunction;

#ifdef EMULATION_HARDWARE
	timerOff ();		
	started = false;
	return ASE_OK;
#else
	// de-activate hardware
	ASIOError retVal = m_device->Stop() ? ASE_OK : ASE_HWMalfunction;
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
	*numInputChannels = m_NumInputs;
	*numOutputChannels = m_NumOutputs;
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
	if(!m_device)
		return ASE_HWMalfunction;
	int iSampleRate = (int)sampleRate;
#ifdef _DEBUG
	debugPrintf("ASIOUAC: Checked samplerate %d", iSampleRate);
#endif
	if(m_device->CanSampleRate(iSampleRate))
		return ASE_OK;
	return ASE_NoClock;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::getSampleRate (ASIOSampleRate *sampleRate)
{
	*sampleRate = this->sampleRate;
#ifdef _DEBUG
	//debugPrintf("ASIOUAC: Return current samplerate %d", (int)*sampleRate);
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError AsioUAC2::setSampleRate (ASIOSampleRate sampleRate)
{
	if(!m_device)
		return ASE_HWMalfunction;
	int iSampleRate = (int)sampleRate;

#ifdef _DEBUG
	debugPrintf("ASIOUAC: Try set samplerate %d", iSampleRate);
#endif
	if (sampleRate != this->sampleRate)
	{
		if(m_device->SetSampleRate(iSampleRate))
		{
			this->sampleRate = sampleRate;
			asioTime.timeInfo.sampleRate = sampleRate;
			asioTime.timeInfo.flags |= kSampleRateChanged;
			milliSeconds = (long)((double)(blockFrames * 1000) / this->sampleRate);
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
		info->channel >= m_NumInputs 
			: info->channel >= m_NumOutputs))
		return ASE_InvalidParameter;

	info->type = ASIOSTInt32LSB; //ASIOSTInt32MSB; //ASIOSTInt32LSB24;//ASIOSTInt32MSB24;
	info->channelGroup = 0;
	info->isActive = ASIOFalse;

	long i;
	if (info->isInput)
	{
		for (i = 0; i < activeInputs; i++)
		{
			if (inMap[i] == info->channel)
			{
				info->isActive = ASIOTrue;
				break;
			}
		}
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

	activeInputs = 0;
	activeOutputs = 0;
	blockFrames = bufferSize;
	for (i = 0; i < numChannels; i++, info++)
	{
		if (info->isInput)
		{
			if (info->channelNum < 0 || info->channelNum >= m_NumInputs)
				goto error;
			inMap[activeInputs] = info->channelNum;
			inputBuffers[activeInputs] = new int[blockFrames * 2];	// double buffer
			memset(inputBuffers[activeInputs], 0, blockFrames * 2 * sizeof(int));
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
			if (activeInputs > m_NumInputs)
				goto error;
		}
		else	// output			
		{
			if (info->channelNum < 0 || info->channelNum >= m_NumOutputs)
				goto error;
			outMap[activeOutputs] = info->channelNum;
			outputBuffers[activeOutputs] = new int[blockFrames * 2];	// double buffer
			memset(outputBuffers[activeInputs], 0, blockFrames * 2 * sizeof(int));
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
			if (activeOutputs > m_NumOutputs)
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
	if(!m_device)
		return false;

	m_NumInputs = m_device->GetInputChannelNumber();
	if(inputBuffers)
		delete inputBuffers;
	inputBuffers = new int*[m_NumInputs * 2];
	if(inMap)
		delete inMap;
	inMap = new long[m_NumInputs];
	for (int i = 0; i < m_NumInputs; i++)
	{
		inputBuffers[i] = NULL;
		inMap[i] = 0;
	}
	return true;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::inputClose ()
{
	if(inMap)
		delete inMap;
	if(inputBuffers)
		delete inputBuffers;
	m_NumInputs = 0;
	inMap = NULL;
	inputBuffers = NULL;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::input()
{
}

//------------------------------------------------------------------------------------------------------------------
// output
//------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------
bool AsioUAC2::outputOpen()
{
	if(!m_device)
		return false;

	m_NumOutputs = m_device->GetOutputChannelNumber();
	if(outputBuffers)
		delete outputBuffers;
	outputBuffers = new int*[m_NumOutputs * 2];
	if(outMap)
		delete outMap;
	outMap = new long[m_NumOutputs];
	for (int i = 0; i < m_NumOutputs; i++)
	{
		outputBuffers[i] = NULL;
		outMap[i] = 0;
	}
	m_device->SetDACCallback(AsioUAC2::sFillOutputData, (void*)this);
	return true;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::outputClose ()
{
	if(outMap)
		delete outMap;
	if(outputBuffers)
		delete outputBuffers;
	m_NumOutputs = 0;
	outMap = NULL;
	outputBuffers = NULL;
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



struct AudioSample
{
	int left;
	int right;
};

void AsioUAC2::FillOutputData(UCHAR *buffer, int& len)
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

void AsioUAC2::FillInputData(UCHAR *buffer, int& len)
{

}

void AsioUAC2::sFillOutputData(void* context, UCHAR *buffer, int& len)
{
	if(context)
		((AsioUAC2*)context)->FillOutputData(buffer, len);
}

void AsioUAC2::sFillInputData(void* context, UCHAR *buffer, int& len)
{
	if(context)
		((AsioUAC2*)context)->FillInputData(buffer, len);
}
