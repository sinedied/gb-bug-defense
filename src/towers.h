#ifndef GBTD_TOWERS_H
#define GBTD_TOWERS_H

#include "gtypes.h"

/* Iter-3 #18: kind discriminator for tower behavior dispatch. */
enum { TKIND_DAMAGE = 0, TKIND_STUN = 1 };

typedef struct {
    u8  cost;
    u8  upgrade_cost;
    u8  upgrade_cost_l2;    /* L1→L2 cost (iter-4 #27) */
    u8  cooldown;           /* level 0 */
    u8  cooldown_l1;
    u8  cooldown_l2;        /* level 2 (iter-4 #27) */
    u8  damage;             /* only for TKIND_DAMAGE */
    u8  damage_l1;          /* only for TKIND_DAMAGE */
    u8  damage_l2;          /* only for TKIND_DAMAGE (iter-4 #27) */
    u8  range_px;           /* level-independent */
    u8  bg_tile;
    u8  bg_tile_alt;
    u8  bg_tile_l1;
    u8  bg_tile_alt_l1;
    u8  bg_tile_l2;         /* L2 BG tile (iter-4 #27) */
    u8  bg_tile_alt_l2;     /* L2 idle-blink tile (iter-4 #27) */
    u8  hud_letter;
    u8  kind;               /* TKIND_DAMAGE or TKIND_STUN */
    u8  stun_frames;        /* only for TKIND_STUN — level 0 */
    u8  stun_frames_l1;     /* only for TKIND_STUN — level 1 */
    u8  stun_frames_l2;     /* only for TKIND_STUN — level 2 (iter-4 #27) */
} tower_stats_t;

void towers_init(void);
void towers_update(void);
void towers_render(void);                /* VBlank-safe BG tile writes */

bool towers_try_place(u8 tx, u8 ty, u8 type);  /* play-field-local */
bool towers_at(u8 tx, u8 ty);            /* occupancy check */
i8   towers_index_at(u8 tx, u8 ty);      /* -1 if none */

const tower_stats_t *towers_stats(u8 type);
u8   towers_get_type(u8 idx);
u8   towers_get_level(u8 idx);
u8   towers_get_spent(u8 idx);
bool towers_can_upgrade(u8 idx);         /* level<2 && energy >= next upgrade cost */
bool towers_upgrade(u8 idx);             /* spends, increments level */
void towers_sell(u8 idx);                /* refunds spent/2, schedules tile clear */

/* Screen-pixel top-left of the tower's tile (for menu anchor). */
u8   towers_tile_screen_x(u8 idx);
u8   towers_tile_screen_y(u8 idx);

#endif
