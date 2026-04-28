#ifndef GBTD_INPUT_H
#define GBTD_INPUT_H

#include "gtypes.h"

void input_init(void);
void input_poll(void);

bool input_is_held(u8 mask);     /* J_UP, J_A, ... */
bool input_is_pressed(u8 mask);  /* edge: pressed-this-frame */
bool input_is_repeat(u8 mask);   /* edge OR auto-repeat tick (for D-pad) */

#endif
