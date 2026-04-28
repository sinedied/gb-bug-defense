#ifndef GBTD_MAP_H
#define GBTD_MAP_H

#include "gtypes.h"

#define TC_GROUND   0
#define TC_PATH     1
#define TC_COMPUTER 2

#define WAYPOINT_COUNT 8

typedef struct { i8 tx, ty; } waypoint_t;

void map_load(void);                       /* render gameplay map onto BG;
                                            * caller MUST bracket DISPLAY_OFF */
void map_render(void);                     /* VBlank-safe deferred redraws */
u8   map_class_at(u8 tx, u8 ty);           /* play-field-local tile coords */
const waypoint_t *map_waypoints(void);
void map_set_computer_state(u8 hp);

#endif
