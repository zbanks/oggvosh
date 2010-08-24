
#include <vs1000.h>
void UserInterfaceIdleHook(void);
/// Busy wait i hundreths of second at 12 MHz clock
auto void BusyWaitHundreths(u_int16 i) {
  while(i--){
    BusyWait10(); // Rom function, busy loop 10ms at 12MHz
  }
}

void main(void) {

  PERIP(GPIO1_MODE) = 0x30; /* UART=peripheral(1) , SPI=GPIO(0) */
  PERIP(GPIO1_DDR) = 0x0c;  /* SI and SO pins (GPIO1[3:2]) are output(1) */
  //SetHookFunction((u_int16)IdleHook, UserInterfaceIdleHook);  
  //SetHookFunction((u_int16)PlayCurrentFile, RealPlayCurrentFile);
  while(1){    
    PERIP(GPIO1_ODATA) = 0x04; /* GPIO1[2] (LQFP pin 24) = 1 */
    BusyWaitHundreths(50);
    PERIP(GPIO1_ODATA) = 0x08; /* GPIO1[3] (LQFP pin 25) = 1 */
    BusyWaitHundreths(50);
  }
}
