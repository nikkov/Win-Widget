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

// USB Audio defines

#ifndef USB_AUDIO_DEF
#define USB_AUDIO_DEF

#define U8 unsigned char
#define U16 unsigned short
#define U32	unsigned __int32

//_____ M A C R O S ____________________________________________________________
#define CS_INTERFACE                    0x24
#define CS_ENDPOINT                     0x25
#define GENERAL_SUB_TYPE                0x01
#define FORMAT_SUB_TYPE                 0x02
#define HEADER_SUB_TYPE                 0x01
#define MIXER_UNIT_SUB_TYPE             0x04
#define FEATURE_UNIT_SUB_TYPE           0x06
#define INPUT_TERMINAL_SUB_TYPE         0x02
#define OUTPUT_TERMINAL_SUB_TYPE        0x03

/*! \name Audio specific definitions (Class, subclass and protocol)
 */
//! @{
#define AUDIO_CLASS                           0x01
#define AUDIOCONTROL_SUBCLASS                      0x01
#define AUDIOSTREAMING_SUBCLASS                    0x02
#define MIDISTREAMING_SUBCLASS                     0x03
//! @}


//! \name Audio Interface Subclass Code pp. A.5
//! @{
#define  AUDIO_INTERFACE_SUBCLASS_UNDEFINED         0x00
#define  AUDIO_INTERFACE_SUBCLASS_AUDIOCONTROL      0x01
#define  AUDIO_INTERFACE_SUBCLASS_AUDIOSTREAMING    0x02
#define  AUDIO_INTERFACE_SUBCLASS_MIDISTREAMING     0x03
//! @}

//! \name Audio Interface Protocol Code pp. A.6
//! @{
#define  AUDIO_INTERFACE_PROTOCOL_UNDEFINED         0x00
#define  AUDIO_INTERFACE_IP_VERSION_02_00           0x20
#define  IP_VERSION_02_00	AUDIO_INTERFACE_IP_VERSION_02_00
//! @}

//! \name Audio Class-Specific AC Interface Descriptor Subtypes pp. A.9
//! @{
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_UNDEFINED              0x00
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_HEADER                 0x01
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_INPUT_TERMINAL         0x02
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_OUTPUT_TERMINAL        0x03
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_MIXER_UNIT             0x04
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_SELECTOR_UNIT          0x05
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_FEATURE_UNIT           0x06
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_EFFECT_UNIT            0x07
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_PROCESSING_UNIT        0x08
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_EXTENSION_UNIT         0x09
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_CLOCK_SOURCE           0x0A
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_CLOCK_SELECTOR         0x0B
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_CLOCK_MULTIPLIER       0x0C
#define  DESCRIPTOR_SUBTYPE_AUDIO_AC_SAMPLE_RATE_CONVERTE   0x0D
//! @}


//! \name Audio Class-Specific Request Code pp. A.14
//! @{
#define  AUDIO_CS_REQUEST_UNDEFINED                        0x00
#define  AUDIO_CS_REQUEST_CUR                              0x01
#define  AUDIO_CS_REQUEST_RANGE                            0x02
#define  AUDIO_CS_REQUEST_MEM                              0x03


//! \name AudioStreaming Interface Control Selectors pp. A.17.11
//! @{
#define  AUDIO_AS_UNDEFINED                                0x00
#define  AUDIO_AS_ACT_ALT_SETTINGS                         0x01
#define  AUDIO_AS_VAL_ALT_SETTINGS                         0x02
#define  AUDIO_AS_AUDIO_DATA_FORMAT                        0x03
//! @}

//! \name Clock Source Control Selectors pp. A17.1
//! @{
#define  AUDIO_CS_UNDEFINED                      0x00
#define  AUDIO_CS_CONTROL_SAM_FREQ               0x01
#define  AUDIO_CS_CONTROL_CLOCK_VALID            0x02
//! @}


/* ensure byte-packed structures */
#include <pshpack1.h>

struct usb_ac_interface_descriptor_2
{
  U8  bLength;               /* Size of this descriptor in bytes */
  U8  bDescriptorType;       /* CS_INTERFACE descriptor type */
  U8  bDescritorSubtype;     /* HEADER subtype */
  U16 bcdADC;          		  /* Revision of class spec */
  U8  bCategory;				/* Primary use of this function */
  U16 wTotalLength;       	  /* Total size of class specific descriptor */
  U8  bmControls;		     /* Latency Control Bitmap */
};

//! USB Standard AS interface Descriptor pp 4.9.1
struct usb_as_interface_descriptor
{
	U8		bLength;			/* Size of this descriptor in bytes */
	U8 		bDescriptorType;		/* INTERFACE descriptor type */
	U8		bInterfaceNumber;	/* Number of the interface (0 based) */
	U8		bAlternateSetting;
	U8		bNumEndpoints;		/* Number of endpoints in this interface */
	U8		bInterfaceClass;	/* AUDIO Interface class code */
	U8		bInterfaceSubclass;	/* AUDIO_STREAMING Interface subclass code */
	U8		bInterfaceProtocol;	/* IP_VERSION_02_00 Interface protocol code */
	U8		iInterface;			/* String descriptor of this Interface */
};

//! USB Class-Specific AS general interface descriptor pp 4.9.2
struct usb_as_g_interface_descriptor_2
{
	U8		bLength;			/* Size of this descriptor in bytes */
	U8 	bDescriptorType;		/* CS_INTERFACE descriptor type */
	U8 	bDescriptorSubType;		/* AS_GENERAL subtype */
	U8		bTerminalLink;		/* Terminal ID to which this interface is connected */
	U8  	bmControls;			/* Bitmap of controls */
	U8  bFormatType;			/* Format type the interface is using */
	U32 bmFormats;				/* Bitmap of Formats this interface supports */
	U8  bNrChannels;			/* Number of Physical channels in this interface cluster */
	U32 bmChannelConfig;		/* Bitmap of spatial locations of the physical channels */
	U8  iChannelNames;			/* String descriptor of the first physical channel */
};


struct usb_clock_source_descriptor
{
  U8  bLength;               /* Size of this descriptor in bytes */
  U8  bDescriptorType;       /* CS_INTERFACE descriptor type */
  U8  bDescritorSubtype;     /* CLOCK_SOURCE subtype */
  U8  bClockID;       	  /* Clock Source ID */
  U8  bmAttributes;		     /* Clock Type Bitmap */
  U8  bmControls;			/* Clock control bitmap */
  U8  bAssocTerminal;		/* Terminal ID associated with this source */
  U8  iClockSource;			/* String descriptor of this clock source */
};

//! USB INPUT Terminal Descriptor pp 4.7.2.4
struct usb_in_ter_descriptor_2
{
	U8	bLength;		/* Size of this descriptor in bytes */
	U8 	bDescriptorType;	/* CS_INTERFACE descriptor type */
	U8 	bDescriptorSubType;	/* INPUT_TERMINAL subtype */
	U8	bTerminalID;	/* Input Terminal ID */
	U16	wTerminalType;		/* Terminal type */
	U8	bAssocTerminal;	/* Output terminal this input is associated with */
	U8	bCSourceID;		/* ID of Clock entity to which this terminal is connected */
	U8	bNrChannels;	/* Number of Logical output channels */
	U32	bmChannelConfig;	/* Spatial location of logical channels */
	U8	iChannelNames;	/* String descriptor of first logical channel */
	U16 bmControls;		/* Paired Bitmap of controls */
	U8	iTerminal;		/* String descriptor of this Input Terminal */
};


//! USB OUTPUT Terminal Descriptor pp 4.7.2.5
struct usb_out_ter_descriptor_2
{
	U8	bLength;		/* Size of this descriptor in bytes */
	U8 	bDescriptorType;	/* CS_INTERFACE descriptor type */
	U8 	bDescriptorSubType;	/* OUTPUT_TERMINAL subtype */
	U8	bTerminalID;	/* Output Terminal ID */
	U16	wTerminalType;		/* Terminal type */
	U8	bAssocTerminal;	/* Input Terminal this output is associated with */
	U8	bSourceID;		/* ID of the Unit or Terminal to which this teminal is connected to */
	U8  bCSourceID;		/* ID od the Clock Entity to which this terminal is connected */
	U16 bmControls;		/* Paired Bitmap of controls */
	U8	iTerminal;		/* String descriptor of this Output Terminal */
};

//! USB Audio Feature Unit descriptor pp 4.7.2.8
struct usb_feature_unit_descriptor_2
{
	U8	bLength;			/* Size of this descriptor in bytes */
	U8 	bDescriptorType;        /* CS_INTERFACE descriptor type */
	U8 	bDescriptorSubType; 	/* FEATURE_UNIT  subtype */
	U8	bUnitID;			/* Feature unit ID */
	U8	bSourceID;			/* ID of the Unit or Terminal to which this teminal is connected to */
	U32	bmaControls;	/* Master Channel 0*/
	U32	bmaControls_1;  // Channel 1
	U32 bmaControls_2;  // Channel 2
	U8	iFeature;  /* String Descriptor of this Feature Unit */
};

//! Class-Specific Audio Format Type descriptor pp 4.9.3 -> 2.3.1.6 Type I Format
struct usb_format_type_2
{
	U8	bLength;			/* Size of this descriptor in bytes */
	U8 	bDescriptorType;		/* CS_INTERFACE descriptor type */
	U8 	bDescriptorSubType;		/* FORMAT_TYPE subtype */
	U8	bFormatType;		/* Format Type this streaming interface is using */
	U8	bSubslotSize;		/* Number of bytes in one audio subslot */
	U8	bBitResolution;		/* Number of bits used from the available bits in the subslot */
};


//! Usb Standard AS Isochronous Audio Data Endpoint Descriptors pp 4.10.1.1
struct usb_endpoint_audio_descriptor_2
{
   U8      bLength;               /* Size of this descriptor in bytes */
   U8      bDescriptorType;       /* CS_ENDPOINT descriptor type */
   U8      bEndpointAddress;      /* Address of the endpoint */
   U8      bmAttributes;          /* Endpoint's attributes */
   U16     wMaxPacketSize;        /* Maximum packet size for this EP */
   U8      bInterval;             /* Interval for polling EP in ms */
};

//! Usb Class_Specific AS Isochronous Audio Data Endpoint Descriptors pp 4.10.1.2
struct usb_endpoint_audio_specific_2
{
	U8		bLength;			/* Size of this descriptor in bytes */
	U8 		bDescriptorType;	/* CS_ENDPOINT descriptor type*/
	U8 		bDescriptorSubType;	/* EP_GENERAL subtype */
	U8		bmAttributes;		/* Bitmap of attributes 8 */
	U8      bmControls;			/* Paired bitmap of controls */
	U8    	bLockDelayUnits;		/* units for wLockDelay */
	U16		wLockDelay;				/* time to lock endpoint */
};

struct sample_rate_triplets
{
	int min_freq;
	int max_freq;
	int res_freq;
};

#endif