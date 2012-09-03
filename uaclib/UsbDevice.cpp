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

#include "UsbDevice.h"


union DESCRIPTORS_UNION
{
	BYTE*							Bytes;
	USB_DESCRIPTOR_HEADER*			Common;
	USB_CONFIGURATION_DESCRIPTOR*	Config;
	USB_INTERFACE_DESCRIPTOR*		Interface;
	USB_ENDPOINT_DESCRIPTOR*		Endpoint;
};

/*
struct DeviceID
{
	USHORT vid;
	USHORT pid;
};

DeviceID deviceID[] = { 
	{0x16C0, 0x03E8},
	{0x16C0, 0x05dc}
};

int deviceIDNum = sizeof(deviceID)/sizeof(DeviceID);
*/

#define _DeviceInterfaceGUID "{09e4c63c-ce0f-168c-1862-06410a764a35}"

USBDevice::USBDevice() : m_usbDeviceHandle(NULL), m_errorCode(ERROR_SUCCESS), m_deviceSpeed(HighSpeed), m_deviceMutex(NULL)
{
	InitDescriptors();
#ifdef _DEBUG
	debugPrintf("ASIOUAC: USBDevice()\n");
#endif
}

USBDevice::~USBDevice()
{
	FreeDevice();
#ifdef _DEBUG
	debugPrintf("ASIOUAC: ~USBDevice()\n");
#endif
	if(m_deviceMutex)
		CloseHandle(m_deviceMutex);
}

void USBDevice::FreeDevice()
{
    // Close the device handle
    // if handle is invalid (NULL), has no effect
    UsbK_Free(m_usbDeviceHandle);
}

void USBDevice::InitDescriptors()
{
	memset(&m_deviceDescriptor, 0, sizeof(USB_DEVICE_DESCRIPTOR));
	memset(&m_configDescriptor, 0, sizeof(USB_CONFIGURATION_DESCRIPTOR));
	m_errorCode = ERROR_SUCCESS;
}

bool USBDevice::ParseDescriptors(BYTE *configDescr, DWORD length)
{
	DWORD remaining = length;

	DESCRIPTORS_UNION curDescrPtr;
	USB_INTERFACE_DESCRIPTOR* interfaceDescriptor = NULL;
	USB_ENDPOINT_DESCRIPTOR* endpointDescriptor = NULL;

	while(remaining > 0)
	{
		curDescrPtr.Bytes = configDescr;
		UCHAR curLen = curDescrPtr.Common->bLength;
		if(curDescrPtr.Common->bDescriptorType == USB_DESCRIPTOR_TYPE_CONFIGURATION)
			memcpy(&m_configDescriptor, curDescrPtr.Config, sizeof(USB_CONFIGURATION_DESCRIPTOR));
		else
			ParseDescriptorInternal(curDescrPtr.Common);
		remaining -= curLen;
		configDescr += curLen;
	}

	return TRUE;
}

//bool USBDevice::SendUsbControl(int dir, int type, int recipient, int request, int value, int index,
//				   unsigned char *buff, int size, ULONG *lengthTransferred)
bool USBDevice::SendUsbControl(int dir, int type, int recipient, int request, int value, int index,
				   unsigned char *buff, int size, UINT *lengthTransferred)
{
	bool retVal = FALSE;
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
	if(UsbK_ControlTransfer(m_usbDeviceHandle, packet, buff, size, lengthTransferred, NULL))
		return TRUE;
	m_errorCode = GetLastError();
#ifdef _DEBUG
	debugPrintf("ASIOUAC: UsbK_ControlTransfer failed. ErrorCode: %08Xh\n",  m_errorCode);
#endif
	return FALSE;
}

KUSB_HANDLE USBDevice::FindDevice()
{
	KLST_HANDLE DeviceList = NULL;
	KUSB_HANDLE handle = NULL;

	KLST_DEVINFO_HANDLE DeviceInfo = NULL;
	KLST_DEVINFO_HANDLE tmpDeviceInfo = NULL;

//	ULONG deviceCount = 0;
	UINT deviceCount = 0;
	m_errorCode = ERROR_SUCCESS;

	// Get the device list
	if (!LstK_Init(&DeviceList, KLST_FLAG_NONE))
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Error initializing device list.\n");
#endif
		return NULL;
	}

	LstK_Count(DeviceList, &deviceCount);
	if (!deviceCount)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: No devices in device list.\n");
#endif
		SetLastError(ERROR_DEVICE_NOT_CONNECTED);
		// If LstK_Init returns TRUE, the list must be freed.
		LstK_Free(DeviceList);
		return NULL;
	}

#ifdef _DEBUG
	debugPrintf("ASIOUAC: Looking for device with DeviceInterfaceGUID %s\n", _T(_DeviceInterfaceGUID));
#endif
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

	if (!DeviceInfo)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Device not found.\n");
#endif
		// If LstK_Init returns TRUE, the list must be freed.
		LstK_Free(DeviceList);
		return NULL;
	}

    // Initialize the device with the "dynamic" Open function
    if (!UsbK_Init(&handle, DeviceInfo))
    {
		handle = NULL;
        m_errorCode = GetLastError();
#ifdef _DEBUG
        debugPrintf("ASIOUAC: UsbK_Init failed. ErrorCode: %08Xh\n",  m_errorCode);
#endif
    }
	LstK_Free(DeviceList);
	return handle;
}

bool USBDevice::InitDevice()
{
	BYTE configDescriptorBuffer[4096];
//	ULONG lengthTransferred;
	UINT lengthTransferred;

	m_deviceMutex = CreateMutex(NULL, FALSE, "Global\\ASIOUAC2");
	if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Can't start device! Device already used!\n");
#endif
		if(m_deviceMutex)
		{
			CloseHandle(m_deviceMutex);
			m_deviceMutex = NULL;
			m_errorCode = ERROR_BUSY;
		}
		return FALSE;
	}

	FreeDevice();
	InitDescriptors();

	m_usbDeviceHandle = FindDevice();
	if(m_usbDeviceHandle == NULL)
		return FALSE;

	lengthTransferred = sizeof(configDescriptorBuffer);
	if(!UsbK_QueryDeviceInformation( m_usbDeviceHandle, DEVICE_SPEED, &lengthTransferred, configDescriptorBuffer))
	{
		m_errorCode = GetLastError();
#ifdef _DEBUG
        debugPrintf("ASIOUAC: UsbK_QueryDeviceInformation failed. ErrorCode: %08Xh\n",  m_errorCode);
#endif
	    UsbK_Free(m_usbDeviceHandle);
		m_usbDeviceHandle = NULL;
		return FALSE;
	}
	m_deviceSpeed = (int)configDescriptorBuffer[0];
#ifdef _DEBUG
	if(m_deviceSpeed == HighSpeed)
        debugPrintf("ASIOUAC: Device speed: high\n");
	else
        debugPrintf("ASIOUAC: Device speed: low/full\n");
#endif
	if(!UsbK_GetDescriptor(m_usbDeviceHandle, USB_DESCRIPTOR_TYPE_DEVICE, 0, 0, configDescriptorBuffer, sizeof(configDescriptorBuffer), &lengthTransferred))
	{
        m_errorCode = GetLastError();
#ifdef _DEBUG
        debugPrintf("ASIOUAC: Get device descriptor failed. ErrorCode: %08Xh\n",  m_errorCode);
#endif
	    UsbK_Free(m_usbDeviceHandle);
		m_usbDeviceHandle = NULL;
		return FALSE;
	}
	memcpy(&m_deviceDescriptor, configDescriptorBuffer, sizeof(USB_DEVICE_DESCRIPTOR));

	if(SendUsbControl(BMREQUEST_DIR_DEVICE_TO_HOST, BMREQUEST_TYPE_STANDARD, BMREQUEST_RECIPIENT_DEVICE, 
					USB_REQUEST_GET_DESCRIPTOR, (USB_DESCRIPTOR_TYPE_CONFIGURATION << 8), 0,
					configDescriptorBuffer, sizeof(configDescriptorBuffer), &lengthTransferred))
	{
		ParseDescriptors(configDescriptorBuffer, lengthTransferred);
		return m_usbDeviceHandle != NULL;
	}
	else
	{
        m_errorCode = GetLastError();
#ifdef _DEBUG
        debugPrintf("ASIOUAC: Get config descriptor failed. ErrorCode: %08Xh\n",  m_errorCode);
#endif
	    UsbK_Free(m_usbDeviceHandle);
		m_usbDeviceHandle = NULL;
		return FALSE;
	}
}

