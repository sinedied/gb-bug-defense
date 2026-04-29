/*
 * Host-side regression tests for iter-3 #19 score subsystem.
 *
 * Covers:
 *   - score_calc.h: kill bases, wave bases, multiplier, saturation
 *   - difficulty_calc.h: difficulty_score_mult_num
 *   - score.c: reset/get/add* with stub game_difficulty()
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc \
 *      tests/test_score.c src/score.c -o build/test_score
 */
#include <stdio.h>
#include <stdint.h>

#include "score_calc.h"
#include "difficulty_calc.h"
#include "score.h"

/* score.c calls game_difficulty() (and depends transitively on no
 * other gtypes runtime). The stub returns whatever the test scripts. */
static uint8_t g_test_diff = 1; /* DIFF_NORMAL */
unsigned char game_difficulty(void) { return g_test_diff; }

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)
#define CHECK_EQ(a, b) do { \
    unsigned long _a = (unsigned long)(a), _b = (unsigned long)(b); \
    if (_a != _b) { \
        fprintf(stderr, "FAIL %s:%d: %s == %s (got %lu, expected %lu)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        failures++; \
    } \
} while (0)

/* --- score_calc.h pure helpers --- */

static void t_kill_base(void) {
    CHECK_EQ(score_kill_base(0), SCORE_KILL_BUG);
    CHECK_EQ(score_kill_base(1), SCORE_KILL_ROBOT);
    CHECK_EQ(score_kill_base(2), SCORE_KILL_ARMORED);
    CHECK_EQ(score_kill_base(99), 0);
}

static void t_wave_base(void) {
    CHECK_EQ(score_wave_base(1), 100);
    CHECK_EQ(score_wave_base(5), 500);
    CHECK_EQ(score_wave_base(10), 1000);
}

static void t_apply_mult(void) {
    /* NORMAL: (base * 12) >> 3 = base * 1.5 */
    CHECK_EQ(score_apply_mult(10, 8),  10);    /* CASUAL */
    CHECK_EQ(score_apply_mult(10, 12), 15);    /* NORMAL */
    CHECK_EQ(score_apply_mult(10, 16), 20);    /* VETERAN */
    CHECK_EQ(score_apply_mult(100, 12), 150);
    CHECK_EQ(score_apply_mult(5000, 16), 10000);
    /* No int overflow in u16 path (uses u32 internally). */
    CHECK_EQ(score_apply_mult(0xFFFF, 16), (uint16_t)((uint32_t)0xFFFF * 16u >> 3));
}

static void t_add_clamped(void) {
    CHECK_EQ(score_add_clamped(0, 0), 0);
    CHECK_EQ(score_add_clamped(100, 50), 150);
    CHECK_EQ(score_add_clamped(0xFFF0, 100), 0xFFFF);   /* saturate */
    CHECK_EQ(score_add_clamped(0xFFFF, 1), 0xFFFF);
    CHECK_EQ(score_add_clamped(0xFFFE, 1), 0xFFFF);
    /* Exact-edge case: cur+delta == 0xFFFF (no clamp). */
    CHECK_EQ(score_add_clamped(0xFFFE, 0), 0xFFFE);
}

/* --- difficulty_calc.h extension --- */

static void t_diff_mult_num(void) {
    CHECK_EQ(difficulty_score_mult_num(DIFF_CASUAL),  8);
    CHECK_EQ(difficulty_score_mult_num(DIFF_NORMAL), 12);
    CHECK_EQ(difficulty_score_mult_num(DIFF_VETERAN), 16);
    /* Garbage clamps to NORMAL. */
    CHECK_EQ(difficulty_score_mult_num(99), 12);
}

/* --- score.c (with stub difficulty) --- */

static void t_score_reset_and_get(void) {
    score_reset();
    CHECK_EQ(score_get(), 0);
}

static void t_score_add_kill_normal(void) {
    g_test_diff = DIFF_NORMAL;
    score_reset();
    score_add_kill(0);  /* BUG: 10 * 12 / 8 = 15 */
    CHECK_EQ(score_get(), 15);
    score_add_kill(1);  /* ROBOT: 25 * 12 / 8 = 37 */
    CHECK_EQ(score_get(), 15 + 37);
    score_add_kill(2);  /* ARMORED: 50 * 12 / 8 = 75 */
    CHECK_EQ(score_get(), 15 + 37 + 75);
}

static void t_score_add_kill_easy_hard(void) {
    g_test_diff = DIFF_CASUAL;
    score_reset();
    score_add_kill(0);  /* 10 * 8 / 8 = 10 */
    CHECK_EQ(score_get(), 10);

    g_test_diff = DIFF_VETERAN;
    score_reset();
    score_add_kill(0);  /* 10 * 16 / 8 = 20 */
    CHECK_EQ(score_get(), 20);
    score_add_kill(2);  /* 50 * 16 / 8 = 100 */
    CHECK_EQ(score_get(), 120);
}

static void t_score_add_wave_clear(void) {
    g_test_diff = DIFF_NORMAL;
    score_reset();
    score_add_wave_clear(1);   /* 100 * 12 / 8 = 150 */
    CHECK_EQ(score_get(), 150);
    score_add_wave_clear(10);  /* 1000 * 12 / 8 = 1500 */
    CHECK_EQ(score_get(), 1650);
}

static void t_score_add_win_bonus(void) {
    g_test_diff = DIFF_VETERAN;
    score_reset();
    score_add_win_bonus();   /* 5000 * 16 / 8 = 10000 */
    CHECK_EQ(score_get(), 10000);
}

static void t_score_saturation(void) {
    g_test_diff = DIFF_VETERAN;
    score_reset();
    /* VETERAN win bonus = 10 000. Add a few wins to overflow u16. */
    score_add_win_bonus();
    score_add_win_bonus();
    score_add_win_bonus();
    score_add_win_bonus();
    score_add_win_bonus();
    score_add_win_bonus();
    score_add_win_bonus();   /* 7×10 000 = 70 000 — clamped */
    CHECK_EQ(score_get(), 0xFFFF);
}

int main(void) {
    t_kill_base();
    t_wave_base();
    t_apply_mult();
    t_add_clamped();
    t_diff_mult_num();
    t_score_reset_and_get();
    t_score_add_kill_normal();
    t_score_add_kill_easy_hard();
    t_score_add_wave_clear();
    t_score_add_win_bonus();
    t_score_saturation();
    if (failures) {
        fprintf(stderr, "test_score: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_score: all checks passed\n");
    return 0;
}
