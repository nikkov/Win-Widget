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
/*
# Copyright (c) 2011 Travis Robinson <libusbdotnet@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
# 	  
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL TRAVIS LEE ROBINSON 
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
# THE POSSIBILITY OF SUCH DAMAGE. 
#
*/

// Simple test application for play test signal
// Contains parts from LibUsbK examples by Travis Lee Robinson (http://libusb-win32.sourceforge.net/libusbKv3/)


#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "libusbk.h"
#include "usb_audio.h"
#include "AudioTask.h"

#define WIDGET
/*
#ifdef WIDGET
//#define WIDGET_VID			0x16C0
//#define WIDGET_PID			0x03E8
#define EP_TRANSFER_OUT         0x02
#define EP_TRANSFER_IN          0x81
//#define EP_PACKET_SIZE          4
#define ISO_PACKETS_PER_XFER    16
#else
#define WIDGET_VID			0x08BB
#define WIDGET_PID			0x2902
// #define EP_TRANSFER_IN             0x84
#define EP_TRANSFER_OUT             0x02
#define EP_PACKET_SIZE          90
#define ISO_PACKETS_PER_XFER    10
#endif
*/
#define _DeviceInterfaceGUID "{09e4c63c-ce0f-168c-1862-06410a764a35}"


#define TEST_VERSION		0x03

void debugPrintf(const char *szFormat, ...)
{
    char str[4096];
    va_list argptr;
    va_start(argptr, szFormat);
    vsprintf_s(str, szFormat, argptr);
    va_end(argptr);

    OutputDebugStringA(str);
}


BOOL isUAC1 = false;

ULONG LoadStringDescriptor(KUSB_HANDLE handle, UCHAR index, PUCHAR  Buffer, ULONG  BufferLength)
{
	ULONG  LengthTransferred = 0;
	if(!UsbK_GetDescriptor( handle, USB_DESCRIPTOR_TYPE_STRING, index, 0, Buffer, BufferLength, &LengthTransferred))
		return 0;
	return LengthTransferred;
}

BOOL Unicode16ToAnsi(WCHAR *in_Src, CHAR *out_Dst, INT in_MaxLen)
{
    /* locals */
    INT  lv_Len;
    BOOL lv_UsedDefault;
  
  // do NOT decrease maxlen for the eos
  if (in_MaxLen <= 0)
    return FALSE;

  // let windows find out the meaning of ansi
  // - the SrcLen=-1 triggers WCTMB to add a eos to Dst and fails if MaxLen is too small.
  // - if SrcLen is specified then no eos is added
  // - if (SrcLen+1) is specified then the eos IS added
  lv_Len = WideCharToMultiByte(
     CP_ACP, 0, in_Src, -1, out_Dst, in_MaxLen, 0, &lv_UsedDefault);

  // validate
  if (lv_Len < 0)
    lv_Len = 0;

  // ensure eos, watch out for a full buffersize
  // - if the buffer is full without an eos then clear the output like WCTMB does
  //   in case of too small outputbuffer
  // - unfortunately there is no way to let WCTMB return shortened strings,
  //   if the outputbuffer is too small then it fails completely
  if (lv_Len < in_MaxLen)
    out_Dst[lv_Len] = 0;
  else if (out_Dst[in_MaxLen-1])
    out_Dst[0] = 0;

  // return whether invalid chars were present
  return !lv_UsedDefault;
}

// Globals
KUSB_DRIVER_API Usb;


BOOL SendUsbControl(KUSB_HANDLE handle, int interfaceNumber, 
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
    if(Usb.ClaimInterface(handle, interfaceNumber, FALSE))
	{
		if(Usb.ControlTransfer(handle, packet, buff, size, lengthTransferred, NULL))
			retVal = TRUE;

		Usb.ReleaseInterface(handle, interfaceNumber, FALSE);
	}
	return retVal;
}


int GetRangeFreq(KUSB_HANDLE handle, int interfaceNumber, int clockId)
{
	unsigned char buff[64];
	ULONG lengthTransferred = 0;
	if(SendUsbControl(handle, interfaceNumber, BMREQUEST_DIR_DEVICE_TO_HOST, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_INTERFACE, 
		AUDIO_CS_REQUEST_RANGE, AUDIO_CS_CONTROL_SAM_FREQ << 8, (clockId << 8) + interfaceNumber,
				   buff, sizeof(buff), &lengthTransferred))
	{
		if(lengthTransferred > 2)
		{
			unsigned short length = *((unsigned short*)buff);
			struct sample_rate_triplets *triplets = (sample_rate_triplets *)(buff + 2);
			for(int i = 0; i < length; i++)
			{
				for(int freq = triplets[i].min_freq; freq <= triplets[i].max_freq; freq += triplets[i].res_freq)
				{
					printf("Supported freq: %d\n", freq);
				}
			}
			return length;
		}
	}
	return 0;
}


int GetCurrentFreq(KUSB_HANDLE handle, int interfaceNumber, int clockId)
{
	int freq = 0;
	ULONG lengthTransferred = 0;
	if(isUAC1)
	{
		if(SendUsbControl(handle, interfaceNumber, BMREQUEST_DIR_DEVICE_TO_HOST, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_ENDPOINT, 
			0x81, AUDIO_CS_CONTROL_SAM_FREQ << 8, interfaceNumber,
					   (unsigned char*)&freq, 3, &lengthTransferred) && lengthTransferred == 3)
		{
			return freq;
		}
	}
	else
		if(SendUsbControl(handle, interfaceNumber, BMREQUEST_DIR_DEVICE_TO_HOST, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_INTERFACE, 
			AUDIO_CS_REQUEST_CUR, AUDIO_CS_CONTROL_SAM_FREQ << 8, (clockId << 8) + interfaceNumber,
					   (unsigned char*)&freq, sizeof(freq), &lengthTransferred) && lengthTransferred == 4)
		{
			return freq;
		}
	return 0;
}


BOOL SetCurrentFreq(KUSB_HANDLE handle, int interfaceNumber, int clockId, int freq)
{
	ULONG lengthTransferred = 0;
	if(isUAC1)
	{
		if(SendUsbControl(handle, interfaceNumber, BMREQUEST_DIR_HOST_TO_DEVICE, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_ENDPOINT, 
			AUDIO_CS_REQUEST_CUR, AUDIO_CS_CONTROL_SAM_FREQ << 8, interfaceNumber,
					   (unsigned char*)&freq, 3, &lengthTransferred) && lengthTransferred == 3)
		{
			return TRUE;
		}
	}
	else
		if(SendUsbControl(handle, interfaceNumber, BMREQUEST_DIR_HOST_TO_DEVICE, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_INTERFACE, 
			AUDIO_CS_REQUEST_CUR, AUDIO_CS_CONTROL_SAM_FREQ << 8, (clockId << 8) + interfaceNumber,
					   (unsigned char*)&freq, sizeof(freq), &lengthTransferred) && lengthTransferred == 4)
		{
			return TRUE;
		}
	return FALSE;
}


int main(int argc, char* argv[])
{
	do
	{
		KLST_FLAG Flags = KLST_FLAG_INCLUDE_DISCONNECT;//KLST_FLAG_NONE;
		ULONG deviceCount = 0;
		DWORD errorCode = ERROR_SUCCESS;
		UCHAR  Buffer[4096];
		ULONG  BufferLength = sizeof(Buffer);
		ULONG  LengthTransferred = 0;

		KLST_HANDLE DeviceList = NULL;
		KUSB_HANDLE handle = NULL;
		KLST_DEVINFO_HANDLE DeviceInfo = NULL;
		KLST_DEVINFO_HANDLE tmpDeviceInfo = NULL;
		USB_DEVICE_DESCRIPTOR deviceDescriptor;
		USB_CONFIGURATION_DESCRIPTOR configurationDescriptor;
		USB_INTERFACE_DESCRIPTOR interfaceDescriptor;
		WINUSB_PIPE_INFORMATION     gPipeInfoRead;
		WINUSB_PIPE_INFORMATION     gPipeInfoWrite;
		AudioTask *readtask = NULL;
		AudioTask *writetask = NULL;

		printf("WidgetTest version %X\n", TEST_VERSION);

		memset(&interfaceDescriptor, 0, sizeof(interfaceDescriptor));
		// Select interface by pipe id and get descriptors.
		memset(&gPipeInfoRead, 0, sizeof(gPipeInfoRead));
		memset(&gPipeInfoWrite, 0, sizeof(gPipeInfoWrite));

		//printf("Press any key for continue!\n");
		//_getch();

		// Get the device list
		if (!LstK_Init(&DeviceList, Flags))
		{
			printf("Error initializing device list.\n");
			goto Done1;
		}

		LstK_Count(DeviceList, &deviceCount);
		if (!deviceCount)
		{
			printf("No device not connected.\n");
			SetLastError(ERROR_DEVICE_NOT_CONNECTED);
			// If LstK_Init returns TRUE, the list must be freed.
			LstK_Free(DeviceList);
			goto Done1;
		}


		printf("Looking for device with DeviceInterfaceGUID %s\n", _DeviceInterfaceGUID);
		LstK_MoveReset(DeviceList);
		//
		//
		// Call LstK_MoveNext after a LstK_MoveReset to advance to the first
		// element.
		while(LstK_MoveNext(DeviceList, &tmpDeviceInfo)
			&& DeviceInfo == NULL)
		{
			if(!_stricmp(tmpDeviceInfo->DeviceInterfaceGUID, _DeviceInterfaceGUID) && tmpDeviceInfo->Connected)
			{
				DeviceInfo = tmpDeviceInfo;
				break;
			}
		}

	//	LstK_FindByVidPid(DeviceList, WIDGET_VID, WIDGET_PID, &DeviceInfo);
		if (!DeviceInfo)
		{
			printf("Device with DeviceInterfaceGUID %snot found.\n", _DeviceInterfaceGUID);
			// If LstK_Init returns TRUE, the list must be freed.
			LstK_Free(DeviceList);
			goto Done1;
		}


		// load a dynamic driver api for this device.  The dynamic driver api
		// is more versatile because it adds support for winusb.sys devices.
		if (!LibK_LoadDriverAPI(&Usb, DeviceInfo->DriverID))
		{
			errorCode = GetLastError();
			printf("LibK_LoadDriverAPI failed. ErrorCode: %08Xh\n",  errorCode);
			goto Done1;
		}

		// Display some information on the driver api type.
		switch(DeviceInfo->DriverID)
		{
		case KUSB_DRVID_LIBUSBK:
			printf("libusbK driver api loaded!\n");
			break;
		case KUSB_DRVID_LIBUSB0:
			printf("libusb0 driver api loaded!\n");
			break;
		case KUSB_DRVID_WINUSB:
			printf("WinUSB driver api loaded!\n");
			break;
		case KUSB_DRVID_LIBUSB0_FILTER:
			printf("libusb0/filter driver api loaded!\n");
			break;
		}

		/*
		From this point forth, do not use the exported "UsbK_" functions. Instead,
		use the functions in the driver api initialized above.
		*/

		// Initialize the device with the "dynamic" Open function
		if (!Usb.Init(&handle, DeviceInfo))
		{
			errorCode = GetLastError();
			printf("Usb.Init failed. ErrorCode: %08Xh\n",  errorCode);
			goto Done1;
		}
		printf("Device opened successfully!\n");

		BufferLength = sizeof(Buffer);
		if(!Usb.QueryDeviceInformation( handle, DEVICE_SPEED, &BufferLength, Buffer))
		{
			errorCode = GetLastError();
			printf("UsbK_QueryDeviceInformation failed. ErrorCode: %08Xh\n",  errorCode);
			goto Done1;
		}
		if(Buffer[0] == 0x3)
		{
			printf("Device speed: high\n");
		}
		else
		{
			printf("Device speed: low/full\n");
		}

		if(!Usb.GetDescriptor( handle, USB_DESCRIPTOR_TYPE_DEVICE, 0, 0, (PUCHAR)&deviceDescriptor, sizeof(deviceDescriptor), &LengthTransferred))
		{
			errorCode = GetLastError();
			printf("UsbK_GetDescriptor failed. ErrorCode: %08Xh\n",  errorCode);
			goto Done1;
		}

		if(!Usb.GetDescriptor( handle, USB_DESCRIPTOR_TYPE_CONFIGURATION, 0, 0, Buffer, sizeof(Buffer), &LengthTransferred))
		{
			errorCode = GetLastError();
			printf("UsbK_GetDescriptor failed. ErrorCode: %08Xh\n",  errorCode);
			goto Done1;
		}

		memcpy(&configurationDescriptor, Buffer, sizeof(USB_CONFIGURATION_DESCRIPTOR));

		BOOL r;
	/*
		GetRangeFreq(handle, 1, 5);
		int freq = GetCurrentFreq(handle, 1, 5);
		r = SetCurrentFreq(handle, 1, 5, 44100);
		freq = GetCurrentFreq(handle, 1, 5);
		r = SetCurrentFreq(handle, 1, 5, 48000);
	*/

		//printf("Try found output pipe id %X and feedback id %X\n", EP_TRANSFER_OUT, EP_TRANSFER_IN);
		printf("Try found output and feedback pipe\n");

		UCHAR interfaceIndex = (UCHAR) - 1;
		ULONG lengthTranserred = 0;

		while(gPipeInfoRead.PipeId == 0 && gPipeInfoWrite.PipeId == 0 && 
			Usb.SelectInterface(handle, ++interfaceIndex, TRUE))
		{
			memset(&interfaceDescriptor, 0, sizeof(interfaceDescriptor));
			UCHAR gAltsettingNumber = (UCHAR) - 1;
			while(Usb.QueryInterfaceSettings(handle, ++gAltsettingNumber, &interfaceDescriptor))
			{
				UCHAR pipeIndex = (UCHAR) - 1;
				while(Usb.QueryPipe(handle, gAltsettingNumber, ++pipeIndex, &gPipeInfoRead))
				{
					printf("Pipe id %x found.\n", gPipeInfoRead.PipeId);
					if (USB_ENDPOINT_DIRECTION_OUT(gPipeInfoRead.PipeId) || gPipeInfoRead.PipeType != UsbdPipeTypeIsochronous)
						memset(&gPipeInfoRead, 0, sizeof(gPipeInfoRead));
					else
						break;
				}
				pipeIndex = (UCHAR) - 1;
				while(Usb.QueryPipe(handle, gAltsettingNumber, ++pipeIndex, &gPipeInfoWrite))
				{
					if (!USB_ENDPOINT_DIRECTION_OUT(gPipeInfoWrite.PipeId) || gPipeInfoWrite.PipeType != UsbdPipeTypeIsochronous)
						memset(&gPipeInfoWrite, 0, sizeof(gPipeInfoWrite));
					else
						break;
				}
				if (gPipeInfoRead.PipeId && gPipeInfoWrite.PipeId) break;
				memset(&interfaceDescriptor, 0, sizeof(interfaceDescriptor));
			}
		}

		if (!gPipeInfoRead.PipeId)
		{
			printf("Input pipe not found.\n");
			goto Done1;
		}
		if (!gPipeInfoWrite.PipeId)
		{
			printf("Output pipe not found.\n");
			goto Done1;
		}

		if(gPipeInfoWrite.Interval == 1)
			isUAC1 = true;

		r = Usb.ClaimInterface(handle, interfaceDescriptor.bInterfaceNumber, FALSE);
		r = Usb.SetAltInterface(handle, interfaceDescriptor.bInterfaceNumber, FALSE, interfaceDescriptor.bAlternateSetting); 


		int freq = 48000;
		if(argc > 1) 
			freq = atoi(argv[1]);

		if(!isUAC1)
		{
			r = SetCurrentFreq(handle, 1, 5, freq);
			freq = GetCurrentFreq(handle, 1, 5);
		}
		else
		{
			r = SetCurrentFreq(handle, 3, 0, freq);
			freq = GetCurrentFreq(handle, 3, 0);
		}

		printf("Using freq=%d\n", freq);

		float defaultPacketSize = isUAC1 ? 6.0f * (float)freq / 1000.0f :
			8.0f * (float)freq / 1000.0f / (float)(1 << gPipeInfoWrite.Interval);

		int packetSize = isUAC1 ? (int)defaultPacketSize + 6 :
			(int)defaultPacketSize + 8; //+ 1 stereo extra sample

		if(!isUAC1)
		{
			readtask = new AudioTask(handle, &gPipeInfoRead, 16, gPipeInfoRead.MaximumPacketSize, gPipeInfoRead.MaximumPacketSize, TRUE, 4);
			writetask = new AudioTask(handle, &gPipeInfoWrite, 32, packetSize, defaultPacketSize, FALSE, 4);
		}
		else
		{
			readtask = new AudioTask(handle, &gPipeInfoRead, 16, gPipeInfoRead.MaximumPacketSize, gPipeInfoRead.MaximumPacketSize, TRUE, 3);
 			writetask = new AudioTask(handle, &gPipeInfoWrite, 16, packetSize, defaultPacketSize, FALSE, 3);
		}

		printf("Press any key for exit.\n");

		readtask->Start();
		writetask->Start();

		_getch();
		writetask->Stop();
		readtask->Stop();

	Done1:
		if(interfaceDescriptor.bLength != 0)
		{
			r = Usb.SetAltInterface(handle, interfaceDescriptor.bInterfaceNumber, FALSE, 0);
			r = Usb.ReleaseInterface(handle, interfaceDescriptor.bInterfaceNumber, FALSE);
		}

		if(writetask)
			delete writetask;

		if(readtask)
			delete readtask;

		// Close the device handle
		// if handle is invalid (NULL), has no effect
		UsbK_Free(handle);

		// Free the device list
		// if deviceList is invalid (NULL), has no effect
		LstK_Free(DeviceList);
	} while( 0 );

	_CrtDumpMemoryLeaks();

	return 0;
}