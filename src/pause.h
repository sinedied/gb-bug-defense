#ifndef GBTD_PAUSE_H
#define GBTD_PAUSE_H

#include "gtypes.h"

/* Iter-3 #22: pause overlay (modal sprite-overlay menu).
 *
 * Pause is a flag inside the GS_PLAYING state, NOT a top-level state.
 * Mutually exclusive with the upgrade/sell menu (menu.c) — both share
 * OAM 1..16. game.c::playing_update is the single source of truth that
 * decides which modal handler runs each frame.
 *
 * pause_quit_requested() is a one-shot flag: caller (game.c) reads it,
 * and on `true` performs `pause_close(); audio_reset(); enter_title();`.
 * The flag self-clears on read so a single A-press on QUIT triggers
 * exactly one transition. */

void pause_init(void);
bool pause_is_open(void);
void pause_open(void);    /* defensive no-op if menu_is_open() */
void pause_close(void);

void pause_update(void);  /* called from game.c when paused */
void pause_render(void);  /* sprite OAM only — VBlank-safe */

bool pause_quit_requested(void);

#endif
