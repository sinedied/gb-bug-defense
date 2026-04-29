/*
 * Host-side regression tests for src/gate_calc.h —
 * the pure blink-phase computation helper for the first-tower gate
 * (iter-4 feature #24).
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_gate.c \
 *      -o build/test_gate
 *
 * Zero GBDK linkage required: gate_calc.h is header-only.
 */
#include <stdio.h>

#include "gate_calc.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

/* T1: visible at frame 0 */
static void t1_frame0_visible(void) {
    CHECK(gate_blink_visible(0) == 1);
}

/* T2: still visible at frame 29 */
static void t2_frame29_visible(void) {
    CHECK(gate_blink_visible(29) == 1);
}

/* T3: hidden at frame 30 */
static void t3_frame30_hidden(void) {
    CHECK(gate_blink_visible(30) == 0);
}

/* T4: still hidden at frame 59 */
static void t4_frame59_hidden(void) {
    CHECK(gate_blink_visible(59) == 0);
}

/* T5: visible again at frame 60 */
static void t5_frame60_visible(void) {
    CHECK(gate_blink_visible(60) == 1);
}

/* T6: frame 255 → (255/30=8, 8&1=0) → visible */
static void t6_frame255(void) {
    CHECK(gate_blink_visible(255) == 1);
}

/* T7: boundary sweep — transitions happen exactly at multiples of 30 */
static void t7_boundary_sweep(void) {
    uint8_t f;
    for (f = 0; f < 240; f++) {
        uint8_t expected = ((f / 30u) & 1u) == 0 ? 1 : 0;
        if (gate_blink_visible(f) != expected) {
            fprintf(stderr, "FAIL %s:%d: boundary sweep at frame %u: got %u expected %u\n",
                    __FILE__, __LINE__, (unsigned)f,
                    (unsigned)gate_blink_visible(f), (unsigned)expected);
            failures++;
            return;
        }
    }
}

int main(void) {
    t1_frame0_visible();
    t2_frame29_visible();
    t3_frame30_hidden();
    t4_frame59_hidden();
    t5_frame60_visible();
    t6_frame255();
    t7_boundary_sweep();
    if (failures) {
        fprintf(stderr, "test_gate: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_gate: all checks passed\n");
    return 0;
}
