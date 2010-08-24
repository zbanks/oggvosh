
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vs1000.h>
#include <audio.h>
#include <mappertiny.h>
#include <minifat.h>
#include "player.h"
#include "codec.h"
#include "romfont.h"
#include "usb.h"

// Boot
void main(void) {

  // These are the default voltage regulator settings.
  // NOTE THAT ANALOG, CORE AND IO VOLTAGES HAVE DIFFERENT
  // SCALES AND THAT YOU CAN SET NON-FUNCTIONAL OR DAMAGING
  // VALUES. Experiment carefully.

  // If GPIO0_7 is high during boot, IOVDD's will be set to 3.3V.

  // voltages when playing Vorbis
  voltages[voltCorePlayer] = 22;  // 2.2 V
  voltages[voltIoPlayer]   = 0;   // 1.8 V
  voltages[voltAnaPlayer]  = 8;   // 2.8 V
  
  // voltages when USB is attached and active
  voltages[voltCoreUSB] = 27;     // 2.5 V
  voltages[voltIoUSB]   = 0;      // 1.8 V
  voltages[voltAnaUSB]  = 30;     // 3.6 V
  
  // voltages when USB is suspended
  voltages[voltCoreSuspend] = 14; // 1.8 V
  voltages[voltIoSuspend]   = 0;  // 1.8 V
  voltages[voltAnaSuspend]  = 30; // 3.6 V
  
  // User settings can be activated by user
  voltages[voltCoreUser] = 22;    // 2.2V
  voltages[voltIoUser]   = 0;     // 1.8 V
  voltages[voltAnaUser]  = 8;     // 2.8 V

  PowerSetVoltages(voltages);     // Load player voltages

}


