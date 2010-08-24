
// VS1000A EEPROM Writer Program
// Reads eeprom.img file from PC via vs3emu cable and programs it to EEPROM.

#define MY_IDENT "25LC640 EEPROM promming routine for VS1000A"

#include <stdio.h>
#include <stdlib.h>
#include <vs1000.h>
#include <minifat.h>

__y const char hex[] = "0123456789abcdef";
void puthex(u_int16 a) {
  char tmp[8];
  tmp[0] = hex[(a>>12)&15];  tmp[1] = hex[(a>>8)&15];
  tmp[2] = hex[(a>>4)&15];   tmp[3] = hex[(a>>0)&15];
  tmp[4] = ' ';              tmp[5] = '\0';
  fputs(tmp, stdout);
}

#define SPI_EEPROM_COMMAND_WRITE_ENABLE  0x06
#define SPI_EEPROM_COMMAND_WRITE_DISABLE  0x04
#define SPI_EEPROM_COMMAND_READ_STATUS_REGISTER  0x05
#define SPI_EEPROM_COMMAND_WRITE_STATUS_REGISTER  0x01
#define SPI_EEPROM_COMMAND_READ  0x03
#define SPI_EEPROM_COMMAND_WRITE 0x02

//macro to set SPI to MASTER; 8BIT; FSYNC Idle => xCS high
#define SPI_MASTER_8BIT_CSHI   PERIP(SPI0_CONFIG) = \
        SPI_CF_MASTER | SPI_CF_DLEN8 | SPI_CF_FSIDLE1

//macro to set SPI to MASTER; 8BIT; FSYNC not Idle => xCS low
#define SPI_MASTER_8BIT_CSLO   PERIP(SPI0_CONFIG) = \
        SPI_CF_MASTER | SPI_CF_DLEN8 | SPI_CF_FSIDLE0

//macro to set SPI to MASTER; 16BIT; FSYNC not Idle => xCS low
#define SPI_MASTER_16BIT_CSLO  PERIP(SPI0_CONFIG) = \
        SPI_CF_MASTER | SPI_CF_DLEN16 | SPI_CF_FSIDLE0

void SingleCycleCommand(u_int16 cmd){
  SPI_MASTER_8BIT_CSHI; 
  SpiDelay(0);
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(cmd);
  SPI_MASTER_8BIT_CSHI;
  SpiDelay(0);
}

/// Wait for not_busy (status[0] = 0) and return status
u_int16 SpiWaitStatus(void) {
  u_int16 status;
  SPI_MASTER_8BIT_CSHI;
  SpiDelay(0);
  SPI_MASTER_8BIT_CSLO;

  SpiSendReceive(SPI_EEPROM_COMMAND_READ_STATUS_REGISTER); 
  while ((status = SpiSendReceive(0xff)) & 0x01){
    SpiDelay(0);
  }
  SPI_MASTER_8BIT_CSHI; 
  return status;
}

void SpiWriteBlock(u_int16 blockn, u_int16 *dptr) {
  u_int16 i;
  u_int16 addr = blockn*512;

  for (i=0; i<32; i++){
    SingleCycleCommand(SPI_EEPROM_COMMAND_WRITE_ENABLE);
    SPI_MASTER_8BIT_CSLO;
    SpiSendReceive(SPI_EEPROM_COMMAND_WRITE);
    SPI_MASTER_16BIT_CSLO;
    SpiSendReceive(addr);
    {
      u_int16 j;
      for (j=0; j<16; j++){ //Write 16 words (32 bytes)
	SpiSendReceive(*dptr++); 
      }
    }
    SPI_MASTER_8BIT_CSHI;
    SpiWaitStatus();
    addr+=32;
  }
}


u_int16 SpiReadBlock(u_int16 blockn, u_int16 *dptr) {
  SpiWaitStatus();  
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_READ);
  SpiSendReceive((blockn<<1)&0xff);     // Address[15:8]  = blockn[6:0]0
  SpiSendReceive(0);                    // Address[7:0]   = 00000000
  SPI_MASTER_16BIT_CSLO;
  {
    u_int16 i;
    for (i=0; i<256; i++){
      *dptr++ = SpiSendReceive(0);
    }
  }
  SPI_MASTER_8BIT_CSHI;
  return 0;
}


// This routine programs the EEPROM.
// The minifat module has a memory buffer of 512 bytes (minifatBuffer)
// that is used here as temporary memory.
// The routine does not verify the data that is written, but after
// programming, the eeprom start is checked for a VLSI boot id.

void main(void) {

  FILE *fp;

  SPI_MASTER_8BIT_CSHI;
  PERIP(SPI0_FSYNC) = 0;
  PERIP(SPI0_CLKCONFIG) = SPI_CC_CLKDIV * (12-1);
  PERIP(GPIO1_MODE) |= 0x1f; /* enable SPI pins */
  PERIP(INT_ENABLEL) &= ~INTF_RX; //Disable UART RX interrupt
  
  puts("");
  puts(MY_IDENT);
  puts("Trying to open eeprom.img");

  if (fp = fopen ("eeprom.img", "rb")){ // Open a file in the PC
    u_int16 len;
    u_int16 sectorNumber=0;

    puts("Programming...");
    while ((len=fread(minifatBuffer,1,256,fp))){      
      fputs("Sector ",stdout); puthex(sectorNumber); puts("...");
      SpiWriteBlock(sectorNumber, minifatBuffer);
      sectorNumber++;
    }
    fclose(fp);  // Programming complete.

    minifatBuffer[0]=0;    
    fputs("Reading first 2 words of EEPROM: ",stdout); 
    SpiReadBlock(0,minifatBuffer);

    puthex(minifatBuffer[0]); 
    puthex(minifatBuffer[1]);
    
    fputs(" (\"",stdout);
    putchar(minifatBuffer[0]>>8); putchar(minifatBuffer[0]&0xff);
    putchar(minifatBuffer[1]>>8); putchar(minifatBuffer[1]&0xff);

    if ((minifatBuffer[0]==0x564c) && (minifatBuffer[1]==0x5349)){
      puts("\"), which is a valid VLSI boot id.");
    } else {
      puts("\"), which is NOT a valid VLSI boot id!");
    }
    puts("Done.");

  }else{
    puts("File not found\n");
  }

  PERIP(INT_ENABLEL) |= INTF_RX; //Re-enable UART RX interrupt
  while(1)
    ; //Stop here
}
