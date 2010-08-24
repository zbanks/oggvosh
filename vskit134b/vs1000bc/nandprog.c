// NANDPROG.C -- Program for erasing/initializing nand flash chip

#include <stdio.h>
#include <vs1000.h>
#include <minifat.h>
#include <stdlib.h>
#include <string.h>
#include <vsNand.h>

extern struct FsNandPhys fsNandPhys;
#define NandSelect() USEX(GPIO0_ODATA) &= ~GPIO0_CS1
#define NandDeselect() USEX(GPIO0_ODATA) |= GPIO0_CS1
void NandPutCommand (register __a0 u_int16 command);
void NandPutBlockAddress(register __c u_int32 addr);
void NandPutDataAddress(register __c u_int32 addr);
void NandPutLargePageSpareAddress(register __c u_int32 addr);
u_int16 NandGetStatus(void);
void NandPutAddressOctet(register __a0 u_int16 address);
void NandGetOctets(register __c0 s_int16 length, register __i2 u_int16 *buf);

extern __y u_int16 mallocAreaY[]; /* for ramdisk */
extern u_int16 mallocAreaX[];     /* for ramboot */
extern struct FsPhysical *ph;

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

void put2hex(u_int16 a) {
  char tmp[8];
  tmp[0] = hex[(a>>12)&15];
  tmp[1] = hex[(a>>8)&15];
  tmp[2] = ' ';
  tmp[3] = hex[(a>>4)&15];
  tmp[4] = hex[(a>>0)&15];
  tmp[5] = ' ';
  tmp[6] = '\0';
  fputs(tmp, stdout);
}



void ScreenPutUInt(register u_int32 value){
  register u_int16 i; 
  register u_int16 p = 0;
  register u_int16 length=8;
  u_int16 s[8];
  for (i=0; i<8; i++){
    s[i]=value % 10;
    value = value / 10;
  }
  do {   
    i = s[--length];
    if (i){
      p=1;
    }
    if (length==0){
      p=1;
    }
    if (p){
      putchar('0' + i);
    }
  } while (length);     
}



void PutInt (const char *s, u_int32 n){
  fputs(s,stdout);
  ScreenPutUInt(n);
}


#define EndLine fputs("\n",stdout)


u_int16 NandGetIdent(void){
  u_int16 ident[4];
  NandDeselect();
  NandSelect();
  NandPutCommand(NAND_OP_READ_SIGNATURE);
  NandPutAddressOctet(0);
  NandGetOctets(8,ident);
  NandDeselect();
  fputs("Nand Chip returns ID data: ",stdout);
  {
    register int i;
    for (i=0; i<4; i++){
      put2hex (ident[i]);
    }
  }
  fputs("\n",stdout);
  return ident[0];
}


void main(void) {
  register u_int16 i;
  static u_int16 ident[4];
  static u_int16 pageSize, blockSize, dataSize;
  static s_int16 blockSizeBits, dataSizeBits;
  
  pageSize=0; 
  blockSize=0;
  dataSize=0;
  
  puts("\nVS1000 Nand Flash Init Utility");
  
  PutInt("Current VS1000 NandType Setting: ",fsNandPhys.nandType);
  if (fsNandPhys.nandType==0xffffU){
    fputs(" (not set)",stdout);
  }
  EndLine;
  
  
  
  puts("\nNow trying to auto-detect NAND chip...");
  
  // Read Nand Flash Ident Data
  NandDeselect();
  NandSelect();
  NandPutCommand(NAND_OP_READ_SIGNATURE);
  NandPutAddressOctet(0);
  NandGetOctets(8,ident);
  NandDeselect();
  fputs("Nand Chip returns ID data: ",stdout);
  for (i=0; i<4; i++){
    put2hex (ident[i]);
  }
  fputs("\n",stdout);
  
  //Hynix HY27UF081G2A
  if ((ident[0]) == 0xadf1U){
    puts("This is a Hynix HY27UF081G2A Chip");
    pageSize = 2048; // Bytes
    blockSize = 128; // Kilobytes
    dataSize = 128; // Megabytes
  }
  //Micron 29F2G08AAC
  if ((ident[0]) == 0x2cdaU){
    puts("This seems to be a Micron 29F2G08AAC Chip");
    pageSize = 2048; // Bytes
    blockSize = 128; // Kilobytes
    dataSize = 256; // Megabytes
  }

  if ((ident[0] & 0xff00U) == 0xec00U){ //Samsung
    puts("This is a Samsung Chip.");
    if (ident[1] & 0x0F00U){
      puts("This is a multi-level or multi-wafer chip (not supported)");
      
    } else { //Single level, Single chip select
      
      puts("Detecting settings based on K9XXG08UXM datasheet");
      pageSize = 1024 << ((ident[1] >> 0) & 0x03);
      blockSize = 64 << ((ident[1] >> 4) & 0x03);
      dataSize = 8 << ((ident[2] >> 12) & 0x07);
      // does the chip have multiple data planes?
      dataSize <<= ((ident[2] >> 10) & 0x03);
    }
  }
  
  if ((ident[0] | 0x0040) == 0x2073U){
    puts("This is a NAND128 Chip");
    pageSize = 512;
    blockSize = 16;
    dataSize = 16;
  }
  if ((ident[0] | 0x0040) == 0x2075U){
    puts("This is a NAND256 Chip");
    pageSize = 512;
    blockSize = 16;
    dataSize = 32;
  }
  if ((ident[0] | 0x0040) == 0x2076U){
    puts("This is a NAND512 Chip");
    pageSize = 512;
    blockSize = 16;
    dataSize = 64;
  }
  if ((ident[0] | 0x0040) == 0x2079U){
    puts("This is a NAND01G Chip");
    pageSize = 512;
    blockSize = 16;
    dataSize = 128;
  }
  
  puts("Oh hai");
  pageSize = 2048;
  blockSize = 128;
  dataSize = 256;
  
  PutInt("Page Size (B): ",pageSize);EndLine;
  PutInt("Erasable Block Size (KB): ",blockSize);EndLine;
  PutInt("Data Plane Size (MB): ",dataSize);EndLine;  
  
  
  if (pageSize==0){
    puts("Cannot detect Nand Type. Using forced settings");

    pageSize = 2048;
    blockSize = 20;
    dataSize = 256;

    //    while(1)
    //      ;
  }
  
  puts("\nCalculating new NandType setting");
  fsNandPhys.nandType = 0;
  if ((pageSize!=512) && (pageSize!=2048)){
    puts("Error: Only 512B and 2048B page sizes are supported!");
    while(1)
      ;
  }       
  
  if (pageSize == 2048) { // Large page
    fsNandPhys.nandType = 1;
    puts("Set Large Page type");
    if (dataSize>128) {
      fsNandPhys.nandType |= 2; //Add 3rd block address byte
    }
  } else { // Small Page
    if (dataSize>32) {
      fsNandPhys.nandType |= 2; //Add 3rd block address byte
    }       
  }    
  
  // Calculate how many bits are needed to represent block size in sectors
  i = blockSize*2;
  PutInt("Sectors per Block: ",i);
  
  blockSizeBits = -1;
  while (i){
    i/=2;
    blockSizeBits++;
  }   
  PutInt(" -> Bits: ",blockSizeBits); EndLine;
  
  
  
  // Calculate how many bits are needed to represent data size in sectors
  i = dataSize;
  PutInt("Data area sectors: ",(u_int32)i*2048);
  
  dataSizeBits = 10;
  while (i){
    i/=2;
    dataSizeBits++;
  }   
  PutInt(" -> Bits: ",dataSizeBits); EndLine;
  
  PutInt("\nUsing NandType setting ",fsNandPhys.nandType);EndLine;
  
  // Nand type detection complete.
  
  
  
  while(1) {
    char s[5];
    
    puts("Reading First Sector");
    ph->Read(ph,0,1,minifatBuffer,NULL);
    
    fputs("Current Signature is ",stdout);
    for (i=0; i<8; i++){
      put2hex (minifatBuffer[i]);
    }
    fputs("\n",stdout);
    
    memset(minifatBuffer, -1/*0xffffU*/, 256);
    
    puts("\n\n1) Erase all blocks\n2) Write ident (no boot code)\n3) Remove ident (to get ramdisk)\n4) Flash code from NAND.IMG\n");
    fgets(s,5,stdin);
    
    switch(s[0]) {
    case '1':
      puts("Erasing Chip.");
      {
	u_int32 pos;
	pos = 0;
	while (pos < ((u_int32)dataSize * 2048)) {
	  ph->Erase(ph, pos);
	  pos += ((u_int32)blockSize * 2);
	}
      }
      puts("Done.");
      break;
      
    case '2':
      minifatBuffer[0] = 0x564c;
      minifatBuffer[1] = 0x5349;
      minifatBuffer[2] = fsNandPhys.nandType;
      minifatBuffer[3] = (blockSizeBits<<8) | dataSizeBits;
      minifatBuffer[4] = 70;
      //Intentional fall-through
      
    case '3':
      fputs("New Signature is     ",stdout);
      for (i=0; i<8; i++){
	put2hex (minifatBuffer[i]);
      }
      fputs("\n",stdout);
      
      puts("Erasing Block 0");
      ph->Erase(ph, 0);
      puts("Writing New Block");
      ph->Write(ph,0,1,minifatBuffer,NULL);
      break;   
      
    case '4':
      {
	FILE *fp;
	puts ("Writing NAND.IMG (no erase, no verify)");
	if (fp = fopen ("NAND.IMG", "rb")){ // Open a file in the PC
	  u_int16 len;
	  u_int16 sectorNumber;
	  
	  //Detach vs3emu debugger interface interrupt for binary xfer...
	  PERIP(INT_ENABLEL) &= ~INTF_RX; 
	  ph->Erase(ph, 0);
	  
	  sectorNumber = 0;
	  
	  while ((len=fread(minifatBuffer,1,256,fp))){

	    if (sectorNumber == 0){
	      puts("Putting correct NAND parameters");
	      minifatBuffer[0] = 0x564c;
	      minifatBuffer[1] = 0x5349;
	      minifatBuffer[2] = fsNandPhys.nandType;
	      minifatBuffer[3] = (blockSizeBits<<8) | dataSizeBits;
	      minifatBuffer[4] = 70;  	      
	    }
	    
	    ph->Write(ph,sectorNumber,1,minifatBuffer,NULL);
	    sectorNumber++;
	    
	    if (sectorNumber & 0x01){
	      ScreenPutUInt(sectorNumber / 2);fputs("kB ",stdout);
	    }
	  }
	  fclose(fp);       
	}else{
	  puts("File not found\n");
	}
	
	//Reattach vs3emu debugger interface interrupt
	PERIP(INT_ENABLEL) |= INTF_RX;
	
	
	
      }
      break;      
    }
  }       
}

