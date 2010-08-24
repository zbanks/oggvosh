#include <stdio.h>

#define MAX_PROG_SIZE (2*0x1000)
unsigned char file[MAX_PROG_SIZE];

const unsigned short chainBoot[64-8] = {
    /* words 8..55 */
    0x8000,0x0029,0x0050,0x3613,0x0024,0x3e10,0x3808,0x0000,
    0x0000,0x3e10,0x0024,0x0009,0x0017,0x3e15,0xc024,0x0006,
    0x9ed7,0x3705,0xc024,0x3e15,0xc024,0x3e10,0x0024,0x3e10,
    0x0024,0x0000,0x0024,0x0007,0x3297,0x3e05,0xdd8c,0x3702,
    0x0024,0x2000,0x0000,0x0000,0x1848,0x0009,0x1010,0x36a3,
    0x0024,0x2820,0x64c0,0x36f0,0x1808,0x8003,0x0000,0x0050,
    0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff
};


int main(int argc, char *argv[]) {
    const char *in = NULL, *chain = NULL, *out = NULL;
    FILE *fpin, *fpout;
    int n, err = 0;

    for (n=1; n<argc; n++) {
	if (argv[n][0] == '-') {
	    int i = 1;
	    while (argv[n][i]) {
		if (argv[n][i] == 'c') {
		    if (argv[n][i+1]) {
			chain = &argv[n][i+1];
			break;
		    } else {
			chain = argv[++n];
			break;
		    }
		} else {
		    err = 1;
		    i++;
		}
	    }
	} else {
	    if (!in) {
		in = argv[n];
	    } else if (!out) {
		out = argv[n];
	    } else {
		err = 1;
		fprintf(stderr, "Too many filenames\n");
	    }
	}
    }
    if (err || !out || !in) {
	fprintf(stderr, "Usage: %s <prog.img> <out.img>\n",
		argv[0]);
	exit(10);
    }
    if ((fpin = fopen(in, "rb"))) {
	long size = fread(file, 1, MAX_PROG_SIZE, fpin);
	fclose(fpin);

	if (size < 2*8 || memcmp(file, "VLSI", 4)) {
	    fprintf(stderr,
		    "%s: input file '%s' does not have NAND information\n",
		    argv[0], in);
	    exit(10);
	}

	if ((fpout = fopen(out, "wb"))) {
	    if (size > 512) {
		int i;
		fprintf(stderr,
			"%s: inserting chain loader for '%s'.\n",
			argv[0], in);
		i = size/2 - (256-64/*chain*/+8/*header*/);
		i = (i+255)/256 + 1; /* sectors to read for chain loader */
		file[2*5+0] = i>>8;
		file[2*5+1] = i;
		fwrite(file, 1, 2*8, fpout); /* Copy parameters + BoOt */
		/* Insert the default chain loader */
		for (i=0;i<sizeof(chainBoot)/sizeof(chainBoot[0]);i++) {
		    fputc(chainBoot[i]>>8, fpout);
		    fputc(chainBoot[i], fpout);
		}
		fwrite(file+2*8, 1, size-2*8, fpout);
	    } else {
		fprintf(stderr,
			"%s: no chain loader needed for '%s', file unchanged.\n",
			argv[0], in);
		fwrite(file, 1, size, fpout);
	    }
	    fclose(fpout);
	} else {
	    fprintf(stderr, "%s: Could not open '%s' for writing\n",
		    argv[0], out);
	    exit(10);
	}
    } else {
	fprintf(stderr, "%s: Could not open '%s' for reading\n", argv[0], in);
	exit(10);
    }
    return 0;
}
