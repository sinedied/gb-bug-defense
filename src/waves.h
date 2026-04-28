#ifndef GBTD_WAVES_H
#define GBTD_WAVES_H

#include "gtypes.h"

void waves_init(void);
void waves_update(void);

u8   waves_get_current(void);    /* 1..MAX_WAVES for HUD */
u8   waves_get_total(void);      /* MAX_WAVES */
bool waves_all_cleared(void);

#endif
