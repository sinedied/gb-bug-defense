/*
 * Host-side regression tests for iter-3 #21 animation helpers:
 *   src/map_anim.h     — HP -> 5-state corruption mapping
 *   src/enemies_anim.h — flash timer step + decrement
 *   src/towers_anim.h  — bitrev4 stagger + LED-phase formula
 *
 * All three headers are pure (`<stdint.h>` only) so this test links
 * with zero GBDK stubs.
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_anim.c \
 *      -o build/test_anim
 */
#include <stdio.h>
#include <stdint.h>

#include "map_anim.h"
#include "enemies_anim.h"
#include "towers_anim.h"

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

/* T1 — map_hp_to_corruption_state: HP {5,4,3,2,1,0,255}
 *      -> states {0,1,2,3,4,4,0}. */
static void test_hp_to_state(void) {
    CHECK_EQ(map_hp_to_corruption_state(5), 0);
    CHECK_EQ(map_hp_to_corruption_state(4), 1);
    CHECK_EQ(map_hp_to_corruption_state(3), 2);
    CHECK_EQ(map_hp_to_corruption_state(2), 3);
    CHECK_EQ(map_hp_to_corruption_state(1), 4);
    CHECK_EQ(map_hp_to_corruption_state(0), 4);
    CHECK_EQ(map_hp_to_corruption_state(255), 0); /* clamps to pristine */
    CHECK_EQ(map_hp_to_corruption_state(6), 0);
}

/* T2 — enemies_flash_step: decrements while > 0, returns 1; returns 0
 *      and does NOT underflow when 0. */
static void test_flash_step(void) {
    uint8_t t = 3;
    CHECK_EQ(enemies_flash_step(&t), 1); CHECK_EQ(t, 2);
    CHECK_EQ(enemies_flash_step(&t), 1); CHECK_EQ(t, 1);
    CHECK_EQ(enemies_flash_step(&t), 1); CHECK_EQ(t, 0);
    CHECK_EQ(enemies_flash_step(&t), 0); CHECK_EQ(t, 0);
    CHECK_EQ(enemies_flash_step(&t), 0); CHECK_EQ(t, 0);

    uint8_t t0 = 0;
    CHECK_EQ(enemies_flash_step(&t0), 0); CHECK_EQ(t0, 0);
}

/* T3 — bitrev4 reference table. */
static void test_bitrev4(void) {
    static const uint8_t expect[16] = {
        0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
    };
    for (uint8_t i = 0; i < 16; i++) {
        CHECK_EQ(_towers_bitrev4(i), expect[i]);
    }
}

/* T4 — towers_idle_phase_for: spec-mandated reference points.
 *      Slot 0: offset 0  -> phase = (frame >> 5) & 1
 *      Slot 1: offset 32 -> exactly anti-phase to slot 0
 *      Slot 2: offset 16 -> half-period shift
 *      Slot 15: offset 60 (bitrev4(15)=15, <<2 = 60). */
static void test_idle_phase(void) {
    /* Slot 0 vs slot 1 = anti-phase across the 64-frame period. */
    CHECK_EQ(towers_idle_phase_for(0,  0), 0);
    CHECK_EQ(towers_idle_phase_for(31, 0), 0);
    CHECK_EQ(towers_idle_phase_for(32, 0), 1);
    CHECK_EQ(towers_idle_phase_for(63, 0), 1);
    CHECK_EQ(towers_idle_phase_for(64, 0), 0);

    CHECK_EQ(towers_idle_phase_for(0,  1), 1);
    CHECK_EQ(towers_idle_phase_for(31, 1), 1);
    CHECK_EQ(towers_idle_phase_for(32, 1), 0);

    /* Slot 2: offset = bitrev4(2)<<2 = 4<<2 = 16. */
    CHECK_EQ(towers_idle_phase_for(0,  2), 0);  /* (0+16)>>5 = 0 */
    CHECK_EQ(towers_idle_phase_for(15, 2), 0);  /* (15+16)>>5 = 0 */
    CHECK_EQ(towers_idle_phase_for(16, 2), 1);  /* (16+16)>>5 = 1 */
    CHECK_EQ(towers_idle_phase_for(47, 2), 1);  /* (47+16)>>5 = 1 */
    CHECK_EQ(towers_idle_phase_for(48, 2), 0);  /* (48+16)=64; (64>>5)&1 = 0 */

    /* Slot 15: offset = bitrev4(15)<<2 = 15<<2 = 60.
     * frame=255 -> (255+60)&0xFF = 59 -> (59>>5)&1 = 1. */
    CHECK_EQ(towers_idle_phase_for(255, 15), 1);

    /* Slot index masked to 4 bits: idx=16 must equal idx=0. */
    CHECK_EQ(towers_idle_phase_for(0, 16), towers_idle_phase_for(0, 0));
    CHECK_EQ(towers_idle_phase_for(99, 17), towers_idle_phase_for(99, 1));
}

/* T5 — anim-frame counter wrap. The spec asserts that a 1-byte anim
 *      counter wraps cleanly at 256 and that all phase divisors used
 *      (32, 64) divide evenly so the wrap is invisible to observers. */
static void test_frame_wrap(void) {
    uint8_t f = 255;
    f++;
    CHECK_EQ(f, 0);
    /* Phase observed at frame 0 must equal phase observed at frame 256
     * for every tower index — i.e. 256 is a multiple of every phase
     * divisor we use. (32 | 256, 64 | 256.) */
    /* Exhaustive wrap-period check: idle phase has period 64 frames,
     * so phase(f, idx) must equal phase((f + 64) mod 256, idx) for all
     * f in [0, 192) — tiling the full 256-frame counter space. Catches
     * any regression that breaks the 64-frame period (e.g. introducing
     * a non-period-64 modulation). */
    for (uint8_t f = 0; f < 192; f++) {
        for (uint8_t idx = 0; idx < 16; idx++) {
            CHECK_EQ(towers_idle_phase_for(f, idx),
                     towers_idle_phase_for((uint8_t)(f + 64), idx));
        }
    }
    for (uint8_t idx = 0; idx < 16; idx++) {
        /* Half-period stability across wrap. */
        CHECK_EQ(towers_idle_phase_for(64, idx),
                 towers_idle_phase_for(0, idx));
    }
}

int main(void) {
    test_hp_to_state();
    test_flash_step();
    test_bitrev4();
    test_idle_phase();
    test_frame_wrap();
    if (failures) {
        fprintf(stderr, "test_anim: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_anim: OK\n");
    return 0;
}
