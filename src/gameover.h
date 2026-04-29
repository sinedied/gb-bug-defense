#ifndef GBTD_GAMEOVER_H
#define GBTD_GAMEOVER_H

#include "gtypes.h"

/* Iter-3 #19: signature extended to carry final score and "is record"
 * banner flag. Both are painted statically inside DISPLAY_OFF; only
 * the existing PRESS START blink animates. The caller (game.c::
 * enter_gameover) commits the record to SRAM BEFORE invoking, so this
 * module is purely a renderer. */
void gameover_enter(bool win, u16 final_score, bool new_hi);
void gameover_update(void);
void gameover_render(void);

#endif
