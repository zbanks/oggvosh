#include <stdio.h>
#include <stdlib.h>
#include <vs1000.h>
#include <minifat.h>

__y const char hex[] = "0123456789abcdef";
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

void put2c(u_int16 a) {
  char tmp[8];
  tmp[0] = a>>8;
  tmp[1] = a&0xff;
  if (tmp[0]<32) tmp[0]='.';
  if (tmp[1]<32) tmp[1]='.';
  if (tmp[0]>127) tmp[0]='.';
  if (tmp[1]>127) tmp[1]='.';
  tmp[2] = '\0';
  fputs(tmp, stdout);
}



void DumpBlock (u_int16 *dptr) {
  int n;
  for (n=0; n<256; n++){
    puthex(*dptr++);
    //put2c(*dptr++);
  }
  puts("=block");
}


#define SPI_EEPROM_COMMAND_WRITE_ENABLE  0x06
#define SPI_EEPROM_COMMAND_WRITE_DISABLE  0x04
#define SPI_EEPROM_COMMAND_READ_STATUS_REGISTER  0x05
#define SPI_EEPROM_COMMAND_WRITE_STATUS_REGISTER  0x01
#define SPI_EEPROM_COMMAND_READ  0x03
#define SPI_EEPROM_COMMAND_WRITE 0x02
#define SPI_EEPROM_COMMAND_CLEAR_ERROR_FLAGS 0x30
#define SPI_EEPROM_COMMAND_ERASE_SECTOR 0xD8

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
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(cmd);
  SPI_MASTER_8BIT_CSHI;
}


/// Wait for not_busy (status[0] = 0) and return status
u_int16 SpiWaitStatus(void) {
  u_int16 status;
  SPI_MASTER_8BIT_CSHI;
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_READ_STATUS_REGISTER);
  while ((status = SpiSendReceive(0)) & 0x01)
    ; //Wait until ready
  SPI_MASTER_8BIT_CSHI;
  return status;
}

/// Write a block to EEPROM. Caution: Does not erase block
/// \param blockn number of sector to write: 0..32767 (16Mbytes)
/// \param dptr pointer to data block
u_int16 SpiWriteBlock(u_int16 blockn, u_int16 *dptr) {
  
  SingleCycleCommand(SPI_EEPROM_COMMAND_WRITE_ENABLE);
  SingleCycleCommand(SPI_EEPROM_COMMAND_CLEAR_ERROR_FLAGS);
  SingleCycleCommand(SPI_EEPROM_COMMAND_WRITE_ENABLE);

  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_WRITE_STATUS_REGISTER);
  SpiSendReceive(0x02); //Sector Protections Off
  SPI_MASTER_8BIT_CSHI;

  SingleCycleCommand(SPI_EEPROM_COMMAND_WRITE_ENABLE);
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_WRITE);
  SpiSendReceive(blockn>>7);            // Address[23:16] = blockn[14:7]
  SpiSendReceive((blockn<<1)&0xff);     // Address[15:8]  = blockn[6:0]0
  SpiSendReceive(0);                    // Address[7:0]   = 00000000
 
  SPI_MASTER_16BIT_CSLO;
  {
    u_int16 n;
    u_int16 d;
    for (n=0; n<128; n++){
      SpiSendReceive(*dptr++);
    }
  }
  SPI_MASTER_8BIT_CSHI;
  SpiWaitStatus();


  SingleCycleCommand(SPI_EEPROM_COMMAND_WRITE_ENABLE);
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_WRITE);
  SpiSendReceive(blockn>>7);       
  SpiSendReceive(((blockn<<1)+1)&0xff); // Address[15:8]  = blockn[6:0]1
  SpiSendReceive(0);              
 
  SPI_MASTER_16BIT_CSLO;
  {
    int n;
    for (n=128; n<256; n++){
      SpiSendReceive(*dptr++);
    }
  }
  SPI_MASTER_8BIT_CSHI;
  SpiWaitStatus();


  return 0;
}


void EraseFirstBlock(){
  SingleCycleCommand(SPI_EEPROM_COMMAND_WRITE_ENABLE);  
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_WRITE_STATUS_REGISTER);
  SpiSendReceive(0x02); //Sector Protections Off
  SPI_MASTER_8BIT_CSHI;
  SingleCycleCommand(SPI_EEPROM_COMMAND_WRITE_ENABLE);  
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_ERASE_SECTOR);
  SpiSendReceive(0);       
  SpiSendReceive(0);
  SpiSendReceive(0);              
  SPI_MASTER_8BIT_CSHI;
  SpiWaitStatus();
}

 
u_int16 SpiReadBlock(u_int16 blockn, u_int16 *dptr) {
  SpiWaitStatus();
  
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_READ);
  SpiSendReceive(blockn>>7);            // Address[23:16] = blockn[14:7]
  SpiSendReceive((blockn<<1)&0xff);     // Address[15:8]  = blockn[6:0]0
  SpiSendReceive(0);                    // Address[7:0]   = 00000000
  SPI_MASTER_16BIT_CSLO;
  {
    int n;
    for (n=0; n<256; n++){
      *dptr++ = SpiSendReceive(0);
    }
  }
  SPI_MASTER_8BIT_CSHI;

  return 0;
}




void main(void) {

  FILE *fp;

  SPI_MASTER_8BIT_CSHI;
  PERIP(SPI0_FSYNC) = 0;
  PERIP(SPI0_CLKCONFIG) = SPI_CC_CLKDIV * (1-1);
  PERIP(GPIO1_MODE) |= 0x1f; /* enable SPI pins */
  

#if 1
  SpiReadBlock(0,minifatBuffer);
  DumpBlock(minifatBuffer);
  while(1);
#endif


  puts("Trying to open eeprom.img");
  PERIP(INT_ENABLEL) &= ~INTF_RX; //Disable UART RX interrupt

  if (fp = fopen ("eeprom.img", "rb")){
    u_int16 len;
    u_int16 sectorNumber;
        
    puts("Erasing boot area.");
    EraseFirstBlock(); //Needed for some EEPROMS
    
    puts("Writing...");
    sectorNumber = 0;
    while ((len=fread(minifatBuffer,1,256,fp))){

      fputs("Sector ",stdout); puthex(sectorNumber); puts("...");
      SpiWriteBlock(sectorNumber, minifatBuffer);
      sectorNumber++;
    }
    fclose(fp);       
    puts("Done.");
  }else{
    puts("File not found\n");
  }

  PERIP(INT_ENABLEL) |= INTF_RX; //Re-enable UART RX interrupt
  while(1)
    ; //Stop here

}
