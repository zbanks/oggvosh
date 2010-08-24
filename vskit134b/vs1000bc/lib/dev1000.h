/**
   \file dev1000.h Development library header file.
*/
#ifndef __DEV1000_H__
#define __DEV1000_H__

#include <vstypes.h>

#define PATCH_TEST_UNIT_READY /*PatchMSCPacketFromPC() calls ScsiTestUnitReady()*/
#define MMC_MISO_BIT 8  /*NFRDY with 10kOhm pull-up*/
#define MMC_MOSI_BIT 12 /*NFCLE*/
#define MMC_CLK      (1<<9)  /*NFRD*/
#define MMC_MOSI     (1<<MMC_MOSI_BIT)
#define MMC_MISO     (1<<MMC_MISO_BIT)
#define MMC_XCS      (1<<11) /*NFWR*/

#define MMC_GO_IDLE_STATE   0
#define MMC_SEND_OP_COND   1
#define MMC_SEND_IF_COND   8
#define MMC_SEND_CSD   9
#define MMC_SEND_CID   10
#define MMC_SEND_STATUS   13
#define MMC_SET_BLOCKLEN   16
#define MMC_READ_SINGLE_BLOCK   17
#define MMC_WRITE_BLOCK   24
#define MMC_PROGRAM_CSD   27
#define MMC_SET_WRITE_PROT   28
#define MMC_CLR_WRITE_PROT   29
#define MMC_SEND_WRITE_PROT   30
#define MMC_TAG_SECTOR_START   32
#define MMC_TAG_SECTOR_END   33
#define MMC_UNTAG_SECTOR   34
#define MMC_TAG_ERASE_GROUP_START   35
#define MMC_TAG_ERARE_GROUP_END   36
#define MMC_UNTAG_ERASE_GROUP   37
#define MMC_ERASE   38
#define MMC_READ_OCR     58
#define MMC_CRC_ON_OFF   59
#define MMC_R1_BUSY   0x80
#define MMC_R1_PARAMETER   0x40
#define MMC_R1_ADDRESS   0x20
#define MMC_R1_ERASE_SEQ   0x10
#define MMC_R1_COM_CRC   0x08
#define MMC_R1_ILLEGAL_COM   0x04
#define MMC_R1_ERASE_RESET   0x02
#define MMC_R1_IDLE_STATE   0x01
#define MMC_STARTBLOCK_READ   0xFE
#define MMC_STARTBLOCK_WRITE   0xFE
#define MMC_STARTBLOCK_MWRITE   0xFC
#define MMC_STOPTRAN_WRITE   0xFD
#define MMC_DE_MASK   0x1F
#define MMC_DE_ERROR   0x01
#define MMC_DE_CC_ERROR   0x02
#define MMC_DE_ECC_FAIL   0x04
#define MMC_DE_OUT_OF_RANGE   0x04
#define MMC_DE_CARD_LOCKED   0x04
#define MMC_DR_MASK   0x1F
#define MMC_DR_ACCEPT   0x05
#define MMC_DR_REJECT_CRC   0x0B
#define MMC_DR_REJECT_WRITE_ERROR   0x0D

#ifdef ASM

#else /*elseASM*/
/** Send and receive bits from MMC/SD.
    \param dataTopAligned The first bit to send must be in the most-significant bit.
    \param bits The number of bits to send/receive.
    \return If less than 16 bits are sent, the high bits of the result
            will be the low bits of the dataTopAligned parameter.
 */
auto u_int16 SpiSendReceiveMmc(register __a0 u_int16 dataTopAligned,
			       register __a1 bits);
/** Send MMC clocks with XCS high.
 */
auto void SpiSendClocks(void);
/** Send MMC/SD command. The CRC that is sent will always be 0x95.
    \param cmd The MMC command, must include the start bit (0x40).
    \param arg The command argument.
    \return the result code or 0xff (8th bit set) for timeout.
 */
auto u_int16 MmcCommand(register __b0 s_int16 cmd, register __d u_int32 arg);

/** LBAB patch -- removes 4G restriction from USB (SCSI).
    Is not needed for VS1000d, but is compatible with it. */
void PatchMSCPacketFromPC(void *); /* Set MSCPacketFromPC hook here. */
/** Hook called by PatchMSCPacketFromPC(). */
void ScsiTestUnitReady(void);

/** Replacement for FatOpenFile that is normally in OpenFile hook.
    This version disables subdirectories for FAT12 partitions so
    OpenFile will not go into infinite recursion trying to handle it.
    Is not needed for VS1000d, but is compatible with it.

    SetHookFunction((u_int16)OpenFile, Fat12OpenFile);
    \param fileNum the same as for OpenFile
    \return the same as OpenFile
*/
auto u_int16 Fat12OpenFile(register __c0 u_int16 fileNum);
/** Replacement for FatOpenFile that is normally in OpenFile hook.
    This version speeds up directory processing by detecting end of
    directory instead of reading to the end of the cluster.
    This is significant when using a large card with a large
    sectors-per-cluster value and there are a lot of directories.

    The code also disables subdirectories for FAT12 partitions so
    OpenFile will not go into infinite recursion trying to handle it.
    Is compatible with VS1000d.

    SetHookFunction((u_int16)OpenFile, FatFastOpenFile);
    \param fileNum the same as for OpenFile
    \return the same as OpenFile
*/
auto u_int16 FatFastOpenFile(register __c0 u_int16 fileNum);

/** Open a file based on the name. The suffix is not changed, the caller
    must select the right suffix and restore the original after the call.
    See also the faster OpenFileNamed().
    \param packedName The 8-character name must be in upper case and in packed format.
    \return The file index or 0xffffU if file is not found.
*/
auto u_int16 OpenFileBaseName(register __i2 const u_int16 *packedName);

/** Goes through files with the active suffix, calls IterateFilesCallBack()
    for all files. */
auto void IterateFiles(void);
/** Callback function for IterateFiles(). The parameter points to the start
    of the packed file name (in minifatBuffer). Other FAT fields follow the
    name, so they can be used as well. You can abort scan by setting
    minifatInfo.gFileNum[0] to zero.
*/
void IterateFilesCallback(register __b0 u_int16 *name);
/** A faster named file open routine. Note that only the short filename
    is checked. The FAT supportedSuffixes is set to supportedFiles afterwards.
    \param fname The 8-character name must be in upper case and in packed format.
    \param suffix The desired suffix, for example FAT_MKID('O','G','G')
    \return The file index or 0xffffU if file is not found.
*/
auto s_int16 OpenFileNamed(const u_int16 *fname, u_int32 suffix);


/** Sets the start and end positions for the PlayRange() function.
    \param start Play start time in 1/65536th of a second resolution.
    \param end Play end time in 1/65536th of a second resolution. Set to 0x7fffffffUL to disable end time.
*/
void PlayRangeSet(u_int32 start, u_int32 end);
/** Plays from a previously set start position to end position.
    Handles player and cs structure initialization.
    You must call PlayRangeSet() before calling PlayRange().
*/
void PlayRange(void);
/** For debugging with vs3emu: print 4-digit hex number. */
void puthex(u_int16 d);

/** Replacement routine for KeyScan() to read GPIO[5:0] and power button
    instead of just GPIO[4:0] and power button. See also Suspend7(). */
void KeyScan7(void);
#define KEY_6 (1<<5)
/** Replacement routine for USBSuspend() to wake up from GPIO[5:0] and
    power button (and USB pins) instead of just GPIO[4:0] and power button
    (and USB pins). Also puts the analog to powerdown for the duration
    of USB suspend/low-power pause. See also KeyScan7().
 */
void Suspend7(void);
/** Replacement routine for KeyScan() to read matrix keyboard connected
    to GPIO[7:4] and GPIO[3:0] and power button instead of just
    GPIO[4:0] and power button. See also SuspendMatrix(). */
void KeyScanMatrix(register __i2 const u_int16 *matrix);
/** Replacement routine for USBSuspend() to wake up from a matrix keyboard
    connected to GPIO[3:0] and GPIO[7:4] and power button (and USB pins)
    instead of just GPIO[4:0] and power button
    (and USB pins). Also puts the analog to powerdown for the duration
    of USB suspend/low-power pause. See also KeyScanMatrix().
 */
void SuspendMatrix(void);

auto u_int16 MapperlessReadDiskSector(register __i0 u_int16 *buffer,
				      register __reg_a u_int32 sector);


/*
  Some low-level NAND-interface routines that are useful with parallel
  displays. You must handle CS yourself.
 */
void NandPutCommand(register __a0 u_int16 command); /**< Writes 8 bits with CLE=1. You must handle NFCS yourself. */
void NandPutAddressOctet(register __a0 u_int16 address); /**< writes 8 bits with ALE=1. You must handle NFCS yourself. */
void NandGetOctets(register __c0 s_int16 length, register __i2 u_int16 *buf);/**< Writes packed data to NFIO. You must handle NFCS yourself. */
void NandPutOctets(register __c0 s_int16 length, register __i2 u_int16 *buf);/**< Reads packed data from NFIO. You must handle NFCS yourself. */
void NandSetWaits(register __a0 u_int16 waitns); /**< Sets NAND-interface waitstates in nanoseconds. Uses current clockX value for calculation. */

u_int32 ReadIRam(register __i0 u_int16 addr); /**< Reads instruction RAM. */
void WriteIRam(register __i0 u_int16 addr, register __a u_int32 ins); /**< Writes instruction RAM. */

/**
  Four interrupt stubs that the programmer can use without using ASM.
  For example to set up the first interrupt stub to GPIO0 interrupt,
  use the following:
  \code
    WriteIRam(0x20+INTV_GPIO0, ReadIRam((u_int16)InterruptStub0));
  \endcode

  Then you can enable GPIO0 interrupt and it will call your routine
  Interrupt0() whenever GPIO0 interrupt request is generated.
  \code
    PERIP(GPIO0_INT_FALL) |= DISPLAY_XCS;
    PERIP(INT_ENABLEL) |= INTF_GPIO0;
  \endcode
 */
void InterruptStub0(void);
void InterruptStub1(void); /**< Stub function that can be plugged to any interrupt vector. Calls Interrupt1(), which must be provided by the user. */
void InterruptStub2(void); /**< Stub function that can be plugged to any interrupt vector. Calls Interrupt2(), which must be provided by the user. */
void InterruptStub3(void); /**< Stub function that can be plugged to any interrupt vector. Calls Interrupt3(), which must be provided by the user. */
auto void Interrupt0(void); /**< Called by InterruptStub0. */
auto void Interrupt1(void); /**< Called by InterruptStub1. */
auto void Interrupt2(void); /**< Called by InterruptStub2. */
auto void Interrupt3(void); /**< Called by InterruptStub3. */


/** Initializes RC5 structure and installs interrupt handler.
    RC5 receiver uses one of the interruptable GPIO pins as input.
    Interrupt must be generated on both edges. The polarity of the
    receiver does not matter. Currently receives only codes that have
    two start bits.
    \code
    Rc5Init(INTV_GPIO0);
    PERIP(GPIO0_INT_FALL) |= (1<<14);
    PERIP(GPIO0_INT_RISE) |= (1<<14);
    PERIP(INT_ENABLEL) |= INTF_GPIO0;
    while (1) {
	register u_int16 t = Rc5GetFIFO();
	if (t) {
	    puthex(t);
	    puts("=got");
	}
    }
    \endcode
    See also example code rc5.c .
 */
void Rc5Init(u_int16 vector);
u_int16 Rc5GetFIFO(void); /**< Returns 0 if no values are available, received 14-bit value otherwise. */

#define RC5_SYS_TVSET          0
#define RC5_SYS_TELETEXT       2
#define RC5_SYS_VCR            5
#define RC5_SYS_EXPERIMENTAL7  7
#define RC5_SYS_PREAMP        16
#define RC5_SYS_RECEIVERTUNER 17
#define RC5_SYS_TAPERECORDER  18
#define RC5_SYS_EXPERIMENTAL19 19
#define RC5_SYS_CDPLAYER      20

#define RC5_CMD_0  0
#define RC5_CMD_1  1
#define RC5_CMD_2  2
#define RC5_CMD_3  3
#define RC5_CMD_4  4
#define RC5_CMD_5  5
#define RC5_CMD_6  6
#define RC5_CMD_7  7
#define RC5_CMD_8  8
#define RC5_CMD_9  9
#define RC5_CMD_ONETWODIGITS  10
#define RC5_CMD_STANDBY       12
#define RC5_CMD_MUTE          13
#define RC5_CMD_PRESET        14
#define RC5_CMD_DISPLAY       15
#define RC5_CMD_VOLUME_UP     16
#define RC5_CMD_VOLUME_DOWN   17
#define RC5_CMD_BRIGHT_UP     18
#define RC5_CMD_BRIGHT_DOWN   19
#define RC5_CMD_COLOR_UP      20
#define RC5_CMD_COLOR_DOWN    21
#define RC5_CMD_BASS_UP       22
#define RC5_CMD_BASS_DOWN     23
#define RC5_CMD_TREBLE_UP     24
#define RC5_CMD_TREBLE_DOWN   25
#define RC5_CMD_BALANCE_LEFT  26
#define RC5_CMD_BALANCE_RIGHT 27
#define RC5_CMD_CONTRAST_UP   28
#define RC5_CMD_CONTRAST_DOWN 29
#define RC5_CMD_PROGRAM_UP    32
#define RC5_CMD_PROGRAM_DOWN  33
#define RC5_CMD_ALTERNATE     34 /*P<P swap last channel */
#define RC5_CMD_LANGUAGE      35
#define RC5_CMD_EXPAND        36
#define RC5_CMD_TIMER         38
#define RC5_CMD_STORE         41
#define RC5_CMD_CLOCK         42
#define RC5_CMD_FINETUNE_PLUS 43  /* expand text-tv */
#define RC5_CMD_FINETUNE_MINUS 44 /* ? text-tv */
#define RC5_CMD_CROSS         45 /* X text-tv */
#define RC5_CMD_MIX           46 /* mix text-tv */
#define RC5_CMD_PAUSE         48
#define RC5_CMD_FAST_REVERSE  50
#define RC5_CMD_FAST_FORWARD  52
#define RC5_CMD_PLAY          53
#define RC5_CMD_STOP          54
#define RC5_CMD_RECORD        55
#define RC5_CMD_SWITCH        56 /*swap screens*/
#define RC5_CMD_MENU          59
#define RC5_CMD_TEXT          60
#define RC5_CMD_SYSTEM_SELECT 63

//Replacement for MSCPacketFromPC();
void PatchDiskProtocolCommand(/*USBPacket **/ void *inPacket);
auto u_int16 NewDiskProtocolCommand(register __i2 u_int16 *cmd);

#endif/*!ASM*/
#endif /*__DEV1000_H__*/
