/*
 * Host-side regression tests for iter-3 #20 difficulty helpers in
 * src/difficulty_calc.h (pure header, `<stdint.h>` only).
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_difficulty.c \
 *      -o build/test_difficulty
 */
#include <stdio.h>
#include <stdint.h>

#include "difficulty_calc.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)
#define CHECK_EQ(a, b) do { \
    unsigned _a = (unsigned)(a), _b = (unsigned)(b); \
    if (_a != _b) { \
        fprintf(stderr, "FAIL %s:%d: %s == %s (got %u, expected %u)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        failures++; \
    } \
} while (0)

/* T1 — HP table identity for NORMAL drifts with tuning.h. */
static void test_t1_normal_identity(void) {
    CHECK_EQ(difficulty_enemy_hp(0, DIFF_NORMAL), BUG_HP);
    CHECK_EQ(difficulty_enemy_hp(1, DIFF_NORMAL), ROBOT_HP);
}

/* T2 — Exact HP scaling values for EASY (2,4) and HARD (5,9). */
static void test_t2_easy_hard_values(void) {
    CHECK_EQ(difficulty_enemy_hp(0, DIFF_EASY), 2);
    CHECK_EQ(difficulty_enemy_hp(1, DIFF_EASY), 4);
    CHECK_EQ(difficulty_enemy_hp(0, DIFF_HARD), 5);
    CHECK_EQ(difficulty_enemy_hp(1, DIFF_HARD), 9);
}

/* T3 — Junk inputs clamp to NORMAL row / bug column. */
static void test_t3_clamp(void) {
    CHECK_EQ(difficulty_enemy_hp(99, 99), DIFF_ENEMY_HP[1][0]);
    CHECK_EQ(difficulty_enemy_hp(99, DIFF_HARD), DIFF_ENEMY_HP[2][0]);
    CHECK_EQ(difficulty_enemy_hp(0, 7), DIFF_ENEMY_HP[1][0]);
}

/* T4 — Spawn interval identity for NORMAL across every base used in
 * the iter-2 wave script (50, 60, 75, 90). Catches `num != 8`. */
static void test_t4_normal_interval_identity(void) {
    static const uint8_t bases[] = { 50, 60, 75, 90 };
    for (unsigned i = 0; i < sizeof(bases)/sizeof(bases[0]); i++) {
        CHECK_EQ(difficulty_scale_interval(bases[i], DIFF_NORMAL), bases[i]);
    }
}

/* T5 — Strict ordering EASY > NORMAL > HARD for every wave-script base. */
static void test_t5_ordering(void) {
    static const uint8_t bases[] = { 50, 60, 75, 90 };
    for (unsigned i = 0; i < sizeof(bases)/sizeof(bases[0]); i++) {
        uint8_t e = difficulty_scale_interval(bases[i], DIFF_EASY);
        uint8_t n = difficulty_scale_interval(bases[i], DIFF_NORMAL);
        uint8_t h = difficulty_scale_interval(bases[i], DIFF_HARD);
        CHECK(e > n);
        CHECK(n > h);
    }
}

/* T6 — HARD spawn floor: 30 * 6 / 8 = 22, clamps to DIFF_SPAWN_FLOOR. */
static void test_t6_hard_floor(void) {
    CHECK_EQ(difficulty_scale_interval(30, DIFF_HARD), DIFF_SPAWN_FLOOR);
    /* 50 * 6 / 8 = 37 > floor → no clamp at the lowest wave-script base. */
    CHECK_EQ(difficulty_scale_interval(50, DIFF_HARD), 37);
    /* Invariant: floor applies to every (base, diff) pair. */
    CHECK(difficulty_scale_interval(0, DIFF_EASY)   >= DIFF_SPAWN_FLOOR);
    CHECK(difficulty_scale_interval(0, DIFF_NORMAL) >= DIFF_SPAWN_FLOOR);
    CHECK(difficulty_scale_interval(0, DIFF_HARD)   >= DIFF_SPAWN_FLOOR);
    /* Garbage difficulty defaults to NORMAL behaviour. */
    CHECK_EQ(difficulty_scale_interval(60, 99), 60);
}

/* T7 — Starting energy lookup + junk-clamp to NORMAL. */
static void test_t7_starting_energy(void) {
    CHECK_EQ(difficulty_starting_energy(DIFF_EASY),   45);
    CHECK_EQ(difficulty_starting_energy(DIFF_NORMAL), 30);
    CHECK_EQ(difficulty_starting_energy(DIFF_HARD),   24);
    CHECK_EQ(difficulty_starting_energy(99),          30);
}

/* T8 — Cycle math, including wrap. */
static void test_t8_cycle(void) {
    /* Wrap. */
    CHECK_EQ(difficulty_cycle_left(DIFF_EASY),  DIFF_HARD);
    CHECK_EQ(difficulty_cycle_right(DIFF_HARD), DIFF_EASY);
    /* Non-wrap transitions. */
    CHECK_EQ(difficulty_cycle_left(DIFF_NORMAL), DIFF_EASY);
    CHECK_EQ(difficulty_cycle_left(DIFF_HARD),   DIFF_NORMAL);
    CHECK_EQ(difficulty_cycle_right(DIFF_EASY),   DIFF_NORMAL);
    CHECK_EQ(difficulty_cycle_right(DIFF_NORMAL), DIFF_HARD);
}

/* T9 — No zero-HP enemy can ever spawn. */
static void test_t9_hp_floor(void) {
    for (unsigned d = 0; d < DIFF_COUNT; d++) {
        for (unsigned t = 0; t < 2; t++) {
            CHECK(DIFF_ENEMY_HP[d][t] >= 1);
        }
    }
}

int main(void) {
    test_t1_normal_identity();
    test_t2_easy_hard_values();
    test_t3_clamp();
    test_t4_normal_interval_identity();
    test_t5_ordering();
    test_t6_hard_floor();
    test_t7_starting_energy();
    test_t8_cycle();
    test_t9_hp_floor();

    if (failures) {
        fprintf(stderr, "test_difficulty: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_difficulty: OK\n");
    return 0;
}
