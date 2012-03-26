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
#pragma once
#ifndef __DESCRIPTORS_H__
#define __DESCRIPTORS_H__

#include "targetver.h"
#include <tchar.h>
#include <windows.h>

#include <libusbk.h>
#include "usbdevice.h"
#include "tlist.h"



class USBAudioDevice;
class USBEndpoint;
class USBAudioStreamingInterface;
class USBAudioControlInterface;

class USBAudioInterface
{
protected:
	USB_INTERFACE_DESCRIPTOR		m_interface;
public:
	USBAudioInterface(USB_INTERFACE_DESCRIPTOR*	iface);
	~USBAudioInterface() {}
	virtual bool SetCSDescriptor(USB_DESCRIPTOR_HEADER *csDescriptor) = 0;
	virtual USBEndpoint* CreateEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor) = 0;
	const USB_INTERFACE_DESCRIPTOR& Descriptor() { return m_interface; }

	friend class USBAudioDevice;
};

class USBAudioClockSource : public TElement<USBAudioClockSource, TList<USBAudioClockSource>>
{
protected:
	usb_clock_source_descriptor		m_clockSource;	// Clock source
	USBAudioControlInterface*		m_interface;
public:
	USBAudioClockSource(usb_clock_source_descriptor* clockSource, USBAudioControlInterface* iface);
	~USBAudioClockSource() {}
	void Destroy()
	{ delete this; }
	friend class USBAudioDevice;
	friend class USBAudioControlInterface;
};

class USBAudioInTerminal : public TElement<USBAudioInTerminal, TList<USBAudioInTerminal>>
{
protected:
	usb_in_ter_descriptor_2		m_inTerminal;
	USBAudioControlInterface*		m_interface;
public:
	USBAudioInTerminal(usb_in_ter_descriptor_2* inTerminal, USBAudioControlInterface* iface);
	~USBAudioInTerminal() {}
	void Destroy()
	{ delete this; }
	friend class USBAudioDevice;
	friend class USBAudioControlInterface;
};

class USBAudioOutTerminal : public TElement<USBAudioOutTerminal, TList<USBAudioOutTerminal>>
{
protected:
	usb_out_ter_descriptor_2		m_outTerminal;
	USBAudioControlInterface*		m_interface;
public:
	USBAudioOutTerminal(usb_out_ter_descriptor_2* outTerminal, USBAudioControlInterface* iface);
	~USBAudioOutTerminal() {}
	void Destroy()
	{ delete this; }
	friend class USBAudioDevice;
	friend class USBAudioControlInterface;
};


class USBAudioFeatureUnit : public TElement<USBAudioFeatureUnit, TList<USBAudioFeatureUnit>>
{
protected:
	usb_feature_unit_descriptor_2		m_featureUnit;
	USBAudioControlInterface*		m_interface;
public:
	USBAudioFeatureUnit(usb_feature_unit_descriptor_2* featureUnit, USBAudioControlInterface* iface);
	~USBAudioFeatureUnit() {}
	void Destroy()
	{ delete this; }
	friend class USBAudioDevice;
	friend class USBAudioControlInterface;
};

class USBEndpoint
{
protected:
	USB_ENDPOINT_DESCRIPTOR	m_descriptor;
public:
	USBEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor);
	~USBEndpoint() {}
	virtual bool SetCSDescriptor(USB_DESCRIPTOR_HEADER *csDescriptor) = 0;
	friend class USBAudioDevice;
	friend class USBAudioControlInterface;
	friend class USBAudioStreamingInterface;
};

class USBAudioStreamingEndpoint : public USBEndpoint, public TElement<USBAudioStreamingEndpoint, TList<USBAudioStreamingEndpoint>>
{
protected:
	usb_endpoint_audio_specific_2	m_asDescriptor;
	USBAudioStreamingInterface*		m_interface;
public:
	USBAudioStreamingEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor, USBAudioStreamingInterface* iface);
	~USBAudioStreamingEndpoint() {}
	void Destroy()
	{ delete this; }
	virtual bool SetCSDescriptor(USB_DESCRIPTOR_HEADER *csDescriptor);
	friend class USBAudioDevice;
	friend class USBAudioControlInterface;
	friend class USBAudioStreamingInterface;
};

class USBAudioControlInterface : public USBAudioInterface, public TElement<USBAudioControlInterface, TList<USBAudioControlInterface>>
{
protected:
	usb_ac_interface_descriptor_2				m_acDescriptor;
	TList<USBAudioClockSource>		m_clockSourceList;
	TList<USBAudioInTerminal>		m_inTerminalList;
	TList<USBAudioFeatureUnit>		m_featureUnitList;
	TList<USBAudioOutTerminal>		m_outTerminalList;
public:
	USBAudioControlInterface(USB_INTERFACE_DESCRIPTOR*	iface);
	~USBAudioControlInterface() {}
	void Destroy()
	{ delete this; }

	virtual bool SetCSDescriptor(USB_DESCRIPTOR_HEADER *csDescriptor);
	bool AddClockSource(usb_clock_source_descriptor* clockSource);
	bool AddInTerminal(usb_in_ter_descriptor_2* inTerminal);
	bool AddOutTerminal(usb_out_ter_descriptor_2* outTerminal);
	bool AddFeatureUnit(usb_feature_unit_descriptor_2* featureUnit);
	USBEndpoint* CreateEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor);

	friend class USBAudioDevice;
};

class USBAudioStreamingInterface : public USBAudioInterface, public TElement<USBAudioStreamingInterface, TList<USBAudioStreamingInterface>>
{
protected:
	usb_as_g_interface_descriptor_2				m_asgDescriptor;
	usb_format_type_2							m_formatDescriptor;
	TList<USBAudioStreamingEndpoint>		m_endpointsList;
	
public:
	USBAudioStreamingInterface(USB_INTERFACE_DESCRIPTOR* iface);
	~USBAudioStreamingInterface() {}

	void Destroy()
	{ delete this; }

	virtual bool SetCSDescriptor(USB_DESCRIPTOR_HEADER* csDescriptor);
	virtual USBEndpoint* CreateEndpoint(USB_ENDPOINT_DESCRIPTOR* descriptor);

	friend class USBAudioDevice;
};


#endif //__DESCRIPTORS_H__
