vcc -P130 -O -fsmall-code -I lib -o a.o $1
vslink -k -m mem_user -L lib -o a.bin lib/c-spi.o lib/rom1000.o a.o -lc -ldev1000
vs3emu -chip vs1002b -s 115200 -l a.bin e.cmd

