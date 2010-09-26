// readfile.c : An example of reading a text file t1.txt and writing it
// to the debug console

#include <stdlib.h>
#include <stdio.h>
#include <vs1000.h>
#include <minifat.h>
#include <player.h>
#include <codec.h>
#include <audio.h>
#include <usb.h>
#include <vsnand.h>
#include <mappertiny.h>
#include <string.h>

// Expose some interfaces in ROM, do not alter!
extern struct FsNandPhys fsNandPhys;
extern struct FsPhysical *ph;
extern struct FsMapper *map;
extern struct CodecServices cs;

u_int16 OpenNamedFile(const u_int32 *fileName, u_int32 extension){
  u_int32 allowedExtensions[] = {0,0};
  register u_int16 i;
  allowedExtensions[0] = extension;
  minifatInfo.supportedSuffixes = allowedExtensions;
  for (i=0; OpenFile(i)<0; i++){
    if ((((__y u_int32 *)minifatInfo.fileName)[0] == fileName[0]) 
	&& (((__y u_int32 *)minifatInfo.fileName)[1] == fileName[1])){
      return 1; // File Found
    }
  }
  return 0; //File Not Found
}
/*
// Write one character to the debug console (vs3emu rs-232 connection)
// Note: console is canonical: line feed is needed to force output.
// Note: console is slow
void ConsolePutChar(u_int16 c){
  u_int16 tmp[2];
  tmp[0] = c & 0xff;
  tmp[1] = 0;
  fputs(tmp,stdout);  
}


void main(void){
  u_int16 myChar;
  
  ph = &fsNandPhys.p; // physical disk is nand flash ROM handler
  map = FsMapTnCreate(ph, 0); // logical disk is ROM tiny mapper (read only)
  
  if (InitFileSystem()) {
    puts("No FAT");	
  } else {    
    OpenNamedFile("\pT1      ",FAT_MKID('T','X','T')); // Open t1.txt
    myChar = 0; // to be sure that the high byte of myChar is 0
    while(ReadFile(&myChar,1,1)){ 
      ConsolePutChar(myChar);     
    }
    puts(""); // Force a line feed
  }

}
*/
