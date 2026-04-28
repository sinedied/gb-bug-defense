#ifndef GBTD_GAME_H
#define GBTD_GAME_H

#include "gtypes.h"
#include "difficulty_calc.h"

void game_init(void);
void game_update(void);
void game_render(void);   /* VBlank-safe BG writes only */

u8   game_get_selected_tower_type(void);   /* iter-2: TOWER_AV | TOWER_FW */

/* Iter-3 #22: true iff a modal overlay (upgrade/sell menu OR pause) is
 * open. Add new modals here and update playing_update accordingly. */
bool game_is_modal_open(void);

/* Iter-3 #21: monotonically-increasing global animation frame counter.
 * Single source of truth for cross-module animation phase (tower idle
 * blink today; future ambient anims). Wraps at 256. Reset to 0 on
 * enter_playing(). Incremented once per game_update(). */
u8   game_anim_frame(void);

/* Iter-3 #20: difficulty (EASY / NORMAL / HARD). Persists across
 * enter_title()/enter_playing() within a power-on session; SRAM is
 * feature #19. Setter is a no-op for d >= DIFF_COUNT. */
u8   game_difficulty(void);
void game_set_difficulty(u8 d);

#endif
