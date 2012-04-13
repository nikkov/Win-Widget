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
// Widget control utility
// Based on code Widget Control
// by Alex Lee, 9V1AL & Loftur Jonasson, TF3LJ 

using System;
using System.Collections;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.IO;

using libusbK;

namespace WidgetControl
{
    public partial class WidgetControl : Form
    {
        private const int vendorid1    = 0x16c0;
        private const int vendorid2    = 0xfffe;
        private const int productid1   = 0x05dc;
        private const int productid2   = 0x03e8;
        private const int productid3   = 0x0007;
        private const int productid4   = 0x03e9;
        private const int interfaceNumber = 0;

        LstK deviceList = null;
        UsbK usb = null;

        Hashtable feature_value_dict = new Hashtable();
        Hashtable feature_value_lookup_dict = new Hashtable();

        uint LengthTransferred;
        const uint globalBufferLength = 256;
        byte[] globalBuffer = new byte[256];

        public WidgetControl()
        {
            InitializeComponent();
            InitializeDevice();
            //SetFeatureFromFile("test.ini");
        }

        USB_DEVICE_DESCRIPTOR ByteToDeviceDescriptor (byte[] packet)
        {
            GCHandle pinnedPacket = GCHandle.Alloc(packet, GCHandleType.Pinned);
            USB_DEVICE_DESCRIPTOR descriptor = (USB_DEVICE_DESCRIPTOR)Marshal.PtrToStructure(
                pinnedPacket.AddrOfPinnedObject(),
                typeof(USB_DEVICE_DESCRIPTOR));
            pinnedPacket.Free();
            return descriptor;
        }

        USB_CONFIGURATION_DESCRIPTOR ByteToConfigDescriptor(byte[] packet)
        {
            GCHandle pinnedPacket = GCHandle.Alloc(packet, GCHandleType.Pinned);
            USB_CONFIGURATION_DESCRIPTOR descriptor = (USB_CONFIGURATION_DESCRIPTOR)Marshal.PtrToStructure(
                pinnedPacket.AddrOfPinnedObject(),
                typeof(USB_CONFIGURATION_DESCRIPTOR));
            pinnedPacket.Free();
            return descriptor;
        }

        string GetStringDescriptor(byte index)
        {
            usb.GetDescriptor((byte)USB_DESCRIPTOR_TYPE.STRING, index, 0,
                globalBuffer, globalBufferLength, out LengthTransferred);

            string s2 = System.Text.Encoding.Unicode.GetString(globalBuffer, 0, (int)LengthTransferred);
            if (s2.Length > 0)
                return s2.Substring(1, s2.Length - 1);
            return string.Empty;
        }

        public bool InitializeDevice()
        {
            if (deviceList != null)
                deviceList.Free();
            deviceList = null;
            if (usb != null)
                usb.Free();
            usb = null;

            feature_value_dict.Clear();
            feature_value_lookup_dict.Clear();

            foreach (Control ctrl in this.Controls)
            {
                if (ctrl is ComboBox)
                {
                    ComboBox box = (ComboBox)ctrl;
                    box.Items.Clear();
                }
            }

            deviceList = new LstK(KLST_FLAG.NONE);
            KLST_DEVINFO_HANDLE deviceInfo;

            DeviceInfo.Text = "";
            bool success = false;
            deviceList.MoveReset();
            while (deviceList.MoveNext(out deviceInfo))
            {
                if (
                    (deviceInfo.Common.Vid == vendorid1 || deviceInfo.Common.Vid == vendorid2) &&
                    (deviceInfo.Common.Pid == productid1 || deviceInfo.Common.Pid == productid2 || deviceInfo.Common.Pid == productid3 || deviceInfo.Common.Pid == productid4)
                    )
                {
                    if (deviceInfo.Connected)
                    {
                        success = true;
                        break;
                    }
                }
            }
            if (!success)
            {
                DeviceInfo.Text += String.Format("Audio-Widget device not found!\r\n");
                if (deviceList != null)
                    deviceList.Free();
                deviceList = null;
                return false;
            }
            usb = new UsbK(deviceInfo);
            DeviceInfo.Text += String.Format("Opening usb device OK\r\n");

            usb.GetDescriptor((byte)USB_DESCRIPTOR_TYPE.DEVICE, 0, 0,
                globalBuffer, globalBufferLength, out LengthTransferred);

            USB_DEVICE_DESCRIPTOR deviceDescriptor = ByteToDeviceDescriptor(globalBuffer);

            usb.GetDescriptor((byte)USB_DESCRIPTOR_TYPE.CONFIGURATION, 0, 0,
                globalBuffer, globalBufferLength, out LengthTransferred);
            USB_CONFIGURATION_DESCRIPTOR configurationDescriptor = ByteToConfigDescriptor(globalBuffer);

            string product = GetStringDescriptor(deviceDescriptor.iProduct);
            string manufacturer = GetStringDescriptor(deviceDescriptor.iManufacturer);
            string serial = GetStringDescriptor(deviceDescriptor.iSerialNumber);

            DeviceInfo.Text += String.Format("Device: VID=0x{0:X04}/PID=0x{1:X04}\r\n", deviceDescriptor.idVendor, deviceDescriptor.idProduct);
            DeviceInfo.Text += String.Format("Product: {0}\r\n", product);
            DeviceInfo.Text += String.Format("Manufacturer: {0}\r\n", manufacturer);
            DeviceInfo.Text += String.Format("Serial number: {0}\r\n", serial);

            success = SendUsbControl(interfaceNumber, (byte)BMREQUEST_DIR.DEVICE_TO_HOST, (byte)BMREQUEST_TYPE.VENDOR,
                (byte)BMREQUEST_RECIPIENT.DEVICE, 0x71, 4, 1,
                   globalBuffer, globalBufferLength, out LengthTransferred);


            ushort max_feature_value_index = globalBuffer[0];
            ushort feature_index = 0;
            ComboBox control = null;

            for (ushort i = 0; i < max_feature_value_index; i++)
            {
                success = SendUsbControl(interfaceNumber, (byte)BMREQUEST_DIR.DEVICE_TO_HOST, (byte)BMREQUEST_TYPE.VENDOR,
                    (byte)BMREQUEST_RECIPIENT.DEVICE, 0x71, 8, i,
                       globalBuffer, globalBufferLength, out LengthTransferred);
                if (globalBuffer[0] == 63)
                    break;

                string output_str = "";
                for (int s = (int)LengthTransferred - 1; s >= 0; s--)
                    output_str += (char)globalBuffer[s];

                if (output_str == "end")
                {
                    feature_index++;
                    control = null;
                }
                else
                {
                    if (control == null)
                        control = FindFeatureControl(feature_index);

                    if (control != null)
                    {
                        control.Items.Add(output_str);
                        feature_value_dict[(int)i] = output_str;
                        feature_value_lookup_dict[feature_index.ToString() + output_str] = (int)i;
                    }
                }

                if (i > 100)
                    break;
            }
            foreach (Control ctrl in this.Controls)
            {
                if (ctrl is ComboBox)
                {
                    int index = Convert.ToInt32(ctrl.Tag);
                    ComboBox box = (ComboBox)ctrl;

                    success = SendUsbControl(interfaceNumber, (byte)BMREQUEST_DIR.DEVICE_TO_HOST, (byte)BMREQUEST_TYPE.VENDOR,
                        (byte)BMREQUEST_RECIPIENT.DEVICE, 0x71, 4, (byte)(2 + index),
                           globalBuffer, globalBufferLength, out LengthTransferred);
                    int feature_value_index = (int)globalBuffer[0];
                    if (feature_value_dict.ContainsKey(feature_value_index))
                    {
                        string text = feature_value_dict[feature_value_index].ToString();
                        box.SelectedIndex = box.Items.IndexOf(text);
                    }
                }
            }
            return true;
        }

        public string GetInfo()
        {
            return DeviceInfo.Text;
        }

        ComboBox FindFeatureControl(int ind)
        { 
            foreach(Control ctrl in this.Controls)
            {
                if (ctrl is ComboBox && Convert.ToInt32(ctrl.Tag) == ind)
                    return (ComboBox)ctrl;
            }
            return null;
        }

        bool SendUsbControl(byte interfaceNum,
                   byte dir, byte type, byte recipient, byte request, ushort value, ushort index,
                   byte[] Buffer, uint BufferLength, out uint LengthTransferred)
        {
	        bool retVal = false;
	        WINUSB_SETUP_PACKET packet;
            packet.RequestType = (byte)((dir << 7) | (type << 5) | (recipient));
            packet.Request = request;
            packet.Value = value;
            packet.Index = index;
            packet.Length = 0;
            LengthTransferred = 0;
            if (usb.ClaimInterface(interfaceNum, false))
	        {
                if (usb.ControlTransfer(packet, Buffer, BufferLength, out LengthTransferred, IntPtr.Zero))
			        retVal = true;

                usb.ReleaseInterface(interfaceNum, false);
	        }
	        return retVal;
        }

        void RefreshValues()
        {
            InitializeDevice();
        }

        public bool SetFeatureFromFile(string fileName)
        {
            bool needReset = false;
            bool retValue;
            try
            {
                if (!File.Exists(fileName))
                {
                    DeviceInfo.Text = String.Format("{0} does not exist.", fileName);
                    return false;
                }
                using (StreamReader sr = File.OpenText(fileName))
                {
                    String input;
                    int i = 0;
                    while ((input = sr.ReadLine()) != null)
                    {
                        //format line feature=value
                        input = input.TrimStart().TrimEnd();
                        if (input.StartsWith(";") || string.IsNullOrEmpty(input))
                            continue;
                        string[] str = input.Split(new char[] {'='});
                        if (str.Length != 2)
                        {
                            DeviceInfo.Text = String.Format("Error in line {0}, string {1}", i, input);
                            return false;
                        }
                        string feature = str[0];
                        string value = str[1];

//                      if (feature.ToUpper() == "NEEDRESET" && value == "1")
                        if (feature.ToUpper() == "NEEDRESET") // Continue on "needreset=0" in .ini file. BSB 20120413
                        {
                            if (value == "1")
                                needReset = true;
                            continue;
                        }

                        int featureIndex = GetFeatureIndex(feature);
                        if (featureIndex == -1)
                        {
                            DeviceInfo.Text = String.Format("Feature {0} not found in possible features", feature);
                            return false;
                        }
                        ComboBox cbox = FindFeatureControl(featureIndex);
                        if (cbox == null)
                        {
                            DeviceInfo.Text = String.Format("Feature {0} not found in device", feature);
                            return false;
                        }
                        bool isFound = false;
                        for (int ci = 0; ci < cbox.Items.Count; ci++)
                        {
                            if (cbox.Items[ci].ToString().ToUpper() == value.ToUpper())
                            {
                                cbox.SelectedIndex = ci;
                                isFound = true;
                                break;
                            }
                        }
                        if (!isFound)
                        {
                            DeviceInfo.Text = String.Format("Feature value {0} for feature {1} not found in device", value, feature);
                            return false;
                        }
                            //Console.WriteLine("The end of the stream has been reached.");
                        i++;
                    }
                }
                retValue = true;
            }
            catch(Exception e)
            {
                DeviceInfo.Text = e.Message;
                return false;
            }

            retValue &= Save();
            if (needReset)
                ResetDevice();

            return retValue;
        }

        private void OnSave(object sender, EventArgs e)
        {
            Save();
        }

        private bool Save()
        {
            bool success = true;
            foreach (Control ctrl in this.Controls)
            {
                if (ctrl is ComboBox)
                {
                    int feature = Convert.ToInt32(ctrl.Tag);
                    ComboBox box = (ComboBox)ctrl;
                    string index = feature.ToString() + box.Text;
                    if (feature_value_lookup_dict.ContainsKey(index))
                    {
                        int feature_value_index = (int)feature_value_lookup_dict[index];

                        success &= SendUsbControl(interfaceNumber, (byte)BMREQUEST_DIR.DEVICE_TO_HOST, (byte)BMREQUEST_TYPE.VENDOR,
                            (byte)BMREQUEST_RECIPIENT.DEVICE, 0x71, 3, (ushort)(2 + feature + feature_value_index * 256),
                               globalBuffer, globalBufferLength, out LengthTransferred);
                    }
                }
            }
            return success;
        }

        private void OnRefresh(object sender, EventArgs e)
        {
            InitializeDevice();
        }

        private void OnReset(object sender, EventArgs e)
        {
            ResetDevice();
            System.Threading.Thread.Sleep(5000);
            InitializeDevice();
        }

        private bool ResetDevice()
        {
            return SendUsbControl(interfaceNumber, (byte)BMREQUEST_DIR.DEVICE_TO_HOST, (byte)BMREQUEST_TYPE.VENDOR,
                (byte)BMREQUEST_RECIPIENT.DEVICE, 0x0F, 0, 0,
                   globalBuffer, globalBufferLength, out LengthTransferred);
        }

        private void OnFactReset(object sender, EventArgs e)
        {
            bool success = SendUsbControl(interfaceNumber, (byte)BMREQUEST_DIR.DEVICE_TO_HOST, (byte)BMREQUEST_TYPE.VENDOR,
                (byte)BMREQUEST_RECIPIENT.DEVICE, 0x41, 0xFF, 0,
                   globalBuffer, globalBufferLength, out LengthTransferred);
            System.Threading.Thread.Sleep(5);
            InitializeDevice();
        }

        struct FeatureDescriptor
        {
            public int index;
            public string name;
            public FeatureDescriptor(int _index, string _name)
            {
                index = _index;
                name = _name;
            }
        };

        static FeatureDescriptor[] features = new FeatureDescriptor[]
        {
            new  FeatureDescriptor(0, "BoardType"),
            new  FeatureDescriptor(1, "ImageType"),
            new  FeatureDescriptor(2, "InType"),
            new  FeatureDescriptor(3, "OutType"),
            new  FeatureDescriptor(4, "AdcType"),
            new  FeatureDescriptor(5, "DacType"),
            new  FeatureDescriptor(6, "LcdType"),
            new  FeatureDescriptor(7, "LogType"),
            new  FeatureDescriptor(8, "FilterType"),
            new  FeatureDescriptor(9, "LQuirkType"),
        };

        static int GetFeatureIndex(string name)
        {
            foreach (FeatureDescriptor feature in features)
            {
                if (name.ToUpper() == feature.name.ToUpper())
                    return feature.index;
            }
            return -1;
        }

        private void comboBox6_SelectedIndexChanged(object sender, EventArgs e)
        {

        }
    }
}
