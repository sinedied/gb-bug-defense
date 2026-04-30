#ifndef GBTD_ENEMIES_H
#define GBTD_ENEMIES_H

#include "gtypes.h"

void enemies_init(void);
void enemies_update(void);
void enemies_hide_all(void);          /* iter-2: hide OAM 17..30 (menu open) */

bool enemies_spawn(u8 type, u8 wave_1based); /* iter-7: wave param for HP scaling */
bool enemies_spawn_boss(u8 wave_1based);     /* iter-7: spawn boss for given wave */
u8   enemies_count(void);             /* alive count */

bool enemies_alive(u8 idx);
u8   enemies_x_px(u8 idx);
u8   enemies_y_px(u8 idx);
u8   enemies_wp_idx(u8 idx);
u8   enemies_gen(u8 idx);
u8   enemies_bounty(u8 idx);          /* iter-2: bounty for the alive enemy */
u8   enemies_type(u8 idx);            /* iter-3 #19: type for score_add_kill */
bool enemies_is_boss(u8 idx);         /* iter-7: read-only boss flag accessor */

/* Damage callback used by projectiles. Returns true if the hit killed it. */
bool enemies_apply_damage(u8 idx, u8 dmg);

/* Iter-3 #21: arm 3-frame hit-flash sprite override on a non-killing hit. */
void enemies_set_flash(u8 idx);

/* Iter-3 #18: stun API — enemy_t stays private to enemies.c.
 * enemies_try_stun: returns true and sets stun_timer iff alive and not
 *   already stunned (stun_timer == 0). False otherwise (no-stack).
 * enemies_is_stunned: read-only accessor for host tests. */
bool enemies_try_stun(u8 idx, u8 frames);
bool enemies_is_stunned(u8 idx);

#endif
