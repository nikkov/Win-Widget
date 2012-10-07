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

#ifdef _ENABLE_TRACE

void debugPrintf(const char *szFormat, ...)
{
    char str[4096];
    va_list argptr;
    va_start(argptr, szFormat);
    vsprintf_s(str, szFormat, argptr);
    va_end(argptr);

    printf(str);
    OutputDebugString(str);
}
#endif


struct AudioSample4
{
	int left;
	int right;
};

struct ThreeByteSample
{
	UCHAR sample[3];
	ThreeByteSample(int val = 0)
	{
		UCHAR *ptrVal = (UCHAR *)&val;
		sample[0] = *(ptrVal);
		sample[1] = *(ptrVal+1);
		sample[2] = *(ptrVal+2);
	}

	operator int()const
	{
		return (sample[0] << 0) + (sample[1] << 8) + (sample[2] << 16);
	}
};

struct AudioSample3
{
	ThreeByteSample left;
	ThreeByteSample right;
};

AudioSample4 dummybuffer[48];
DWORD globalReadBuffer = 0;
DWORD globalPacketCounter = 0;

void FillBuffer(bool zoom)
{
	memset(dummybuffer, 0, 48*sizeof(AudioSample4));
	for(int i = 0; i < 48; i++)
	{
		//dummybuffer[i].left = (int)(0x1FFFFF*sin(2.0*3.14159265358979323846*(double)i/48.));
		dummybuffer[i].left = (int)(0x1FFFFF*sin(2.0*3.14159265358979323846*(double)i/48.));
		dummybuffer[i].right = dummybuffer[i].left;

		if(zoom)
		{
			dummybuffer[i].left = dummybuffer[i].left << 8;
			dummybuffer[i].right = dummybuffer[i].right << 8;
		}
	}
}

void FillData4(void* context, UCHAR *buffer, int& len)
{
	AudioSample4 *sampleBuff = (AudioSample4 *)buffer;
	int sampleLength = len / sizeof(AudioSample4);

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

void FillData3(void* context, UCHAR *buffer, int& len)
{
	AudioSample3 *sampleBuff = (AudioSample3 *)buffer;
	int sampleLength = len / sizeof(AudioSample3);

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


// Parse a wav file header and return file parameters
//  See https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
int parsewavheader (FILE* wavfile, int* NumChannels, int* SampleRate, int* BitsPerSample, long* NumSamples)
{

#define WAVHEADER_L 44				// Length of standard wav file header
#define IG 0xAB					// A byte which doesn't occur elsewhere in header

	unsigned char wavheader[WAVHEADER_L] = {	// Required header contents
		'R', 'I', 'F', 'F',			// Chunk ID
		IG, IG, IG, IG,				// ChunkSize, 36 + bytes of audio data
		'W', 'A', 'V', 'E',			// Format
		'f', 'm', 't', 0x20,		// Subchunk1ID
		0x10, 0x00, 0x00, 0x00,		// Subchunk1Size, 16 for PCM
		0x01, 0x00,					// AudioFormat, 1 for PCM
		IG, IG,						// NumChannels, extract number, only accept 2
		IG, IG, IG, IG,				// SampleRate
		IG, IG, IG, IG,				// ByteRate = SampleRate * NumChannels * BitsPerSample / 8
		IG, IG,						// BlockAlign = NumChannels * BitsPerSample / 8
		IG, IG,						// BitsPerSample
		'd', 'a', 't', 'a',			// Subchunk2ID
		IG, IG, IG, IG};			// SubChunk2Size

	unsigned char readwavheader[WAVHEADER_L];

	rewind (wavfile);				// Make sure to read header from beginning of file!

	// Extract wav file header and do initial error checking
	int n = fread (readwavheader, 1, WAVHEADER_L, wavfile); // Try to read 44 header bytes

	if (n != WAVHEADER_L)
	{
		printf ("ERROR: Could not read %d bytes wav file header\n", WAVHEADER_L);
		return 0;
	}

	// Compare wavfile to default header, ignoring all bytes set to ig
	n=0;
	while ( ( (readwavheader[n] == wavheader[n]) || (wavheader[n] == IG) ) && (n < WAVHEADER_L) ) n++;

	if (n != WAVHEADER_L) 
	{
		printf ("ERROR: wav file header error at position %d\n", n);
		return 0;
	}

	// Extract wav file parameters
	int ChunkSize = (readwavheader[4]) + (readwavheader[5]<<8) + (readwavheader[6]<<16) + (readwavheader[7]<<24);
	*NumChannels = (readwavheader[22]) + (readwavheader[23]<<8);
	*SampleRate = (readwavheader[24]) + (readwavheader[25]<<8) + (readwavheader[26]<<16) + (readwavheader[27]<<24);
	int ByteRate = (readwavheader[28]) + (readwavheader[29]<<8) + (readwavheader[30]<<16) + (readwavheader[31]<<24);
	int BlockAlign = (readwavheader[32]) + (readwavheader[33]<<8);
	*BitsPerSample = (readwavheader[34]) + (readwavheader[35]<<8);
	int SubChunk2Size = (readwavheader[40]) + (readwavheader[41]<<8) + (readwavheader[42]<<16) + (readwavheader[43]<<24);
	*NumSamples = SubChunk2Size / *BitsPerSample * 8 / *NumChannels;
	double Duration = *NumSamples; Duration /= *SampleRate;

	// Print parameters
	printf ("ChunkSize = %d\n", ChunkSize);
	printf ("NumChannels = %d\n", *NumChannels);
	printf ("SampleRate = %d\n", *SampleRate);
	printf ("ByteRate = %d\n", ByteRate);
	printf ("BlockAlign = %d\n", BlockAlign);
	printf ("BitsPerSample = %d\n", *BitsPerSample);
	printf ("SubChunk2Size = %d\n", SubChunk2Size);
	printf ("NumSamples = %d\n", *NumSamples);
	printf ("Duration = %4.2fs\n", Duration);

	// Full error checking
	n = 1; // Assuming a clean return. But report all found errors before returning

	if (SubChunk2Size + 36 != ChunkSize)
	{
		printf ("ERROR: SubChunk2Size, ChunkSize mismatch %d+36 != %d\n", SubChunk2Size, ChunkSize);
		n = 0;
	}

	if (*NumChannels != 2)
	{
		printf ("ERROR: Only 2-channel wav is accepted, not the detected %d-channel.\n", *NumChannels);
		n = 0;
	}

	if ( (*SampleRate != 44100) && (*SampleRate != 48000) && 
		 (*SampleRate != 88200) && (*SampleRate != 96000) &&
		 (*SampleRate != 176400) && (*SampleRate != 192000) )
	{
		printf ("ERROR: Only 44.1/48/88.2/96/176.4/192ksps accepted, not the detected %d.\n", *SampleRate);
		n = 0 ;
	}

	if (ByteRate != *SampleRate * *NumChannels * *BitsPerSample / 8)
	{
		printf ("ERROR: Mismatch between ByteRate, SampleRate, NumChannels, BitsPerSample\n");
		n = 0;
	}

	if (BlockAlign != *NumChannels * *BitsPerSample / 8)
	{
		printf ("ERROR: Mismatch between BlockAlign, NumChannels, BitsPerSample\n");
		n = 0;
	}

	if ( (*BitsPerSample != 16) && (*BitsPerSample != 24) && (*BitsPerSample != 32) )
	{
		printf ("ERROR: Only 16/24/32 bits per sample accepted, not the detected %d.\n", *BitsPerSample);
		n = 0;
	}

	return n;
}


int main(int argc, char* argv[])
{

	int freq;
	int mode; // BSB 20121007 0:signal generator 1:wav file player

	if (argc == 1) {	// No arguments
		freq = 48000;	// Default sampling frequency
		mode = 0;		// Run as signal generator
	}

	else if (argc == 2) // One argument, may it be a valid audio frequency?
	{
		freq = atoi(argv[1]);
		if ( (freq == 44100) || (freq == 48000) || (freq == 88200) || (freq == 96000) || (freq == 176400) || (freq == 192000) )
			mode = 0;	// Run as signal generator
		else
			mode = 1;	// Run as wav file player
	}

	else				// >1 argument, run as wav file player
		mode = 1;		// Run as wav file player

	if (mode == 0)		// signal generator
	{
		printf("Running as signal generator.\n");
		USBAudioDevice device(true);
		device.InitDevice();
		
		if(device.GetDACSubslotSize() == 3)
		{
			FillBuffer(false);
			device.SetDACCallback(FillData3, NULL);
		}
		else
			if(device.GetDACSubslotSize() == 4)
			{
				FillBuffer(true);
				device.SetDACCallback(FillData4, NULL);
			}
		device.SetSampleRate(freq);
		device.Start();

		printf("Press any key for stop...\n");
		_getch();

		device.Stop();
	}

	else if (mode == 1) // wav file player
	{
		int NumChannels = 0;	// Variables to extract from wav file header
		int SampleRate = 0;
		int BitsPerSample = 0;
		long NumSamples = 0;
		FILE * wavfile;

		printf("Running as wav file player.\n");
		// Open files
		for (int n=1; n<argc; n++)
		{
			wavfile = fopen(argv[n],"rb");	// fclose_s is recommended..
			if (wavfile!=NULL)
			{
				printf ("\nFound file: %s\n",argv[n]);
				parsewavheader (wavfile, &NumChannels, &SampleRate, &BitsPerSample, &NumSamples);

				// Now play the darn thing :-) 

				fclose (wavfile);
				printf("Press any key for stop...\n");
				_getch();
			}
			else
				printf ("File not found: %s\n",argv[n]);
		}
	}


	_CrtDumpMemoryLeaks();
	return 0;
}

