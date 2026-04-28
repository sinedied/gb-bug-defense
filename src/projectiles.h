#ifndef GBTD_PROJECTILES_H
#define GBTD_PROJECTILES_H

#include "gtypes.h"

void projectiles_init(void);
void projectiles_update(void);

bool projectiles_fire(fix8 x, fix8 y, u8 target_idx);

#endif
