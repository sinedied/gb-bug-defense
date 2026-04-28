#ifndef GBTD_WAVES_H
#define GBTD_WAVES_H

#include "gtypes.h"

void waves_init(void);
void waves_update(void);

u8   waves_get_current(void);    /* 1..3 for HUD */
bool waves_all_cleared(void);

#endif
