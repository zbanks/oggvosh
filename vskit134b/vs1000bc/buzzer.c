
#include "vs1000.h"

void main(void) {

    // Play strange tone

    register u_int16 *g = (u_int16*) g_yprev0; // get ptr to sintest params
    *g++ = 4; *g++ = 44100; *g++ = 0x5050; *g++ = 120; *g++ = 200;
    SinTest(); //Exit to SinTest: Play weird tone at low volume
}


