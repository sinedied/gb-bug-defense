#ifndef GBTD_SAVE_H
#define GBTD_SAVE_H

/* Iter-3 #19 — battery-backed SRAM save (high scores).
 *
 * Single-owner cache: `static u16 s_hi[9]` lives ONLY in `save.c`.
 * `score.c` does NOT keep a local copy. UI sites (title, gameover)
 * read via `save_get_hi(map, diff)`. Save writes happen ONLY at
 * `enter_gameover` — never per-frame, never mid-game.
 *
 * SRAM access protocol (every transaction):
 *   ENABLE_RAM; SWITCH_RAM(0); read/write _SRAM[i]; DISABLE_RAM;
 * `DISABLE_RAM` is always called before returning, even on read paths,
 * to minimise the power-loss corruption window. */

#include "gtypes.h"

void save_init(void);                          /* boot: validate magic, hydrate cache OR zero+stamp */
void save_load_highscores(void);               /* refresh cache from SRAM (used internally too) */
u16  save_get_hi(u8 map_id, u8 diff);          /* cache read; caller validates ranges */
void save_write_hi(u8 map_id, u8 diff, u16 v); /* cache + SRAM write (single u16 slot) */

#endif /* GBTD_SAVE_H */
