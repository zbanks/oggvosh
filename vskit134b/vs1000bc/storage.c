
// storage.c :  Plug-in for playing from intel "S33" serial flash eeprom. 
// For this example, a QH25F640S33B8 chip is connected to SI, SO, SCLK, XCS.

#include <stdlib.h>
#include <vs1000.h>


#define SPI_EEPROM_COMMAND_READ_STATUS_REGISTER  0x05
#define SPI_EEPROM_COMMAND_READ  0x03


//macro to set SPI to MASTER; 8BIT; FSYNC Idle => xCS high
#define SPI_MASTER_8BIT_CSHI   PERIP(SPI0_CONFIG) = \
        SPI_CF_MASTER | SPI_CF_DLEN8 | SPI_CF_FSIDLE1

//macro to set SPI to MASTER; 8BIT; FSYNC not Idle => xCS low
#define SPI_MASTER_8BIT_CSLO   PERIP(SPI0_CONFIG) = \
        SPI_CF_MASTER | SPI_CF_DLEN8 | SPI_CF_FSIDLE0

//macro to set SPI to MASTER; 16BIT; FSYNC not Idle => xCS low
#define SPI_MASTER_16BIT_CSLO  PERIP(SPI0_CONFIG) = \
        SPI_CF_MASTER | SPI_CF_DLEN16 | SPI_CF_FSIDLE0



void InitSpi() {
  SPI_MASTER_8BIT_CSHI;
  PERIP(SPI0_FSYNC) = 0;        // Frame Sync is used as an active low xCS
  PERIP(SPI0_CLKCONFIG) = SPI_CC_CLKDIV * (1-1);   // Spi clock divider = 1
  PERIP(GPIO1_MODE) |= 0x1f;    // Set SPI pins to be peripheral controlled
}  


void EESingleCycleCommand(u_int16 cmd){
  SPI_MASTER_8BIT_CSHI; 
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(cmd);
  SPI_MASTER_8BIT_CSHI;
}


/// Wait for not_busy (status[0] = 0) and return status
u_int16 EEWaitGetStatus(void) {
  u_int16 status;
  SPI_MASTER_8BIT_CSHI;
  SPI_MASTER_8BIT_CSLO;
  SpiSendReceive(SPI_EEPROM_COMMAND_READ_STATUS_REGISTER);
  while ((status = SpiSendReceive(0)) & 0x01)
    ; //Wait until ready
  SPI_MASTER_8BIT_CSHI;
  return status;
}


/// Read a block from EEPROM
/// \param blockn number of 512-byte sector 0..32767
/// \param dptr pointer to data block
u_int16 EEReadBlock(u_int16 blockn, u_int16 *dptr) {
  EEWaitGetStatus();                    // Wait until EEPROM is not busy
  SPI_MASTER_8BIT_CSLO;                  // Bring xCS low
  SpiSendReceive(SPI_EEPROM_COMMAND_READ);
  SpiSendReceive(blockn>>7);            // Address[23:16] = blockn[14:7]
  SpiSendReceive((blockn<<1)&0xff);     // Address[15:8]  = blockn[6:0]0
  SpiSendReceive(0);                    // Address[7:0]   = 00000000
  SPI_MASTER_16BIT_CSLO;                 // Switch to 16-bit mode
  { int n;
    for (n=0; n<256; n++){
      *dptr++ = SpiSendReceive(0);      // Receive Data
    }
  }
  SPI_MASTER_8BIT_CSHI;                  // Bring xCS back to high
  return 0;
}


// Disk image is prommed to EEPROM at sector 0x80 onwards, leaving
// the first 64 kilobytes (1 erasable block) free for boot code
#define FAT_START_SECTOR 0x80


// This function will replace ReadDiskSector() functionality
auto u_int16 MyReadDiskSector(register __i0 u_int16 *buffer,
			      register __a u_int32 sector) {

  PERIP(GPIO1_MODE) |= 0x1f;    // Set SPI pins to be peripheral controlled
  EEReadBlock(sector+FAT_START_SECTOR, buffer);

  return 0;
}


// Initialize SPI and hook in own disk read function.
// This example plays ogg files from a FAT image that has been
// previously written to a serial EEPROM.

void main(void) {
  InitSpi();

  // Hook in own disk sector read function
  SetHookFunction((u_int16)ReadDiskSector, MyReadDiskSector);

} // Return to ROM code. Player will now play from EEPROM

