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

#include "descriptors.h"

USBAudioInterface::USBAudioInterface(USB_INTERFACE_DESCRIPTOR*	iface) 
{
	memcpy(&m_interface, iface, sizeof(USB_INTERFACE_DESCRIPTOR));
}

USBAudioClockSource::USBAudioClockSource(usb_clock_source_descriptor* clockSource, USBAudioControlInterface* iface) : m_interface(iface)
{
	memcpy(&m_clockSource, clockSource, sizeof(usb_clock_source_descriptor));
}

USBAudioInTerminal::USBAudioInTerminal(usb_in_ter_descriptor_2* inTerminal, USBAudioControlInterface* iface) : m_interface(iface)
{
	memcpy(&m_inTerminal, inTerminal, sizeof(usb_in_ter_descriptor_2));
}

USBAudioOutTerminal::USBAudioOutTerminal(usb_out_ter_descriptor_2* outTerminal, USBAudioControlInterface* iface) : m_interface(iface)
{
	memcpy(&m_outTerminal, outTerminal, sizeof(usb_out_ter_descriptor_2));
}

USBAudioFeatureUnit::USBAudioFeatureUnit(usb_feature_unit_descriptor_2* featureUnit, USBAudioControlInterface* iface) : m_interface(iface)
{
	memcpy(&m_featureUnit, featureUnit, sizeof(usb_feature_unit_descriptor_2));
}

USBEndpoint::USBEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor)
{
	memcpy(&m_descriptor, descriptor, sizeof(USB_ENDPOINT_DESCRIPTOR));
}


USBAudioStreamingEndpoint::USBAudioStreamingEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor, USBAudioStreamingInterface* iface) : USBEndpoint(descriptor), m_interface(iface)
{
	memset(&m_asDescriptor, 0, sizeof(usb_endpoint_audio_specific_2));
}

bool USBAudioStreamingEndpoint::SetCSDescriptor(USB_DESCRIPTOR_HEADER *csDescriptor)
{
	if(csDescriptor->bDescriptorSubType == GENERAL_SUB_TYPE && csDescriptor->bLength == sizeof(usb_endpoint_audio_specific_2))
	{
		memcpy(&m_asDescriptor, csDescriptor, sizeof(usb_endpoint_audio_specific_2));
#ifdef _ENABLE_TRACE
		debugPrintf("ASIOUAC: Found CS descriptor for endpoint 0x%X, audio streaming interface 0x%X\n", m_descriptor.bEndpointAddress, m_interface->Descriptor().bInterfaceNumber);
#endif
		return TRUE;
	}
	return FALSE;
}

USBAudioControlInterface::USBAudioControlInterface(USB_INTERFACE_DESCRIPTOR*	iface) : USBAudioInterface(iface) 
{
	memset(&m_acDescriptor, 0, sizeof(usb_ac_interface_descriptor_2));
}

bool USBAudioControlInterface::SetCSDescriptor(USB_DESCRIPTOR_HEADER *csDescriptor)
{
	switch(csDescriptor->bDescriptorSubType)
	{
		case HEADER_SUB_TYPE:			
			memcpy(&m_acDescriptor, csDescriptor, sizeof(usb_ac_interface_descriptor_2));
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: Found CS descriptor (subtype 0x%X) for audio control interface 0x%X\n", m_interface.bInterfaceSubClass, m_interface.bInterfaceNumber);
#endif
			return TRUE;
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_CLOCK_SOURCE:
			AddClockSource((usb_clock_source_descriptor*)csDescriptor);
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: Found clock source 0x%X in interface 0x%X\n", m_clockSourceList.Last()->m_clockSource.bClockID, m_interface.bInterfaceNumber);
#endif
			return TRUE;

		case DESCRIPTOR_SUBTYPE_AUDIO_AC_INPUT_TERMINAL:
			AddInTerminal((usb_in_ter_descriptor_2*)csDescriptor);
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: Found in terminal 0x%X in interface 0x%X\n",  m_inTerminalList.Last()->m_inTerminal.bTerminalID, m_interface.bInterfaceNumber);
#endif
			return TRUE;
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_OUTPUT_TERMINAL:
			AddOutTerminal((usb_out_ter_descriptor_2*)csDescriptor);
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: Found out terminal 0x%X in interface 0x%X\n", m_outTerminalList.Last()->m_outTerminal.bTerminalID, m_interface.bInterfaceNumber);
#endif
			return TRUE;
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_FEATURE_UNIT:
			AddFeatureUnit((usb_feature_unit_descriptor_2*) csDescriptor);
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: Found feature unit 0x%X in interface 0x%X\n", m_featureUnitList.Last()->m_featureUnit.bUnitID, m_interface.bInterfaceNumber);
#endif
			return TRUE;
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_MIXER_UNIT:
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_SELECTOR_UNIT:
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_EFFECT_UNIT:
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_PROCESSING_UNIT:
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_EXTENSION_UNIT:
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_CLOCK_SELECTOR:
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_CLOCK_MULTIPLIER:
		case DESCRIPTOR_SUBTYPE_AUDIO_AC_SAMPLE_RATE_CONVERTE:
			return FALSE;
	}
	return FALSE;
}

bool USBAudioControlInterface::AddClockSource(usb_clock_source_descriptor* clockSource)
{
	USBAudioClockSource *clkSrc = new USBAudioClockSource(clockSource, this);
	return m_clockSourceList.Add(clkSrc);
}

bool USBAudioControlInterface::AddInTerminal(usb_in_ter_descriptor_2* inTerminal)
{
	USBAudioInTerminal *inTerm = new USBAudioInTerminal(inTerminal, this);
	return m_inTerminalList.Add(inTerm);
}

bool USBAudioControlInterface::AddOutTerminal(usb_out_ter_descriptor_2* outTerminal)
{
	USBAudioOutTerminal *outTerm = new USBAudioOutTerminal(outTerminal, this);
	return m_outTerminalList.Add(outTerm);
}

bool USBAudioControlInterface::AddFeatureUnit(usb_feature_unit_descriptor_2* featureUnit)
{
	USBAudioFeatureUnit *featUnit = new USBAudioFeatureUnit(featureUnit, this);
	return m_featureUnitList.Add(featUnit);
}

USBEndpoint* USBAudioControlInterface::CreateEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor)
{
	return NULL;
}



USBAudioStreamingInterface::USBAudioStreamingInterface(USB_INTERFACE_DESCRIPTOR* iface) : USBAudioInterface(iface) 
{
	memset(&m_asgDescriptor, 0, sizeof(usb_as_g_interface_descriptor_2));
	memset(&m_formatDescriptor, 0, sizeof(usb_format_type_2));
}

bool USBAudioStreamingInterface::SetCSDescriptor(USB_DESCRIPTOR_HEADER* csDescriptor)
{
	switch(csDescriptor->bDescriptorSubType)
	{
		case HEADER_SUB_TYPE:			
			memcpy(&m_asgDescriptor, csDescriptor, sizeof(usb_as_g_interface_descriptor_2));
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: Found CS descriptor (subtype 0x%X) for audio streaming interface 0x%X\n", m_interface.bInterfaceSubClass, m_interface.bInterfaceNumber);
#endif
			return TRUE;
		case FORMAT_SUB_TYPE:			
			memcpy(&m_formatDescriptor, csDescriptor, sizeof(usb_format_type_2));
#ifdef _ENABLE_TRACE
			debugPrintf("ASIOUAC: Found format type descriptor for audio streaming interface 0x%X\n", m_interface.bInterfaceNumber);
#endif
			return TRUE;
	}
	return FALSE;
}

USBEndpoint* USBAudioStreamingInterface::CreateEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor)
{
	USBAudioStreamingEndpoint *ep = new USBAudioStreamingEndpoint(descriptor, this);
#ifdef _ENABLE_TRACE
	debugPrintf("ASIOUAC: Found endpoint 0x%X for audio streaming interface 0x%X\n", ep->m_descriptor.bEndpointAddress, m_interface.bInterfaceNumber);
#endif
	m_endpointsList.Add(ep);
	return ep;
}