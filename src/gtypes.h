#ifndef GBTD_TYPES_H
#define GBTD_TYPES_H

#include <gb/gb.h>
#include <stdint.h>
#include "tuning.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef int8_t   i8;
typedef int16_t  i16;
typedef i16      fix8;          /* 8.8 fixed point */

#define FIX8(n)        ((fix8)((i16)(n) << 8))
#define FIX8_INT(f)    ((i8)((f) >> 8))
#define FIX8_INTU(f)   ((u8)((f) >> 8))

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef bool
#define bool u8
#define true 1
#define false 0
#endif

/* Pool sizes, economy, enemy/tower/projectile/wave tuning all live in
 * tuning.h (host-compilable, shared with tests/test_math.c). */

/* OAM slot allocation (iter-3) — see memory/conventions.md "Iter-3 conventions".
 * Pause and the upgrade/sell menu are mutually exclusive (game.c enforces
 * single-modal-at-a-time) and both own the physical range 1..16. Iter-2
 * counted 14 here (slots 15..16 reserved unused); iter-3 #22 promoted them
 * into the menu/pause range. menu.c::hide_menu_oam now zeroes all 16
 * slots even though menu_render only paints cells 0..13. */
#define OAM_CURSOR        0
#define OAM_MENU_BASE     1   /* 1..16 — menu glyphs (only when menu open) */
#define OAM_MENU_COUNT   16
#define OAM_PAUSE_BASE    OAM_MENU_BASE   /* alias: pause shares OAM 1..16 */
#define OAM_PAUSE_COUNT   OAM_MENU_COUNT
#define OAM_ENEMIES_BASE 17   /* 17..30 (14 slots) */
#define OAM_PROJ_BASE    31   /* 31..38 (8 slots) — was 29..36 in MVP */
#define OAM_TOTAL        40

/* Play field origin in screen tile coords. HUD on row 0. */
#define HUD_ROWS         1
#define PF_COLS         20
#define PF_ROWS         17

/* Enemy / tower type enums (mirrored in enemies.c / towers.c stat tables). */
enum { ENEMY_BUG = 0, ENEMY_ROBOT = 1, ENEMY_ARMORED = 2, ENEMY_TYPE_COUNT };
enum { TOWER_AV  = 0, TOWER_FW    = 1, TOWER_EMP = 2, TOWER_TYPE_COUNT };

#endif
