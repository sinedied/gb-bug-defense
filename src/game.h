#ifndef GBTD_GAME_H
#define GBTD_GAME_H

#include "gtypes.h"

void game_init(void);
void game_update(void);
void game_render(void);   /* VBlank-safe BG writes only */

u8   game_get_selected_tower_type(void);   /* iter-2: TOWER_AV | TOWER_FW */

#endif
