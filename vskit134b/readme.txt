VLSI Solution Oy VS1000 Toolkit


******************** IMPORTANT ************************
  The Nand Flash Chip contains boot code. 
  When you run code from the emulator, the boot code
 in Nand Flash Chip will interfere with the code
 loaded from the emulator. 
  Before you load code from the vs3emu emulator,
 you should remove the Nand Flash Boot Code
 in VS1000B/C by running nandprog.c by typing

   build nandprog

 and selecting "2) write ident (no boot code)" from the menu.

*******************************************************
  ABSOLUTELY NO WARRANTY
 VLSI Solution provides these files as an example only.
 Under no circumstances will VLSI guarantee the functio-
 nality of any of the files in this package.
*******************************************************

SECTION 01-OCT-2008 for VSKIT1.34b (see previous sections below)

Changes for VSKIT1.34b
======================
Windows binaries updated


SECTION 03-JUL-2008 for VSKIT1.34 (see previous sections below)

Changes for VSKIT1.34
=====================
VS1000 Developer Library
------------------------
A link library dev1000.h / libdev1000.a has been added to the build
scripts (-ldev1000). Only the functions you use will be linked in
from the library. Currently this library contains useful functions
for using your own interrupts, MMC/SD communication, and RC5 reception.

Most of the functions have been written in ASM to be efficient in
speed and space.

Other Changes
-------------
- Separate include/ and lib/ subdirectories have been merged together
  to make it easier to update the header files and libraries.

- Updated build.bat

- Updated nandprog.c has a couple of more supported NAND FLASH types.

- Updated VS1000 Programmer's Guide




SECTION 27-JUN-2007 for VSKIT1.33 (see previous sections below)

VS1000B / VS1000C Support
-------------------------

With the release of VS1000B, most of the small bugs in VS1000A have 
been corrected and life has been made simpler for the programmer in 
many ways. Notably, the patches that have been necessary for VS1000A
(1000a_fix1.c) no longer need to be loaded, which frees chip resources 
for the programmer. Also some new service hooks have been added and
the boot process is simplified. Most of the changes are invisible
to the user/programmer. Binaries compiled for the VS1000A don't
generally run in VS1000B and need to be recompiled. For this,
the environment for compiling is now in directory vs1000bc. 
(VS1000C is internal production test chip that has same code as VS1000B,
only VS1000B will be sold but some VLSI boards have VS1000C chips 
installed - don't worry about this, the chip you are coding for is VS1000B.)


FLASHING THE VS1000B DEVELOPER BOARD

Flashing the NAND flash in Developer Board is made easier so that
all Nand Flash operations can be handled by one program - nandprog.c,
which can be run by typing

build nandprog

in the VSKIT133\VS1000BC directory. It auto-detects the nand chip used
(at least all VLSI installed chips are supported). The options are:
 
1) Erase all blocks
- This erases the entire boot and data areas of the Nand chip. It's useful
if the filesystem is unrecoverably garbled for instance due to building a filesystem
with incorrect erasable-block-size and/or data-area-size parameters.

2) Write ident (no boot code)
- This removes all boot code from the boot area but installs the VLSI 
ident string that is needed for the filesystem to run. This is most useful 
option when you are loading code from the emulator and you don't want any
Nand Flash Boot code to interfere with your development

3) Remove ident (to get ramdisk)
- This clears the boot block so the next time you attach the board to
the USB you get ramdisk (now identified by having an empty file called "NO_FLASH").
This way you can test ramdisk programming (by dropping VS1000_B.RUN and xxxxxxxx.IMG
files to the ramdisk and detaching USB) which is useful in (mass) productions of
devices

4) Flash code from NAND.IMG
- This writes the correct nand flash identifier string and programs nand
flash boot code from file NAND.IMG. The idea is that you build your project (such
as the example display demo application display.c by typing build display)
and then copy file K9F2G08.IMG to NAND.IMG and program it by writing

BUILD DISPLAY                    (or your project name instead of DISPLAY)
COPY K9F2G08.IMG NAND.IMG
BUILD NANDPROG

and select 4) from the menu. This way the NAND.IMG from the DISPLAY build is
programmed to the nand flash and you can test booting from the nand flash.
Although the nandprog.c puts correct nand flash parameters based on nand flash
autodetect, the parameters inside file K9F2G08.IMG must be correct if the
ramdisk loading method is used. 

To use another kind of nand flash chips you need to change the parameters. See 
documentation (datasheet, programmer's guide) and make adjustments to build.bat 
as necessary.



EXEC FUNCTION FOR RUNNING BINARIES STORED ON FILESYSTEM
-------------------------------------------------------

An example of Exec() -function that loads code from the filesystem and 
exits to executing the new code is provided in files execdemo*.c Using
Exec() a complex system can be made, which boots from the FLASH, has
a main program in one file and other programs in other files, can be
created. The idea is that each program ends by Exec():ing the
main program file. See the documentation insed execdemo1.c file.




DEBUGGING HELP
--------------
Sometimes it's useful to know in which function VS1000B is
executing - especially if the program has crashed. You can try to
connect to the chip in debug mode without loading code by typing:

..\bin\vs3emu -chip vs1000 -p 1 -s 115200

If the chip responds, you can proceed by giving commands:

loadsymbols o_emuspi.bin
loadsymbols romsymbols.bin
where

And with some luck, you find out in which function the chip
was executing before emulator was invoked. Sometimes it's
even possible to continue execution with the command

e

...though sometimes it's impossible to continue (depends which opcode
was executing).




CPP.EXE LOCATION PROBLEMS
-------------------------
Sometimes compilation fails because CPP.EXE is not found.
In that case adding the vskit133\bin directory to PATH helps.





==END OF SECTION==


SECTION 18-MAY-2007 for VSKIT1.32

1) Quick Instructions for Compiling
2) Notes For Demonstration Board Users
3) Notes for Developer Board Users
4) Initializing the Nand Flash chip
5) Default display example application


== 1 == Quick Instructions for Compiling

The "MS-DOS Prompt" or "Command Prompt" under Windows is used to compile 
the sources.

Unzip the zip file to a folder such as "C:\VSKIT". Be sure to "use folder names".
There should now be folders "C:\VSKIT\BIN", "C:\VSKIT\VS1000A", 
"C:\VSKIT\VS1000A\INCLUDE" etc. The "C:\VSKIT\BIN" directory should contain
the binaries (vcc.exe, vslink.exe, vs3emu.exe,...).

Then navigate to directory "C:\VSKIT\VS1000A" using command like

C:\some_directory_name> cd \vskit\vs1000a


There you can edit the C code files with command such as:

C:\VSKIT\VS1000A> edit hello.c


You can use the command line tools directly if you add the C:\VSKIT\BIN folder
to the system PATH. You can run the "ADDPATH.BAT" file to do this in most cases.

C:\VSKIT\VS1000A> addpath


To help you with the compilation process, there is a MS-DOS BAT file "BUILD.BAT"
included in the directory VS1000A. It can be used to build a single source file
project by writing the base name of the source file name, e.g. to compile project
whose source code is in "hello.c" you should write

C:\VSKIT\VS1000A> build hello


The compiler batch tries to do the compilation. If successful, it should write
something like:

  Compiling hello.c ...
  Successfully compiled 129 lines (10 in source file)
  (17 14 14 14 14)
  C 14 CF 0 X 14 Y 0 F 0
  Linking for Nand Flash loading... (combining with libraries)
  Linking for Emulator and SPI EEPROM loading... (combining with libraries)
  ==OK== Emulator runnable program is now in o_emuspi.bin.
  Creating SPI EEPROM image (25LC640 etc)...
  I: 0x0050-0x0093 In:  273, out:  273
  X: 0x1fa0-0x1fae In:   33, out:   33
  In: 306, out: 311
  ==OK== EEPROMmable image is now in eeprom.img
  Creating NAND FLASH loadable image for ST NAND128W chip...
  NandType: 0 Small-Page 4-byte addr, 16kB blocks, 16MB flash
  I: 0x0050-0x0091 In:  266, out:  266
  X: 0x1fa0-0x1fae In:   34, out:   34
  In: 300, out: 306
  ..\bin\makenandimage: no chain loader needed for 'nand.rec', file unchanged.
  ==OK== ST NAND128W compatible image is in NANDFLSH.IMG
  -
  ==OK== Compilation seems to be successful.
  Hit [Ctrl-C] to quit this batch job or [Enter] to load code via emulator


If you are using VS1000 Developer Board, it has a RS-232 interface connector. You
can use a RS-232 cable to attach the board to COM1 and use vs3emu to load an run
code.

For further information see the VS1000 Programmer's Guide.



The examples in the VS1000A folder can be run easily with commands like

C:\VSKIT\VS1000A> build hello

C:\VSKIT\VS1000A> build led

etc.


== 2 == VSKIT131 Notes For Demonstration Board Users

The directory C:\VSKIT\VS1000A\DEMO_FLA contains the original flash boot block
writing image for the VS1000A demonstration board with ST NAND128W flash.
The files can be used to restore the flash boot block to the "original" version
after you have overwritten the flash with your own boot-up code.

Flashing instructions:

- Connect the VS1000A demonstration board to the USB port with CS1 shorted to GND.
  (short CS1 and TP2 on the demonstration board PCB)
- The VS1000A RAM DISK (16 kB, "VLSIFATDISK") appears
- Remove the CS1 short to ground
- Copy file VS1000_A.RUN to the RAM DISK
- Copy file NAND128W.IMG to the RAM DISK
- Detach USB cable without detaching power (this is easy with a USB power Y cable,
  which has separate connector for power (red connector) and data (black connector)
  in this case detach the black connector and leave the red connector connected to pc)
- The flash boot block is now written, it only takes milliseconds.
  when programming is done, VS1000A blinks the LEDs unsymmetrically
- Next power-up boots vs1000 with new boot software



== 3 == VSKIT132 Notes for Developer Board Users


Build.bat compiler script now creates 2 nand flash images: one for the ST NAND128W
chips and one for the SAMSUNG K9F2G chips (shipped with some developer boards). Choose
the right one.


Flashing instructions when you have RS-232 connection:

- Run the nandclr.c file by typing:  build nandclr
- Push RESET
- VS1000A detects empty nand flash and starts generating internal filesystem
- Put switch to "USB att." position
- Wait until VS1000A completes initializing the internal filesystem
- 16K RAM disk will appear to your computer (SCSI name "VLSIFATDISK")
- Put VS1000_A.RUN file to ramdisk
- IF your board has Samsung K9F2G chip, then put K9F2G08.IMG to ramdisk
- IF your board has ST NAND128W chip, then put NAND128W.IMG to ramdisk
  (do NOT put both files)
- Put switch to "USB det." position
- LED will blink
- Remove USB cable
Next time you start VS1000 will boot with the new code.


Flashing instructions when you don't have RS-232 connection:
- Remove "NAND Boot" jumper
- Put switch to "USB att." position
- Attach USB cable
- Push "PWR/PAUSE" to power up the developer board
- 16K RAM disk will appear to your computer (SCSI name "VLSIFATDISK")
- Put VS1000_A.RUN file to ramdisk
- IF your board has Samsung K9F2G chip, then put K9F2G08.IMG to ramdisk
- IF your board has ST NAND128W chip, then put NAND128W.IMG to ramdisk
  (do NOT put both files)
- Put switch to "USB det." position
- LED will blink
- Remove USB cable
- Put "NAND Boot" jumper back
Next time you start VS1000 will boot with the new code.



== 4 == INITIALIZING THE NAND FLASH CHIP ==

The C file nandinit.c now contains utility for initializing the nand flash chip.
This needs to be done for 1) a new nand flash chip or 2) to remove boot code from 
the nand flash chip.

If you run code from the emulator, you should remove boot code from nand flash, because 
they will affect each other and have side-effects.

Run nandinit.c by compiling and running it by typing:

C:\VSKIT\VS1000A> build nandinit

This will auto-detect the NAND FLASH chip and write correct signature if
you are using a ST NANDxxx or SAMSUNG nand chip. It also removes the boot area code,
but it does not touch the FAT data area.

Example of program output:


  VS1000 Nand Flash Init Utility
  Current VS1000 NandType Setting: 0   

  Now trying to auto-detect NAND chip...
  Nand Chip returns ID data: 20 73 20 73 20 73 20 73
  This is a NAND128 Chip
  Page Size (B): 512
  Erasable Block Size (KB): 16
  Data Plane Size (MB): 16

  Calculating new NandType setting
  Sectors per Block: 32 -> Bits: 5
  Data area sectors: 32768 -> Bits: 15

  Using NandType setting 0
  Reading First Sector
  Current Signature is 56 4c 53 49 00 00 05 0f 00 46 ff ff ff ff ff ff
  Completely clearing signature for nand flashing.
  New Signature is     ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  Erasing Boot Block
  Writing New Block
  Done.



Another utility program is nandfmt.c, with which you can erase the entire
data area, boot area or signature.





== 5 == Default display example application

The C file display.c contains the default display demo application. It's also pre-built
in the *.IMG files in the package. You can rebuild the demo by typing 

 build display


==END OF SECTION==

VLSI SOLUTION OY TAMPERE FINLAND 
AUDIO SOLUTION SUPPORT / Panu-Kristian Poiksalo
*******************************************************
  ABSOLUTELY NO WARRANTY
 VLSI Solution provides these files as an example only.
 Under no circumstances will VLSI guarantee the functio-
 nality of any of the files in this package.
*******************************************************

==END OF FILE==













