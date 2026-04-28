#ifndef GBTD_HUD_H
#define GBTD_HUD_H

#include "gtypes.h"

void hud_init(void);
void hud_update(void);

void hud_mark_hp_dirty(void);
void hud_mark_e_dirty(void);
void hud_mark_w_dirty(void);
void hud_mark_t_dirty(void);   /* iter-2: tower-select indicator (col 19) */

#endif
