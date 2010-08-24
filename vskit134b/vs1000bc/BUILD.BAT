@echo off
if x%1==x goto noparams
if exist ..\bin\vcc.exe goto compiler_in_dotdot
if exist c:\vskit\bin\vcc.exe goto compiler_in_c_root
SET VSBIN=  
goto compile

:compiler_in_dotdot
SET VSBIN=..\bin\
goto compile

:compiler_in_c_root
SET VSBIN=c:\vskit\bin\
goto compile

:compile
echo Compiling %1.c ...
%VSBIN%vcc -P130 -O -fsmall-code -I lib -o a_out.o %1.c
IF ERRORLEVEL 1 GOTO vccfail

:link
echo Linking for all targets...
rem Linking for Nand Flash loading... (combining with libraries)
%VSBIN%vslink -k -m mem_user -L lib -lc -ldev1000 -o o_nand.bin lib/c-nand.o lib/rom1000.o a_out.o
IF ERRORLEVEL 1 GOTO linkfail

rem Linking for Emulator and SPI EEPROM loading... (combining with libraries)
%VSBIN%vslink -k -m mem_user -L lib -lc -ldev1000 -o o_emuspi.bin lib/c-spi.o lib/rom1000.o a_out.o
IF ERRORLEVEL 1 GOTO linkfail
echo   - Emulator runnable program is now in:     o_emuspi.bin

%VSBIN%coff2nandboot -t 0 -b 5 -s 15 -x 0x50 o_emuspi.bin %1.prg > null
echo   - Exec() -loadable file is in:             %1.prg

%VSBIN%coff2nandboot -n -x 0x50 o_emuspi.bin VS1000_B.RUN > null
echo   - "NO_FLASH" ramdisk-loadable file is in:  VS1000_B.RUN

%VSBIN%coff2nandboot -n -x 0x50 o_emuspi.bin %1.run > null
echo   -                         ...and also in:  %1.run

%VSBIN%vslink -k -m mem_user -L lib -lc -ldev1000 -o o_usbrpc.bin lib/c-usbl.o lib/rom1000.o a_out.o
%VSBIN%coff2nandboot -n -x 0x50 o_usbrpc.bin %1.rpc > null
echo   - VS1000D USB RPC callable file is in:     %1.rpc (max 512 Bytes)

rem echo Creating SPI EEPROM image (25LC640 etc)...
%VSBIN%coff2spiboot -x 0x50 o_emuspi.bin ee_%1.img > null
echo   - EEPROMmable image is now in:             %1_ee.img

rem default NAND FLASH parameters for 256 megabyte nand flash
%VSBIN%coff2nandboot -t 3 -b 8 -s 19 -x 0x50 o_nand.bin nf_%1.img > null
echo   - Nand Flash compatible image is in:       %1_nf.img

:load
echo -
echo ==OK== Compilation seems to be successful.
%VSBIN%vssym -Q o_emuspi.bin
if x%2==xNOLOAD goto :end
if exist SKIPEMU goto :end
echo Switch VS1000 on now. (You may need to turn vs1000 power off first)
echo Remember that you may need to clear code from nand flash before loading.
echo Hit [Ctrl-C] to quit this batch job or [Enter] to load code via emulator
pause >null
%VSBIN%vs3emu -chip vs1000 -s 115200 -l o_emuspi.bin e.cmd
goto end


:linkfail
echo The %1.c file was compiled but not linked. This probably means that there is something wrong with a call to the libarary functions or the library functions are not found.
goto end

:vccfail
echo The file %1.c was not compiled. Either the compiler is not found or there is an error in the file.
if exist %VSBIN%vcc.exe goto end
echo The compiler was not found, you can put vcc.exe into c:\vskit\bin\vcc.exe
goto end


:noparams
echo VLSI Script for building single-file projects in MS-DOS Prompt window
echo Usage:    BUILD sourcefileBaseName
echo Example:  If your source file name is hello.c, use the command
echo             BUILD hello         
goto end

:end
