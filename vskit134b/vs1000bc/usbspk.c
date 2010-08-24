/** \file usbspk.c VS1000B/C/D USB Speaker + Mass Storage + Player demo */
// This uses interrupts to handle simultaneous USB audio and mass storage
// This version is for standard SLC nand flash (routines in ROM)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vs1000.h>
#include <dev1000.h>
#include <audio.h>
#include <mapper.h>
#include <physical.h>
#include <vsNand.h>
#include <mappertiny.h>
#include <minifat.h>
#include "player.h"
#include "codec.h"
#include "romfont.h"
#include "usb.h"
#include "vectors.h"
#include "scsi.h"
// Publish some VS1000 internal data structures
extern struct FsMapper *map;
extern struct FsPhysical *ph;
extern struct CodecServices cs; 
void UserInterfaceIdleHook(void); 
extern struct SCSIVARS {
  SCSIStageEnum State;
  SCSIStatusEnum Status;
  u_int16 *DataOutBuffer;
  u_int16 DataOutSize;
  u_int16 DataBuffer[32];
  u_int16 *DataInBuffer;
  u_int16 DataInSize;
  unsigned int BlocksLeftToSend;
  unsigned int BlocksLeftToReceive;
  u_int32 CurrentDiskSector;
  u_int32 mapperNextFlushed;
  u_int16 cswPacket[7];
  u_int32 DataTransferLength;
  s_int32 Residue;
  s_int16 DataDirection;
} SCSI;
__y u_int16 mySpectrum[16];
u_int16 pleaseUpdateSpectrum = 0;
u_int16 currentUSBMode = 0;




// OLED Support Routines

u_int16 screenX, screenY;

const unsigned char oledInit[]={
  0xae, // Display off
  0xd3, // Display offset
  0x00, // 'value
  0xa8, // Multiplex ratio
  0x3f, // 'value
  0xad, // DC/DC on
  0x8b, // 'value 8b
  0x40, // Display start line
  0xa0, // Segment remap
  0xc8, // Scan direction
  0x81, // Set Contrast (=brightness)
  0x30, // 00 is minimum, ff is maximum
  0xb0, // Page address
  0x00, // Column address low nibble
  0x10, // Column address high nibble
  0xa4, // Entire display on
  0xa6, // Normal (not inverted) display
  0xaf, // Display on
  0xd8, // Power Save
  0x05  // Low power mode
};


// Basic SPI functionality for OLED display controller connection

//MASTER, 8BIT, FSYNC PIN IDLE=1
#define SPI_MASTER_8BIT_CSHI   PERIP(SPI0_CONFIG) = SPI_CF_MASTER | SPI_CF_DLEN8 | SPI_CF_FSIDLE1

//MASTER, 8BIT, FSYNC PIN IDLE=0 (makes /CS)
#define SPI_MASTER_8BIT_CSLO   PERIP(SPI0_CONFIG) = SPI_CF_MASTER | SPI_CF_DLEN8 | SPI_CF_FSIDLE0

//MASTER, 16BIT, FSYNC PIN IDLE=0 (makes /CS)
#define SPI_MASTER_16BIT_CSLO  PERIP(SPI0_CONFIG) = SPI_CF_MASTER | SPI_CF_DLEN16 | SPI_CF_FSIDLE0

void InitSpi(){
  SPI_MASTER_8BIT_CSHI;
  PERIP(SPI0_FSYNC) = 0; // frame sync active level=low -> can be used to make /CS
  PERIP(SPI0_CLKCONFIG) = SPI_CC_CLKDIV * (2-1); // Spi clock mclk/2 = max 24 MHz
  PERIP(GPIO1_MODE) |= 0x1f; /* enable SPI pins */
}

// Definitions for OLED display 

#define SCREEN_SET_COMMAND_MODE PERIP(GPIO1_CLEAR_MASK) = 0x0004
#define SCREEN_SET_DATA_MODE PERIP(GPIO1_SET_MASK) = 0x0004
#define SCREEN_SELECT PERIP(GPIO0_CLEAR_MASK) = 0x4000
#define SCREEN_END PERIP(GPIO0_SET_MASK) = 0x4000 


// Put n binary octets to display controller
void ScreenPutsN(unsigned char *c, int n){
  SCREEN_SELECT;
  SPI_MASTER_8BIT_CSHI; // SPI's "own" CS remains high
  while(n--) {
    SpiSendReceive(*c++);
  }
}

// Write command c to display controller
void ScreenPutCommand(unsigned char c){
  SCREEN_SET_COMMAND_MODE;
  SCREEN_SELECT;
  SPI_MASTER_8BIT_CSHI; // SPI's "own" CS remains high  
  SpiSendReceive(c);
  SCREEN_END;
}

// Set x,y pointer of display controller
void ScreenLocate(int x, int y){
  screenX = x;
  screenY = y;
  y |= 0xB0;
  ScreenPutCommand(y);
  ScreenPutCommand(x & 0x0f);
  ScreenPutCommand((x >> 4) | 0x0010);
  SCREEN_END;
}


// Write image data octet to display controller
void ScreenPutData(u_int16 c){
  SCREEN_SET_DATA_MODE;
  SCREEN_SELECT;
  SPI_MASTER_8BIT_CSHI; // SPI's "own" CS remains high
  SpiSendReceive(c);
  SCREEN_END;

  SCREEN_SET_COMMAND_MODE;
  SCREEN_SELECT;
  SPI_MASTER_8BIT_CSHI; // SPI's "own" CS remains high  
  SpiSendReceive(0xE3);
  SCREEN_END;

  screenX++;
}


// Start the display controller
void ScreenInit(){
  SPI_MASTER_8BIT_CSHI;
  PERIP(SPI0_FSYNC) = 0; // frame sync active level=low -> can be used to make /CS
  PERIP(SPI0_CLKCONFIG) = SPI_CC_CLKDIV * (1-1); // Spi clock mclk/4 = max 12 MHz
  PERIP(GPIO1_MODE) |= 0x1f; /* enable SPI pins */

  PERIP(GPIO0_MODE) &= ~0x4000; //CS2 is GPIO (Display Chip Select)
  PERIP(GPIO0_DDR)  |= 0x4000; //CS2 is Output

  PERIP(GPIO1_MODE) &= ~0x0004; //SI is GPIO (Display Command/Data)
  PERIP(GPIO1_DDR)  |= 0x0004; //SI is Output

  SCREEN_SET_COMMAND_MODE;
  ScreenPutsN(oledInit,sizeof(oledInit));
  SCREEN_END;  
}

#define ScreenPutChar(c) ScreenOutputChar(c,ScreenPutData)

// Draw character c to screen using custom data output function
// This function is responsible for character mapping.
void ScreenOutputChar(register u_int16 c, void (*OutputFunction)(u_int16 c) ){
  u_int16 length;
  register u_int16 i;
  register u_int16 bit_index;
  register const u_int16 *p;
  
  if ((c<32)) { //Invalid char
    return;
  }    

  if ((c>210)) { //Invalid char
    return;
  }    

  if (c>132){ //Fixed width charset
    length = 5;    
    bit_index = 8;
    p = fontData+((c-133)*5);
  } else { //variable width charset ("normal" ascii)
    c -= 32; // ASCII ' ' (32 decimal) is in ROM index 0
    length = fontPtrs[c+1] - fontPtrs[c];
    bit_index = 0;
    p = &fontData[fontPtrs[c]];
  }

  if (screenX>131){
    screenY++;
    ScreenLocate(0,screenY);
  }

  for(i=0;i<length;i++){
    OutputFunction((*p++) >> bit_index);
  }  
  OutputFunction(0);
}



// Screen Clear
void ScreenClear(){
  u_int16 x,y;
  for (y=0; y<8; y++){
    ScreenLocate(2,y);
    for (x=0; x<132; x++){
      ScreenPutData(0);
    }
  }    
  ScreenLocate(0,0);
}


#define SCOPE_PUT(d) \
    ScreenLocate (scopeX++,7); \
    if (scopeX==132) { \
      scopeX=0; \
    } \
    ScreenPutData((d)); \
    ScreenPutData(0xff); \
    ScreenPutData(0); 


// Draw a u_int16 value to the screen as decimal number
void ScreenPutUInt(register u_int16 value, register u_int16 length){
  register u_int16 i; 
  u_int16 s[8];

  for (i=0; i<8; i++){
    s[i]=value % 10;
    value = value / 10;
  }
  do {
    ScreenPutChar(SYMBOL_NUMBER + s[--length]);
  } while (length);     
}

// Write zero data (black pixels) to display
auto void ScreenZeros (u_int16 n){
  while (n--) {
    ScreenPutData(0);
  }
}




// ---- USB DESCRIPTOR STUFF ---- //

#define DT_LANGUAGES 0
#define DT_VENDOR 1
#define DT_MODEL 2
#define DT_SERIAL 3
#define DT_DEVICE 4
#define DT_CONFIGURATION 5

#define VENDOR_NAME_LENGTH 7
const u_int16 myVendorNameStr[] = {
  ((VENDOR_NAME_LENGTH * 2 + 2)<<8) | 0x03,
  'V'<<8,'l'<<8,'s'<<8,'i'<<8,'F'<<8,'i'<<8,'n'<<8};

#define MODEL_NAME_LENGTH 8
const u_int16 myModelNameStr[] = {
  ((MODEL_NAME_LENGTH * 2 + 2)<<8) | 0x03,
  'S'<<8,'P'<<8,'I'<<8,'A'<<8,'u'<<8,'d'<<8,'i'<<8,'o'<<8};

#define SERIAL_NUMBER_LENGTH 12
u_int16 mySerialNumberStr[] = {
  ((SERIAL_NUMBER_LENGTH * 2 + 2)<<8) | 0x03,
  '0'<<8, // You should put your own serial number here
  '0'<<8,
  '0'<<8,
  '0'<<8,
  '0'<<8,
  '0'<<8,
  '0'<<8,
  '0'<<8,
  '0'<<8,
  '0'<<8,
  '0'<<8,
  '1'<<8, // Serial number should be unique for each unit
};

// This is the new Device Descriptor. See the USB specification! 
const u_int16  myDeviceDescriptor [] = "\p"
  "\x12" // Length
  "\x01" // Type (Device Descriptor)
  "\x10" // LO(bcd USB Specification version) (.10)
  "\x01" // HI(bcd USB Specification version) (1.)
  "\x00" // Device Class (will be specified in configuration descriptor)
  "\x00" // Device Subclass
  "\x00" // Device Protocol
  "\x40" // Endpoint 0 size (64 bytes)

  "\xfb" // LO(Vendor ID) (0x19fb=VLSI Solution Oy)
  "\x19" // HI(Vendor ID)

  "\xf3" // LO(Product ID) (0xeee0 - 0xeeef : VLSI Customer Testing)
  "\xee" // HI(Product ID) (customers can request an ID from VLSI)

  "\x00" // LO(Release Number)
  "\x00" // HI(Release Number)
  "\x01" // Index of Vendor (manufacturer) name string
  "\x02" // Index of Product (model) name string (most shown by windows)
  "\x03" // Index of serial number string
  "\x01" // Number of configurations (1)
;

#define CONFIG_DESC_SIZE 123

u_int16 myConfigurationDescriptor[] = "\p"
  
  // ______Configuration_descriptor____ at offset 0
  "\x09"   // Length: 9 bytes // (9)
  "\x02"   // Descriptor type: Configuration // (2)
  "\x7b"    // LO(TotalLength) (123)
  "\x00"    // HI(TotalLength) (0)
  "\x03"    // Number of Interfaces (3)
  "\x01"    // bConfigurationValue (1)
  "\x00"    // iConfiguration (0)
  "\x80"    // Attributes (128)
  "\x32"    // Max. Power 100mA (50)
  

  // ______Interface_descriptor____ at offset 9
  "\x09"   // Length: 9 bytes // (9)
  "\x04"   // Descriptor type: Interface // (4)
  "\x00"   // Interface number  // (1)
  "\x00"   // Alternate setting  // (0)
  "\x00"   // Number of endpoints  // (0)
  "\x01"   // Interface Class: Audio // (1)
  "\x01"   // Interface Subclass: Audio Control // (1)
  "\x00"   // Interface Protocol  // (0)
  "\x00"   // String index  // (0)
  
  // ______Audio Control Class Interface_descriptor____ at offset 18
  "\x09"   // Length: 9 bytes // (9)
  "\x24"   // Descriptor type: Audio Control Class Interface // (36)
  "\x01"   // Subtype Header // (1)
  "\x00"    // (0)
  "\x01"    // (1)
  "\x1E"    // (30)
  "\x00"    // (0)
  "\x01"    // (1)
  "\x01"    // (1)
  
  // ______Audio Control Class Interface_descriptor____ at offset 27
  "\x0C"   // Length: 12 bytes // (12)
  "\x24"   // Descriptor type: Audio Control Class Interface // (36)
  "\x02"   // Subtype Input Terminal // (2)
  "\x01"    // (1)
  "\x01"    // (1)
  "\x01"    // (1)
  "\x00"    // (0)
  "\x02"    // (2)
  "\x00"    // (0)
  "\x00"    // (0)
  "\x00"    // (0)
  "\x00"    // (0)
  
  // ______Audio Control Class Interface_descriptor____ at offset 39
  "\x09"   // Length: 9 bytes // (9)
  "\x24"   // Descriptor type: Audio Control Class Interface // (36)
  "\x03"   // Subtype Output Terminal // (3)
  "\x02"    // (2)
  "\x04"    // (4)
  "\x03"    // (3)
  "\x00"    // (0)
  "\x01"    // (1)
  "\x00"    // (0)
  
  // ______Interface_descriptor____ at offset 48
  "\x09"   // Length: 9 bytes // (9)
  "\x04"   // Descriptor type: Interface // (4)
  "\x01"   // Interface number  // (1)
  "\x00"   // Alternate setting  // (0)
  "\x00"   // Number of endpoints  // (0)
  "\x01"   // Interface Class: Audio // (1)
  "\x02"   // Interface Subclass: Audio Streaming // (2)
  "\x00"   // Interface Protocol  // (0)
  "\x00"   // String index  // (0)
  
  // ______Interface_descriptor____ at offset 57
  "\x09"   // Length: 9 bytes // (9)
  "\x04"   // Descriptor type: Interface // (4)
  "\x01"   // Interface number  // (1)
  "\x01"   // Alternate setting  // (1)
  "\x01"   // Number of endpoints  // (1)
  "\x01"   // Interface Class: Audio // (1)
  "\x02"   // Interface Subclass: Audio Streaming // (2)
  "\x00"   // Interface Protocol  // (0)
  "\x00"   // String index  // (0)
  
  // ______Audio Streaming Class Interface_descriptor____ at offset 66
  "\x07"   // Length: 7 bytes // (7)
  "\x24"   // Descriptor type: Audio Streaming Class Interface // (36)
  "\x01"   // Subtype  // (1)
  "\x01"    // (1)
  "\x0C"    // (12)
  "\x01"    // (1)
  "\x00"    // (0)
  
  // ______Audio Streaming Class Interface_descriptor____ at offset 73
  "\x0B"   // Length: 11 bytes // (11)
  "\x24"   // Descriptor type: Audio Streaming Class Interface // (36)
  "\x02"   // Subtype  // (2)
  "\x01"    // (1)
  "\x02"    // (2)
  "\x02"    // (2)
  "\x10"    // (16)
  "\x01"    // (1)
  "\x44"    // (68)
  "\xAC"    // (172)
  "\x00"    // (0)
  
  // ______Endpoint_descriptor____ at offset 84
  "\x09"   // Length: 9 bytes // (9)
  "\x05"   // Descriptor type: Endpoint // (5)
  "\x01"   // Endpoint Address  // (1)
  "\x09"   // Attributes.TransferType Isochronous(1) // (9)
  "\xFF"   //EP Size LSB // (255)
  "\x03"   //EP Size MSB (total 1023 bytes) // (3)
  "\x01"   //Polling Interval ms // (1)
  "\x00"   //Refresh // (0)
  "\x00"   //Sync Address // (0)
  
  // ______Class Endpoint_descriptor____ at offset 93
  "\x07"   // Length: 7 bytes // (7)
  "\x25"   // Descriptor type: Class Endpoint // (37)
  "\x01"   // Subtype  // (1)
  "\x00"    // (0)
  "\x00"    // (0)
  "\x00"    // (0)
  "\x00"    // (0)

  // ______Interface_descriptor____ at offset 9
  "\x09"   // Length: 9 bytes // (9)
  "\x04"   // Descriptor type: Interface // (4)
  "\x02"   // Interface number  // (2)
  "\x00"   // Alternate setting  // (0)
  "\x02"   // Number of endpoints  // (2)
  "\x08"   // Interface Class: Mass Storage // (8)
  "\x06"   // Interface Subclass:  // (6)
  "\x50"   // Interface Protocol  // (80)
  "\x00"   // String index  // (0)
  
  // ______Endpoint_descriptor____ at offset 18
  "\x07"   // Length: 7 bytes // (7)
  "\x05"   // Descriptor type: Endpoint // (5)
  "\x82"   // Endpoint Address  // (130)
  "\x02"   // Attributes.TransferType Bulk(2) // (2)
  "\x40"   //EP Size LSB // (64)
  "\x00"   //EP Size MSB (total 64 bytes) // (0)
  "\x10"   //Polling Interval ms // (16)
  
  // ______Endpoint_descriptor____ at offset 25
  "\x07"   // Length: 7 bytes // (7)
  "\x05"   // Descriptor type: Endpoint // (5)
  "\x03"   // Endpoint Address  // (3)
  "\x02"   // Attributes.TransferType Bulk(2) // (2)
  "\x40"   //EP Size LSB // (64)
  "\x00"   //EP Size MSB (total 64 bytes) // (0)
  "\x10"   //Polling Interval ms // (16)
  // end of descriptor  
  ;

// When a USB setup packet is received, install our descriptors 
// and then proceed to the ROM function RealDecodeSetupPacket.
void RealInitUSBDescriptors(u_int16 initDescriptors);
void MyInitUSBDescriptors(u_int16 initDescriptors){
  RealInitUSBDescriptors(1); // ROM set descriptors for mass storage
  USB.descriptorTable[DT_VENDOR] = myVendorNameStr;
  USB.descriptorTable[DT_MODEL]  = myModelNameStr;
  USB.descriptorTable[DT_SERIAL] = mySerialNumberStr;
  USB.descriptorTable[DT_DEVICE] = myDeviceDescriptor;
  USB.descriptorTable[DT_CONFIGURATION] = myConfigurationDescriptor;
  USB.configurationDescriptorSize = CONFIG_DESC_SIZE;
}



/// Busy wait i hundreths of second at 12 MHz clock
auto void BusyWaitHundreths(register u_int16 i) {
  while(i--){
    BusyWait10(); // Rom function, busy loop 10ms at 12MHz
  }
}



// New USB+MSC+USB AUDIO routines //

/// Handle incoming packet from PC.
/// All packets sent by pc to the bulk out endpoint should be sent here.
void ThisMSCPacketFromPC(USBPacket *inPacket) {
  // Check if it's a Command Block Wrapper
  if (inPacket->length == 31) {
    if (*(u_int32 *)inPacket->payload == 0x42435553UL) {

      // COMMAND BLOCK WRAPPER RECEIVED
      // Store Tag (word order copied as-is, no need to swap twice)
      *(u_int32 *)&SCSI.cswPacket[2] = *(u_int32*)&inPacket->payload[2];
      SCSI.DataTransferLength =
	((u_int32)SwapWord(inPacket->payload[5]) << 16) |
	SwapWord(inPacket->payload[4]);
      SCSI.DataDirection = inPacket->payload[6]; /*highest bit=1 device->host*/
    } else {
      SCSI.State = SCSI_INVALID_CBW; /* wait until BOT resets.. */
      USBStallEndpoint(0x80|MSC_BULK_IN_ENDPOINT);
      USBStallEndpoint(0x00|MSC_BULK_OUT_ENDPOINT);
      return;
    }
    // Call SCSI subsystem.
    DiskProtocolCommand(&(inPacket->payload[7])); /* ptr to len+cmd */
#if 1
    ScsiTaskHandler();//This is an extra function call to the
    //...scsi task handler to speed up time critical responses
    //to scsi requests. PC seems to have short nerve (3ms) sometimes
#endif
  } else {
    //NON-CBW BLOCK RECEIVED, must be a data block
    DiskDataReceived(inPacket->length, &(inPacket->payload[0]));

  }
}

// Update the screen in playing mode
void MyUiFunc(void){
  // Do nothing fancy here, just call default ROM routine
  // (fancy screen printing could be added here)
  UserInterfaceIdleHook();
}


// New mass storage handler
auto void MyMassStorage(void) {
  register __b0 int usbMode = 0;

  map->Delete(map); /* delete tiny mapper */
  PowerSetVoltages(&voltages[voltCoreUSB]);
  /** a 2.5ms-10ms delay to let the voltages change (rise) */
  BusyWait10();
  player.volume = 0;
  PlayerVolume();
  LoadCheck(NULL, 1); /* 4.0x clock required! */
  SetRate(44100U); /* Our USB-audio requires 44100Hz */

  ph = FsPhNandCreate(0);
  map = FsMapFlCreate(ph, 0);
  CleanDisk(0); 
  InitUSB(USB_MASS_STORAGE);  
  
  while (1) {
    USBHandler();
    PERIP(USB_CONTROL) |= USB_STF_SOF; 
    PERIP(USB_CONTROL) |= USB_STF_RX;

    ScsiTaskHandler();    

    /* If no SOF in such a long time.. */
    if (USBWantsSuspend()) {
      /* Do we seem to be detached?
	 Note: Now also requires USBWantsSuspend() to return TRUE! */
      if (USBIsDetached()) {
	break;
      }
      {
	register u_int16 io = PERIP(GPIO1_ODATA);
	PERIP(GPIO1_ODATA) = io & ~(LED1|LED2);      /* LED's off */
	USBSuspend(0); /* no timeout */
	PERIP(GPIO1_ODATA) = io;      /* Power LED on */
	/* with USB audio the LED settings are handled by user interface */
      }
      USB.lastSofTime = ReadTimeCount();
    }

    if (usbMode == 1) {
      if (AudioBufFill() < 32) {
	memset(tmpBuf, 0, sizeof(tmpBuf));
	AudioOutputSamples(tmpBuf, sizeof(tmpBuf)/2);
      }
      Sleep(); /* calls the user interface -- handle volume control */
    }
  }//while(1)


  hwSampleRate = 1; /* Forces FREQCTL update at next SetRate() */
  /* If disconnected, take us 'out of the bus' */
  PERIP(SCI_STATUS) &= ~SCISTF_USB_PULLUP_ENA;
  /* USB block reset */
  PERIP(USB_CONFIG) = 0x8000U;
  /* Clean the filesystem on disk */
  map->Flush(map, 1);
  /* No need to wait for the voltage to drop */
  PowerSetVoltages(&voltages[voltCorePlayer]);

  /* Clean unused FAT sectors. If RAMDISK, search for boot file */
  CleanDisk(0);

  map->Delete(map);
  map = FsMapTnCreate(ph, 0); /* re-initialize tiny mapper */
}



void ResetBulkEndpoints(void);
// Attach to DecodeSetupPacket to display count of SETUP packets
// NOTE: this is decorative only, not really needed by VS1000A
// NOTE: this function is timing critical!
void MyDecodeSetupPacket(){
  static u_int16 setupPacketCount = 0;
  setupPacketCount++;      
  //  ScreenLocate(15,2);
  //ScreenPutUInt(setupPacketCount,4);

  //EP0 shalt not be transmitting now.
  USB.EPReady[0] = 1; //EP0 shalt be ready now.
    
  if ((s_int16)USB.pkt.payload[0] >= 0 &&
      (USB.pkt.payload[USB_SP_REQUEST] & 0xff) ==
      USB_REQUEST_SET_INTERFACE) {
    USBResetEndpoint(0);
    /* index = interface, value = alternate setting */
    USB.interfaces =
      (USB.pkt.payload[USB_SP_VALUE] & 0xff00U) |
      (USB.pkt.payload[USB_SP_INDEX] >> 8);
#if 1 /* ok with windoze */
    if ( USB.pkt.payload[USB_SP_INDEX] != 0x0100 /*0x0001*/)
      ResetBulkEndpoints();
#endif
    USBSendZeroLengthPacketToEndpoint0(); 
  } else {
    RealDecodeSetupPacket();
  }
}


// Call wrapper to mass storage request handler
void MyMSCPacketFromPC(USBPacket *inPacket){
  static u_int16 mscPacketCount = 0;

  if (inPacket->length == 31) {
    mscPacketCount++;
  }
  ThisMSCPacketFromPC(&USB.pkt);
  PERIP(USB_EP_ST3) &= ~USB_STF_IN_FORCE_NACK;
}


// Get Packet from USB receive ring buffer
u_int16 GetPacket(USBPacket *packet) {
  register __c0 u_int16 st;
  register __c1 u_int16 ep;
  register __b0 u_int16 tmp;
  register __b1 u_int16 lenW;
  /*
   * There should be now some data waiting at receive buffer
   */
  st = USEX(PERIP(USB_RDPTR) + USB_RECV_MEM);
  ep = (s_int16)(st & 0x3C00) >> 10;
  st &= 0x03FF;
  packet->length = st;

  lenW = (st + 1)>>1;
  tmp = (PERIP(USB_RDPTR) + 1) & 0x03FF; 

  if (lenW <= (ENDPOINT_SIZE_1+1)>>1) {
    RingBufCopyX(packet->payload, (void *)(tmp + USB_RECV_MEM), lenW);
    PERIP(USB_RDPTR) = (lenW + tmp) & 0x03FF;
  }
  return ep & 0x7f;
}


u_int16 epPackets[4];
u_int32 bytesReceived=0;
u_int16 btimeCount=0;
// New USB handler
void MyUSBHandler(){
  __y u_int16 ep;
  register u_int16 info;
  u_int16 g;

  if (PERIP(USB_STATUS) & 0x8000U) {
    InitUSB(0);
    PERIP(USB_EP_ST3) &= ~USB_STF_IN_FORCE_NACK; // Release EP3
  }
  
  if (USB.EPReady[0] == 0) {
    USBContinueTransmission(0);
  }
  if (USB.EPReady[MSC_BULK_IN_ENDPOINT] == 0) {
    USBContinueTransmission(MSC_BULK_IN_ENDPOINT);
  }

  // Are there packet(s) in the receive buffer?
  Disable();
  if (PERIP(USB_RDPTR) != PERIP(USB_WRPTR) &&
      (USEX(PERIP(USB_RDPTR) + USB_RECV_MEM) & 0x3c00)
      != (AUDIO_ISOC_OUT_EP << 10) ) {
    Enable();
    PERIP(USB_EP_ST3) |= USB_STF_IN_FORCE_NACK;
    ep = USBReceivePacket(&USB.pkt);

    if (USB.pkt.length) { //Does it actually contain data?
      if (ep == 0) { // Setup Packet
	DecodeSetupPacket();	
      } else if (ep == AUDIO_ISOC_OUT_EP) {
	AudioPacketFromUSB(USB.pkt.payload, (USB.pkt.length+1)/2);
      } else if (ep == MSC_BULK_OUT_ENDPOINT) {
	MSCPacketFromPC(&USB.pkt);
      } else {
	// Unknown packet
      }
    } else {
      // Empty packet
    }
  } else {
    Enable();
  }
  PERIP(USB_EP_ST3) &= ~USB_STF_IN_FORCE_NACK;
}


// Interrupt handler for USB perip
auto void Interrupt0() {
  static u_int16 audioStash[90];
  register u_int16 g = PERIP(USB_STATUS);
  register u_int16 st;
  //SCOPE_PUT(g>>8);

PERIP(USB_EP_ST3) |= USB_STF_IN_FORCE_NACK;

  if (PERIP(USB_RDPTR) != PERIP(USB_WRPTR)) {
    st = USEX(PERIP(USB_RDPTR) + USB_RECV_MEM);
    if (((st >> 10) & 0x0f) == AUDIO_ISOC_OUT_EP) {
      GetPacket(audioStash);
      if (AudioBufFree() > 45) {
	Enable();
	AudioPacketFromUSB(&audioStash[1], (audioStash[0]+1)/2);
	Disable();
      }
    } else {
      /* not audio packet */
    }
  }

  if (g & USB_STF_SOF) {
    PERIP(USB_STATUS) = USB_STF_SOF;
    USB.lastSofTime = ReadTimeCount();
    USB.lastSofFill = AudioBufFill();
  }
  else if ((g & 0x0f) == 3) {
  }
}









// Boot Here
void main(void) {

  u_int16 i;

  // voltages when USB is attached and active
  voltages[voltCoreUSB] = 31;     // 31=max

  PERIP(GPIO1_ODATA) |=  LED1|LED2;      /* POWER led on */
  PERIP(GPIO1_DDR)   |=  LED1|LED2; /* SI and SO to outputs */
  PERIP(GPIO1_MODE)  &= ~(LED1|LED2); /* SI and SO to GPIO */

  memset(epPackets,0,sizeof(epPackets));

  player.pauseOn = 0;


  // Set Hook Functions
  SetHookFunction((u_int16)IdleHook, MyUiFunc);  
  SetHookFunction((u_int16)USBHandler, MyUSBHandler);
  SetHookFunction((u_int16)DecodeSetupPacket, MyDecodeSetupPacket);  
  SetHookFunction((u_int16)MSCPacketFromPC, MyMSCPacketFromPC);
  SetHookFunction((u_int16)InitUSBDescriptors, MyInitUSBDescriptors);


  // Install USB Interrupt Handler
  WriteIRam (0x20+INTV_USB, ReadIRam((u_int16)InterruptStub0));
  PERIP(INT_ENABLEL) |= INTF_USB;
  
  map = FsMapTnCreate(ph, 0); /* tiny mapper */
  PlayerVolume();


  // Main Loop
  while (1) {
    if (USBIsAttached()) {

      MyMassStorage();
    }
    
    if (InitFileSystem() == 0) {
      minifatInfo.supportedSuffixes = supportedFiles;
      player.totalFiles = OpenFile(0xffffU);
      if (player.totalFiles == 0)
	goto noFSnorFiles;
      
      player.nextStep = 1;
      player.nextFile = 0;
      
      while (1) {
	if (player.randomOn) {
	  register s_int16 nxt;
	retoss:
	  nxt = rand() % player.totalFiles;
	  if (nxt == player.currentFile && player.totalFiles > 1)
	    goto retoss;
	  player.currentFile = nxt;
	} else {
	  player.currentFile = player.nextFile;
	}
	if (player.currentFile < 0)
	  player.currentFile += player.totalFiles;
	if (player.currentFile >= player.totalFiles) {
	  player.currentFile -= player.totalFiles;
	}
	player.nextFile = player.currentFile + 1;
	
	if (OpenFile(player.currentFile) < 0) {
	  
	  //player.volumeOffset = -12; /*default volume offset*/
	  //PlayerVolume();

	  player.ffCount = 0;
	  cs.cancel = 0;
	  cs.goTo = -1;
	  cs.fileSize = cs.fileLeft = minifatInfo.fileSize;
	  cs.fastForward = 1; /* reset play speed to normal */
        //puts("play");
	  
	  {
	    register s_int16 ret;
	    ret = PlayCurrentFile();
		    }
	} else {
	  player.nextFile = 0;
	}
	if (USBIsAttached()) {
	  break;
	}
      }
    } else {
    noFSnorFiles:
      LoadCheck(&cs, 32); /* decrease or increase clock */
      memset(tmpBuf, 0, sizeof(tmpBuf));
      AudioOutputSamples(tmpBuf, sizeof(tmpBuf)/2);
      //puts("no samples\n");
      /* When no samples fit, calls the user interface
	 -- handle volume control and power-off */
    }
  }
}


