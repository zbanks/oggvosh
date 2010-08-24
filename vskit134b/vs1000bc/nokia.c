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
//#include "vectors.h"

#include "menu.c"

void lcdPut(register __a1 unsigned char c);
void ScreenInit();
u_int16 cpxLCDRect(unsigned int x, unsigned int y, unsigned int dx, unsigned int dy);
extern struct CodecServices cs; // Publish VS1000 internal data structure



#include <romfont.h>

#define DISON       0xaf
#define DISOFF      0xae
#define DISNOR      0xa6
#define DISINV      0xa7
#define COMSCN      0xbb
#define DISCTL      0xca
#define SLPIN       0x95
#define SLPOUT      0x94
#define PASET       0x75
#define CASET       0x15
#define DATCTL      0xbc
#define RGBSET8     0xce
#define RAMWR       0x5c
#define RAMRD       0x5d
#define PTLIN       0xa8
#define PTLOUT      0xa9
#define RMWIN       0xe0
#define RMWOUT      0xee
#define ASCSET      0xaa
#define SCSTART     0xab
#define OSCON       0xd1
#define OSCOFF      0xd2
#define PWRCTR      0x20
#define VOLCTR      0x81
#define VOLUP       0xd6
#define VOLDOWN     0xd7
#define TMPGRD      0x82
#define EPCTIN      0xcd
#define EPCOUT      0xcc
#define EPMWR       0xfc
#define EPMRD       0xfd
#define EPSRRD1     0x7c
#define EPSRRD2     0x7d
#define NOP         0x25

#define SPI_CF_DLEN9            (8<<1)
#define SPI_MASTER_9BIT_CSHI    PERIP(SPI0_CONFIG) = SPI_CF_MASTER | SPI_CF_DLEN9 | SPI_CF_FSIDLE1
#define LCD_CSEN                PERIP(GPIO0_CLEAR_MASK) = 0x4000
#define LCD_CSDS                PERIP(GPIO0_SET_MASK) = 0x4000
//#define LCD_CSEN                PERIP(GPIO0_CLEAR_MASK) = 0x4000;PERIP(GPIO1_CLEAR_MASK) = 0x1
//#define LCD_CSDS                PERIP(GPIO0_SET_MASK) = 0x4000;PERIP(GPIO1_SET_MASK) = 0x1
#define LCD_RESET_CLR           PERIP(GPIO1_CLEAR_MASK) = (1<<4)
#define LCD_RESET_SET           PERIP(GPIO1_SET_MASK) = (1<<4)
#define SPI_DATA_MASK           (0x100)
#define SPI_CMD_MASK            (0x000)

#define dxLCDScreen 130
#define dyLCDScreen 130

#define BG_COLOR 0xFF


/* Key Mapping to switches */

const struct KeyMapping myKeyMap[] = {
  {KEY_5,                                ke_volumeUp     }, // Key A: Volume step up
  {KEY_5 | KEY_LONG_PRESS,               ke_volumeUp     }, // Key A: Volume up continuous
  {KEY_4,                                ke_volumeDown2  }, // Key B: Volume step dn
  {KEY_4 | KEY_LONG_PRESS,               ke_volumeDown2  }, // Key B: Volume dn continuous
  {KEY_2,                                ke_previous     }, // Key C: Previous song
  {KEY_3,                                ke_next         }, // Key D: Next song
  {KEY_2 | KEY_LONG_PRESS,               ke_rewind       }, // Key E with Key A: rewind
  {KEY_3 | KEY_LONG_PRESS,               ke_forward      }, // Key E with Key B: fast forward
  {KEY_POWER | KEY_LONG_ONESHOT,         ke_powerOff     }, // Only one event after long press
  {KEY_POWER,                            ke_pauseToggle  },
  {KEY_1,                                ke_earSpeaker   },
  {KEY_1 | KEY_LONG_ONESHOT,             ke_randomToggle },
  {0, ke_null} // End of key mappings
};


const u_int16 lcdInit[]={
    (SPI_CMD_MASK | DISCTL),	// Display Control
    (SPI_DATA_MASK | 0x00),	// default
    (SPI_DATA_MASK | 0x20),	// (32 + 1) * 4 = 132 lines (of which 130 are visible)
    (SPI_DATA_MASK | 0x00),	// 0x00 OR 0x0A ?

    (SPI_CMD_MASK | COMSCN),	// Common Scan
    (SPI_DATA_MASK | 0x01),	// COM1-80: 1->80; COM81-160: 160<-81 (otherwise bottom of screen is upside down)

    (SPI_CMD_MASK | OSCON),	// Oscillation On
    (SPI_CMD_MASK | SLPOUT),	// Sleep Out (exit sleep mode)

    (SPI_CMD_MASK | VOLCTR),	// Electronic Volume Control (brightness/contrast)
    (SPI_DATA_MASK | 45),       // 20s is too low. 50 is a bit too high.
    (SPI_DATA_MASK | 3),        // was 3

    (SPI_CMD_MASK | TMPGRD),
    (SPI_DATA_MASK | 0),		// default

    (SPI_CMD_MASK | PWRCTR),	// Power Control Set
    (SPI_DATA_MASK | 0x0f),

    (SPI_CMD_MASK | DISINV),	// Inverse Display
    (SPI_CMD_MASK | PTLOUT),	// Partial Out (no partial display)

    (SPI_CMD_MASK | DATCTL),	// Data Control
    (SPI_DATA_MASK | 0x00),	// normal orientation; scan across cols), then rows
    (SPI_DATA_MASK | 0x00),	// RGB arrangement (RGB all rows/cols)
//	SPI_DATA_MASK | 0x04),	// RGB arrangement (RGB row 1), BGR row 2)
    (SPI_DATA_MASK | 0x01),	// 8 bit-color display
//	SPI_DATA_MASK | 0x02),	// 16 bit-color display

/* if 256-color mode), bytes represent RRRGGGBB; the following
    (maps to 4-bit color for each value in range (0-7 R/G), 0-3 B) */
    (SPI_CMD_MASK | RGBSET8),	// 256-color position set
    (SPI_DATA_MASK | 0x00),	// 000 RED
    (SPI_DATA_MASK | 0x02),	// 001  
    (SPI_DATA_MASK | 0x04),	// 010
    (SPI_DATA_MASK | 0x06),	// 011
    (SPI_DATA_MASK | 0x08),	// 100
    (SPI_DATA_MASK | 0x0a),	// 101
    (SPI_DATA_MASK | 0x0c),	// 110
    (SPI_DATA_MASK | 0x0f),	// 111
    (SPI_DATA_MASK | 0x00),	// 000 GREEN
    (SPI_DATA_MASK | 0x02),	// 001  
    (SPI_DATA_MASK | 0x04),	// 010
    (SPI_DATA_MASK | 0x06),	// 011
    (SPI_DATA_MASK | 0x08),	// 100
    (SPI_DATA_MASK | 0x0a),	// 101
    (SPI_DATA_MASK | 0x0c),	// 110
    (SPI_DATA_MASK | 0x0f),	// 111
    (SPI_DATA_MASK | 0x00),	//  00 BLUE
    (SPI_DATA_MASK | 0x06),	//  01
    (SPI_DATA_MASK | 0x09),	//  10
    (SPI_DATA_MASK | 0x0f)	//  11
};


/// Busy wait i hundreths of second at 12 MHz clock
auto void BusyWaitHundreths(register u_int16 i) {
  while(i--){
    BusyWait10(); // Rom function, busy loop 10ms at 12MHz
  }
}

#define lcdCmd(x)  lcdPut(SPI_CMD_MASK | x)
#define lcdData(x) lcdPut(SPI_DATA_MASK | x)
#define lcdEnd()   lcdPut(SPI_CMD_MASK | NOP)

void lcdPut(register __a1 unsigned char c){
    LCD_CSEN;
    SpiSendReceive(c);
    LCD_CSDS;
}

void ScreenInit(){
  
  SPI_MASTER_9BIT_CSHI;
  PERIP(SPI0_CLKCONFIG) = SPI_CC_CLKDIV * 3;
  PERIP(GPIO1_MODE) |= 0x1f; // enable SPI pins

  PERIP(GPIO0_MODE) &= ~0x4000; //CS2 is GPIO (Display Chip Select)
  PERIP(GPIO0_DDR)  |= 0x4000; //CS2 is Output
  
    
  //PERIP(GPIO0_MODE) &= ~0x1; //XCS 
  //PERIP(GPIO0_DDR) |= 0x1;


  PERIP(GPIO1_MODE) &= ~0x0004; //SI is GPIO (Display Command/Data)
  PERIP(GPIO1_DDR)  |= 0x0004; //SI is Output
  
  //PERIP(GPIO1_MODE) &= ~(1<<4);
  //PERIP(GPIO1_DDR)  |= 1<<4;
  //LCD_RESET_CLR;
  //BusyWaitHundreths(20);
  //LCD_RESET_SET;
  //BusyWaitHundreths(20);
  LCD_CSEN;
  {
    register const u_int16 *c = lcdInit;
    register int i;
    for(i=0; i<sizeof(lcdInit); i++){
        SpiSendReceive(*c++);
    } 
  }
  LCD_CSDS;
  
  BusyWaitHundreths(30);
  lcdCmd(DISON);
}



u_int16 cpxLCDRect(unsigned int x, unsigned int y, unsigned int dx, unsigned int dy) 
{
	unsigned char xlFirst, ylFirst, xlLast, ylLast;	// LCD coordinates

	/* check upper left corner */
	/* (x and y aren't too low since unsigned can't be < 0!) */
	if (x >= dxLCDScreen || y >= dyLCDScreen)	// completely off-screen
		return 0;

	/* check lower right corner */
	if (x + dx > dxLCDScreen)
		dx = dxLCDScreen - x;
	if (y + dy > dyLCDScreen)
		dy = dyLCDScreen - y;

	/* convert to LCD coordinates */
	xlLast = (xlFirst = 0 + x) + dx - 1;
	ylLast = (ylFirst = 2 + y) + dy - 1;

	/* note: for PASET/CASET, docs say that start must be < end,
	but <= appears to be OK; end is a "last" not "lim" value */
	lcdCmd(PASET);	// Page Address Set
	lcdData(ylFirst);	// start page (line)
	lcdData(ylLast);	// end page
	lcdCmd(CASET);	// Column Address Set
	lcdData(xlFirst);	// start address
	lcdData(xlLast);	// end address
	lcdCmd(RAMWR);	// Memory Write

	return (unsigned int)dx * dy;
}

void lcdClear(register char c){
  register u_int16 cpx;
  cpx = cpxLCDRect(0, 0, dyLCDScreen, dyLCDScreen);	
  while(cpx-- > 0)
        lcdData(c);
  lcdCmd(NOP);
}

void lcdPutPx(char x, char y, char c){
    u_int16 cpx;
    cpx = cpxLCDRect(x, y, 1, 1);
    while(cpx-- > 0){
        lcdData(c);
    }
    lcdCmd(NOP);
}

void lcdClrRect(char x, char y, char dx, char dy, register char c){
      register u_int16 cpx;
      cpx = cpxLCDRect(x, y, dx, dy);	
      while(cpx-- > 0)
            lcdData(c);
      lcdCmd(NOP);
}

#define ScreenPutChar(c) ScreenOutputChar(c,ScreenPutData)
#define ScreenPutBigChar(c) ScreenOutputChar(c,ScreenPutDoubleData)

// Draw character c to screen using custom data output function
// This function is responsible for character mapping.
void lcdPutChar(char *x, char *y, register u_int16 c, char bg, char fg){
  u_int16 length;
  register u_int16 i;
  register u_int16 bit_index;
  register const u_int16 *p;
  register u_int16 cpx;
  
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
  
  while ((*x+length)>131){
    *y += 9;
    if((*y+8) > dxLCDScreen)
        return;
    *x -= 131;
    if(*x < 2){
        *x = 2;
        break;
    }
  }
  
  
  lcdCmd(DATCTL);
  lcdData(0x04);
  lcdData(0x00);
  lcdData(0x01);
  //lcdCmd(DISOFF);
  
  cpx = cpxLCDRect(*x, *y, length, 8);
  
  while(cpx > 0){
    for(i=0; i<8; i++){
        if(*p & (1<<(i+bit_index))){
            lcdData(fg);
        }else{
            lcdData(bg);
        }
        cpx--;
    }
    p++;
  }  
  *x += length;
  
  cpx = cpxLCDRect(*x, *y, ++*x, 8);
  for(i=0; i<8; i++){
    lcdData(bg);
  }
  
  lcdCmd(DATCTL);
  lcdData(0x00);
  lcdData(0x00);
  lcdData(0x01);
  //lcdCmd(DISON);
  lcdCmd(NOP);
}



void lcdPutStr(char x, char y, char *str, char fg, char bg){
    char i;
    for(i=0;str[i] && i<100;i++){
        lcdPutChar(&x, &y, str[i], fg, bg);
    }
}

void lcdPutInt(char *x, char *y, register u_int16 value, register u_int16 length, char bg, char fg){
  register u_int16 i; 
  u_int16 s[8];

  for (i=0; i<8; i++){
    s[i]=value % 10;
    value = value / 10;
  }
  do {
    lcdPutChar(x, y, SYMBOL_NUMBER + s[--length], bg, fg);
  } while (length);     
}

// Put packed string in Y ram (mainly for file name output)
void lcdPutPackedStr(char x, char y, __y const char *d, char fg, char bg){
  int i;

    for (i=0; i<100; i++){      
      register u_int16 c;
      c=*d++;
      if ((c & 0xff00U) == 0){
	break;
      }
      lcdPutChar(&x, &y, c>>8, fg, bg);
      if ((c & 0xffU) == 0){
	break;
      }
      lcdPutChar(&x, &y, c&0xff, fg, bg);
    }  
              
}

void lcdPutBar(char y, register u_int32 currentValue, u_int32 maxValue, char bg){
  #define xMin 5
  #define xMax 125
  u_int16 xUntil=0;
  u_int16 xEndLen=0;
  register char i;
  static u_int16 lastUntil = 200;
  u_int16 cpx;
  if (maxValue==0){
    maxValue=1;
  }
  if (currentValue>maxValue){
    currentValue=maxValue;
  }  
  xUntil=xMin+((currentValue*(xMax-xMin))/maxValue);
  //             1   2   3   4   5   6   7   8
  //#define LARR 0b00110111111111110111001100010001
  //#define RARR 0b00001000010001101001000100101100
  #define LARR 0b00010011011111111111011100110001
  #define RARR 0b10000100001000010001001001001000
  if(lastUntil > xUntil){
      /*
      cpx = cpxLCDRect(xMin, y, 4, 8);
      while(cpx-- > 0){
        if((LARR >> cpx) & 1)
            lcdData(fg);
        else
            lcdData(bg);
      }
      lcdCmd(NOP);
      */
      
      xEndLen = xMax-xUntil-1;
      
      lcdClrRect(xMin, y+1, xMax-xMin, 6, bg);
      lcdClrRect(xMin, y, xMax-xMin, 1, 0x00);
      lcdClrRect(xMin, y+7, xMax-xMin, 1, 0x00);
      
      //lcdClrRect(xMin, y+1, xUntil-xMin, 6, 0x00);
      
      
      cpxLCDRect(xMax, y+1, 1, 6);
        lcdData(0b01001001);
        lcdData(0b10010010);
        lcdData(0b11011011);
        lcdData(0b10010010);
        lcdData(0b10010010);
        lcdData(0b01001001);
    
      /*
      cpx = cpxLCDRect(xMax-4, y, 4, 8);
      while(cpx-- > 0){
        if((RARR >> cpx) & 1)
            lcdData(fg);
        else
            lcdData(bg);
      }
      */
     lastUntil = xMin-1;
  }
  //lcdClrRect(lastUntil, y+1, xUntil-xMin-4, 6, 'U');
    for(;lastUntil < xUntil;lastUntil++){
        cpxLCDRect(lastUntil, y+1, 1, 6);

        lcdData(0b01001001);
        lcdData(0b10010010);
        lcdData(0b11011011);
        lcdData(0b10010010);
        lcdData(0b10010010);
        lcdData(0b01001001);
    }
  
  lastUntil = xUntil;
  lcdCmd(NOP);
  /*
  ScreenPutData(12); //0b00001100
  ScreenPutData(30); //0b00011110
  ScreenPutData(63); //0b00111111
  ScreenPutData(127);//0b11111111
  
  
  while (screenX < (xMax-4)){
    register u_int16 d=161; //frame pixels //0b10100001
    if (screenX<xUntil){
      d |= 30; //fill pixels               //0b00011110        
    }
    ScreenPutData(d);
  }
    
  ScreenPutData(146); //0b10010010
  ScreenPutData(140); //0b10001100
  ScreenPutData(72);  //0b01001000
  ScreenPutData(48);  //0b00110000
  */
}




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

// Update the screen in playing mode
void MyUiFunc(void){
  
  register u_int16 i;
  static u_int16 playTime=-1;
  static u_int16 isPaused=-1;
  static u_int16 isRandom=-1;
  static u_int16 volume=200;
  static u_int16 trackNum=-1;
  static u_int16 phase = 0;
  
  char x, y;
  
  haltTime = 0;

  currentUSBMode=0;
  //lcdPutPx(129,129,0xff);
  //puts("u");
 /* 
  if (uiTrigger){    

    pleaseUpdateSpectrum = 1;
    PERIP(GPIO1_MODE) |= 0x0008; //SO is PERIP

    if (++phase>16) phase=0;
  */  

    // Fast updates
/*
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
*/
    x = 32;
    y = 2;
    //lcdClrRect(30, 0, 70, 10, BG_COLOR);
    
    
    //ScreenPutPacked(0,0,50);

    // Sample Rate / Random
    if(player.randomOn != isRandom){
        lcdClrRect(x, y, 40, 10, BG_COLOR);
        if (player.randomOn){
          lcdPutChar(&x, &y, 'r', BG_COLOR, 0b00000100);
          lcdPutChar(&x, &y, 'n', BG_COLOR, 0b00000100);
          lcdPutChar(&x, &y, 'd', BG_COLOR, 0b00000100);
        }else{
          lcdPutInt(&x, &y, cs.sampleRate/1000, 2, BG_COLOR, 0b00000100);
          lcdPutChar(&x, &y, 'k', BG_COLOR, 0b00000100);
          //ScreenPutPacked(0,0,69);
        }
        isRandom = player.randomOn;
        // also redraw volume
        volume = -1;
    }

    // EarSpeaker
    //ScreenLocate(69,0);
    x = 51;
    
    //if(...)
    i=(earSpeakerReg/18000);
    if(volume != earSpeakerReg + player.volume){
      lcdPutChar(&x, &y, SYMBOL_LEFTSPK, BG_COLOR, 0b00000010);
      lcdPutChar(&x, &y, 'A'+i, BG_COLOR, 0b00000111);
      lcdPutChar(&x, &y, SYMBOL_RIGHTSPK, BG_COLOR, 0b00000010);
      
      x++;x++;
      if(player.volume < 0){
        lcdPutChar(&x, &y, '-', BG_COLOR, 0b00000010);
        lcdPutInt(&x, &y, -player.volume, 2, BG_COLOR, 0b00000010);
      }else{
        lcdPutInt(&x, &y, player.volume, 3, BG_COLOR, 0b00000010);
      }
      lcdPutChar(&x, &y, 'd', BG_COLOR, 0b00000010);
      lcdPutChar(&x, &y, 'b', BG_COLOR, 0b00000010);
    }
    volume = earSpeakerReg + player.volume;
    //ScreenPutPacked(0,0,100);


/*
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

*/

    // Once per second updates
    if (cs.playTimeSeconds>-1){
      if (playTime != cs.playTimeSeconds || player.pauseOn != isPaused){
	    playTime = cs.playTimeSeconds;	

	    //lcdClrRect(0, 0, 130, 10, BG_COLOR);
	    x = 2;  
	    y = 2;
	    lcdPutInt(&x, &y, player.currentFile+1,2, BG_COLOR, 0x01); 
	    lcdPutChar(&x, &y, '/', BG_COLOR, 0x01); 
	    lcdPutInt(&x, &y, player.totalFiles,2, BG_COLOR, 0x01); 
	
	
	    x = 99;
	    y = 2;   
	    lcdPutInt(&x, &y, playTime / 60, 2, BG_COLOR, 0x01); 
	    if (cs.playTimeSeconds & 1){
	      lcdPutChar(&x, &y, ':', BG_COLOR, 0x01); 
	    }else{
	      lcdPutChar(&x, &y, ' ', BG_COLOR, 0x01); 
	    }
	    lcdPutInt(&x, &y, playTime % 60 ,2, BG_COLOR, 0x01); 
        
        
        x++;

        if (player.pauseOn){
          lcdPutChar(&x, &y, SYMBOL_PAUSE, BG_COLOR, 0b11101000);
        }else{
          //ScreenZeros(phase>>1); //TODO
          lcdPutChar(&x, &y, SYMBOL_PLAY, BG_COLOR, 0b00111100);
        }
        isPaused = player.pauseOn;
        
        
        
	    //ScreenLocate(0,4);
	    //ScreenPutPacked(minifatInfo.longFileName,100,131);
	    //DBG_PUTS(minifatInfo.longFileName);
	    lcdPutPackedStr(2, 22, minifatInfo.longFileName, BG_COLOR, 0x00);
	
	    lcdPutBar(12, cs.fileSize-cs.fileLeft,cs.fileSize, BG_COLOR);
	    //lcdPutStr(50,50,"t", 0xff, 0x00);
	    
	    
    
	  }
	}
	// Play/Pause
    

	
	/*
      }
    } // End of once per second updates


    PERIP(GPIO1_MODE) &= ~0x0008; //S0 is GPIO
   
  }
 */
  UserInterfaceIdleHook();

}




void UserInterfaceIdleHook(void); // Prototype of ROM idle hook function
//u_int16 RealPlayCurrentFile(void);


u_int16 MyPlayFile(void){
    //DBG_PUTS("p");
    unsigned char x = 100, y = 100;
    BusyWaitHundreths(50);
    ScreenInit();
    BusyWaitHundreths(50);
    lcdClear(BG_COLOR);
    lcdPutChar(&x, &y, '3', BG_COLOR, 0);
    RealPlayCurrentFile();
    

    //DBG_PUTS(minifatInfo.longFileName);
    //lcdPutPx(100,100,0xff);

    
}






// Boot
void main(void) {

  u_int16 i;
  char x;
  char y;
  //PERIP(GPIO1_ODATA) |=  (1<<4);      /* POWER led on */
  //PERIP(GPIO1_DDR)   |=  (1<<4); /* SI and SO to outputs */
  //PERIP(GPIO1_MODE)  |= ((1<<4)); /* SI and SO to GPIO */
  //for (i=0; i<1; i++){ //running it twice helps ensure it starts correctly.
        //PERIP(GPIO1_CLEAR_MASK) = (1<<4);
        //BusyWaitHundreths(70);
        //PERIP(GPIO1_SET_MASK) = (1<<4);
    //BusyWaitHundreths(50);
    //ScreenInit();
    //BusyWaitHundreths(50);
    //lcdClear(BG_COLOR);
  //}
  //BusyWaitHundreths(5000);
  //DBG_PUTS("h");
    /*
    {
    u_int16 cpx;
    cpx = cpxLCDRect(0, 0, dyLCDScreen, dyLCDScreen);	
    while(cpx-- > 0)
          lcdData(cpx & 0xff);
    lcdCmd(NOP);
    }
    */
    
    //lcdClear(0xff);
    //x = y = 40;
    //lcdPutChar(&x, &y, 32+(i%(210-32)), 0xff, 0x03); 
    //lcdPutChar(&x, &y, 32+((i+2)%(210-32)), 0xff, 0x03); 
    //lcdPutStr(2, 2, "TEST123!", 0xf7, 0x00);
    //lcdPutStr(2, 10, "This is a long, multi-line test that should determine if the quick brown fox really CAN jump over a lazy dog, or if it is MERELY a typographic test. I can count to 1,234,567,890.0123; can you?", 0xff, 0x80);
  currentKeyMap = myKeyMap;
  player.volume = 60;
  PlayerVolume();
  player.pauseOn = 0;
  
  //cs.Spectrum = MySpectrumAnalyzer;
  SetHookFunction((u_int16)IdleHook, MyUiFunc);  
  //SetHookFunction((u_int16)IdleHook, UIdleHook);
  SetHookFunction((u_int16)PlayCurrentFile, MyPlayFile); //CAUSES PROBLEMS!
//DBG_PUTS("r");
  //PlayerVolume();
    while (1) {
    
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
        //DBG_PUTS("play");
	  
	  {
	    register s_int16 ret;
	    ret = PlayCurrentFile();
		    }
	} else {
	  player.nextFile = 0;
	}
      }
    } else {
    noFSnorFiles:
      LoadCheck(&cs, 32); /* decrease or increase clock */
      memset(tmpBuf, 0, sizeof(tmpBuf));
      AudioOutputSamples(tmpBuf, sizeof(tmpBuf)/2);
      puts("no samples\n");
      /* When no samples fit, calls the user interface
	 -- handle volume control and power-off */
    }
  }
}


