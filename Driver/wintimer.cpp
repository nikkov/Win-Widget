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

#include <windows.h>
#include "asiouac2.h"

#ifdef EMULATION_HARDWARE
extern void debugPrintf(const char *szFormat, ...);
static DWORD __stdcall ASIOThread (void *param);
static HANDLE ASIOThreadHandle = 0;
static bool done = false;

AsioUAC2* theDriver = 0;
#endif

const double twoRaisedTo32 = 4294967296.;


//------------------------------------------------------------------------------------------
void getNanoSeconds (ASIOTimeStamp* ts)
{
	double nanoSeconds = (double)((unsigned long)timeGetTime ()) * 1000000.;
	ts->hi = (unsigned long)(nanoSeconds / twoRaisedTo32);
	ts->lo = (unsigned long)(nanoSeconds - (ts->hi * twoRaisedTo32));
}

#ifdef EMULATION_HARDWARE
//------------------------------------------------------------------------------------------
void AsioUAC2::timerOn ()
{
	theDriver = this;
	DWORD asioId;
	done = false;
	ASIOThreadHandle = CreateThread (0, 0, &ASIOThread, 0, 0, &asioId);
}

//------------------------------------------------------------------------------------------
void AsioUAC2::timerOff ()
{
	done = true;
	if (ASIOThreadHandle)
		WaitForSingleObject(ASIOThreadHandle, 1000);
	ASIOThreadHandle = 0;
}

//------------------------------------------------------------------------------------------
static DWORD __stdcall ASIOThread (void *param)
{
#ifdef _ENABLE_TRACE
	debugPrintf("ASIOUAC: Start ASIOThread!, theDriver = %X, MilliSeconds = %d", theDriver, theDriver->getMilliSeconds ());
#endif
	int i = 0;
	do
	{
		if (theDriver)
		{
			theDriver->bufferSwitch ();
			Sleep (theDriver->getMilliSeconds ());
		}
		else
		{
			double a = 1000. / theDriver->sampleRate;
			Sleep ((long)(a * (double)kBlockFrames));

		}
	} while (!done);
#ifdef _ENABLE_TRACE
	debugPrintf("ASIOUAC: Stop ASIOThread!");
#endif
	return 0;
}
#endif