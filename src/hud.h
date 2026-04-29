#ifndef GBTD_HUD_H
#define GBTD_HUD_H

#include "gtypes.h"

void hud_init(void);
void hud_update(void);

void hud_mark_hp_dirty(void);
void hud_mark_e_dirty(void);
void hud_mark_w_dirty(void);
void hud_mark_t_dirty(void);   /* iter-2: tower-select indicator (col 19) */
void hud_set_fast_mode(bool on);  /* iter-4 #30: ">>" indicator at cols 18-19 */

#endif
