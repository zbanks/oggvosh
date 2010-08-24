#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

int nandType = 2; /* 1 = LARGEPAGE 2 = ADDRBYTES3 4 = ADDRBYTES4 */
int nandWaitNs = 40;
int blockSizeBits = 5; /* 32 */
int flashSizeBits = 15; /* 16M */

void PutHeader(FILE *fp, int addr) {
    if (fp) {
	unsigned char head[16];
	int cnt = 0;

	head[cnt++] = 'V'; /* exec */
	head[cnt++] = 'L'; /* exec */
	head[cnt++] = 'S';
	head[cnt++] = 'I';
	head[cnt++] = nandType>>8; /*nandType hi */
	head[cnt++] = nandType;
	head[cnt++] = blockSizeBits;
	head[cnt++] = flashSizeBits;
	head[cnt++] = nandWaitNs/256;
	head[cnt++] = nandWaitNs;
	head[cnt++] = 0xff; /* reserved */
	head[cnt++] = 0xff; /* reserved */

	printf("NandType: %d %s %s addr, %dkB blocks, %dMB flash\n", nandType,
	       (nandType & 1) ? "Large-Page" : "Small-Page",
	       (nandType & 2) ?
	       ((nandType & 4) ? "6-byte" : "5-byte") : "4-byte",
	       (1<<blockSizeBits)/2,
	       (1<<flashSizeBits)/2048);

	if (addr != 0xffff) {
	    head[cnt++] = 'B';
	    head[cnt++] = 'o';
	    head[cnt++] = 'O';
	    head[cnt++] = 't';
	}

	fwrite(head, 1, cnt, fp);
    }
}

/*
FUNCTION: SavePack()
DESCRIPTION: Writes a data block to a file.
  0, 1, and 2 are used for uncompressed I, X, and Y blocks.
INPUTS:
RESULT:
 */
int SavePack(FILE *fp, int page, unsigned char *origData, int origSize,
	     int addr) {
    if (fp) {
	unsigned char head[16];
	int cnt = 0;

	if (origData == NULL) {
	    if (addr != 0xffff) {
		head[cnt++] = 0x80; /* exec */
		head[cnt++] = 0x03; /* exec */
		head[cnt++] = 0;
		head[cnt++] = 0;
		head[cnt++] = (addr>>8);
		head[cnt++] = (addr & 255);

		fwrite(head, 1, cnt, fp);
	    }
	    return 0;
	} else {
	    head[cnt++] = 0x80;
	    head[cnt++] = page;
	    head[cnt++] = (origSize/2 - 1) >> 8;
	    head[cnt++] = (origSize/2 - 1) & 255;
	    head[cnt++] = (addr>>8);
	    head[cnt++] = (addr & 255);

	    fwrite(head, 1, cnt, fp);
	    fwrite(origData, origSize, 1, fp);
	    return 0;
	}
    }
    return 10;
}

#ifndef min
#define min(a,b) ((a<b)?(a):(b))
#endif

#define F_ERROR   (1<<15)


/******************************** COFF ********************************/

#ifndef COFF_MAGIC16
#define COFF_MAGIC16  0xb5b4
#define COFF_MAGIC    0xb5b3
#define OPTMAGIC      0xb502
#define CF_EXEC     0x002
#define STYP_TEXT   0x020
#define STYP_DATA   0x040
#define STYP_BSS    0x080
#define STYP_DATAX  0x200
#define STYP_DATAY  0x400
#endif

unsigned short GetShort(FILE *fp, char swap) {
    unsigned short c1, c2;
    c1 = fgetc(fp);
    c2 = fgetc(fp);
    return swap ? ((c2<<8) | c1) : ((c1<<8) | c2);
}

unsigned long GetLong(FILE *fp, char swap) {
    unsigned long c1, c2, c3, c4;
    c1 = fgetc(fp);
    c2 = fgetc(fp);
    c3 = fgetc(fp);
    c4 = fgetc(fp);
    return swap ? ((c4<<24) | (c3<<16) | (c2<<8) | c1) :
	((c1<<24) | (c2<<16) | (c3<<8) | c4);
}

/******************************** COFF ********************************/

FILE *outFp = NULL;
int totalIn = 0, totalOut = 0;
int execAddr = 0x80;

void SendFlashBlock(FILE *fp, int page, unsigned char *data, short len,
		    long addr) {
    if (len) {
	int out = len + 6;

	SavePack(fp, page, data, len, addr);
	len += 6; /* header would take 6 bytes */
	totalIn += len;
	totalOut += out;
	printf("In:%5d, out:%5d\n", len,  out);
    } else {
	SavePack(fp, 0, NULL, 0, execAddr);

	totalOut += 6;
	printf("In: %d, out: %d\n", totalIn, totalOut);
    }
}

struct MYSECT {
    int page;
    long addr;
    long size; /* size in words */
    long len;  /* size in bytes */
    unsigned char *data;
};
#define MAX_SECT 4000
struct MYSECT block[MAX_SECT];
int blockNum = 0;

int SendFlashData(int putHeader, int addr, char *fname, char *fileOut) {
    FILE *fp;
    unsigned short c1, c2;
    unsigned short magic1, magic2;
    unsigned short f_nscns;
    unsigned short f_opthdr;
    unsigned short f_flags;
    char swap = 0;
    int sectNum;

    fp = fopen(fname, "rb");
    if (!fp) {
	printf("could not open %s\n", fname);
	return 10;
    }
    c1 = fgetc(fp);
    c2 = fgetc(fp);
    magic1 = (c1<<8) | c2;
    magic2 = (c2<<8) | c1;
    if (magic1 == COFF_MAGIC) {
	swap = 0;
    } else if (magic2 == COFF_MAGIC) {
	swap = 1;
    } else if (magic1 == COFF_MAGIC16) {
	swap = 0;
    } else if (magic2 == COFF_MAGIC16) {
	swap = 1;
    } else {
	printf("not a COFF file: %s\n", fname);
	fclose(fp);
	return 10;
    }
    f_nscns = GetShort(fp, swap);
    GetLong(fp, swap);
    GetLong(fp, swap);
    GetLong(fp, swap);
    f_opthdr = GetShort(fp, swap);
    f_flags = GetShort(fp, swap);
    if (!(f_flags & CF_EXEC)) {
	fclose(fp);
	printf("%s not executable\n", fname);
	return 10;
    }
    if (f_opthdr) {
	fseek(fp, f_opthdr, SEEK_CUR);
    }
    sectNum = 0;
    while (sectNum < f_nscns) {
	long s_paddr, s_size, s_scnptr, s_flags;

	sectNum++;
	for (c1=0; c1<8; c1++) {
	    fgetc(fp);
	}
	s_paddr = GetLong(fp, swap);
	fseek(fp, 4, SEEK_CUR);
	s_size = GetLong(fp, swap);
	s_scnptr = GetLong(fp, swap);
	fseek(fp, 4+4+2+2, SEEK_CUR);
	s_flags = GetLong(fp, swap);

	if (!s_size || (s_flags & STYP_BSS))
	    continue; /* Skip BSS */

	if (blockNum < MAX_SECT) {
	    int page = 0, ss;
	    unsigned char *data = NULL;
	    long filePos;

	    s_size /= 4; /* size in words */
	    if (s_flags & STYP_TEXT) {
		page = 0;
	    } else if ((s_flags & STYP_DATAX)) {
		page = 1;
	    } else if ((s_flags & STYP_DATAY)) {
		page = 2;
	    }
	    /* Check merging possibilities --
	       increases the compression ratio and reduce overhead */
	    for (ss = 0; ss < blockNum; ss++) {
		if (page == block[ss].page &&
		    s_paddr == block[ss].addr + block[ss].size) {
		    /* Add to the tail */
		    unsigned char *tmp = realloc(block[ss].data,
						 block[ss].len +
						 s_size * ((page == 0)?4:2));
		    if (tmp) {
			block[ss].data = tmp;
			data = tmp + block[ss].len; /* point to tail */
			block[ss].size += s_size; /* size in words */
			block[ss].len += s_size * ((page == 0) ? 4 : 2);
			ss = -1;
			break;
		    }
		}
		if (page == block[ss].page &&
		    s_paddr + s_size == block[ss].addr) {
		    /* Add to the head -- although quite rare */
		    unsigned char *tmp = malloc(block[ss].len +
						s_size * ((page == 0)? 4 : 2));
		    if (tmp) {
			/* copy old data to the tail */
			memcpy(tmp + s_size * ((page == 0)?4:2),
			       block[ss].data, block[ss].len);
			free(block[ss].data);
			block[ss].data = tmp;
			data = tmp; /* point to head -- new data here */
			block[ss].size += s_size; /* size in words */
			block[ss].len += s_size * ((page == 0) ? 4 : 2);
			block[ss].addr = s_paddr;
			ss = -1;
			break;
		    }
		}
	    }
	    if (ss != -1) {
		/* create new header */
		block[blockNum].page = page;
		block[blockNum].addr = s_paddr; /* physical addr! */
		block[blockNum].size = s_size;
		block[blockNum].len = s_size * ((page == 0) ? 4 : 2);
		data = calloc(s_size, (page == 0) ? 4 : 2);
		block[blockNum].data = data;
		blockNum++;
	    }
	    /* Add data */
	    filePos = ftell(fp); /* remember current location */
	    fseek(fp, s_scnptr, SEEK_SET); /* jump to start of data */
	    if (page == 0) {
		while (s_size--) {
		    unsigned long b = GetLong(fp, swap);
		    *data++ = b>>24;
		    *data++ = b>>16;
		    *data++ = b>>8;
		    *data++ = b;
		}
	    } else {
		while (s_size--) {
		    unsigned long b = GetLong(fp, swap);
		    *data++ = b>>8;
		    *data++ = b;
		}
	    }
	    fseek(fp, filePos, SEEK_SET); /* restore location */
	} else {
	    puts("too many sections");
	    return 10;
	}
    }

    outFp = fopen(fileOut, "wb");
    if (!outFp) {
	printf("could not open %s for writing\n", fileOut);
	return 10;
    }
//    fwrite("P&H", 1, 3, outFp);
    if (putHeader)
	PutHeader(outFp, addr);
    sectNum = 0;
    while (sectNum < blockNum) {
	static const char pstr[] = "IXY";
	printf("%c: 0x%04lx-0x%04lx ",
	       pstr[block[sectNum].page], block[sectNum].addr,
	       block[sectNum].addr + block[sectNum].size);
	SendFlashBlock(outFp,
		       block[sectNum].page, block[sectNum].data,
		       block[sectNum].len, block[sectNum].addr);
	sectNum++;
    }
    SendFlashBlock(outFp, 0, "", 0, 0);
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    int n;
    int flags = 0, putHeader = 1;
    char *fileIn = NULL, *fileOut = NULL;

    for (n=1; n<argc; n++) {
	if (argv[n][0]=='-') {
	    int i = 1, ch;
	    char *val, *tmp;
	    long tmpval;

	    while (argv[n][i]) {
		switch (ch = argv[n][i]) {
		case 'h':
		case '?':
		    flags |= F_ERROR;
		    break;
		case 'n':
		    putHeader = 0;
		    break;

		case 'x':
		case 'w':
		case 't':
		case 'b':
		case 's':
		case 'c':
		    if (argv[n][i+1]) {
			val = argv[n]+i+1;
		    } else if (n+1 < argc) {
			val = argv[n+1];
			n++;
		    } else {
			flags |= F_ERROR;
			break;
		    }
		    i = strlen(argv[n])-1;
		    tmpval = strtol(val, &tmp, 0);
		    if (*tmp) {
			fprintf(stderr,
				"Error: invalid number: \"%s\"\n", val);
			flags |= F_ERROR;
			break;
		    }
		    if (ch == 'c') {
			if (tmpval == 0) {
			    /* 16M small-page flash */
			    nandWaitNs = 80;
			    nandType = 0;
			    blockSizeBits = 5;
			    flashSizeBits = 15;
			} else if (tmpval == 1) {
			    /* 128M small-page flash */
			    nandWaitNs = 100;
			    nandType = 2;
			    blockSizeBits = 5;
			    flashSizeBits = 18;
			} else if (tmpval == 2) {
			    /* 256M large-page flash */
			    nandWaitNs = 100;
			    nandType = 3;
			    blockSizeBits = 8;
			    flashSizeBits = 19;
			} else {
			    fprintf(stderr,
				    "Error: unknown chip: \"%s\"\n", val);
			    flags |= F_ERROR;
			}
		    } else if (ch == 'x')
			execAddr = tmpval;
		    else if (ch == 'w')
			nandWaitNs = tmpval;
		    else if (ch == 't')
			nandType = tmpval;
		    else if (ch == 'b')
			blockSizeBits = tmpval;
		    else if (ch == 's')
			flashSizeBits = tmpval;
		    break;

		default:
		    fprintf(stderr, "Error: Unknown option \"%c\"\n",
			    argv[n][i]);
		    flags |= F_ERROR;
		}
		i++;
	    }
	} else {
	    if (!fileIn) {
		fileIn = argv[n];
	    } else if (!fileOut) {
		fileOut = argv[n];
	    } else {
		fprintf(stderr, "Only two filenames wanted!\n");
		flags |= F_ERROR;
	    }
	}
    }
    if ((flags & F_ERROR) || !fileIn || !fileOut) {
	fprintf(stderr, "Usage: %s [-w <waitns>] [-t <type>] [-b <block>] [-s <size>] [-x <exec>] [-c] [<infile> [<outfile>]]\n",
		argv[0]);
	fprintf(stderr, "\t x<val>    set execution address\n");
	fprintf(stderr, "\t n         no NAND header\n");
	return EXIT_FAILURE;
    }
    return SendFlashData(putHeader, execAddr, fileIn, fileOut);
}


