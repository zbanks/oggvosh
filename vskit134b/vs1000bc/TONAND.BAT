call build.bat %1 NOLOAD
copy nand128w.img nand.img
call build.bat nandprog
