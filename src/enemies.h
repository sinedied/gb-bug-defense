#ifndef GBTD_ENEMIES_H
#define GBTD_ENEMIES_H

#include "gtypes.h"

void enemies_init(void);
void enemies_update(void);
void enemies_hide_all(void);          /* iter-2: hide OAM 17..30 (menu open) */

bool enemies_spawn(u8 type);          /* iter-2: type-parameterised; false if pool full */
u8   enemies_count(void);             /* alive count */

bool enemies_alive(u8 idx);
u8   enemies_x_px(u8 idx);
u8   enemies_y_px(u8 idx);
u8   enemies_wp_idx(u8 idx);
u8   enemies_gen(u8 idx);
u8   enemies_bounty(u8 idx);          /* iter-2: bounty for the alive enemy */

/* Damage callback used by projectiles. Returns true if the hit killed it. */
bool enemies_apply_damage(u8 idx, u8 dmg);

#endif
