
// usbdesc.c : Example for changing USB descriptors 
// We will hook DecodeSetupPacket so that it sets our string descriptors
// each time a Setup packet (USB device/class request) is sent.

#include <vs1000.h>
#include <usblowlib.h>

#define VENDOR_NAME_LENGTH 6
const u_int16 myVendorNameStr[] = {
  ((VENDOR_NAME_LENGTH * 2 + 2) << 8) | 0x03,
  'M' << 8,
  'y' << 8,
  'C' << 8,
  'o' << 8,
  'r' << 8,
  'p' << 8
};

#define MODEL_NAME_LENGTH 6
const u_int16 myModelNameStr[] = {
  ((MODEL_NAME_LENGTH * 2 + 2) << 8) | 0x03,
  'G' << 8,
  'a' << 8,
  'd' << 8,
  'g' << 8,
  'e' << 8,
  't' << 8
};

#define SERIAL_NUMBER_LENGTH 12
u_int16 mySerialNumberStr[] = {
  ((SERIAL_NUMBER_LENGTH * 2 + 2) << 8) | 0x03,
  '1' << 8, // You can
  '2' << 8, // put any
  '3' << 8, // numbers you
  '4' << 8, // like here (over the '1' '2' '3' and '4')
  0x3000, 0x3000, 0x3000, 0x3000, // Last 8 digits of serial 
  0x3000, 0x3000, 0x3000, 0x3000  // number will be calculated here
};

// This is the new Device Descriptor. See the USB specification! 
// Note that since VS_DSP is 16-bit Big-Endian processor,
// tables MUST be given as byte-swapped 16-bit tables for USB compatibility!
// This device descriptor template is ok for mass storage devices.
const u_int16  myDeviceDescriptor [] = { 
  0x1201, 0x1001, 0x0000, 0x0040,   
  0x3412,   // byte-swapped Vendor ID (0x1234) Get own from usb.org!
  0x4523,   // byte-swapped Product ID (0x2345)
  0x5634,   // byte-swapped Device ID (0x3456)
  0x0102, 0x0301   
};



// When a USB setup packet is received, install our descriptors 
// and then proceed to the ROM function RealDecodeSetupPacket.
void MyInitUSBDescriptors(u_int16 initDescriptors) {
  RealInitUSBDescriptors(initDescriptors); /* call the default first */
  USB.descriptorTable[DT_VENDOR] = myVendorNameStr;
  USB.descriptorTable[DT_MODEL]  = myModelNameStr;
  USB.descriptorTable[DT_SERIAL] = mySerialNumberStr;
  USB.descriptorTable[DT_DEVICE] = myDeviceDescriptor;
}

const u_int16  bHexChar16[] = { // swapped Unicode hex characters
  0x3000, 0x3100, 0x3200, 0x3300, 0x3400, 0x3500, 0x3600, 0x3700,
  0x3800, 0x3900, 0x4100, 0x4200, 0x4300, 0x4400, 0x4400, 0x4500
};

void main(void) {
  u_int16 i;
  u_int32 mySerialNumber = 0x1234abcd;  // Unique serial number

  // Put unique serial number to serial number descriptor
  for (i=5; i<13; i++){
    mySerialNumberStr[i]=bHexChar16[mySerialNumber>>28];
    mySerialNumber <<= 4;
  }

  // Hook in function that will load new descriptors to USB struct
  SetHookFunction((u_int16)InitUSBDescriptors, MyInitUSBDescriptors);

} // Return to ROM code. 
