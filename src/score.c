#include "score.h"
#include "score_calc.h"
#include "difficulty_calc.h"
#include "game.h"

/* Single u16 accumulator. Saturates at 0xFFFF (defensive — real runs
 * top out around ~22 000). */
static u16 s_score;

void score_reset(void)        { s_score = 0; }
u16  score_get(void)          { return s_score; }

static void add_scaled(u16 base) {
    u8  mult = difficulty_score_mult_num(game_difficulty());
    u16 scaled = score_apply_mult(base, mult);
    s_score = score_add_clamped(s_score, scaled);
}

void score_add_kill(u8 enemy_type) {
    add_scaled(score_kill_base(enemy_type));
}

void score_add_wave_clear(u8 wave_num) {
    add_scaled(score_wave_base(wave_num));
}

void score_add_win_bonus(void) {
    add_scaled(SCORE_WIN_BONUS);
}
