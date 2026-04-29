#ifndef GBTD_WAVES_H
#define GBTD_WAVES_H

#include "gtypes.h"

void waves_init(void);
void waves_update(void);

u8   waves_get_current(void);    /* 1..MAX_WAVES for HUD */
u8   waves_get_total(void);      /* MAX_WAVES */
bool waves_all_cleared(void);

/* Iter-3 #19: one-shot edge — returns the wave number (1..MAX_WAVES)
 * that just cleared (last spawn of that wave succeeded), or 0 if no
 * edge this frame. Cleared on read. Polled from game.c::playing_update
 * and forwarded to score_add_wave_clear(n). */
u8   waves_just_cleared_wave(void);

#endif
