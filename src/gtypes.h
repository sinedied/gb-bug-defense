#ifndef GBTD_TYPES_H
#define GBTD_TYPES_H

#include <gb/gb.h>
#include <stdint.h>

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

/* Pool sizes (authoritative — also in memory/conventions.md) */
#define MAX_ENEMIES     12
#define MAX_PROJECTILES  8
#define MAX_TOWERS      16

/* OAM slot allocation */
#define OAM_CURSOR        0
#define OAM_TOWERS_BASE   1   /* 1..16 */
#define OAM_ENEMIES_BASE 17   /* 17..28 */
#define OAM_PROJ_BASE    29   /* 29..36 */
#define OAM_TOTAL        40

/* Play field origin in screen tile coords. HUD on row 0. */
#define HUD_ROWS         1
#define PF_COLS         20
#define PF_ROWS         17

/* Tuning constants */
#define START_HP         5
#define START_ENERGY    30
#define TOWER_COST      10
#define KILL_BOUNTY      3
#define MAX_ENERGY     255

#define BUG_HP           3
#define BUG_SPEED       0x0080  /* 0.5 px/frame */

#define TOWER_RANGE_PX  24
#define TOWER_RANGE_SQ  (TOWER_RANGE_PX * TOWER_RANGE_PX)
#define TOWER_COOLDOWN  60
#define TOWER_DAMAGE     1

#define PROJ_SPEED      0x0200  /* 2 px/frame */
#define PROJ_HIT_PX      4
#define PROJ_HIT_SQ     (PROJ_HIT_PX * PROJ_HIT_PX)
#define PROJ_DAMAGE      1

#define INTER_WAVE_DELAY 180
#define FIRST_GRACE       60

#endif
