
// playloop.c :  Playloop example
// This code overrides the main player loop as an example on how to take
// full control of the playing. Each file is repeated until user
// pushes NEXT or PREVIOUS buttons to change the file that is playing

// This code is provided as an example for your convenience. Learn what
// you can out of it or use it as-is (recommended).

#include <stdio.h>
#include <stdlib.h>
#include <vs1000.h>
#include <minifat.h>
#include <player.h>
#include <codec.h>
#include <audio.h>
#include <usb.h>
#include <vsnand.h>
#include <mappertiny.h>

extern __y s_int32 btmp[64]; //Storage for "multi-exec" variables.
extern u_int16 mallocAreaX[]; //Storage for loading file
extern struct FsNandPhys fsNandPhys;
extern struct FsPhysical *ph;
extern struct FsMapper *map;

// Load and execute a file. Exec can only return to main() in new code.
void Exec (u_int32 extension){
  static u_int32 allowedExtensions[] = {0,0};  
  allowedExtensions[0] = extension;

  // Initialize the filesystem for nand flash physical "just in case".
  // If filesystem has already been initialized,
  // then these 3 lines can be omitted:
  //ph = &fsNandPhys.p; /* Physical disk is Nand Flash */
  //map = FsMapTnCreate(ph, 0); /* create tiny disk mapper (read only) */
  //InitFileSystem();  /* Initialize the filing system */

  minifatInfo.supportedSuffixes = allowedExtensions;
  if (OpenFile(0)<0){
    if (ReadFile(mallocAreaX, 0, 0x2000)) {
      BootFromX (mallocAreaX+8); // Exit to new image     
      // BootFromX returns only in case of invalid prg file.
      // Program execution would be unsafe after failed BootFromX
      // so the system falls down to the while(1) loop below        
    }
  }
  // Exec failed, it is not safe to continue anymore since memory
  // is overwritten by ReadFile and BootFromX. Only option is to halt.
  puts("Exec failed");
  while(1)
    ; // Halt. Some LED blink code etc could just and just be here.
}

void main(void) {
  
  if (USBIsAttached()){
    return; // Quit to ROM code to handle USB traffic
  }
  
  ph = &fsNandPhys.p; // physical disk is nand flash ROM handler
  map = FsMapTnCreate(ph, 0); // logical disk mapper is ROM tiny mapper (read only)
    
  if (InitFileSystem()) {
    // File System Not Found, play low sine wave as an "error message"
    register u_int16 *g = (u_int16*) g_yprev0; // get ptr to sintest params
    *g++ = 4; *g++ = 44100; *g++ = 0x5050; *g++ = 120; *g++ = 200;
    SinTest(); //Exit to SinTest: Play weird tone at low volume
    
  } else {
    // Run any .prg file on the disk
    //puts("Opening .prg");
    Exec(FAT_MKID('O','V','F'));
    
    /*
    // FAT File System Found, play files from it.
    // The code below is an example playing routine. It's provided
    // here as an example of one possible way of implementing the player
    // main routine.
    
    minifatInfo.supportedSuffixes = supportedFiles;
    player.totalFiles = OpenFile(0xffffU);        
    player.currentFile = 0;

    while (1) { // player loop

      // "Previous" file selected at the first file
      if (player.currentFile < 0) {
	player.currentFile += player.totalFiles;
      }
      // "Next" file selected at or after the last file
      if (player.currentFile >= player.totalFiles){
	player.currentFile -= player.totalFiles;
      }

      // Try to open file number player.currentFile
      if (OpenFile(player.currentFile) < 0) { 
	// File opened. (OpenFile returns negative number for ok
	// or number of files if not ok)

	OpenFile(player.currentFile);
	
	player.nextStep = 0; // This can be changed by keyboard-handler
	{
	  u_int16 result = PlayCurrentFile();

	  if (result == ceFormatNotFound){
	    player.currentFile++; // Skip erroneous file
	  }
	  if (result == ceFormatNotSupported){
	    player.currentFile++; // Skip erroneous file
	  }
	  if (result == ceOtherError){
	    player.currentFile++; // Skip erroneous file
	  }
	}
	player.currentFile += player.nextStep;
	  
      } else { // Could not OpenFile.
	player.currentFile = 0; // Revert to playing the first file
      }
    } //end of player loop
    */
  } 
}

