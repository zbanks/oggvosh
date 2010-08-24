// MMCI.C - SDCARD Ogg Player + Simultaneous USB Speakers and SD Card Reader/Writer

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vs1000.h>
#include <audio.h>
#include <mappertiny.h>
#include <minifat.h>
#include <codec.h>
#include <vsNand.h>
#include <player.h>
#include <usblowlib.h>
#include <usb.h>
#include <scsi.h>

#define USE_USB
//#define DISPLAY

//#define USE_WAV /* Play linear wav files using CodMiniWav from libdev1000. */
//#define USE_MATRIX_KEYBOARD /* upto 16 keys using a keyboard matrix */
//#define USE_DEBUG
//#define DEBUG_LEVEL 3

#define USE_HC      /* Support SD-HC cards. */
//#define PATCH_LBAB 
/* Removes 4G restriction from USB (SCSI).
   Also detects MMC/SD removal while attached to USB.
   (62 words) */
//#define DISPLAY /* Use serial display. */



#ifdef DISPLAY
#include <romfont.h>
struct screen {
    u_int16 X, Y, invertMask;
} screen;
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
#define SPI_MASTER_8BIT_CSHI    PERIP(SPI0_CONFIG) = SPI_CF_MASTER | SPI_CF_DLEN8 | SPI_CF_FSIDLE1
#define SCREEN_SET_COMMAND_MODE PERIP(GPIO1_CLEAR_MASK) = 0x0004
#define SCREEN_SET_DATA_MODE    PERIP(GPIO1_SET_MASK) = 0x0004
#define SCREEN_SELECT           PERIP(GPIO0_CLEAR_MASK) = 0x4000
#define SCREEN_END              PERIP(GPIO0_SET_MASK) = 0x4000 


// Write command c to display controller
void ScreenPutCommand(register __a0 unsigned char c) {
  SCREEN_SET_COMMAND_MODE;
  SCREEN_SELECT;
  //SPI_MASTER_8BIT_CSHI; // SPI's "own" CS remains high  
  SpiSendReceive(c);
  SCREEN_END;
}

// Set x,y pointer of display controller
void ScreenLocate(register __b0 int x, register __b1 int y){
  screen.X = x;
  screen.Y = y;
  ScreenPutCommand(y | 0xb0);
  ScreenPutCommand(x & 0x0f);
  ScreenPutCommand((x >> 4) | 0x0010);
  //SCREEN_END;
}


// Write image data octet to display controller
void ScreenPutData(register __a1 u_int16 c) {
  SCREEN_SET_DATA_MODE;
  SCREEN_SELECT;
  //SPI_MASTER_8BIT_CSHI; // SPI's "own" CS remains high
  SpiSendReceive(c ^ screen.invertMask);
  SCREEN_END;

  SCREEN_SET_COMMAND_MODE;
  SCREEN_SELECT;
  //SPI_MASTER_8BIT_CSHI; // SPI's "own" CS remains high  
  SpiSendReceive(0xE3);
  SCREEN_END;
  ++screen.X;
  if (screen.X > 131) {
    ScreenLocate(0, ++screen.Y);
  }
}
void ScreenInit(){
  PERIP(SPI0_FSYNC) = -1; // keep SPI CS high during transfer!
  SPI_MASTER_8BIT_CSHI;
  PERIP(SPI0_CLKCONFIG) = SPI_CC_CLKDIV * (1-1); // Spi clock mclk/4 = max 12 MHz
  PERIP(GPIO1_MODE) |= 0x1f; /* enable SPI pins */

  PERIP(GPIO0_MODE) &= ~0x4000; //CS2 is GPIO (Display Chip Select)
  PERIP(GPIO0_DDR)  |= 0x4000; //CS2 is Output

  PERIP(GPIO1_MODE) &= ~0x0004; //SI is GPIO (Display Command/Data)
  PERIP(GPIO1_DDR)  |= 0x0004; //SI is Output

  SCREEN_SET_COMMAND_MODE;
  SCREEN_SELECT;
  {
      register const unsigned char *c = oledInit;
      register int i;
      //SPI_MASTER_8BIT_CSHI; // SPI's "own" CS remains high
      for (i=0; i<sizeof(oledInit); i++)
	  SpiSendReceive(*c++);
  }
  SCREEN_END;
  screen.X = screen.Y = screen.invertMask = 0;
}

// Draw character c to screen using custom data output function
// This function is responsible for character mapping.
void ScreenPutChar(register __c0 u_int16 c) {
  register u_int16 length;
  register u_int16 bit_index;
  register const u_int16 *p;
  
  if (c < 32 || c > 210) { //Invalid char
    return;
  }
  if (c > 132) { //Fixed width charset
    length = 5;    
    bit_index = 8;
    p = &fontData[5*(c-133)];
  } else { //variable width charset ("normal" ascii)
    // ASCII ' ' (32 decimal) is in ROM index 0
    length = fontPtrs[c-31] - fontPtrs[c-32];
    bit_index = 0;
    p = &fontData[fontPtrs[c-32]];
  }
  /* prevent wrap in the middle of character */
  if (screen.X + length > 132) {
    ScreenLocate(0, ++screen.Y);
  }
  {
      register u_int16 i;
      for (i=0; i<length; i++){
	  ScreenPutData((s_int16)(*p++) >> bit_index);
      }
  }  
  ScreenPutData(0);
}



// Screen Clear
void ScreenClear(){
    register u_int16 x, y;
    for (y=0; y<8; y++){
	ScreenLocate(0, y);
	for (x=0; x<132; x++){
	    ScreenPutData(0);
	}
    }    
    ScreenLocate(0, 0);
}


// Put packed string in Y ram (mainly for file name output)
void ScreenPutPacked(__y const u_int16 *d, u_int16 maxChars, u_int16 xEnd) {
    register int i;
    for (i=0; i<maxChars; i++) {
	register u_int16 c = *d++;
	if ((c & 0xff00U) == 0)
	    break;
	ScreenPutChar(c>>8);
	if ((c & 0xffU) == 0)
	    break;
	ScreenPutChar(c & 0xff);
    }
    i = screen.X;
    for (; i < xEnd; i++) {
	ScreenPutData(0);
    }            
}
void ScreenPutPackedX(const u_int16 *d, u_int16 maxChars, u_int16 xEnd) {
    register int i;
    for (i=0; i<maxChars; i++) {
	register u_int16 c = *d++;
	if ((c & 0xff00U) == 0)
	    break;
	ScreenPutChar(c>>8);
	if ((c & 0xffU) == 0)
	    break;
	ScreenPutChar(c & 0xff);
    }
    i = screen.X;
    for (; i < xEnd; i++) {
	ScreenPutData(0);
    }            
}


#endif/*DISPLAY*/

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
  u_int16 cswPacket[7];  /*< command reply (Command Status Wrapper) data */
  u_int32 DataTransferLength; /*< what is expected by CBW */
  s_int32 Residue; /*< difference of what is actually transmitted by SCSI */
  s_int16 DataDirection;
} SCSI;

#ifdef PATCH_LBAB
#include <scsi.h>
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
  u_int16 cswPacket[7];  /*< command reply (Command Status Wrapper) data */
  u_int32 DataTransferLength; /*< what is expected by CBW */
  s_int32 Residue; /*< difference of what is actually transmitted by SCSI */
  s_int16 DataDirection;
} SCSI;
#endif/*PATCH_LBAB*/


#include <dev1000.h>

enum mmcState {
    mmcNA = 0,
    mmcOk = 1,
};
struct {
    enum mmcState state;
    s_int16 errors;
    s_int16 hcShift;
    u_int32 blocks;
} mmc;


#ifdef USE_DEBUG
static char hex[] = "0123456789ABCDEF";
void puthex(u_int16 d) {
    register int i;
    char mem[5];
    for (i=0;i<4;i++) {
	mem[i] = hex[(d>>12)&15];
	d <<= 4;
    }
    mem[4] = '\0';
    fputs(mem, stdout);
}
#endif

extern struct FsNandPhys fsNandPhys;
extern struct FsPhysical *ph;
extern struct FsMapper *map;
extern struct Codec *cod;
extern struct CodecServices cs;
extern u_int16 codecVorbis[];


s_int16 InitializeMmc(s_int16 tries) {
    register s_int16 i, cmd;
    mmc.state = mmcNA;
    mmc.blocks = 0;
    mmc.errors = 0;

#if DEBUG_LEVEL > 1
    puthex(clockX);
    puts(" clockX");
#endif
 tryagain:
    IdleHook();

    mmc.hcShift = 9;
    if (tries-- <= 0) {
	return ++mmc.errors;
    }

    for (i=0; i<512; i++) {
	SpiSendClocks();
    }

    /* MMC Init, command 0x40 should return 0x01 if all is ok. */
    i = MmcCommand(MMC_GO_IDLE_STATE/*CMD0*/|0x40,0);
    if (i != 1) {
#if DEBUG_LEVEL > 1
	puthex(i);
	puts(" Init");
#endif
	BusyWait10();
	goto tryagain;//continue; /* No valid idle response */
    }
    cmd = MMC_SEND_OP_COND|0x40;
#ifdef USE_HC
    /*CMD8 is mandatory before ACMD41
      for hosts compliant to phys. spec. 2.00 */
    i = MmcCommand(MMC_SEND_IF_COND/*CMD8*/|0x40,
		   0x00000122/*2.7-3.6V*/); /*note: 0x22 for the right CRC!*/
#if DEBUG_LEVEL > 2
    puthex(i);
    puts("=IF_COND");
#endif
    if (i == 1) {
	/* MMC answers: 0x05 illegal command,
	   v2.0 SD(HC-SD) answers: 0x01 */
	/* Should we read the whole R7 response? */
#if DEBUG_LEVEL > 1
	//SpiSendReceiveMmc(-1, 32);
	puthex(SpiSendReceiveMmc(-1, 16));
	puthex(SpiSendReceiveMmc(-1, 16));
	puts("=R7");
#endif
	cmd = 0x40|41; /* ACMD41 - SD_SEND_OP_COND */
    }
#endif /*USE_HC*/


#if 0
    do {
	i = MmcCommand(MMC_READ_OCR/*CMD58*/|0x40, 0);
#if DEBUG_LEVEL > 1
	puthex(i);
	puthex(SpiSendReceiveMmc(-1, 16));
	puthex(SpiSendReceiveMmc(-1, 16));
	puts("=READ_OCR");
#endif
    } while (0);
#endif

    /* MMC Wake-up call, set to Not Idle (mmc returns 0x00)*/
    i = 0; /* i is 1 when entered .. but does not make the code shorter*/
    while (1) {
	register int c;
#ifdef USE_HC
	if (cmd == (0x40|41)) {
	    c = MmcCommand(0x40|55/*CMD55*/,0);
#if DEBUG_LEVEL > 2
	    puthex(c);
	    puts("=CMD55");
#endif
	}
#endif

	c = MmcCommand(cmd/*MMC_SEND_OP_COND|0x40*/,
#ifdef USE_HC
		       /* Support HC for SD, AND for MMC! */
		       //i ? 0x40100000UL : 0x00000000UL
		       0x40100000UL
#else
		       0
#endif
	    );

#if DEBUG_LEVEL > 1
	puthex(c);
	puts("=ACMD41");
#endif

	if (c == 0) {
#if DEBUG_LEVEL > 1
	    puts("got 0");
#endif
	    break;
	}
	//BusyWait10();
	if (++i >= 25000 /*Large value (12000) required for MicroSD*/
	    || c != 1) {
#if 0
	    /* Retry with CMD1 */
	    if (c == 0xff && i == 2) {
		cmd = MMC_SEND_OP_COND|0x40;
		continue;
	    }
#endif

#if DEBUG_LEVEL > 1
	    puthex(c);
	    puts(" Timeout 2");
#endif
	    goto tryagain; /* Not able to power up mmc */
	}
    }
#ifdef USE_HC
    i = MmcCommand(MMC_READ_OCR/*CMD58*/|0x40, 0);
#if DEBUG_LEVEL > 1
    puthex(i);
    puts("=READ_OCR");
#endif
    if (//cmd == (0x40|41) &&
	i == 0) {
	if (SpiSendReceiveMmc(-1, 16) & (1<<(30-16))) {
	    /* OCR[30]:CCS - card capacity select */
	    /* HighCapacity! */
#if DEBUG_LEVEL > 1
	    puts("=HC");
#endif
	    mmc.hcShift = 0;
	}
	//SpiSendReceiveMmc(-1, 16);
    }
#endif /*USE_HC*/

    if (MmcCommand(MMC_SEND_CSD/*CMD9*/|0x40, 0) == 0) {
	register s_int16 *p = (s_int16 *)minifatBuffer;
	register int t = 640;
	while (SpiSendReceiveMmc(0xff00, 8) == 0xff) {
	    if (t-- == 0) {
#if DEBUG_LEVEL > 1
		puts("Timeout 3");
#endif
		goto tryagain;
	    }
	}
	for (i=0; i<8; i++) {
	    *p++ = SpiSendReceiveMmc(-1, 16);
#if DEBUG_LEVEL > 1
	    puthex(p[-1]);
#endif
/*
            00 00 11 11 222 2 3 3334444555566667777
  64M  MMC: 8C 0E 01 2A 03F 9 8 3D3F6D981E10A4040DF
            CSD  NSAC   CCC  flg       MUL+E
	      TAAC  SPEE   RBL  C_SIZE(>>2=FDB)
	                        33 333333333344 444 444 444 444 445
	                        00 111101001111 110 110 110 110 011 000000111
				   C_SIZE                       MULT
  256M MMC+ 90 5E 00 2A 1F5 9 8 3D3EDB683FF9640001F=CSD (Kingston)
  128M SD:  00 26 00 32 175 9 8 1E9F6DACF809640008B=CSD (DANE-ELEC)

  4G SD-HC: 40 7F 0F 32 5B5 9 8 0001E44 7F801640004B
            00 00 11 11 222 2 3 3334444 555566667777


 */
	}
	if ((minifatBuffer[0] & 0xf000) == 0x4000) {
	    /* v2.0 in 512kB resolution */
	  //puts("v2");
	    mmc.blocks = 
		(((((u_int32)minifatBuffer[3]<<16) | minifatBuffer[4])
		 & 0x3fffff) + 1) << 10;
	} else {
	    /* v1.0 */
	    register s_int32 c_size = (((minifatBuffer[3] & 0x03ff)<<2) |
				       ((minifatBuffer[4]>>14) & 3)) + 1;
	    register s_int16 c_mult = (((minifatBuffer[4] & 3)<<1) |
				       ((minifatBuffer[5]>>15) & 1));
	    //puts("v1");
	    c_size <<= 2 + c_mult + (minifatBuffer[2] & 15);
	    mmc.blocks = c_size >> 9;
	}
#if DEBUG_LEVEL > 1
	puts("=CSD");
	puthex(mmc.blocks>>16);
	puthex(mmc.blocks);
	puts("=mmcBlocks");
#endif
    }
#if DEBUG_LEVEL > 4
    if (MmcCommand(MMC_SEND_CID/*CMD10*/|0x40, 0) == 0) {
	register s_int16 *p = minifatBuffer;
	register int t = 3200;
	while (SpiSendReceiveMmc(0xff00, 8) == 0xff) {
	    if (t-- == 0)
		goto tryagain;
	}
	for (i=0; i<8; i++) {
	    *p++ = SpiSendReceiveMmc(-1, 16);
#if DEBUG_LEVEL > 1
	    puthex(p[-1]);
#endif
/*
       Man     Productname   serial#   date
          App             rev        res    crc7+stop
  4G:  1D 4144 0000000000 00 0000157A 0 06A E3
  64M: 02 0000 53444D3036 34 40185439 0 C77 75
       00 0011 1122223333 44 44555566 6 677 77
*/
	}
#if DEBUG_LEVEL > 1
	puts("=CID");
#endif
    }
#endif

#if 1
    /* Set Block Size of 512 bytes -- default for at least HC */
    /* Needed by MaxNova S043618ATA 2J310700 MV016Q-MMC */
    /* Must check return value! (MicroSD restart!) */
    {
	register int c;
	if ((c = MmcCommand(MMC_SET_BLOCKLEN|0x40, 512)) != 0) {
#if DEBUG_LEVEL > 1
	    puthex(c);
	    puts(" SetBlockLen failed");
#endif
	    goto tryagain;
	}
    }
#endif
    /* All OK return */
    //mmc.errors = 0;
    mmc.state = mmcOk;
    map->blocks = mmc.blocks;
    return 0;//mmc.errors;
}


auto u_int16 MyReadDiskSector(register __i0 u_int16 *buffer,
			      register __reg_a u_int32 sector) {
    register s_int16 i;
    register u_int16 t = 65535;

    if (mmc.state == mmcNA || mmc.errors) {
	cs.cancel = 1;
	return 5;
    }
#if 0 && defined(USE_DEBUG)
    puthex(sector>>16);
    puthex(sector);
    puts("=ReadDiskSector");
#endif
#ifdef USE_HC
    MmcCommand(MMC_READ_SINGLE_BLOCK|0x40,(sector<<mmc.hcShift));
#else
    MmcCommand(MMC_READ_SINGLE_BLOCK|0x40,(sector<<9));
#endif
    do {
	i = SpiSendReceiveMmc(0xff00,8);
    } while (i == 0xff && --t != 0);

    if (i != 0xfe) {
	memset(buffer, 0, 256);
	if (i > 15 /*unknown error code*/) {
	    mmc.errors++;
#if 0
	    putch('R');
#endif
	} else {
	    /* data retrieval failed:
	       D4-D7 = 0
	       D3 = out of range
	       D2 = Card ECC Failed
	       D1 = CC Error
	       D0 = Error
	     */
	}
	SpiSendClocks();
	return 1;
    }
    for (i=0; i<512/2; i++) {
	*buffer++ = SpiSendReceiveMmc(0xffff,16);
    }
    SpiSendReceiveMmc(0xffff,16); /* discard crc */

    /* generate some extra SPI clock edges to finish up the command */
    SpiSendClocks();
    SpiSendClocks();

    /* We force a call of user interface after each block even if we
       have no idle CPU. This prevents problems with key response in
       fast play mode. */
    IdleHook();
    return 0; /* All OK return */
}

struct FsMapper *FsMapMmcCreate(struct FsPhysical *physical,
				u_int16 cacheSize);
u_int16 FsMapMmcRead(struct FsMapper *map, u_int32 firstBlock,
		     u_int16 blocks, u_int16 *data);
u_int16 FsMapMmcWrite(struct FsMapper *map, u_int32 firstBlock,
		      u_int16 blocks, u_int16 *data);

const struct FsMapper mmcMapper = {
    0x010c, /*version*/
    256,    /*blocksize in words*/
    0,      /*blocks -- read from CSD*/
    0,      /*cacheBlocks*/
    FsMapMmcCreate,
    FsMapFlNullOk,//RamMapperDelete,
    FsMapMmcRead,
    FsMapMmcWrite,
    NULL,//FsMapFlNullOk,//RamMapperFree,
    FsMapFlNullOk,//RamMapperFlush,
    NULL /* no physical */
};
struct FsMapper *FsMapMmcCreate(struct FsPhysical *physical,
				u_int16 cacheSize) {
    PERIP(GPIO0_MODE) &= ~(MMC_MISO|MMC_CLK|MMC_MOSI|MMC_XCS);
    PERIP(GPIO0_DDR) = (PERIP(GPIO0_DDR) & ~(MMC_MISO))
	| (MMC_CLK|MMC_MOSI|MMC_XCS);
    PERIP(GPIO0_ODATA) = (PERIP(GPIO0_ODATA) & ~(MMC_CLK|MMC_MOSI))
	| (MMC_XCS | GPIO0_CS1); /* NFCE high */
#if DEBUG_LEVEL > 1
    puts("Configured MMC pins\n");
#endif
    memset(&mmc, 0, sizeof(mmc));
    return &mmcMapper;
}
u_int16 FsMapMmcRead(struct FsMapper *map, u_int32 firstBlock,
		     u_int16 blocks, u_int16 *data) {
    register u_int16 bl = 0;
#ifndef PATCH_LBAB /*if not patched already*/
    firstBlock &= 0x00ffffff; /*remove sign extension: 4G -> 8BG limit*/
#endif
    while (bl < blocks) {
	if (MyReadDiskSector(data, firstBlock))
	    break; /* probably MMC detached */
	data += 256;
	firstBlock++;
	bl++;
    }
    return bl;
}
u_int16 FsMapMmcWrite(struct FsMapper *map, u_int32 firstBlock,
		      u_int16 blocks, u_int16 *data) {
    u_int16 bl = 0;
#ifndef PATCH_LBAB /*if not patched already*/
    firstBlock &= 0x00ffffff; /*remove sign extension: 4G -> 8BG limit*/
#endif
    while (bl < blocks) {
	register u_int16 c;
	if (mmc.state == mmcNA || mmc.errors)
	    break;
#ifdef USE_HC
	c = MmcCommand(MMC_WRITE_BLOCK|0x40, firstBlock<<mmc.hcShift);
#else
	c = MmcCommand(MMC_WRITE_BLOCK|0x40, firstBlock<<9);
#endif
	//wait for BUSY token, if you get 0x01(idle), it's an ERROR!
	while (c != 0x00) {
	    if (c == 0x01) {
		mmc.errors++;
		goto out;
	    }
	    c = SpiSendReceiveMmc(0xff00, 8);
	    /*TODO: timeout?*/
	}
	SpiSendReceiveMmc(0xfffe, 16);
	for (c=0; c<256; c++) {
	    SpiSendReceiveMmc(*data++, 16);
	}
	SpiSendReceiveMmc(0xffff, 16); /* send dummy CRC */

	/* wait until MMC write finished */
	while ( SpiSendReceiveMmc(0xffffU, 16) != 0xffffU) {
	    /*TODO: timeout?*/
	}
	firstBlock++;
	bl++;
    }
 out:
    SpiSendClocks();
    return bl;
}

#ifdef USE_USB



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











/*
  We replace the MassStorage() functionality.
  Uses ScsiTestUnitReady() to check the MMC/SD removal.
  Only appears as a mass storage device when MMC/SD is present.
 */
auto void MyMassStorage(void) {
  register __b0 int usbMode = 0;

#ifdef USE_DEBUG
  puts("MyMassStorage");
#endif
  if (mmc.state == mmcNA || mmc.errors) {
#ifdef USE_DEBUG
      puts("No MMC -> no USB MassStorage");
#endif
      return;
  }

  PowerSetVoltages(&voltages[voltCoreUSB]);
  /** a 2.5ms-10ms delay to let the voltages change (rise) */
  BusyWait10();

  LoadCheck(NULL, 1); /* 4.0x clock required! */
  SetRate(44100U); /* Our USB-audio requires 44100Hz */
  InitUSB(USB_MASS_STORAGE);  

#ifdef USE_DEBUG
  puts("USB loop");
#endif
  while (mmc.state != mmcNA && mmc.errors == 0) {
    USBHandler();
    PERIP(USB_CONTROL) |= USB_STF_SOF; 
    PERIP(USB_CONTROL) |= USB_STF_RX;

    ScsiTaskHandler();

    /* If no SOF in such a long time.. */
    if (USBWantsSuspend()) {
      /* Do we seem to be detached?
	 Note: Now also requires USBWantsSuspend() to return TRUE! */
      if (USBIsDetached()) {
#ifdef USE_DEBUG
	puts("Detached");
#endif
	break;
      }
#ifdef USE_DEBUG
      puts("Suspend");
#endif
      {
	register u_int16 io = PERIP(GPIO1_ODATA);
	PERIP(GPIO1_ODATA) = io & ~(LED1|LED2);      /* LEDs off */
	PERIP(SCI_STATUS) |= SCISTF_ANA_PDOWN;       /*fix suspend*/
	USBSuspend(0); /* no timeout */
	PERIP(GPIO1_ODATA) = io;      /* LEDs back to old states */
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

#if 0
    if (mmc.state != mmcNA) {
      puthex(mmc.state);puts("=mmc.state");
    }
    
    if (mmc.errors) {
      puthex(mmc.errors);puts("=mmc.errors");
    }
#endif


  }
#ifdef USE_DEBUG
  puts("Leaving USB");
#endif
  hwSampleRate = 1; /* Forces FREQCTL update at next SetRate() */
  PERIP(SCI_STATUS) &= ~SCISTF_USB_PULLUP_ENA; /* Take us 'out of the bus' */
  PERIP(USB_CONFIG) = 0x8000U; /* Reset USB */
  map->Flush(map, 1);

  /* Set player voltages. No need to wait for the voltages to drop. */
  PowerSetVoltages(&voltages[voltCorePlayer]);
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
  RealMSCPacketFromPC(&USB.pkt);
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
    SCSI.DataOutBuffer = SCSI.DataBuffer;
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
  static u_int16 audioStash[91];
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



#endif //USE_USB

#if defined(PATCH_TEST_UNIT_READY) && defined(PATCH_LBAB)
void ScsiTestUnitReady(void) {
    /* Poll MMC present by giving it a command. */
    if (mmc.state == mmcOk && mmc.errors == 0 &&
	MmcCommand(MMC_SET_BLOCKLEN|0x40, 512) != 0) {
	mmc.errors++;
    }
    if (mmc.state == mmcNA || mmc.errors) {
	SCSI.Status = SCSI_REQUEST_ERROR; /* report error at least once! */
    }
}
#endif

#ifdef USE_WAV
#include <codecminiwav.h>
#include <dev1000.h>
extern u_int16 codecVorbis[];
extern u_int16 ogg[];
extern u_int16 mInt[];

extern struct CodecServices cs;
extern struct Codec *cod;
enum CodecError PlayWavOrOggFile(void) {
  register enum CodecError ret = ceFormatNotFound;
  const char *eStr;
  LoadCheck(NULL, 0); /* higher clock, but 4.0x not absolutely required */
  if ((cod = CodMiniWavCreate())) {
      ret = cod->Decode(cod, &cs, &eStr);
      cod->Delete(cod);
      {
	  register int i;
	  for (i=0;i<DEFAULT_AUDIO_BUFFER_SAMPLES/(sizeof(tmpBuf)/2);i++) {
	      /* Must be memset each time, because effects like EarSpeaker are
		 performed in-place in the buffer */
	      memset(tmpBuf, 0, sizeof(tmpBuf));
	      AudioOutputSamples(tmpBuf, sizeof(tmpBuf)/2);
	  }
      }
      if (ret == ceFormatNotSupported)
	  return ceFormatNotFound; /* makes prev work with unsupported files */
      if (ret != ceFormatNotFound)
	  return ret;
      cs.Seek(&cs, 0, SEEK_SET);
  }
  return RealPlayCurrentFile();
}
#endif/*USE_WAV*/


#ifdef USE_MATRIX_KEYBOARD

enum keyEvent2 {
  // end of default events
  ke_select_song_0 = 100,
  ke_select_song_1 = 101,
  ke_select_song_2 = 102,
  ke_select_song_3 = 103,
  ke_select_song_4 = 104,
  ke_select_song_5 = 105,
  ke_select_song_6 = 106,
  ke_select_song_7 = 107,
  ke_select_song_8 = 108,
  ke_select_song_9 = 109,
};


const struct KeyMapping matrixKeyMap[] = {
  {0xb, ke_pauseToggle},
  {KEY_POWER, ke_pauseToggle},
  {KEY_LONG_PRESS|KEY_POWER, ke_powerOff},

  {0x1, (u_int16)ke_select_song_1},
  {0x2, (u_int16)ke_select_song_2},
  {0x3, (u_int16)ke_select_song_3},
  {0x4, (u_int16)ke_select_song_4},
  {0x5, (u_int16)ke_select_song_5},
  {0x6, (u_int16)ke_select_song_6},
  {0x7, (u_int16)ke_select_song_7},
  {0x8, (u_int16)ke_select_song_8},
  {0x9, (u_int16)ke_select_song_9},

  {0xa, ke_previous},
  {0xc, ke_next},
  {KEY_LONG_PRESS|0xa, ke_volumeDown},
  {KEY_LONG_PRESS|0xc, ke_volumeUp},

//  {KEY_LONG_PRESS|0xb, ke_earSpeaker| KEY_LONG_ONESHOT},
  {KEY_LONG_PRESS|0xb, ke_earSpeakerToggle| KEY_LONG_ONESHOT},

//  {KEY_LONG_PRESS|0x4, ke_ff_faster},
//  {KEY_LONG_PRESS|KEY_RELEASED, ke_ff_off},
//  {KEY_LONG_PRESS|0x5, (s_int16)(ke_randomToggleNewSong|KEY_LONG_ONESHOT)},
  {0, ke_null}
};

/* Key matrix defines what keycodes different keys generate. */
const u_int16 keyMatrix[4][4] = { /* Note: 0 can not be used as key code! */
    {0x01, 0x04, 0x07, 0x0a}, /* column 1: 1 4 7 * */
    {0x02, 0x05, 0x08, 0x0b}, /* column 2: 2 5 8 0 */
    {0x03, 0x06, 0x09, 0x0c}, /* column 3: 3 6 9 # */
    {0x10, 0x11, 0x12, 0x13}  /* column 4: A B C D */
};
#include <codec.h>
extern struct CodecServices cs;
/* Called from ASM function KeyScanMatrix() */
auto u_int16 MatrixScan(register __i2 const u_int16 *matrix) {
  __y u_int16 mode = PERIP(GPIO0_MODE), ddr = PERIP(GPIO0_DDR), dat = PERIP(GPIO0_ODATA);
  register u_int16 k;
  register u_int16 col = 0x10;

#if 0
  /* worst case.. we have already driven it high. */
  NandPutCommand(0xff);
#endif
  PERIP(GPIO0_MODE) = mode & 0xff00U; /* GPIO0..8 into GPIO mode */
  PERIP(GPIO0_DDR)  = (ddr & 0xff00U) | 0x00f0U; /* GPIO4..7 into outputs */
/*TODO: if presses are infrequent enough, scan all columns at once,
  and only if keys are pressed, scan by column. */
  do {
      PERIP(GPIO0_ODATA) = (dat & 0xff00U) | col;
      /* Wait max 4*85us = 340us, not calculating interrupts */
      {
	  register u_int16 i;
	  k = clockX << 9; /* 85us (43us seems just enough for some flash) */
	  for (i=0;i<k;i++)
	      PERIP(GPIO0_IDATA);
      }
      k = PERIP(GPIO0_IDATA) & 15;
      col <<= 1;
      matrix += 4;
  } while (k == 0 && col < 0x100);
 out:
#if 0
  puthex(k);
  puts("");
#endif
  PERIP(GPIO0_DDR) = ddr;
  PERIP(GPIO0_ODATA) = dat;
  PERIP(GPIO0_MODE) = mode;
  if (k & 1) return matrix[-4];
  if (k & 2) return matrix[-3];
  if (k & 4) return matrix[-2];
  if (k & 8) return matrix[-1];
  return 0;
}

void MatrixUserInterfaceIdleHook(void) { /*94 words*/
    if (uiTrigger) {
	uiTrigger = 0;
	KeyScanMatrix(&keyMatrix[0][0]);

	/*Update LEDs in the same way as the ROM firmware. */
	{
	  register u_int16 leds = LED1;
	  if (player.pauseOn > 0 && (uiTime & 0x06) != 0)
	    leds = 0; /* power led flashes when in pause mode */
	  if (player.randomOn)
	    leds |= LED2;
	  PERIP(GPIO1_ODATA) = (PERIP(GPIO1_ODATA) & ~(LED1 | LED2)) | leds;
	}
    }
}


void MatrixKeyEventHandler(enum keyEvent event) {
    /* directly select a song to start playing from */
    if (event >= ke_select_song_1 && event <= ke_select_song_9) {
	//player.nextFile = keyCheck-1;
	player.nextFile = event - ke_select_song_1;
#if 0
puthex(player.nextFile);
puts("=next");
#endif
	player.pauseOn = 0;
	cs.cancel = 1;
	return;
    }
    /* execute the default actions (earspeaker and volume) */
    RealKeyEventHandler(event);
}

#endif/*USE_MATRIX_KEYBOARD*/




void main(void) {
#if 1 /*Perform some extra inits because we are started from SPI boot. */
    InitAudio(); /* goto 3.0x..4.0x */
    PERIP(INT_ENABLEL) = INTF_RX | INTF_TIM0;
    PERIP(INT_ENABLEH) = INTF_DAC;
    //ph = FsPhNandCreate(0); /*We do not use FLASH, so no init for it. */
#endif

#ifdef USE_MATRIX_KEYBOARD
    SetHookFunction((u_int16)IdleHook, MatrixUserInterfaceIdleHook);
    currentKeyMap = matrixKeyMap;
    SetHookFunction((u_int16)USBSuspend, SuspendMatrix);
    SetHookFunction((u_int16)KeyEventHandler, MatrixKeyEventHandler);
#endif/*USE_MATRIX_KEYBOARD*/

#ifdef USE_WAV
    /* allow both .WAV and .OGG */
    supportedFiles = defSupportedFiles;
    /* Install the new file player function and return to player. */
    SetHookFunction((u_int16)PlayCurrentFile, PlayWavOrOggFile);
#endif

#if 0 /* Available only on newest (currently unreleased) developer library. */
    SetHookFunction((u_int16)OpenFile, FatFastOpenFile); /*Faster!*/
#endif
    //voltages[voltCorePlayer] = voltages[voltCoreUSB] = 29;//27;
    //voltages[voltIoUSB] = voltages[voltIoPlayer] = 27; /* 3.3V */
    voltages[voltAnaPlayer] = 30; /*3.6V for high-current MMC/SD!*/
    PowerSetVoltages(&voltages[voltCorePlayer]);

#if 0
    /* Set differential audio mode, i.e. invert the other channel. */
    audioPtr.leftVol = -audioPtr.rightVol;
#endif

    /* Set the leds after nand-boot! */
    PERIP(GPIO1_ODATA) |=  LED1|LED2;      /* POWER led on */
    PERIP(GPIO1_DDR)   |=  LED1|LED2; /* SI and SO to outputs */
    PERIP(GPIO1_MODE)  &= ~(LED1|LED2); /* SI and SO to GPIO */


#ifdef DISPLAY
    ScreenInit(); // 27-JUN-2007 still something wrong with init,
                  // doesn't start every time, must consult hw designer
    ScreenInit(); // 27-JUN-2007 still something wrong with init,
                  // doesn't start every time, must consult hw designer
    ScreenClear();
    ScreenPutChar(SYMBOL_DISKDRIVE);
#endif


#ifdef PATCH_LBAB
    /* Increases the allowed disk size from 4GB to 2TB.
       Reference to PatchMSCPacketFromPC will link the function from
       libdev1000.a. We need to implement ScsiTestUnitReady().  */
    SetHookFunction((u_int16)MSCPacketFromPC, PatchMSCPacketFromPC);
#endif/*PATCH_LBAB*/

    /* Replicate main loop */
    player.volume = 17;
    player.volumeOffset = -24;
    player.pauseOn = 0;
    //player.randomOn = 0;
    keyOld = KEY_POWER;
    keyOldTime = -32767; /* ignores the first release of KEY_POWER */

    /*
      MMC mapper replaces FLASH mapper, so we do not need to hook
      ReadDiskSector.

      We must also replace the MassStorage() functionality, because it
      automatically deletes mapper, creates a read-write FLASH mapper,
      and restores read-only FLASH mapper at exit.

      We need to replace the main loop because we want to detect
      MMC/SD insertion and removal.

      We do not hook the MassStorage, because we can call our own version
      from the main loop without using the RAM hook. This saves some
      instruction words.
     */
    map = FsMapMmcCreate(NULL, 0); /* Create MMC Mapper */
    PlayerVolume();
    while (1) {
	/* If MMC is not available, try to reinitialize it. */
	if (mmc.state == mmcNA || mmc.errors) {
#ifdef USE_DEBUG
	    puts("InitializeMmc(50)");
#endif
	    InitializeMmc(50);
	}

#ifdef USE_USB

	
  SetHookFunction((u_int16)USBHandler, MyUSBHandler);
  SetHookFunction((u_int16)DecodeSetupPacket, MyDecodeSetupPacket);  
  SetHookFunction((u_int16)MSCPacketFromPC, MyMSCPacketFromPC);
  SetHookFunction((u_int16)InitUSBDescriptors, MyInitUSBDescriptors);
  // Install USB Interrupt Handler
  WriteIRam (0x20+INTV_USB, ReadIRam((u_int16)InterruptStub0));
  PERIP(INT_ENABLEL) |= INTF_USB;



	/* If USB is attached, call our version of the MassStorage()
	   handling function. */
	if (USBIsAttached()) {
#ifdef USE_DEBUG
	    puts("USB Attached");
#endif
	    MyMassStorage();
	}
#endif

	/* Try to init FAT. */
	if (InitFileSystem() == 0) {
	    /* Restore the default suffixes. */
	    minifatInfo.supportedSuffixes = supportedFiles;
	    player.totalFiles = OpenFile(0xffffU);

	    if (player.totalFiles == 0) {
		/* If no files found, output some samples.
		   This causes the idle hook to be called, which in turn
		   scans the keys and you can turn off the unit if you like.
		*/
#ifdef USE_DEBUG
		puts("no .ogg");
#endif
		goto noFSnorFiles;
	    }

	    /*
	      Currently starts at the first file after a card has
	      been inserted.

	      Possible improvement would be to save play position
	      to SPI EEPROM for a couple of the last-seen MMC/SD's.
	      Could also save the play mode, volume, and earspeaker
	      state to EEPROM.
	    */
	    player.nextStep = 1;
	    player.nextFile = 0;
	    while (1) {
		/* If random play is active, get the next random file,
		   but try to avoid playing the same song twice in a row. */
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
		if (player.currentFile >= player.totalFiles)
		    player.currentFile -= player.totalFiles;
		player.nextFile = player.currentFile + 1;

		/* If the file can be opened, start playing it. */
		if (OpenFile(player.currentFile) < 0) {
		    //player.volumeOffset = -12; /*default volume offset*/
		    //PlayerVolume();
		    player.ffCount = 0;
		    cs.cancel = 0;
		    cs.goTo = -1; /* start playing from the start */
		    cs.fileSize = cs.fileLeft = minifatInfo.fileSize;
		    cs.fastForward = 1; /* reset play speed to normal */

#ifdef DISPLAY
		    ScreenLocate(0,4);
		    if (minifatInfo.longFileName[0]) {
			ScreenPutPacked(minifatInfo.longFileName,
					FAT_LFN_SIZE/2/*52/2*/, 132);
		    } else {
			ScreenPutPacked(minifatInfo.fileName, 8/2, 0);
			ScreenPutPackedX("\p.ogg", 4/2, 132);
		    }
#endif
		    {
			register s_int16 oldStep = player.nextStep;
			register s_int16 ret;
			ret = PlayCurrentFile();

			/* If unsupported, keep skipping */
			if (ret == ceFormatNotFound)
			    player.nextFile = player.currentFile + player.nextStep;
			/* If a supported file found, restore play direction */
			if (ret == ceOk && oldStep == -1)
			    player.nextStep = 1;
		    }
		} else {
		    player.nextFile = 0;
		}
		/* Leaves play loop when MMC changed */
		if (mmc.state == mmcNA || mmc.errors) {
		    break;
		}
#ifdef USE_USB
		if (USBIsAttached()) { /* .. or USB is attached. */
		    break;
		}
#endif
	    }
	} else {
	    /* If not a valid FAT (perhaps because MMC/SD is not inserted),
	       just send some samples to audio buffer and try again. */
	noFSnorFiles:
	    LoadCheck(&cs, 32); /* decrease or increase clock */
	    memset(tmpBuf, 0, sizeof(tmpBuf));
	    AudioOutputSamples(tmpBuf, sizeof(tmpBuf)/2);
	    /* When no samples fit, calls the user interface
	       -- handles volume control and power-off. */
	}
    }
}
