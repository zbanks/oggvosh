/** \file display.c VS1000 Developer Board Demo */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vs1000.h>
#include <audio.h>
#include <mappertiny.h>
#include <minifat.h>
#include "player.h"
#include "codec.h"
#include "romfont.h"
#include "usb.h"
#include "vectors.h"

#define PRETTY_USB_DISPLAY 1



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

u_int16 screenX, screenY;
__y u_int16 mySpectrum[16];
u_int16 pleaseUpdateSpectrum = 0;
u_int16 currentUSBMode = 0;



#define EMULATOR_DEBUG 0
#if EMULATOR_DEBUG
// put u_int16 as hex to serial console for debugging
__y const char hex[] = "0123456789abcdef";
// Note: VS1000A can't have inited y-vars in NAND boot
void puthex(u_int16 a) {
  char tmp[8];
  tmp[0] = hex[(a>>12)&15];
  tmp[1] = hex[(a>>8)&15];
  tmp[2] = hex[(a>>4)&15];
  tmp[3] = hex[(a>>0)&15];
  tmp[4] = ' ';
  tmp[5] = '\0';
  fputs(tmp, stdout);
}
#endif



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


// Write double height image data octet to display controller 
void ScreenPutDoubleData(u_int16 c){
  register u_int16 d;
  register u_int16 i;

  d=0;
  for (i=0; i<8; i++){
    d <<= 1;
    d |= ((c & 0x80) >> 7);
    d <<= 1;
    d |= ((c & 0x80) >> 7);
    c <<= 1;     
  }

  ScreenPutData(d);
  ScreenPutData(d);
  ScreenLocate(screenX-2, screenY+1);
  ScreenPutData(d>>8);
  ScreenPutData(d>>8);  
  ScreenLocate(screenX, screenY-1);
  
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
#define ScreenPutBigChar(c) ScreenOutputChar(c,ScreenPutDoubleData)

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


/// Busy wait i hundreths of second at 12 MHz clock
auto void BusyWaitHundreths(register u_int16 i) {
  while(i--){
    BusyWait10(); // Rom function, busy loop 10ms at 12MHz
  }
}



extern struct CodecServices cs; // Publish VS1000 internal data structure
void UserInterfaceIdleHook(void); // Prototype of ROM idle hook function


// Put packed string in Y ram (mainly for file name output)
void ScreenPutPacked(__y const char *d, u_int16 max, u_int16 xEnd){
  int i;

    for (i=0; i<max; i++){      
      register u_int16 c;
      c=*d++;
      if ((c & 0xff00U) == 0){
	break;
      }
      ScreenPutChar(c>>8);
      if ((c & 0xffU) == 0){
	break;
      }
      ScreenPutChar(c&0xff);
    }  
    
    while (screenX <= xEnd){
      ScreenPutData(0);
    }            
}


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


// Draw a u_int16 value to the screen as decimal number in large numbers
void ScreenPutBigInt(register u_int16 value, register u_int16 length){
  register u_int16 i; 
  u_int16 s[8];

  for (i=0; i<8; i++){
    s[i]=value % 10;
    value = value / 10;
  }
  do {
    ScreenPutBigChar(SYMBOL_DIGIT + s[--length]);
  } while (length);     
}


// Draw a volume bar (triangle) to screen
void VolumeBar(u_int16 volume){
  register u_int16 i;
  for (i=0; i<32; i++){
    register u_int16 d=0xff80;
    if (volume>i) {
      d >>= (i>>2)+1;
    }
    d &= 0x7f;
    ScreenPutData(d);
  }
}


// Draw a progress bar to the screen

// Allow code optimization by making the progress bar always start at
// the leftmost edge of screen
#define xMin 0

void ScreenPutBar(register u_int32 currentValue, u_int32 maxValue, u_int16 xMax){
  //u_int16 xMin=screenX;
  u_int16 xUntil=0;
  if (maxValue==0){
    maxValue=1;
  }
  if (currentValue>maxValue){
    currentValue=maxValue;
  }  
  xUntil=xMin+((currentValue*(xMax-xMin-4))/maxValue);

  
  ScreenPutData(12); //0b00001100
  ScreenPutData(30); //0b00011110
  ScreenPutData(63); //0b00111111
  ScreenPutData(127);//0b10100001
  
  while (screenX < (xMax-4)){
    register u_int16 d=161; //frame pixels //0b00011110
    if (screenX<xUntil){
      d |= 30; //fill pixels               //0b10010010
    }
    ScreenPutData(d);
  }
    
  ScreenPutData(146);
  ScreenPutData(140);
  ScreenPutData(72);
  ScreenPutData(48);
}




// Write zero data (black pixels) to display
auto void ScreenZeros (u_int16 n){
  while (n--) {
    ScreenPutData(0);
  }
}



// Update the screen in playing mode
void MyUiFunc(void){
  
  register u_int16 i;
  static u_int16 playTime=-1;
  static u_int16 phase = 0;
  
  haltTime = 0;

  currentUSBMode=0;

  if (uiTrigger){    

    pleaseUpdateSpectrum = 1;
    PERIP(GPIO1_MODE) |= 0x0008; //SO is PERIP

    if (++phase>16) phase=0;
    

    // Fast updates

    ScreenLocate(1,1);
    ScreenPutBigInt(player.currentFile+1,3);
    

    ScreenLocate(37,0);
    

    // Play/Pause
    if (player.pauseOn){
      ScreenPutChar(SYMBOL_PAUSE);
    }else{
      ScreenZeros(phase>>1);
      ScreenPutChar(SYMBOL_PLAY);
    }
    ScreenPutPacked(0,0,50);

    // Sample Rate / Random
    if (player.randomOn){
      ScreenPutChar('r');
      ScreenPutChar('n');
      ScreenPutChar('d');
    }else{
      ScreenPutUInt(cs.sampleRate/1000,2);
      ScreenPutChar('k');
      ScreenPutPacked(0,0,69);
    }

    // EarSpeaker
    //ScreenLocate(69,0);
    
    if (i=(earSpeakerReg/9000))
    {
      ScreenZeros(6-i);
      ScreenPutChar(SYMBOL_LEFTSPK);
      ScreenZeros(i*2);
      ScreenPutChar(SYMBOL_RIGHTSPK);
    }
    ScreenPutPacked(0,0,100);



    // Volume Bar
    // ScreenLocate(101,0);
    {
      s_int16 myVolume = 128-(24+player.volume);
      if (myVolume<0) myVolume = 0;
      VolumeBar(myVolume>>2);
    }

    

    ScreenLocate(0,5);
    for (i=0; i<16; i++){
      register s_int16 d,k;
      k=(mySpectrum[i]-8);
      if (k<0) k=0;
      if (k>8) k=8;
      d=0xff00u >> k;      
      ScreenPutData(d);
      ScreenPutData(d);
      ScreenPutData(d);
      ScreenPutData(d);
      ScreenZeros(4);

    }
    ScreenLocate(0,6);
    for (i=0; i<16; i++){
      register u_int16 d,k;
      k=mySpectrum[i];
      if (k>1){
	mySpectrum[i]-=2;
      }	      
      if (k>8) k=8;
      d=0xff00u >> k;      
      d &= 0x7f;

      ScreenPutData(d);
      ScreenPutData(d);
      ScreenPutData(d);
      ScreenPutData(d);
      ScreenZeros(4);

    }     



    // Once per second updates
    if (cs.playTimeSeconds>-1){
      if (playTime != cs.playTimeSeconds){
	playTime = cs.playTimeSeconds;	

	
	ScreenLocate(1,0);
	ScreenPutUInt(player.currentFile+1,2);
	ScreenPutChar('/');
	ScreenPutUInt(player.totalFiles,2);
	
	
	ScreenLocate(76,1);    
	ScreenPutBigInt(playTime / 60, 2);
	if (cs.playTimeSeconds & 1){
	  ScreenPutBigChar(':');
	}else{
	  ScreenPutBigChar(' ');
	}
	ScreenPutBigInt(playTime % 60 ,2);
 
	ScreenLocate(0,4);
	ScreenPutPacked(minifatInfo.longFileName,100,131);
	
	ScreenLocate(0,7);
	ScreenPutBar(cs.fileSize-cs.fileLeft,cs.fileSize,128);
      }
    } // End of once per second updates


    PERIP(GPIO1_MODE) &= ~0x0008; //S0 is GPIO
  }

  UserInterfaceIdleHook();
}




// Put all chars to screen
void FontTest(){
  u_int16 i;  

  ScreenInit();
  ScreenClear();

  ScreenLocate(0,0);

  for (i=32;i<211;i++){
    ScreenPutChar(i);
  }

}



// Custom spectrum analyzer function in RAM

const u_int16 specBins[16] = {
  12,    19,    29,    45,    70,   108,   166,   256,
  395,   609,   939,  1448,  2233,   3444,  5311,  8180
};

void MySpectrumAnalyzer (struct CodecServices *cs, s_int16 __y *data, s_int16 n,
			 s_int16 ch){
  u_int16 i; 
  u_int16 step = 8192/n; /* Tamankin saa longlog2(n):lla */
  u_int16 bin = 0;
  u_int32 acc = 0;
  u_int16 j;

  if (ch) return;
  if (pleaseUpdateSpectrum){
    pleaseUpdateSpectrum = 0;
    
    for (i=0; i<8192; i+=step) {
      acc += (s_int32)(*data) * *data;
      data++;
      
      if (i > specBins[bin]) {
	j=0;
        while (acc){  // crude log2 function
	  j++;
	  acc >>= 1;
	}
	if (j) j--; //decrease by 1

	if (j>mySpectrum[bin]){
	  mySpectrum[bin] = j;
	}
	
	bin++;
      }
    }    
  }
}





#if PRETTY_USB_DISPLAY
// Attach to USB handler to clear screen when usb becomes active
// NOTE: this is decorative only, not really needed by VS1000A
// NOTE: this function is timing critical!
void MyUSBHandler(){

  if (currentUSBMode==0){ // set to zero in MyUiFunc
    ScreenInit();
    ScreenClear();
    ScreenLocate(37,0);
    ScreenPutChar(SYMBOL_USB);
    ScreenPutChar('U');
    ScreenPutChar('S');
    ScreenPutChar('B');

    ScreenLocate(2,2);
    ScreenPutChar(SYMBOL_USB);
    ScreenLocate(76,2);
    ScreenPutChar(SYMBOL_DISKDRIVE);

    currentUSBMode=1;
  }
  RealUSBHandler();

}
#endif


#if PRETTY_USB_DISPLAY
// Attach to DecodeSetupPacket to display count of SETUP packets
// NOTE: this is decorative only, not really needed by VS1000A
// NOTE: this function is timing critical!
void MyDecodeSetupPacket(){
  static u_int16 setupPacketCount = 0;

  setupPacketCount++;      

  ScreenLocate(15,2);
  ScreenPutUInt(setupPacketCount,4);
  RealDecodeSetupPacket();
}
#endif


#if PRETTY_USB_DISPLAY
// Attach to MassStoragePacketFromPC to display number of MSC commands
// NOTE: this is decorative only, not really needed by VS1000A
// NOTE: this function is timing critical!
void MyMSCPacketFromPC(USBPacket *inPacket){
  static u_int16 mscPacketCount = 0;

  if (inPacket->length == 31) {
    mscPacketCount++;
    ScreenLocate(90,2);
    ScreenPutUInt(mscPacketCount,4);
  }
  RealMSCPacketFromPC(&USB.pkt);

}
#endif



// Boot
void main(void) {

  u_int16 i;

  for (i=0; i<2; i++){
    BusyWaitHundreths(50);
    ScreenInit(); // 27-JUN-2007 still something wrong with init,
                  // doesn't start every time, must consult hw designer
    BusyWaitHundreths(50);
    ScreenClear();
  }
  
  ScreenLocate(5,5);
  ScreenPutChar(SYMBOL_DISKDRIVE);
  ScreenPutChar('.');
  ScreenPutChar('.');
  ScreenPutChar('.');
  BusyWaitHundreths(100);

  {
    register u_int16 i;
    for (i=0; i<16; i++){
      mySpectrum[i] = 0;
    }
  }

  player.pauseOn = 0;
  cs.Spectrum = MySpectrumAnalyzer;
  SetHookFunction((u_int16)IdleHook, MyUiFunc);  

#if PRETTY_USB_DISPLAY
  SetHookFunction((u_int16)USBHandler, MyUSBHandler);
  SetHookFunction((u_int16)DecodeSetupPacket, MyDecodeSetupPacket);  
  SetHookFunction((u_int16)MSCPacketFromPC, MyMSCPacketFromPC);
#endif

}


