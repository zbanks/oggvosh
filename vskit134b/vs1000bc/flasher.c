// Program for flashing first sector of a compatible Nand Flash chip
// from the first .IMG on the VS1000A USB RAMDISK. 
// Since this program is run from file VS1000_A.RUN in the ramdisk, *map already
// points to an existing ramdisk, so OpenFile() etc work from the ramdisk.

#include <vs1000.h>
#include <minifat.h>
#include <vsNand.h>
extern __y u_int16 mallocAreaY[]; /* for ramdisk */
extern u_int16 mallocAreaX[];     /* for ramboot */
extern struct FsNandPhys fsNandPhys;
extern struct FsPhysical *ph;

void main(void) {
  register int j = 0;
  ph = &fsNandPhys.p; // Physical disk is nand flash handler in ROM
  mallocAreaY[29] = 0x3220; // Force disk image to be FAT12
    
  if (InitFileSystem() == 0) { // Reinitialize file system in FAT12 mode

    static const u_int32 bootFiles[] = { FAT_MKID('I','M','G'), 0 };
    minifatInfo.supportedSuffixes = bootFiles; // Only read .IMG files
    
    if (OpenFile(0) < 0) { // Open first .IMG file on ramdisk
      j = ReadFile(mallocAreaX, 0, 2*0x1000) / 2;
      if (j==0) goto fail; // Could not read from the file
    } else goto fail; // OpenFile() did not find any .IMG file from the ramdisk
    
    // File is now read to mallocAreaX and j contains its length.    
    ((struct FsNandPhys *)ph)->nandType = mallocAreaX[2]; //nandType from imgfile
    ((struct FsNandPhys *)ph)->waitns = 200; //Set 200 ns wait states
    
    if (ph->Erase(ph, 0)){ // Call ROM routine to erase flash
      goto fail; // In case of erase failure
    }
    
    // Call ROM routine to write sector, goto fail if chip reports write error
    if (ph->Write(ph, 0, (j+255)/256, mallocAreaX, NULL) == 0) goto fail;

    /* Programming done, do special LED blink */
    while(1){    
      PERIP(GPIO1_ODATA) = 0x04; /* GPIO1[2] (LQFP pin 24) = 1 */
      for (j=0; j<10; j++) BusyWait10();
      PERIP(GPIO1_ODATA) = 0x08; /* GPIO1[3] (LQFP pin 25) = 1 */
      for (j=0; j<100; j++) BusyWait10();
    } // Continue the blinking forever
  }
  
 fail:
  PERIP(GPIO1_ODATA) = 0x08; // in fail condition constantly light LED 2
  while(1)
    ;  
}

