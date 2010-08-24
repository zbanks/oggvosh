
// Example on how to change the key mapping of the user interface

#include <vs1000.h>
#include <player.h>

// Define key masks for the buttons on the PCB. This order is
// of the Demonstration Board, leftmost button is "KEY_A"
#define KEY_A 0x0004
#define KEY_B 0x0008
#define KEY_C 0x0001
#define KEY_D 0x0002
#define KEY_E 0x0010

// Define custom key mapping
const struct KeyMapping myKeyMap[] = {
  {KEY_A,                            ke_volumeUp  }, // Key A: Volume step up
  {KEY_A | KEY_LONG_PRESS,           ke_volumeUp  }, // Key A: Volume up continuous
  {KEY_B,                            ke_volumeDown}, // Key B: Volume step dn
  {KEY_B | KEY_LONG_PRESS,           ke_volumeDown}, // Key B: Volume dn continuous
  {KEY_C,                            ke_previous  }, // Key C: Previous song
  {KEY_D,                            ke_next      }, // Key D: Next song
  {KEY_E | KEY_A | KEY_LONG_PRESS,   ke_rewind    }, // Key E with Key A: rewind
  {KEY_E | KEY_B | KEY_LONG_PRESS,   ke_forward   }, // Key E with Key B: fast forward
  {KEY_C | KEY_D | KEY_LONG_ONESHOT, ke_powerOff  }, // Only one event after long press
  {0, ke_null} // End of key mappings
};

// Load own key mapping
void main(){
  currentKeyMap = myKeyMap; // Use own key mapping

  // Note that if there is boot record in NAND, it's run after 
  // this point, if this code is run from the emulator
}



