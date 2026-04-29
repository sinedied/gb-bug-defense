#ifndef GBTD_MAP_H
#define GBTD_MAP_H

#include "gtypes.h"

#define TC_GROUND   0
#define TC_PATH     1
#define TC_COMPUTER 2

/* Iter-3 #17: map registry. WAYPOINT_COUNT is removed — readers MUST
 * call map_waypoint_count() (Map 2 has 10 waypoints; the old
 * compile-time constant of 8 would mis-despawn enemies mid-path). */
#define MAP_COUNT       3
#define MAX_WAYPOINTS  10

typedef struct { i8 tx, ty; } waypoint_t;

typedef struct {
    const u8         *tilemap;     /* PF_COLS*PF_ROWS = 340 bytes */
    const u8         *classmap;    /* PF_COLS*PF_ROWS = 340 bytes */
    const waypoint_t *waypoints;
    u8                waypoint_count;
    u8                computer_tx;  /* play-field-local TL of 2x2 cluster */
    u8                computer_ty;
} map_def_t;

/* Iter-3 #17: caller MUST bracket DISPLAY_OFF — writes ~340 BG tiles.
 * `id` is clamped to < MAP_COUNT; map.c does NOT include game.h
 * (decisions D8). Also resets the iter-3 #21 corruption tracking. */
void              map_load(u8 id);
void              map_render(void);          /* VBlank-safe deferred redraws */
u8                map_class_at(u8 tx, u8 ty);
u8                map_tile_at(u8 tx, u8 ty);  /* iter-4 #24: read original tilemap tile */
const waypoint_t *map_waypoints(void);
u8                map_waypoint_count(void);
u8                map_active(void);
void              map_set_computer_state(u8 hp);

#endif

