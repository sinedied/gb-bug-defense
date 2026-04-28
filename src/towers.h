#ifndef GBTD_TOWERS_H
#define GBTD_TOWERS_H

#include "gtypes.h"

typedef struct {
    u8  cost;
    u8  upgrade_cost;
    u8  cooldown;       /* level 0 */
    u8  cooldown_l1;
    u8  damage;
    u8  damage_l1;
    u8  range_px;       /* level-independent */
    u8  bg_tile;        /* TILE_TOWER or TILE_TOWER_2 */
    u8  hud_letter;     /* 'A' or 'F' */
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
bool towers_can_upgrade(u8 idx);         /* level==0 && energy >= upgrade_cost */
bool towers_upgrade(u8 idx);             /* spends, sets level=1 */
void towers_sell(u8 idx);                /* refunds spent/2, schedules tile clear */

/* Screen-pixel top-left of the tower's tile (for menu anchor). */
u8   towers_tile_screen_x(u8 idx);
u8   towers_tile_screen_y(u8 idx);

#endif
