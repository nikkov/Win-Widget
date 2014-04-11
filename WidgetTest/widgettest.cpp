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
# Contact: nikkov@gmail.com, borge.strand@gmail.com
#----------------------------------------------------------------------------
*/

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <conio.h>
#include <math.h>
#include <string.h>

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


// Global variables keep this program running.

// A fixed array for signal generator sine table
AudioSample4 globalSineBuffer[48];

// Memory pointers to Wav file data
AudioSample3 * globalWavBuffer3;
AudioSample4 * globalWavBuffer4;
DWORD globalNumParsedSamples = 0;		// The number of samples read in from the wav file, independant of sample format
DWORD globalBufferIndex = 0;
DWORD globalPacketCounter = 0;
DWORD globalPreFetch = 0;

DWORD globalCPUTimeMonitor = 0;			// Monitor CPU time spent in callback function
unsigned __int64 globalCPUTimeMax, globalCPUTimeMin, globalCPUTimeLast;


// Assume we're running in verbose mode
DWORD globalVerbose = 0; 

// Assume we're not running in silent mode
DWORD globalSilent = 0; 

// Parse a wav file header and return file parameters
// See https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
int ParseWavHeader (FILE* wavfile, int* NumChannels, int* SampleRate, int* BytesPerSample, long* NumSamples) {
#define WAVHEADER_L 44				// Length of standard wav file header
#define IG 0xAB						// A byte which doesn't occur elsewhere in header

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

	if (n != WAVHEADER_L) {
		if (!globalSilent)
			printf ("WidgetTest: ERROR: Could not read %d bytes wav file header\n", WAVHEADER_L);
		return 0;
	}

	// Compare wavfile to default header, ignoring all bytes set to IG
	n=0;
	while ( ( (readwavheader[n] == wavheader[n]) || (wavheader[n] == IG) ) && (n < WAVHEADER_L) ) n++;

	if (n != WAVHEADER_L) {
		if (!globalSilent)
			printf ("WidgetTest: ERROR: wav file header error at position %d\n", n);
		return 0;
	}

	// Extract wav file parameters
	int ChunkSize = (readwavheader[4]) + (readwavheader[5]<<8) + (readwavheader[6]<<16) + (readwavheader[7]<<24);
	*NumChannels = (readwavheader[22]) + (readwavheader[23]<<8);
	*SampleRate = (readwavheader[24]) + (readwavheader[25]<<8) + (readwavheader[26]<<16) + (readwavheader[27]<<24);
	int ByteRate = (readwavheader[28]) + (readwavheader[29]<<8) + (readwavheader[30]<<16) + (readwavheader[31]<<24);
	int BlockAlign = (readwavheader[32]) + (readwavheader[33]<<8);
	int BitsPerSample = (readwavheader[34]) + (readwavheader[35]<<8);
	*BytesPerSample = BitsPerSample >> 3;
	int SubChunk2Size = (readwavheader[40]) + (readwavheader[41]<<8) + (readwavheader[42]<<16) + (readwavheader[43]<<24);
	*NumSamples = SubChunk2Size / *BytesPerSample / *NumChannels;
	double Duration = *NumSamples; Duration /= *SampleRate;

	// Print parameters
	if (globalVerbose) {
		printf ("WidgetTest: ChunkSize = %d\n", ChunkSize);
		printf ("WidgetTest: NumChannels = %d\n", *NumChannels);
		printf ("WidgetTest: SampleRate = %d\n", *SampleRate);
		printf ("WidgetTest: ByteRate = %d\n", ByteRate);
		printf ("WidgetTest: BlockAlign = %d\n", BlockAlign);
		printf ("WidgetTest: BytesPerSample = %d\n", *BytesPerSample); // Bytes per MONO sample, *2 for stereo
		printf ("WidgetTest: SubChunk2Size = %d\n", SubChunk2Size);
		printf ("WidgetTest: NumSamples = %d\n", *NumSamples);
		printf ("WidgetTest: Duration = %4.3fs\n", Duration);
	}

	// Full error checking
	n = 1; // Assuming a clean return. But report all found errors before returning

	if (SubChunk2Size + 36 != ChunkSize) {
		if (!globalSilent)
			printf ("WidgetTest: ERROR: SubChunk2Size, ChunkSize mismatch %d+36 != %d\n", SubChunk2Size, ChunkSize);
		n = 0;
	}

	if (*NumChannels != 2) {
		if (!globalSilent)
			printf ("WidgetTest: ERROR: Only 2-channel wav is accepted, not the detected %d-channel.\n", *NumChannels);
		n = 0;
	}

	if ( (*SampleRate != 44100) && (*SampleRate != 48000) && 
		 (*SampleRate != 88200) && (*SampleRate != 96000) &&
		 (*SampleRate != 176400) && (*SampleRate != 192000) ) {
		if (!globalSilent)
			printf ("WidgetTest: ERROR: Only 44.1/48/88.2/96/176.4/192ksps accepted, not the detected %d.\n", *SampleRate);
		n = 0 ;
	}

	if (ByteRate != *SampleRate * *NumChannels * *BytesPerSample) {
		if (!globalSilent)
			printf ("WidgetTest: ERROR: Mismatch between ByteRate, SampleRate, NumChannels, BitsPerSample\n");
		n = 0;
	}

	if (BlockAlign != *NumChannels * *BytesPerSample) {
		if (!globalSilent)
			printf ("WidgetTest: ERROR: Mismatch between BlockAlign, NumChannels, BitsPerSample\n");
		n = 0;
	}

	if ( (*BytesPerSample != 2) && (*BytesPerSample != 3) && (*BytesPerSample != 4) ) {
		if (!globalSilent)
			printf ("WidgetTest: ERROR: Only 2/3/4 bytes per mono sample accepted, not the detected %d.\n", *BytesPerSample);
		n = 0;
	}

	return n;
}


// Generate a sine table of 48 32-bit entries
void FillSineBuffer(AudioSample4* sinebuffer, bool zoom)
{
	memset(sinebuffer, 0, 48*sizeof(AudioSample4));
	for(int i = 0; i < 48; i++) {
		sinebuffer[i].left  = (int)(0x1FFFFF*sin(2.0*3.14159265358979323846*(double)i/48.));
		sinebuffer[i].right = sinebuffer[i].left;

		if(zoom) {
			sinebuffer[i].left  = sinebuffer[i].left << 8;
			sinebuffer[i].right = sinebuffer[i].right << 8;
		}
	}
}


// Move data from sine table to USB device. This function is passed as a callback
void FillSineData3(void* context, UCHAR *buffer, int& len)
{
	AudioSample3 *sampleBuff = (AudioSample3 *)buffer;
	int sampleLength = len / sizeof(AudioSample3);

	for(int i = 0; i < sampleLength; i++) {
		sampleBuff[i].left  = globalSineBuffer[globalBufferIndex].left;
		sampleBuff[i].right = globalSineBuffer[globalBufferIndex].right;
		globalBufferIndex++;
		if(globalBufferIndex >= 48) 
			globalBufferIndex = 0;
	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;
}


// Move data from sine table to USB device. This function is passed as a callback
void FillSineData4(void* context, UCHAR *buffer, int& len)
{
	AudioSample4 *sampleBuff = (AudioSample4 *)buffer;
	int sampleLength = len / sizeof(AudioSample4);

	for(int i = 0; i < sampleLength; i++) {
		sampleBuff[i].left  = globalSineBuffer[globalBufferIndex].left;
		sampleBuff[i].right = globalSineBuffer[globalBufferIndex].right;
		globalBufferIndex++;
		if(globalBufferIndex >= 48) 
			globalBufferIndex = 0;
	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;
}


// Fill 3L, 3R bytes from wavfile into RAM (replaces sine table above)
long FillWavBuffer3 (AudioSample3* wavbuffer, FILE* wavfile, int* BytesPerSample, long* NumSamples) {
	unsigned char temp[8];	// Temporary variable for reading a stereo sample from the wavfile
	long readsamples = 0;
	int m = 0;

	if (*BytesPerSample == 2) {
		for (long n=0; n<*NumSamples; n++) {
			m = fread(temp, 1, 4, wavfile);
			if (m == 4) { 		// 16 bits -> 24 bits stereo
				wavbuffer[n].left  = (temp[0]<<8) + (temp[1]<<16);
				wavbuffer[n].right = (temp[2]<<8) + (temp[3]<<16);
				readsamples++;
			}
			else {
				if (!globalSilent)
					printf ("WidgetTest: ERROR: Read error n=%d, ReadSamples=%d, m=%d", n, readsamples, m);
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	else if (*BytesPerSample == 3) {
		for (long n=0; n<*NumSamples; n++) {
			m = fread(temp, 1, 6, wavfile);
			if (m == 6) { 		// 24 bits -> 24 bits stereo. Can we do simple copy?
				wavbuffer[n].left  = (temp[0]) + (temp[1]<<8) + (temp[2]<<16);
				wavbuffer[n].right = (temp[3]) + (temp[4]<<8) + (temp[5]<<16);
				readsamples++;
			}
			else {
				if (!globalSilent)
					printf ("WidgetTest: ERROR: Read error n=%d, ReadSamples=%d, m=%d", n, readsamples, m);
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	else if (*BytesPerSample == 4) {
		for (long n=0; n<*NumSamples; n++) {
			m = fread(temp, 1, 8, wavfile);
			if (m == 8) {		// 32 bits -> 24 bits stereo. FIX: add optional dither!
				wavbuffer[n].left  = (temp[1]) + (temp[2]<<8) + (temp[3]<<16);
				wavbuffer[n].right = (temp[5]) + (temp[6]<<8) + (temp[7]<<16);
				readsamples++;
			}
			else {
				if (!globalSilent)
					printf ("WidgetTest: ERROR: Read error n=%d, ReadSamples=%d, m=%d", n, readsamples, m);
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	return readsamples;
}


// Fill 4L, 4R bytes from wavfile into RAM (replaces sine table above)
long FillWavBuffer4 (AudioSample4* wavbuffer, FILE* wavfile, int* BytesPerSample, long* NumSamples) {
	unsigned char temp[8];	// Temporary variable for reading a stereo sample from the wavfile
	long readsamples = 0;
	int m = 0;

	if (*BytesPerSample == 2) {
		for (long n=0; n<*NumSamples; n++) {
			m = fread(temp, 1, 4, wavfile);
			if (m == 4) {		// 16 bits -> 32 bits stereo
				wavbuffer[n].left  = (temp[0]<<16) + (temp[1]<<24);
				wavbuffer[n].right = (temp[2]<<16) + (temp[3]<<24);
				readsamples++;
			}
			else {
				if (!globalSilent)
					printf ("WidgetTest: ERROR: Read error n=%d, ReadSamples=%d, m=%d", n, readsamples, m);
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	else if (*BytesPerSample == 3) {
		for (long n=0; n<*NumSamples; n++) {
			m = fread(temp, 1, 6, wavfile);
			if (m == 6) {		// 24 bits -> 32 bits stereo.
				wavbuffer[n].left  = (temp[0]<<8) + (temp[1]<<16) + (temp[2]<<24);
				wavbuffer[n].right = (temp[3]<<8) + (temp[4]<<16) + (temp[5]<<24);
				readsamples++;
			}
			else {
				if (!globalSilent)
					printf ("WidgetTest: ERROR: Read error n=%d, ReadSamples=%d, m=%d", n, readsamples, m);
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	else if (*BytesPerSample == 4) {
		for (long n=0; n<*NumSamples; n++) {
			m = fread(temp, 1, 8, wavfile);
			if (m == 8) { 		// 32 bits -> 32 bits stereo.  Can we do simple copy?
				wavbuffer[n].left  = (temp[0]) + (temp[1]<<8) + (temp[2]<<16) + (temp[3]<<24);
				wavbuffer[n].right = (temp[4]) + (temp[5]<<8) + (temp[6]<<16) + (temp[7]<<24);
				readsamples++;
			}
			else {
				if (!globalSilent)
					printf ("WidgetTest: ERROR: Read error n=%d, ReadSamples=%d, m=%d", n, readsamples, m);
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	return readsamples;
}


// Move data from wav file RAM to USB device. This function is passed as a callback
void FillWavData3(void* context, UCHAR *buffer, int& len)
{
	unsigned __int64 CPUTime, deltaCPUTime;
	static unsigned __int64 prevCPUTime=0;

	AudioSample3 *sampleBuff = (AudioSample3 *)buffer;
	int sampleLength = len / sizeof(AudioSample3);

	for(int i = 0; i < sampleLength; i++)
	{
		// Have we read past parsed end of wav file? Determine by counting audio samples
		if (globalBufferIndex < globalNumParsedSamples) { // initially, both are 0
			sampleBuff[i].left  =  globalWavBuffer3[globalBufferIndex].left;
			sampleBuff[i].right =  globalWavBuffer3[globalBufferIndex].right;
			globalBufferIndex++;
		}
		else {
			sampleBuff[i].left  =  0;
			sampleBuff[i].right =  0;
		}

	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;

	if (globalCPUTimeMonitor) {
		CPUTime = __rdtsc();
		if (prevCPUTime != 0) {		// Check every time except 1st time callback executes
			deltaCPUTime = CPUTime - prevCPUTime;

			if (deltaCPUTime > globalCPUTimeMax)
				globalCPUTimeMax = deltaCPUTime;
			if (deltaCPUTime < globalCPUTimeMin)
				globalCPUTimeMin = deltaCPUTime;

			globalCPUTimeLast = deltaCPUTime;
		}
		prevCPUTime = CPUTime;
	}
}


// Move data from wav file RAM to USB device. This function is passed as a callback
void FillWavData4(void* context, UCHAR *buffer, int& len)
{
	unsigned __int64 CPUTime, deltaCPUTime;
	static unsigned __int64 prevCPUTime=0;

	AudioSample4 *sampleBuff = (AudioSample4 *)buffer;
	int sampleLength = len / sizeof(AudioSample4);

	for(int i = 0; i < sampleLength; i++)
	{
		// Have we read past parsed end of wav file? Determine by counting audio samples
		if (globalBufferIndex < globalNumParsedSamples) { // initially, both are 0
			sampleBuff[i].left  =  globalWavBuffer4[globalBufferIndex].left;
			sampleBuff[i].right =  globalWavBuffer4[globalBufferIndex].right;
			globalBufferIndex++;
		}
		else {
			sampleBuff[i].left  =  0;
			sampleBuff[i].right =  0;
		}

	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;

	if (globalCPUTimeMonitor) {
		CPUTime = __rdtsc();
		if (prevCPUTime != 0) {		// Check every time except 1st time callback executes
			deltaCPUTime = CPUTime - prevCPUTime;

			if (deltaCPUTime > globalCPUTimeMax)
				globalCPUTimeMax = deltaCPUTime;
			if (deltaCPUTime < globalCPUTimeMin)
				globalCPUTimeMin = deltaCPUTime;

			globalCPUTimeLast = deltaCPUTime;
		}
		prevCPUTime = CPUTime;
	}
}


int main(int argc, char* argv[]) {

	int freq;
	int mode; // BSB 20121007 0:signal generator 1:wav file player
	int SubSlotSize = 0;
	DWORD tempNumParsedSamples;

	if (argc == 1) {	// No arguments
		mode = 2;		// Display help file
	}

	else if (argc == 2) { // One argument, may it be a valid audio frequency?

		// Time to display help information?
		if (strchr(argv[1], '?')) { 
			mode = 2;
		}
		else {
			freq = atoi(argv[1]);
			if ( (freq == 44100) || (freq == 48000) || (freq == 88200) || (freq == 96000) || (freq == 176400) || (freq == 192000) )
				mode = 0;	// A valid sample rate. Run as signal generator
			else
				mode = 1;	// Not a valid sample rate. Run as wav file player
		}
	}

	else				// >1 argument, run as wav file player
		mode = 1;		// Run as wav file player

	if (mode == 2) {	// Help file
		printf("WidgetTest Help - Program version 20140410B\n");
		printf("\n");
		printf(" WidgetTest is a test and playback program developed for the AudioWidget.\n");
		printf(" See http://www.henryaudio.com for details.\n");
		printf("\n");
		printf(" Use WidgetTest in one of these two ways:\n");
		printf(" * As sine generator use a single parameter for the sampling frequency.\n");
		printf("   The sampling freqnecy must be one of 44100, 48000, 88200, 96000,\n");
		printf("   176400, 192000\n");
		printf("   Example: \"WidgetTest 44100\" repeats a 48-sample sine wave at fs=44.1kHz.\n");
		printf("\n");
		printf(" * As a wav file player. The parameter(s) may be a single .wav file, \n");
		printf("   multiple.wav files or wildcards\n");
		printf("   Example: \"WidgetTest foo.wav\" will play back the file foo.wav\n");
		printf("   Example: \"WidgetTest foo.wav bar.wav\" will play the two files in sequence\n");
		printf("\n");
		printf(" Optional wav file switches may be used before the .wav file parmaeters.\n");
		printf(" Switches must follow the character '-' and be a continuous string.\n");
		printf(" Recognized switches are:\n");
		printf(" -v Verbose operation (default OFF)\n");
		printf(" -c CPU time logging of time between callback function by means of\n");
		printf("    __rdtsc(); (default OFF)\n");
		printf(" -s Silent operation without Warnings and Errors (default OFF).\n");
		printf("    Switch -s will override -v but not -c.\n");
		printf(" -f main() loop pre-fetches audio data. No buffering, but perhaps\n");
		printf("    a way to prevent too late swap-in of audio data.\n");
		printf(" Example: \"WidgetTest -vc foo.wav\" will play back foo.wav verbosely while\n");
		printf(" logging CPU time\n");
		printf("\n");
		printf(" Single-letter keyboard commands are during wav file playback:\n");
		printf(" 'p' (un)pauses\n");
		printf(" 'r' replays current track\n");
		printf(" 't' terminates session\n");
		printf(" any other key skips to next track\n");
		printf("\n");
	}

	if (mode == 0) {	// signal generator, parameters don't apply
		printf("WidgetTest: Running as signal generator.\n");

		// First check for valid playback device
		USBAudioDevice device(true);
		if (!device.InitDevice()) {
			printf ("WidgetTest: ERROR: UAC2 Audio device not found\n");
			return -1;
		}
		SubSlotSize = device.GetDACSubslotSize();
		
		if(SubSlotSize == 3) {
			FillSineBuffer(globalSineBuffer, false);
			device.SetDACCallback(FillSineData3, NULL);
		}
		else if(SubSlotSize == 4) {
			FillSineBuffer(globalSineBuffer, true);
			device.SetDACCallback(FillSineData4, NULL);
		}
		device.SetSampleRate(freq);
		device.Start();
		printf("WidgetTest: Press any key to stop...\n");
		_getch();
		device.Stop();
	}

	else if (mode == 1) { // wav file player
		int paused = 0;				// 0 = not paused, loop through songs. 1 = paused, play music. -1 = terminate playback
		char command;
		int NumChannels = 0;
		int SampleRate = 0;
		int prevSampleRate = 0;
		int BytesPerSample = 0;
		long NumSamples = 0;		// The number of samples in the wav file, independant of sample format
		FILE * wavfile;
		int n;						// Wav file counter
		DWORD PreFetchIndex, PreFetchIndexEnd;
		AudioSample3 DummySample3;
		AudioSample4 DummySample4;


		// http://www.dailyfreecode.com/code/find-first-occurrence-character-string-2235.aspx
		char *ptr = strchr(argv[1], '-');
		if (ptr-argv[1] == 0) {		// Parameters! Test if the '-' came first in argv[1]
			if (strchr(argv[1], 'v'))	// Verbose!
				globalVerbose = 1;

			if (strchr(argv[1], 's')) {	// Silent!
				globalVerbose = 0;
				globalSilent = 1;
			}

			if (strchr(argv[1], 'c')) 	// CPU time logging
				globalCPUTimeMonitor = 1;

			if (strchr(argv[1], 'f')) 	// Pre fetch
				globalPreFetch = 1;

			n = 2;					// Start playing wav file(s) from argv[2]
		}
		else
			n = 1;					// Start playing wav files from argv[1]
			
		if (globalVerbose)
			printf("WidgetTest: Running as wav file player.\n");

		// First check for valid playback device
		USBAudioDevice device(true);
		if (!device.InitDevice()) {
			if (!globalSilent)
				printf ("WidgetTest: ERROR: UAC2 Audio device not found\n");
			return -1;
		}
		SubSlotSize = device.GetDACSubslotSize();

		// Set the data filling callback function. It should initially output zeros 
		// and NOT touch the not-yet-created global buffers.
		if(SubSlotSize == 3)
			device.SetDACCallback(FillWavData3, NULL);
		else if(SubSlotSize == 4)
			device.SetDACCallback(FillWavData4, NULL);

		// Open a sequence of wav files

		while ( (n<argc) && (paused != -1) ) {			// n++ happens at end of while loop
			wavfile = fopen(argv[n],"rb");				// fopen_s is recommended...

			if (wavfile==NULL) {
				if (!globalSilent)
					printf ("\nWidgetTest: WARNING: File not found: %s\n",argv[n]);
				n++;									// Here due to following continue statement
				continue;
			}

			if (!globalSilent)
				printf ("\nWidgetTest: Found file: %s\n",argv[n]);

			n++;										// Prepare for next wav file, above continue didn't happen

			if (!ParseWavHeader (wavfile, &NumChannels, &SampleRate, &BytesPerSample, &NumSamples))
				continue;								// Function does its own error reporting

			if (globalVerbose)
				printf ("WidgetTest: Start reading wav file\n");

			// Don't yet touch globalNumParsedSamples. Playback of nonzero audio data 
			// starts when it becomes nonzero!
			if(SubSlotSize == 3) {
				globalWavBuffer3 = new AudioSample3[NumSamples];
				tempNumParsedSamples = FillWavBuffer3 (globalWavBuffer3, wavfile, &BytesPerSample, &NumSamples);
			}
			else if(SubSlotSize == 4) {
				globalWavBuffer4 = new AudioSample4[NumSamples];
				tempNumParsedSamples = FillWavBuffer4 (globalWavBuffer4, wavfile, &BytesPerSample, &NumSamples);
			}
			if (globalVerbose)
				printf ("WidgetTest: tempNumParsedSamples=%d NumSamples=%d\n", tempNumParsedSamples, NumSamples);

			fclose (wavfile);

			if (tempNumParsedSamples == NumSamples) {
				if (globalVerbose)
					printf ("WidgetTest: End reading wav file\n");

				if ( (prevSampleRate != SampleRate) && (prevSampleRate != 0) ) {
					device.Stop();						// When on >1st wav file, and with new s-rate, first stop device
					if (globalVerbose)
						printf("WidgetTest: device.Stop()\n");
				}

				if (prevSampleRate != SampleRate) {		// With 1st wav file, prevSampleRate==0. For 1st wav file and
					device.SetSampleRate(SampleRate);	// sample rate changes, change the rate and restart device
					device.Start();
					if (globalVerbose)
						printf("WidgetTest: device.SetSampleRate() and device.Start()\n");
				}

				prevSampleRate = SampleRate;			// Record sample rate changes

				// Now play the darn thing :-) 
				// It's the last action before the wait loop
				paused = 0;
				globalNumParsedSamples = tempNumParsedSamples; 

				// Wait loop while music hopefully plays
				if (!globalSilent) {
					printf("WidgetTest: Use commands 'p', 'r', 't' or other key.\n");
				}

				while ( ((globalBufferIndex < globalNumParsedSamples) && (paused != -1)) || (paused == 1) ) {

					// Print out callback response time every 300ms
					if (globalCPUTimeMonitor) {
						globalCPUTimeMax = 0;
						globalCPUTimeMin = 0x7FFFFFFFFFFFFFFF; // A fairly large 64-bit number. YES, it is unsigned, but still!

						if (globalPreFetch) {
							// Look ahead in RAM at the audio samples before they are consumed by callback
							// function
							// Start 1/4s ahead of globalBufferIndex
							PreFetchIndex = globalBufferIndex + (SampleRate>>2);
							if (PreFetchIndex >= globalNumParsedSamples)
								PreFetchIndex = globalNumParsedSamples;	
							// End 1/4 + 1/2 s ahead of globalBufferIndex
							PreFetchIndexEnd = globalBufferIndex + (SampleRate>>2) + (SampleRate>1);
							if (PreFetchIndexEnd >= globalNumParsedSamples)
								PreFetchIndexEnd = globalNumParsedSamples;	

							while (PreFetchIndex < PreFetchIndexEnd) {
								if(SubSlotSize == 3)
									DummySample3.left = globalWavBuffer3[PreFetchIndex].left;
								else if(SubSlotSize == 4)
									DummySample4.left = globalWavBuffer4[PreFetchIndex].left;
								PreFetchIndex++;
							}
							// FIX: verify: Is this code really executed when DummySample? isn't used anywhere?
						} // globalPreFetch

						Sleep(300);
						printf ("WidgetTest: Min:%8I64d Max:%8I64d Last:%8I64d\n", globalCPUTimeMin, globalCPUTimeMax, globalCPUTimeLast);
					}
					else {
						if (globalPreFetch) {
							// Look ahead in RAM at the audio samples before they are consumed by callback
							// function
							// Start 1/32s ahead of globalBufferIndex
							PreFetchIndex = globalBufferIndex + (SampleRate>>5);
							if (PreFetchIndex >= globalNumParsedSamples)
								PreFetchIndex = globalNumParsedSamples;	
							// End 1/8s ahead of globalBufferIndex
							PreFetchIndexEnd = globalBufferIndex + (SampleRate>>3);
							if (PreFetchIndexEnd >= globalNumParsedSamples)
								PreFetchIndexEnd = globalNumParsedSamples;	

							while (PreFetchIndex < PreFetchIndexEnd) {
								if(SubSlotSize == 3)
									DummySample3.left = globalWavBuffer3[PreFetchIndex].left;
								else if(SubSlotSize == 4)
									DummySample4.left = globalWavBuffer4[PreFetchIndex].left;
								PreFetchIndex++;
							}
							// FIX: verify: Is this code really executed when DummySample? isn't used anywhere?
						} // globalPreFetch

						Sleep(50);						// ms sleep between keyboard polls / termination checks
					}

					if (_kbhit()) {
						command = _getch();
						if (globalVerbose)
							printf ("WidgetTest: Got command '%c'\n", command);

						if (command == 'p') {			// 't' toggles pause mode
							if (paused) {
								if (!globalSilent)
									printf ("WidgetTest: Un-pausing\n");
								paused = 0;				// Un-pause, restore index
								globalNumParsedSamples = tempNumParsedSamples;
							}
							else {
								if (!globalSilent)
									printf ("WidgetTest: Pausing\n");
								paused = 1;				// Pause, back up index to recycled variable
								tempNumParsedSamples = globalNumParsedSamples;
								globalNumParsedSamples = 0;	// Cause zeros playback
							}
						}

						else if (command == 'r') {		// 'r' replays track in RAM
							globalBufferIndex = 0;
						}

						else if (command == 't') {		// 't' terminates playback
							paused = -1;
						}

						else {							// All other commands terminate playback of current file
							if (globalVerbose)
								printf ("WidgetTest: Terminating this track\n");
							paused = 0;					// Un-pause at termination
							globalNumParsedSamples = 0;	// We haven't yet read anything from the next wav file
							globalBufferIndex = 0;		// This instructs FilWavData? to dump zeros
						}

					}
				}

				// Reaching end of samples will result in zero-data playback, 
				// even without below 2 lines of code. But with below 2 lines,
				// reaching end or keypress will put callback function into zeros mode
				globalNumParsedSamples = 0;				// We haven't yet read anything from the next wav file
				globalBufferIndex = 0;					// This instructs FilWavData? to dump zeros

				if (_kbhit())
					_getch();

				if (globalVerbose)
					printf("WidgetTest: globalBufferIndex=%d globalNumParsedSamples=%d\n", globalBufferIndex, globalNumParsedSamples);
			} // globalNumParsedSamples OK
			else {
				if (!globalSilent)		
					printf ("WidgetTest: ERROR: Wav file not read into memory\n");
				return -1;
			}

			// Replicate here for good measure
			globalNumParsedSamples = 0;					// We haven't yet read anything from the next wav file
			globalBufferIndex = 0;						// This instructs FilWavData? to dump zeros

			// Is it now safe to delete globalWavBuffer? So it seems from basic tests.
			if(SubSlotSize == 3)
				delete [] globalWavBuffer3;
			else if(SubSlotSize == 4)
				delete [] globalWavBuffer4;

		} // while n wav files

		device.Stop();

	} // wav player

	_CrtDumpMemoryLeaks();
	return 0;
} // main()

/*
Todo:

- Smaller memory footprint
- Determine how callback is called (thread?) See mail from Nikolay
- Measure and optimize lateny
  + Differential CPU time measurement in callback
  - Preventing swapping of memory
  - Make callback thread(?) more real-time-y
- Improve on USB feedback poll rate understanding

*/