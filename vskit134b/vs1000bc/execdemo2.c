/// \file execdemo2.c counterpart to execdemo1.c

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



// Small hex output function for console debugging purposes:
__y const char hex[] = "0123456789abcdef";
void puthex(u_int16 a) {
  char tmp[8];
  tmp[0] = hex[(a>>12)&15];  tmp[1] = hex[(a>>8)&15];
  tmp[2] = hex[(a>>4)&15];   tmp[3] = hex[(a>>0)&15];
  tmp[4] = ' ';              tmp[5] = '\0';
  fputs(tmp, stdout);
}




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

  puts("Now running file execdemo2.");

  btmp[0]++;
  fputs("btmp[0] has value ",stdout); puthex(btmp[0]); puts("");

  Exec(FAT_MKID('P','R','1')); // Run program.PR1

}




