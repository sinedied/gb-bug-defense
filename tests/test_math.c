/*
 * Host-side regression tests for the squared-distance arithmetic used by
 * tower target acquisition and projectile hit detection (F2).
 *
 * The on-device code uses i16 for the per-axis pixel deltas, then computes
 * d2 = adx*adx + ady*ady as u16 (was i16, which silently overflows past
 * sqrt(32767) ≈ 181 px — a pair of corner positions on the 160x144 DMG
 * screen exceeds that). These tests reproduce the failing pre-fix
 * behaviour and lock in the post-fix correctness.
 *
 * Compile: cc -std=c99 -Wall -Wextra -O2 tests/test_math.c -o build/test_math
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef int16_t  i16;
typedef uint16_t u16;
typedef uint8_t  u8;

/* Pre-fix (BUGGY) implementation, kept for regression demonstration. */
static i16 d2_signed_buggy(u8 ax, u8 ay, u8 bx, u8 by) {
    i16 dx = (i16)ax - (i16)bx;
    i16 dy = (i16)ay - (i16)by;
    return (i16)(dx*dx + dy*dy);
}

/* Post-fix implementation, matching towers.c::acquire_target and
 * projectiles.c::step_proj. */
static u16 d2_unsigned(u8 ax, u8 ay, u8 bx, u8 by) {
    i16 dx = (i16)ax - (i16)bx;
    i16 dy = (i16)ay - (i16)by;
    u16 adx = dx < 0 ? (u16)-dx : (u16)dx;
    u16 ady = dy < 0 ? (u16)-dy : (u16)dy;
    return (u16)(adx*adx + ady*ady);
}

static int failures = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void test_short_distance_unaffected(void) {
    /* In-range tower targeting: TOWER_RANGE_SQ = 24*24 = 576. */
    EXPECT(d2_unsigned(100, 100, 100, 100) == 0);
    EXPECT(d2_unsigned(100, 100, 110, 100) == 100);
    EXPECT(d2_unsigned(100, 100, 110, 110) == 200);
    EXPECT(d2_unsigned(100, 100, 124, 100) == 576); /* exactly at range  */
    EXPECT(d2_unsigned(100, 100, 125, 100) == 625); /* just out of range */

    /* Projectile hit: PROJ_HIT_SQ = 4*4 = 16. */
    EXPECT(d2_unsigned( 80, 72,  80, 72) == 0);
    EXPECT(d2_unsigned( 80, 72,  84, 72) == 16);
    EXPECT(d2_unsigned( 80, 72,  84, 73) == 17);
}

static void test_overflow_corner_cases(void) {
    /* Worst case: opposite-corner pixels of the 160x144 screen.
     * dx = 160, dy = 144 -> d2 = 25600 + 20736 = 46336 (> i16 max 32767). */
    u16 d2 = d2_unsigned(0, 0, 160, 144);
    EXPECT(d2 == 46336);

    /* The signed-i16 implementation overflows here: 46336 doesn't fit in
     * i16 (max 32767). The bit pattern survives in u16 (same 16 bits)
     * but the SIGNED comparison used by the original code (`if (d2 >
     * TOWER_RANGE_SQ)` with d2 typed as i16) sees a negative value and
     * accepts it as "in range" — that's the surface bug we lock in below. */
    i16 buggy = d2_signed_buggy(0, 0, 160, 144);
    EXPECT(buggy < 0); /* signed overflow → negative — the real bug surface */

    /* Single-axis 160 -> 25600, fits in i16 (32767), still correct both ways. */
    EXPECT(d2_unsigned(0, 0, 160, 0) == 25600);
    EXPECT((u16)d2_signed_buggy(0, 0, 160, 0) == 25600);
}

static void test_signed_cast_paths(void) {
    /* Negative deltas (target left/above source): abs cast must work. */
    EXPECT(d2_unsigned(160, 144,   0,   0) == 46336);

    /* Past u16 too: 255²+255² = 130050, mod 65536 = 64514. We aren't
     * required to handle this distance (sprite coords cap at 255, play
     * field tops out near 160x144), but the wrap behaviour is defined. */
    EXPECT(d2_unsigned(255, 255,   0,   0) == (u16)64514);
    EXPECT(d2_unsigned(  0,   0, 255, 255) == (u16)64514);
}

static void test_in_vs_out_of_range_decision(void) {
    /* The fix matters because the buggy code, on overflow, can produce a
     * negative i16 that compares <= TOWER_RANGE_SQ and falsely accepts an
     * obviously-out-of-range enemy. Reproduce one such case. */
    const i16 TOWER_RANGE_SQ = 576;
    /* dx=180, dy=180 -> true d2=64800 (out of range). */
    i16 buggy = d2_signed_buggy(0, 0, 180, 180);
    /* Demonstrates the bug: signed-overflow result is small/negative and
     * would erroneously be accepted as "in range". */
    EXPECT(buggy <= TOWER_RANGE_SQ);
    /* Post-fix: correct unsigned arithmetic rejects it. */
    EXPECT(d2_unsigned(0, 0, 180, 180) > (u16)TOWER_RANGE_SQ);
}

int main(void) {
    test_short_distance_unaffected();
    test_overflow_corner_cases();
    test_signed_cast_paths();
    test_in_vs_out_of_range_decision();

    if (failures) {
        fprintf(stderr, "%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("test_math: all assertions passed\n");
    return 0;
}
