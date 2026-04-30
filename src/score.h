#ifndef GBTD_SCORE_H
#define GBTD_SCORE_H

/* Iter-3 #19 — per-run score accumulator.
 *
 * No local high-score cache: `score.c` reads via `save_get_hi(...)` on
 * demand. Cache lives only in `save.c`. */

#include "gtypes.h"

void score_reset(void);              /* call from enter_playing */
void score_add_kill(u8 enemy_type);  /* BUG=10 / ROBOT=25 / ARMORED=50, ×diff_mult */
void score_add_boss_kill(void);      /* iter-7: 300 × diff_mult */
void score_add_wave_clear(u8 wave_num);  /* 100×wave×diff_mult */
void score_add_win_bonus(void);      /* 5000×diff_mult */
u16  score_get(void);

#endif /* GBTD_SCORE_H */
