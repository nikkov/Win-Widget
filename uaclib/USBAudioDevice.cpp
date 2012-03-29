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
	This code based on samples from library LibUsbK by Travis Robinson
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


#include "USBAudioDevice.h"


USBAudioDevice::USBAudioDevice(bool useInput) : m_fbInfo(), m_dac(NULL), m_adc(NULL), m_feedback(NULL), m_useInput(useInput),
	m_lastParsedInterface(NULL), m_lastParsedEndpoint(NULL), m_audioClass(0),
	m_dacEndpoint(NULL), m_adcEndpoint(NULL), m_fbEndpoint(NULL), m_notifyCallback(NULL), m_notifyCallbackContext(NULL), m_isStarted(FALSE)
{
	InitDescriptors();
}

USBAudioDevice::~USBAudioDevice()
{
	if(m_dac)
		delete m_dac;
	if(m_feedback)
		delete m_feedback;
	if(m_adc)
		delete m_adc;
}

void USBAudioDevice::InitDescriptors()
{
	//memset(&m_iad, 0, sizeof(USB_INTERFACE_ASSOCIATION_DESCRIPTOR));
	m_lastParsedInterface = NULL;
}

void USBAudioDevice::FreeDevice()
{
	FreeDeviceInternal();
	USBDevice::FreeDevice();
}

void USBAudioDevice::FreeDeviceInternal()
{
	InitDescriptors();
	if(m_dac)
		delete m_dac;
	if(m_feedback)
		delete m_feedback;
	if(m_adc)
		delete m_adc;

	m_feedback = NULL;
	m_adc = NULL;
	m_dac = NULL;
}

bool USBAudioDevice::ParseDescriptorInternal(USB_DESCRIPTOR_HEADER* uDescriptor)
{
	USB_INTERFACE_DESCRIPTOR* interfaceDescriptor;
	switch(uDescriptor->bDescriptorType)
	{
/*
		case USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION:
#ifdef _DEBUG
			debugPrintf("ASIOUAC: Found IAD descriptor\n");
#endif
			memcpy(&m_iad, uDescriptor, sizeof(USB_INTERFACE_ASSOCIATION_DESCRIPTOR));
		return TRUE;
*/
		case USB_DESCRIPTOR_TYPE_INTERFACE:
		{
			interfaceDescriptor = (USB_INTERFACE_DESCRIPTOR*)uDescriptor;
			switch(interfaceDescriptor->bInterfaceSubClass)
			{
				case AUDIO_INTERFACE_SUBCLASS_AUDIOCONTROL:
					{	
#ifdef _DEBUG
						debugPrintf("ASIOUAC: Found audio control interface 0x%X\n", interfaceDescriptor->bInterfaceNumber);
#endif
						USBAudioControlInterface *iACface = new USBAudioControlInterface(interfaceDescriptor);
						m_lastParsedInterface = iACface;
						m_acInterfaceList.Add(iACface);
					}
					return TRUE;

				case AUDIO_INTERFACE_SUBCLASS_AUDIOSTREAMING:
					{
#ifdef _DEBUG
						debugPrintf("ASIOUAC: Found audio streaming interface 0x%X (alt num 0x%X) with %d endpoints\n", interfaceDescriptor->bInterfaceNumber, 
							interfaceDescriptor->bAlternateSetting, interfaceDescriptor->bNumEndpoints);
#endif
						USBAudioStreamingInterface *iASface = new USBAudioStreamingInterface(interfaceDescriptor);
						m_lastParsedInterface = iASface;
						m_asInterfaceList.Add(iASface);
					}					
					return TRUE;
				default:
					m_lastParsedInterface = NULL;
					return FALSE;
			}
		}
		break;
		
		case CS_INTERFACE:
			if(m_lastParsedInterface)
			{
				bool retVal = m_lastParsedInterface->SetCSDescriptor(uDescriptor);
				if(m_audioClass == 0 && m_lastParsedInterface->Descriptor().bInterfaceSubClass == AUDIO_INTERFACE_SUBCLASS_AUDIOCONTROL)
					m_audioClass = ((USBAudioControlInterface*)m_lastParsedInterface)->m_acDescriptor.bcdADC == 0x200 ? 2 : 1;
				return retVal;
			}
		return FALSE;

		case USB_DESCRIPTOR_TYPE_ENDPOINT:
			if(m_lastParsedInterface)
			{
				m_lastParsedEndpoint = m_lastParsedInterface->CreateEndpoint((USB_ENDPOINT_DESCRIPTOR *)uDescriptor);
				return m_lastParsedEndpoint != NULL;
			}
		return FALSE;

		case CS_ENDPOINT:
			if(m_lastParsedEndpoint)
				return m_lastParsedEndpoint->SetCSDescriptor(uDescriptor);
		return FALSE;
	}

	return FALSE;
}

bool USBAudioDevice::InitDevice()
{
	if(!USBDevice::InitDevice())
		return FALSE;
	if(m_useInput)
	{
		USBAudioStreamingInterface * iface = m_asInterfaceList.First();
		while(iface)
		{
			USBAudioStreamingEndpoint * epoint = iface->m_endpointsList.First();
			while(epoint)
			{
				if(USB_ENDPOINT_DIRECTION_IN(epoint->m_descriptor.bEndpointAddress) && 
					(epoint->m_descriptor.bmAttributes & 0x03) == USB_ENDPOINT_TYPE_ISOCHRONOUS &&
					(epoint->m_descriptor.bmAttributes & 0x0C) != 0) //not feedback
				{
#ifdef _DEBUG
				    debugPrintf("ASIOUAC: Found input endpoint 0x%X\n",  (int)epoint->m_descriptor.bEndpointAddress);
#endif
					int channelNumber = 2;
					USBAudioOutTerminal* outTerm = FindOutTerminal(iface->m_asgDescriptor.bTerminalLink);
					if(outTerm)
					{
						USBAudioFeatureUnit* unit = FindFeatureUnit(outTerm->m_outTerminal.bSourceID);
						if(unit)
						{
							USBAudioInTerminal* inTerm = FindInTerminal(unit->m_featureUnit.bSourceID);
							if(inTerm)
								channelNumber = inTerm->m_inTerminal.bNrChannels;
						}
					}

					m_adc = new AudioADC();
					m_adc->Init(this, &m_fbInfo, epoint->m_descriptor.bEndpointAddress, 
						epoint->m_descriptor.wMaxPacketSize, 
						epoint->m_descriptor.bInterval, 
						channelNumber, 
						iface->m_formatDescriptor.bSubslotSize);
					m_adcEndpoint = epoint;
					break;
				}
				epoint = iface->m_endpointsList.Next(epoint);
			}
			if(m_adc != NULL)
				break;
			iface = m_asInterfaceList.Next(iface);
		}
		if(m_adc == NULL)
			m_useInput = FALSE;
	}

	if(m_adc == NULL)
	{
		USBAudioStreamingInterface * iface = m_asInterfaceList.First();
		while(iface)
		{
			USBAudioStreamingEndpoint * epoint = iface->m_endpointsList.First();
			while(epoint)
			{
				if(USB_ENDPOINT_DIRECTION_IN(epoint->m_descriptor.bEndpointAddress) && 
					(epoint->m_descriptor.bmAttributes & 0x03) == USB_ENDPOINT_TYPE_ISOCHRONOUS &&
					(epoint->m_descriptor.bmAttributes & 0x0C) == 0) //feedback
				{
#ifdef _DEBUG
					debugPrintf("ASIOUAC: Found feedback endpoint 0x%X\n",  (int)epoint->m_descriptor.bEndpointAddress);
#endif
					m_feedback = new AudioFeedback();
					m_feedback->Init(this, &m_fbInfo, epoint->m_descriptor.bEndpointAddress, epoint->m_descriptor.wMaxPacketSize, epoint->m_descriptor.bInterval, 4);
					m_fbEndpoint = epoint;
					break;
				}
				epoint = iface->m_endpointsList.Next(epoint);
			}
			if(m_feedback != NULL)
				break;
			iface = m_asInterfaceList.Next(iface);
		}
	}
//	return TRUE;

	USBAudioStreamingInterface * iface = m_asInterfaceList.First();
	while(iface)
	{
		if(!m_fbEndpoint || m_fbEndpoint->m_interface == iface) //out endpoint and feedback endpoint in same interface
		{
			USBAudioStreamingEndpoint * epoint = iface->m_endpointsList.First();
			while(epoint)
			{
				if(USB_ENDPOINT_DIRECTION_OUT(epoint->m_descriptor.bEndpointAddress) && 
					(epoint->m_descriptor.bmAttributes & 0x03) == USB_ENDPOINT_TYPE_ISOCHRONOUS)
				{
#ifdef _DEBUG
					debugPrintf("ASIOUAC: Found output endpoint 0x%X\n",  (int)epoint->m_descriptor.bEndpointAddress);
#endif
					int channelNumber = 2;
					USBAudioInTerminal* inTerm = FindInTerminal(iface->m_asgDescriptor.bTerminalLink);
					if(inTerm)
						channelNumber = inTerm->m_inTerminal.bNrChannels;

					m_dac = new AudioDAC();
					m_dac->Init(this, &m_fbInfo, epoint->m_descriptor.bEndpointAddress, epoint->m_descriptor.wMaxPacketSize, 
						epoint->m_descriptor.bInterval, 
						channelNumber, 
						iface->m_formatDescriptor.bSubslotSize);
					m_dacEndpoint = epoint;
					break;
				}
				epoint = iface->m_endpointsList.Next(epoint);
			}
		}
		if(m_dac != NULL)
			break;
		iface = m_asInterfaceList.Next(iface);
	}
	return TRUE;
}


bool USBAudioDevice::CheckSampleRate(USBAudioClockSource* clocksrc, int newfreq)
{
	unsigned char buff[64];
	ULONG lengthTransferred = 0;
	bool retVal = FALSE;
	if(UsbClaimInterface(clocksrc->m_interface->Descriptor().bInterfaceNumber))
	{
		bool retValue = SendUsbControl(BMREQUEST_DIR_DEVICE_TO_HOST, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_INTERFACE, 
			AUDIO_CS_REQUEST_RANGE, AUDIO_CS_CONTROL_SAM_FREQ << 8, (clocksrc->m_clockSource.bClockID << 8) + clocksrc->m_interface->Descriptor().bInterfaceNumber,
					 buff, sizeof(buff), &lengthTransferred);
		if(!retValue)
		{
	        m_errorCode = GetLastError();
#ifdef _DEBUG
		    debugPrintf("ASIOUAC: Enumerate samplerate failed. ErrorCode: %08Xh\n",  m_errorCode);
#endif
		}
		else
			if(lengthTransferred <= 2)
			{
#ifdef _DEBUG
				debugPrintf("ASIOUAC: Enumerate samplerate failed. Wrong transfer length\n");
#endif
				retValue = FALSE;
			}
		unsigned short length = *((unsigned short*)buff);
		struct sample_rate_triplets *triplets = (sample_rate_triplets *)(buff + 2);

#ifdef _DEBUG
		//debugPrintf("ASIOUAC: Enumerate samplerate OK\n");
#endif
		for(int i = 0; i < length; i++)
		{
			if(triplets[i].res_freq == 0)
			{
				if(newfreq == triplets[i].min_freq)
					retVal = TRUE;
			}
			else
				for(int freq = triplets[i].min_freq; freq <= triplets[i].max_freq; freq += triplets[i].res_freq)
				{
#ifdef _DEBUG
					//debugPrintf("ASIOUAC: Supported freq: %d\n", freq);
#endif
					if(newfreq == freq)
						retVal = TRUE;
				}
		}
		UsbReleaseInterface(clocksrc->m_interface->Descriptor().bInterfaceNumber);
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Sample freq: %d %s\n", newfreq, retVal ? "is supported" : "isn't supported");
#endif
	}
	else
	{
        m_errorCode = GetLastError();
#ifdef _DEBUG
        debugPrintf("ASIOUAC: Claim interface %d failed. ErrorCode: %08Xh\n", clocksrc->m_interface->Descriptor().bInterfaceNumber, m_errorCode);
#endif
	}
	return retVal;
}

USBAudioClockSource* USBAudioDevice::FindClockSource(int freq)
{
	USBAudioControlInterface * iface = m_acInterfaceList.First();
	while(iface)
	{
		USBAudioClockSource* clockSource = iface->m_clockSourceList.First();
		while(clockSource)
		{
			if(CheckSampleRate(clockSource, freq))
				return clockSource;
			clockSource = iface->m_clockSourceList.Next(clockSource);
		}
		
		iface = m_acInterfaceList.Next(iface);
	}
	return NULL;
}

bool USBAudioDevice::SetSampleRateInternal(int freq)
{
	ULONG lengthTransferred = 0;
	bool retValue = FALSE;
	USBAudioClockSource* clockSource = FindClockSource(freq);
	if(!clockSource)
	{
#ifdef _DEBUG
        debugPrintf("ASIOUAC: Not found clock source for sample rate %d\n", freq);
#endif
		return FALSE;
	}

	if(UsbClaimInterface(clockSource->m_interface->Descriptor().bInterfaceNumber))
	{
		retValue = SendUsbControl(BMREQUEST_DIR_HOST_TO_DEVICE, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_INTERFACE, 
			AUDIO_CS_REQUEST_CUR, AUDIO_CS_CONTROL_SAM_FREQ << 8, (clockSource->m_clockSource.bClockID << 8) + clockSource->m_interface->Descriptor().bInterfaceNumber,
					(unsigned char*)&freq, sizeof(freq), &lengthTransferred);
		if(!retValue)
		{
	        m_errorCode = GetLastError();
#ifdef _DEBUG
		    debugPrintf("ASIOUAC: Set samplerate %d failed. ErrorCode: %08Xh\n",  freq, m_errorCode);
#endif
		}
		else
			if(lengthTransferred != 4)
			{
#ifdef _DEBUG
				debugPrintf("ASIOUAC: Set samplerate %d failed. Wrong transfer length\n",  freq);
#endif
				retValue = FALSE;
			}
		UsbReleaseInterface(clockSource->m_interface->Descriptor().bInterfaceNumber);
	}
	else
	{
        m_errorCode = GetLastError();
#ifdef _DEBUG
        debugPrintf("ASIOUAC: Claim interface %d failed. ErrorCode: %08Xh\n", clockSource->m_interface->Descriptor().bInterfaceNumber, m_errorCode);
#endif
	}
	return retValue;
}

int USBAudioDevice::GetCurrentSampleRate()
{
	if(!IsValidDevice())
		return 0;

	if(m_acInterfaceList.Count() == 1 && m_acInterfaceList.First()->m_clockSourceList.Count() == 1)
	{
		int acInterfaceNum = m_acInterfaceList.First()->Descriptor().bInterfaceNumber;
		int clockID = m_acInterfaceList.First()->m_clockSourceList.First()->m_clockSource.bClockID;
		return GetSampleRateInternal(acInterfaceNum, clockID);
	}
	else
	{
		//more AC interfaces???
	}

	return 0;
}

int USBAudioDevice::GetSampleRateInternal(int interfaceNum, int clockID)
{
	ULONG lengthTransferred = 0;
	int freq = 0;

	if(UsbClaimInterface(interfaceNum))
	{
		if(!SendUsbControl( BMREQUEST_DIR_DEVICE_TO_HOST, BMREQUEST_TYPE_CLASS, BMREQUEST_RECIPIENT_INTERFACE, 
					AUDIO_CS_REQUEST_CUR, AUDIO_CS_CONTROL_SAM_FREQ << 8, (clockID << 8) + interfaceNum,
					(unsigned char*)&freq, sizeof(freq), &lengthTransferred))
		{
			freq = 0;
	        m_errorCode = GetLastError();
#ifdef _DEBUG
		    debugPrintf("ASIOUAC: Get samplerate %d failed. ErrorCode: %08Xh\n",  freq, m_errorCode);
#endif
		}
		else
			if(lengthTransferred != 4)
			{
#ifdef _DEBUG
				debugPrintf("ASIOUAC: Get samplerate %d failed. Wrong transfer length\n",  freq);
#endif
				freq = 0;
			}
		UsbReleaseInterface(interfaceNum);
	}
	else
	{
        m_errorCode = GetLastError();
#ifdef _DEBUG
        debugPrintf("ASIOUAC: Claim interface %d failed. ErrorCode: %08Xh\n", interfaceNum, m_errorCode);
#endif
	}
	return freq;
}

bool USBAudioDevice::SetSampleRate(int freq)
{
	if(!IsValidDevice())
		return FALSE;

#ifdef _DEBUG
	debugPrintf("ASIOUAC: Set samplerate %d\n",  freq);
#endif
	if(SetSampleRateInternal(freq))
	{
		if(m_adc != NULL)
			m_adc->SetSampleFreq(freq);
		if(m_dac != NULL)
			m_dac->SetSampleFreq(freq);
		return TRUE;
	}
	return FALSE;
}

bool USBAudioDevice::CanSampleRate(int freq)
{
	if(!IsValidDevice())
		return FALSE;
	return 	FindClockSource(freq) != NULL;
}


bool USBAudioDevice::Start()
{
	if(m_isStarted || !IsValidDevice())
		return FALSE;

	bool retVal = TRUE;

#ifdef _DEBUG
	debugPrintf("ASIOUAC: USBAudioDevice start\n");
#endif

	if(m_adcEndpoint)
	{
		UsbClaimInterface(m_adcEndpoint->m_interface->Descriptor().bInterfaceNumber);
		UsbSetAltInterface(m_adcEndpoint->m_interface->Descriptor().bInterfaceNumber, m_adcEndpoint->m_interface->Descriptor().bAlternateSetting);
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Claim ADC interface 0x%X (alt 0x%X)\n", m_adcEndpoint->m_interface->Descriptor().bInterfaceNumber, 
			m_adcEndpoint->m_interface->Descriptor().bAlternateSetting);
#endif
		if(m_adc != NULL)
			retVal &= m_adc->Start();
	}

	if(m_dacEndpoint)
	{
		UsbClaimInterface(m_dacEndpoint->m_interface->Descriptor().bInterfaceNumber);
		UsbSetAltInterface(m_dacEndpoint->m_interface->Descriptor().bInterfaceNumber, m_dacEndpoint->m_interface->Descriptor().bAlternateSetting);
#ifdef _DEBUG
		debugPrintf("ASIOUAC: Claim DAC interface 0x%X (alt 0x%X)\n", m_dacEndpoint->m_interface->Descriptor().bInterfaceNumber, 
			m_dacEndpoint->m_interface->Descriptor().bAlternateSetting);
#endif
		if(m_dac != NULL)
			retVal &= m_dac->Start();

		if(m_feedback != NULL)
			retVal &= m_feedback->Start();
	}
	m_isStarted = TRUE;
	return retVal;
}

bool USBAudioDevice::Stop()
{
	if(!m_isStarted || !IsValidDevice())
		return FALSE;

#ifdef _DEBUG
	debugPrintf("ASIOUAC: USBAudioDevice stop\n");
#endif
	bool retVal = TRUE;

	if(m_dac != NULL)
		retVal &= m_dac->Stop();

	if(m_feedback != NULL)
		retVal &= m_feedback->Stop();

	if(m_adc != NULL)
		retVal &= m_adc->Stop();

	if(m_adcEndpoint)
	{
		USBAudioStreamingInterface * iface = m_asInterfaceList.First();
		while(iface)
		{
			if(iface->Descriptor().bInterfaceNumber == m_adcEndpoint->m_interface->Descriptor().bInterfaceNumber &&
				iface->m_endpointsList.Count() == 0)
			{
			
				UsbSetAltInterface(iface->Descriptor().bInterfaceNumber, iface->Descriptor().bAlternateSetting);
				UsbReleaseInterface(iface->Descriptor().bInterfaceNumber);
#ifdef _DEBUG
				debugPrintf("ASIOUAC: Release ADC interface 0x%X\n", iface->Descriptor().bInterfaceNumber);
#endif
				break;
			}
			iface = m_asInterfaceList.Next(iface);
		}
	}
	if(m_dacEndpoint)
	{
		USBAudioStreamingInterface * iface = m_asInterfaceList.First();
		while(iface)
		{
			if(iface->Descriptor().bInterfaceNumber == m_dacEndpoint->m_interface->Descriptor().bInterfaceNumber &&
				iface->m_endpointsList.Count() == 0)
			{
			
				UsbSetAltInterface(iface->Descriptor().bInterfaceNumber, iface->Descriptor().bAlternateSetting);
				UsbReleaseInterface(iface->Descriptor().bInterfaceNumber);
#ifdef _DEBUG
				debugPrintf("ASIOUAC: Release DAC interface 0x%X\n", iface->Descriptor().bInterfaceNumber);
#endif
				break;
			}
			iface = m_asInterfaceList.Next(iface);
		}
	}

	m_isStarted = FALSE;
	return retVal;
}

void USBAudioDevice::SetDACCallback(FillDataCallback readDataCb, void* context)
{
	if(m_dac != NULL)
		m_dac->SetCallback(readDataCb, context);
}

void USBAudioDevice::SetADCCallback(FillDataCallback writeDataCb, void* context)
{
	if(m_adc != NULL)
		m_adc->SetCallback(writeDataCb, context);
}

int USBAudioDevice::GetInputChannelNumber()
{
	if(!IsValidDevice())
		return 0;
	return m_useInput ? 2 : 0;
}

int USBAudioDevice::GetOutputChannelNumber()
{
	if(!IsValidDevice())
		return 0;
	return 2;
}

USBAudioInTerminal* USBAudioDevice::FindInTerminal(int id)
{
	USBAudioControlInterface * iface = m_acInterfaceList.First();
	while(iface)
	{
		USBAudioInTerminal * elem = iface->m_inTerminalList.First();
		while(elem)
		{
			if(elem->m_inTerminal.bTerminalID == id)
				return elem;
			elem = iface->m_inTerminalList.Next(elem);
		}
		iface = m_acInterfaceList.Next(iface);
	}
	return NULL;
}

USBAudioFeatureUnit* USBAudioDevice::FindFeatureUnit(int id)
{
	USBAudioControlInterface * iface = m_acInterfaceList.First();
	while(iface)
	{
		USBAudioFeatureUnit * elem = iface->m_featureUnitList.First();
		while(elem)
		{
			if(elem->m_featureUnit.bUnitID == id)
				return elem;
			elem = iface->m_featureUnitList.Next(elem);
		}
		iface = m_acInterfaceList.Next(iface);
	}
	return NULL;
}

USBAudioOutTerminal* USBAudioDevice::FindOutTerminal(int id)
{
	USBAudioControlInterface * iface = m_acInterfaceList.First();
	while(iface)
	{
		USBAudioOutTerminal * elem = iface->m_outTerminalList.First();
		while(elem)
		{
			if(elem->m_outTerminal.bTerminalID == id)
				return elem;
			elem = iface->m_outTerminalList.Next(elem);
		}
		iface = m_acInterfaceList.Next(iface);
	}
	return NULL;
}
