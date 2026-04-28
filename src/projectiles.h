#ifndef GBTD_PROJECTILES_H
#define GBTD_PROJECTILES_H

#include "gtypes.h"

void projectiles_init(void);
void projectiles_update(void);
void projectiles_hide_all(void);   /* iter-2: hide OAM 31..38 (menu open) */

bool projectiles_fire(fix8 x, fix8 y, u8 target_idx, u8 damage);

#endif
