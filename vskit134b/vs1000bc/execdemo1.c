/// \file execdemo1.c Exec demo

/*
Exec demo.

This demonstrates how to boot to a different file in the filesystem.

Since the internal filing system of VS1000 ROM works by enumerating
all files with allowable extensions, the choice of which file to run
is made by the file's extension only. The idea is that the filesystem
contains files "PROGRAM.PR1" and "PROGRAM.PR2". First file is selected
by running the first file with extension "PR1" and the second file
is run by running the first file with extension "PR2". These are
created by renaming a Nand-Flash bootable file such as NAND128W.IMG
that is created by the build script.

Additionally, parameter passing between programs is demonstrated:
Parameter passing can be done by mapping variables to known same 
locations in different images. In a large project this can be done
using the assmebler and ORG directive cleanly. But in a small project
there are some tricks such as this one: We use some memory area
used by ROM code that is not active, to store the variables.
Normally the Bass Boost is not active, it's not activated by ROM
code, though user has the option of activating it from software.
We use a 64-word table "s_int16 __y btemp[64]" to store variables
that can be seen across different execs.
*/

#include <vs1000.h>
#include <stdio.h>
#include <minifat.h>
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
  ph = &fsNandPhys.p; /* Physical disk is Nand Flash */
  map = FsMapTnCreate(ph, 0); /* create tiny disk mapper (read only) */
  InitFileSystem();  /* Initialize the filing system */

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
  while(1)
    ; // Halt. Some LED blink code etc could just and just be here.
}



void main(void){

  puts("Now running file 1.");
  Exec(FAT_MKID('P','R','2')); // Run program.PR2

}




