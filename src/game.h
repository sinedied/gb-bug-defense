#ifndef GBTD_GAME_H
#define GBTD_GAME_H

#include "gtypes.h"

void game_init(void);
void game_update(void);
void game_render(void);   /* VBlank-safe BG writes only */

#endif
