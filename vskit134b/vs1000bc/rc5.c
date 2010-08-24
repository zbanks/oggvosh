#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vs1000.h>
#include <audio.h>
#include <codec.h>
#include <player.h>

#include <dev1000.h>

extern struct CodecServices cs;

void MyUSBSuspend(u_int16 timeOut) {
    if (timeOut) {
	/* stay in 1.0x to receive the RC5 */
    } else {
	RealUSBSuspend(0);
    }
}

u_int16 oldRc5;
const struct KeyMapping rc5Keys[] = {
#define NO_REPEAT 0x8000U
    {NO_REPEAT|RC5_CMD_MUTE, ke_pauseToggle},
    {NO_REPEAT|RC5_CMD_DISPLAY, ke_randomToggle},
    {NO_REPEAT|RC5_CMD_PROGRAM_UP, ke_next},
    {NO_REPEAT|RC5_CMD_PROGRAM_DOWN, ke_previous},
    {RC5_CMD_VOLUME_UP,   ke_volumeUp2},
    {RC5_CMD_VOLUME_DOWN, ke_volumeDown2},
    {RC5_CMD_STANDBY,     ke_powerOff},
    {RC5_CMD_FAST_REVERSE, ke_rewind},
    {RC5_CMD_FAST_FORWARD, ke_forward}, /*TODO: rotate playspeed */
    {0,0}
};
void MyUserInterfaceIdleHook(void) {
    if (uiTrigger) {
	u_int16 key = Rc5GetFIFO();
	register u_int16 t = key;
	if (t && (t & 0x3c0) == RC5_SYS_TVSET) {
	    register struct KeyMapping *k = rc5Keys;
	    t &= 0x7ff;
#if 1
	    /* direct access to songs 0..9 */
	    if (t >= RC5_CMD_0 && t <= RC5_CMD_9 && key != oldRc5) {
		player.nextFile = t - RC5_CMD_0;
		player.pauseOn = 0;
		cs.cancel = 1;
	    }
#endif
	    while (k->event) {
		if (t == (k->key & 0x7ff)) {
		    if ((s_int16)k->key >= 0 || key != oldRc5) {
			KeyEventHandler(k->event);
		    }
		    break;
		}
		k++;
	    }
	    oldRc5 = key;
	}
	UserInterfaceIdleHook();
    }
}

void main(void) {
#if 0 /*Perform some extra inits because we are started from SPI boot. */
    InitAudio(); /* goto 3.0x..4.0x */
    PERIP(INT_ENABLEL) = INTF_RX | INTF_TIM0;
    PERIP(INT_ENABLEH) = INTF_DAC;
    //ph = FsPhNandCreate(0); /*We do not use FLASH, so no init for it. */
#endif

    /* Set the leds after nand-boot! */
    PERIP(GPIO1_ODATA) |=  LED1|LED2;      /* POWER led on */
    PERIP(GPIO1_DDR)   |=  LED1|LED2; /* SI and SO to outputs */
    PERIP(GPIO1_MODE)  &= ~(LED1|LED2); /* SI and SO to GPIO */

    /* Initialize RC5 reception */
    Rc5Init(INTV_GPIO0);
    PERIP(GPIO0_INT_FALL) |= (1<<14); /*DISPLAY_XCS*/
    PERIP(GPIO0_INT_RISE) |= (1<<14);
    PERIP(INT_ENABLEL) |= INTF_GPIO0;

    SetHookFunction((u_int16)IdleHook, MyUserInterfaceIdleHook);
    SetHookFunction((u_int16)USBSuspend, MyUSBSuspend);
#if 0
    while (1) {
	register u_int16 t = Rc5GetFIFO();
	if (t) {
	    puthex(t);
	    puts("=got");
	}
    }
#endif
#if 0
    while (1) {
	s_int16 tmp[32];
	memset(tmp, 0, sizeof(tmp));
	AudioOutputSamples(tmp, 16);
    }
#endif
}
