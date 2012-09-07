/*!
#
# dump2wav. Windows related software for Audio-Widget/SDR-Widget (http://code.google.com/p/sdr-widget/)
# Copyright (C) 2012 Børge Strand-Bergesen
#
# Permission to copy, use, modify, sell and distribute this software 
# is granted provided this copyright notice appears in all copies. 
# This software is provided "as is" without express or implied
# warranty, and with no claim as to its suitability for any purpose.
#
#----------------------------------------------------------------------------
# Contact: borge.strand@gmail.com
#----------------------------------------------------------------------------
*/


#include "stdafx.h"

int _tmain(int argc, _TCHAR* argv[])
{
	FILE *infile, *outfile;
	int copybuffer[4096];
	int infilesize;
	int samplerate;
	int n, m;

/*  See https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
    Below header is for 32 bit data and 44.1ksps. It contains offset 0:35. 
    Next to be written to .wav file after header is "data" */

	unsigned char wavheader[] = {
		'R', 'I', 'F', 'F',			// Chunk ID
		0x74, 0x00, 0x00, 0x00,		// SubChunk2Size, 36 + bytes of audio data
		'W', 'A', 'V', 'E',			// Format
		'f', 'm', 't', 0x20,		// Subchunk1ID
		0x10, 0x00, 0x00, 0x00,		// Subchunk1Size, 16 for PCM
		0x01, 0x00,					// AudioFormat, 1 for PCM
		0x02, 0x00,					// NumChannels
		0x44, 0xAC, 0x00, 0x00,		// SampleRate
		0x20, 0x62, 0x05, 0x00,		// ByteRate = SampleRate * NumChannels * BitsPerSample / 8
		0x08, 0x00,					// BlockAlign = NumChannels * BitsPerSample / 8
		0x20, 0x00,					// BitsPerSample
		'd', 'a', 't', 'a',			// Subchunk2ID
		0x00, 0x00, 0x00, 0x00};	// SubChunk2Size

	if (argc != 4) 
	{
		printf("You must specify an sample rate (44, 48, .. , 192), an input file and an output file \n");
		return 0;
	}

	samplerate = atoi(argv[1]);

	// Insert sample rate into wav file header, format is 32-bit little-endian in position 24:27
	if (samplerate == 44)
	{
		wavheader[24] = 0x44;	wavheader[25] = 0xAC;
		wavheader[26] = 0x00;	wavheader[27] = 0x00;
	}
	else if (samplerate == 48)
	{
		wavheader[24] = 0x80;	wavheader[25] = 0xBB;
		wavheader[26] = 0x00;	wavheader[27] = 0x00;
	}
	else if (samplerate == 88)
	{
		wavheader[24] = 0x88;	wavheader[25] = 0x58;
		wavheader[26] = 0x01;	wavheader[27] = 0x00;
	}
	else if (samplerate == 96)
	{
		wavheader[24] = 0x00;	wavheader[25] = 0x77;
		wavheader[26] = 0x01;	wavheader[27] = 0x00;
	}
	else if (samplerate == 176)
	{
		wavheader[24] = 0x10;	wavheader[25] = 0xB1;
		wavheader[26] = 0x02;	wavheader[27] = 0x00;
	}
	else if (samplerate == 192)
	{
		wavheader[24] = 0x00;	wavheader[25] = 0xEE;
		wavheader[26] = 0x02;	wavheader[27] = 0x00;
	}
	else
	{
		printf ("Sample rate %d not supported\n", samplerate);
		return 0;
	}

	if ((fopen_s(&infile, argv[2], "rb"))!=0) {
		printf("Infile not found: %s\n", argv[2]);
		return 0;
	}

	// Determine size of input file FIX: what about > 2^32-37 byte files?
	fseek(infile, 0L, SEEK_END);
	infilesize = ftell(infile);
	fseek(infile, 0L, SEEK_SET);

	if ((fopen_s(&outfile, argv[3], "wb"))!=0) 
	{
		printf("Outfile not available: %s\n", argv[3]);
		return 0;
	}

	// Save SubChunk2Size+36 to offset 4:7, 32-bit little-endian number 
	wavheader[4] = (unsigned char) ((infilesize + 36) & 0x000000FF);		// Least significant byte
	wavheader[5] = (unsigned char)(((infilesize + 36) & 0x0000FF00)>>8);	// Intermediate byte
	wavheader[6] = (unsigned char)(((infilesize + 36) & 0x00FF0000)>>16);	// Intermediate byte
	wavheader[7] = (unsigned char)(((infilesize + 36) & 0xFF000000)>>24);	// Most significant

	// Save SubChunk2Size to offset 40:43, 32-bit little-endian number 
	wavheader[40] = (unsigned char) ((infilesize) & 0x000000FF);		// Least significant byte
	wavheader[41] = (unsigned char)(((infilesize) & 0x0000FF00)>>8);	// Intermediate byte
	wavheader[42] = (unsigned char)(((infilesize) & 0x00FF0000)>>16);	// Intermediate byte
	wavheader[43] = (unsigned char)(((infilesize) & 0xFF000000)>>24);	// Most significant

	// Write wav header
	fwrite(wavheader, 1, 44, outfile);


	// Transfer 2048 32-bit stereo samples at a time
	m = 0;
	do 
	{
		// Try to read 2048 32-bit stereo samples
		n = fread (copybuffer, 4, 4096, infile); 
		m += n;

		// Write the read samples
		fwrite(copybuffer, 4, n, outfile);
	} while (n == 4096); // At the end of infile < 4096 words were read
	printf("Copied %d stereo samples\n", m/2);

	fclose (infile);
	fclose (outfile);
	return 0;
}

