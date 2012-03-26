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


#pragma once
#ifndef __USBAUDIO_DEVICE_H__
#define __USBAUDIO_DEVICE_H__

#include "usbdevice.h"
#include "audiotask.h"
#include "tlist.h"
#include "descriptors.h"


typedef TList<USBAudioControlInterface> USBACInterfaceList;
typedef TList<USBAudioStreamingInterface> USBASInterfaceList;

class USBAudioDevice : public USBDevice
{
	//IAD
	//USB_INTERFACE_ASSOCIATION_DESCRIPTOR	m_iad;

	USBACInterfaceList				m_acInterfaceList;
	USBASInterfaceList				m_asInterfaceList;

	void FreeDeviceInternal();
	void InitDescriptors();

	int								m_audioClass;
	bool							m_useInput;

	USBAudioStreamingInterface*		m_currentAsInterface;
protected:
	virtual void FreeDevice();

	virtual bool ParseDescriptorInternal(USB_DESCRIPTOR_HEADER* uDescriptor);

	bool SetSampleRateInternal(int freq);

	USBAudioClockSource* FindClockSource(int freq);
	bool CheckSampleRate(USBAudioClockSource* clocksrc, int freq);
	int GetSampleRateInternal(int interfaceNum, int clockID);

public:
	USBAudioDevice(bool useInput);
	virtual ~USBAudioDevice();
	virtual bool InitDevice();

	bool CanSampleRate(int freq);
	bool SetSampleRate(int freq);
	int GetCurrentSampleRate();

	int GetInputChannelNumber();
	int GetOutputChannelNumber();

	bool Start();
	bool Stop();

	void SetDACCallback(FillDataCallback readDataCb, void* context);
	void SetADCCallback(FillDataCallback writeDataCb, void* context);
private:
	USBAudioInterface*	m_lastParsedInterface;
	USBEndpoint*		m_lastParsedEndpoint;

	FeedbackInfo	m_fbInfo;
	AudioDAC*		m_dac;
	AudioADC*		m_adc;
	AudioFeedback*	m_feedback;
};

#endif //__USBAUDIO_DEVICE_H__
