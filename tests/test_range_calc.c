/*
 * Host-side regression tests for src/range_calc.h —
 * the pure circle-dot position computation helper for tower range preview
 * (iter-4 feature #31).
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_range_calc.c \
 *      -o build/test_range_calc
 *
 * Zero GBDK linkage required: range_calc.h is header-only.
 */
#include <stdio.h>

#include "range_calc.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

/* T1: 8 dots at center (80, 76), R=24 — all visible, verify symmetry. */
static void t1_center_circle(void) {
    range_dot_t d;
    /* dot 0: 0° → dx=24*64/64=24, dy=0 → (104, 76) */
    d = range_calc_dot(80, 76, 24, 0);
    CHECK(d.x == 104 && d.y == 76);
    /* dot 2: 90° → dx=0, dy=24*64/64=24 → (80, 100) */
    d = range_calc_dot(80, 76, 24, 2);
    CHECK(d.x == 80 && d.y == 100);
    /* dot 4: 180° → dx=24*-64/64=-24, dy=0 → (56, 76) */
    d = range_calc_dot(80, 76, 24, 4);
    CHECK(d.x == 56 && d.y == 76);
    /* dot 6: 270° → dx=0, dy=24*-64/64=-24 → (80, 52) */
    d = range_calc_dot(80, 76, 24, 6);
    CHECK(d.x == 80 && d.y == 52);
    /* dot 1: 45° → dx=24*45/64=16, dy=24*45/64=16 → (96, 92) */
    d = range_calc_dot(80, 76, 24, 1);
    CHECK(d.x == 96 && d.y == 92);
    /* dot 3: 135° → dx=24*-45/64=-16, dy=24*45/64=16 → (64, 92) */
    d = range_calc_dot(80, 76, 24, 3);
    CHECK(d.x == 64 && d.y == 92);
    /* dot 5: 225° → dx=24*-45/64=-16, dy=24*-45/64=-16 → (64, 60) */
    d = range_calc_dot(80, 76, 24, 5);
    CHECK(d.x == 64 && d.y == 60);
    /* dot 7: 315° → dx=24*45/64=16, dy=24*-45/64=-16 → (96, 60) */
    d = range_calc_dot(80, 76, 24, 7);
    CHECK(d.x == 96 && d.y == 60);
}

/* T2: center at (4, 12), R=40 — dots at angles that go off-screen left
 * should be hidden. */
static void t2_left_edge_clip(void) {
    range_dot_t d;
    /* dot 3 (135°): dx=40*-45/64=-28, px=4-28=-24 → hidden */
    d = range_calc_dot(4, 12, 40, 3);
    CHECK(d.x == 255 && d.y == 255);
    /* dot 4 (180°): dx=40*-64/64=-40, px=4-40=-36 → hidden */
    d = range_calc_dot(4, 12, 40, 4);
    CHECK(d.x == 255 && d.y == 255);
    /* dot 5 (225°): dx=40*-45/64=-28, px=4-28=-24; dy=-28, py=12-28=-16 → hidden */
    d = range_calc_dot(4, 12, 40, 5);
    CHECK(d.x == 255 && d.y == 255);
    /* dot 6 (270°): dx=0, dy=-40, py=12-40=-28 → hidden */
    d = range_calc_dot(4, 12, 40, 6);
    CHECK(d.x == 255 && d.y == 255);
    /* dot 0 (0°): dx=40, px=44 → visible */
    d = range_calc_dot(4, 12, 40, 0);
    CHECK(d.x == 44 && d.y == 12);
}

/* T3: center at (156, 140), R=40 — dots at angles 315°/0°/45° should be
 * hidden (pixel x > 159 or y > 143). */
static void t3_right_bottom_clip(void) {
    range_dot_t d;
    /* dot 0 (0°): dx=40, px=196 → hidden (>159) */
    d = range_calc_dot(156, 140, 40, 0);
    CHECK(d.x == 255 && d.y == 255);
    /* dot 1 (45°): dx=28, px=184 → hidden (>159) */
    d = range_calc_dot(156, 140, 40, 1);
    CHECK(d.x == 255 && d.y == 255);
    /* dot 7 (315°): dx=28, px=184 → hidden (>159) */
    d = range_calc_dot(156, 140, 40, 7);
    CHECK(d.x == 255 && d.y == 255);
    /* dot 2 (90°): dy=40, py=180 → hidden (>143) */
    d = range_calc_dot(156, 140, 40, 2);
    CHECK(d.x == 255 && d.y == 255);
    /* dot 4 (180°): dx=-40, px=116; dy=0, py=140 → visible */
    d = range_calc_dot(156, 140, 40, 4);
    CHECK(d.x == 116 && d.y == 140);
}

/* T4: R=0 — all 8 dots at center pixel (all visible, all same position). */
static void t4_radius_zero(void) {
    uint8_t i;
    for (i = 0; i < 8; i++) {
        range_dot_t d = range_calc_dot(80, 76, 0, i);
        CHECK(d.x == 80 && d.y == 76);
    }
}

/* T5: dot_idx > 7 — safe (masked to 3 bits internally). */
static void t5_index_wrap(void) {
    range_dot_t d0 = range_calc_dot(80, 76, 24, 0);
    range_dot_t d8 = range_calc_dot(80, 76, 24, 8);   /* 8 & 7 = 0 */
    CHECK(d0.x == d8.x && d0.y == d8.y);

    range_dot_t d3 = range_calc_dot(80, 76, 24, 3);
    range_dot_t d11 = range_calc_dot(80, 76, 24, 11);  /* 11 & 7 = 3 */
    CHECK(d3.x == d11.x && d3.y == d11.y);

    range_dot_t d255 = range_calc_dot(80, 76, 24, 255); /* 255 & 7 = 7 */
    range_dot_t d7 = range_calc_dot(80, 76, 24, 7);
    CHECK(d255.x == d7.x && d255.y == d7.y);
}

/* T6: Division correctness — verify negative offsets.
 * center (40, 40), R=40, dot at angle 180° → x = 40 + (40*-64)/64 = 0.
 * Clip condition is px < 0, so px=0 passes → dot IS visible at (0, 40). */
static void t6_division_negative(void) {
    range_dot_t d;
    /* dot 4 (180°): dx=40*-64/64=-40, px=40-40=0; dy=0, py=40 */
    d = range_calc_dot(40, 40, 40, 4);
    CHECK(d.x == 0 && d.y == 40);

    /* dot 6 (270°): dx=0, px=40; dy=40*-64/64=-40, py=40-40=0 */
    d = range_calc_dot(40, 40, 40, 6);
    CHECK(d.x == 40 && d.y == 0);
}

/* T7: Regression — FW tower at play-field tile (3,2), range_px=40.
 * cx=3*8+4=28, cy=(2+1)*8+4=28, dot_idx=5 (225°):
 * dx=(40*-45)/64=-28, dy=-28 → px=0, py=0.
 * (0,0) is a valid screen pixel and must NOT be treated as hidden. */
static void t7_regression_zero_zero_valid(void) {
    range_dot_t d;
    d = range_calc_dot(28, 28, 40, 5);
    CHECK(d.x == 0 && d.y == 0);
}

int main(void) {
    t1_center_circle();
    t2_left_edge_clip();
    t3_right_bottom_clip();
    t4_radius_zero();
    t5_index_wrap();
    t6_division_negative();
    t7_regression_zero_zero_valid();
    if (failures) {
        fprintf(stderr, "test_range_calc: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_range_calc: all checks passed\n");
    return 0;
}
