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

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <conio.h>
#include <math.h>

#include "USBAudioDevice.h"
#include "tlist.h"


struct AudioSample
{
	int left;
	int right;
};

AudioSample dummybuffer[48];
DWORD globalReadBuffer = 0;
DWORD globalPacketCounter = 0;

void FillBuffer()
{
	memset(dummybuffer, 0, 48*sizeof(AudioSample));
	for(int i = 0; i < 48; i++)
	{
		dummybuffer[i].left = (int)(0x1FFFFF*sin(2.0*3.14159265358979323846*(double)i/48.));
		dummybuffer[i].right = dummybuffer[i].left;

		dummybuffer[i].left = dummybuffer[i].left << 8;
		dummybuffer[i].right = dummybuffer[i].right << 8;
	}
}

void FillData(void* context, UCHAR *buffer, int& len)
{
	AudioSample *sampleBuff = (AudioSample *)buffer;
	int sampleLength = len / sizeof(AudioSample);

	for(int i = 0; i < sampleLength; i++)
	{
		sampleBuff[i].left =  dummybuffer[globalReadBuffer].left;
		sampleBuff[i].right = dummybuffer[globalReadBuffer].right;
		globalReadBuffer++;
		if(globalReadBuffer >= 48) 
			globalReadBuffer = 0;
	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;
}

int main(int argc, char* argv[])
{
	int freq = 48000;
	if(argc > 1) 
		freq = atoi(argv[1]);
	do
	{
		USBAudioDevice device(false);
		device.InitDevice();
		FillBuffer();
		device.SetDACCallback(FillData, NULL);
		device.SetSampleRate(freq);
		device.Start();

		printf("press any key for stop...\n");
		_getch();

		device.Stop();
	} while(0);
	_CrtDumpMemoryLeaks();
	return 0;
}

