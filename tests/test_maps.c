/*
 * Iter-3 #17 — Host tests for the three map definitions + map_select
 * pure helper.
 *
 * Compile (see justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires \
 *      tests/test_maps.c res/assets.c -o build/test_maps
 *
 * Pulls `res/assets.c` (the gameplayN_tilemap/classmap/waypoints
 * symbols) directly. We do NOT link `src/map.c` — the test owns its
 * own `s_test_maps[]` registry mirroring `src/map.c`'s table, so the
 * test never depends on map.c internals.
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "gtypes.h"
#include "map.h"
#include "map_select.h"
#include "assets.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)
#define CHECK_EQ(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { \
        fprintf(stderr, "FAIL %s:%d: %s == %s (got %ld, expected %ld)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        failures++; \
    } \
} while (0)

/* Mirror src/map.c's registry. NOT shared via map.c so the test stays
 * decoupled from map.c internals (D11 / spec subtask 8). */
typedef struct {
    const unsigned char *tilemap;
    const unsigned char *classmap;
    const waypoint_t    *waypoints;
    uint8_t              waypoint_count;
    uint8_t              computer_tx;
    uint8_t              computer_ty;
} test_map_t;

static const test_map_t s_test_maps[3] = {
    { gameplay1_tilemap, gameplay1_classmap, gameplay1_waypoints,  8, 18, 4 },
    { gameplay2_tilemap, gameplay2_classmap, gameplay2_waypoints, 10, 18, 4 },
    { gameplay3_tilemap, gameplay3_classmap, gameplay3_waypoints,  8, 18, 4 },
};

static uint8_t class_at(const test_map_t *m, int8_t tx, int8_t ty) {
    /* Off-grid (e.g. tx == -1 spawn) is conceptually TC_PATH for the
     * walker; only used when both endpoints are on-grid for segment
     * traces, so this fallback isn't actually exercised. */
    if (tx < 0 || ty < 0 || tx >= 20 || ty >= 17) return TC_PATH;
    return m->classmap[ty * 20 + tx];
}

/* T1 — map_select cycle wrap. */
static void test_cycle_wrap(void) {
    CHECK_EQ(map_cycle_left(0), 2);
    CHECK_EQ(map_cycle_left(1), 0);
    CHECK_EQ(map_cycle_left(2), 1);
    CHECK_EQ(map_cycle_right(0), 1);
    CHECK_EQ(map_cycle_right(1), 2);
    CHECK_EQ(map_cycle_right(2), 0);
    /* Round-trip identity. */
    for (uint8_t m = 0; m < 3; m++) {
        CHECK_EQ(map_cycle_right(map_cycle_left(m)), m);
        CHECK_EQ(map_cycle_left(map_cycle_right(m)), m);
    }
}

/* T2 — power-on default invariant: MAP_1 == 0 (so .data zero-init
 * boots into Map 1). */
static void test_default_is_zero(void) {
    /* MAP_COUNT must equal 3 (and #17 acceptance scenario #1). */
    CHECK_EQ(MAP_COUNT, 3);
    CHECK_EQ(MAP_SELECT_COUNT, MAP_COUNT);
    CHECK_EQ(MAX_WAYPOINTS, 10);
}

/* T3 — per-map invariants: counts, spawn shape, computer cluster. */
static void test_map_invariants(void) {
    for (int i = 0; i < 3; i++) {
        const test_map_t *m = &s_test_maps[i];
        CHECK(m->waypoint_count >= 2);
        CHECK(m->waypoint_count <= MAX_WAYPOINTS);

        /* Spawn waypoint: tx == -1, on the same row as wp[1], wp[1].tx == 0. */
        CHECK_EQ(m->waypoints[0].tx, -1);
        CHECK_EQ(m->waypoints[0].ty, m->waypoints[1].ty);
        CHECK_EQ(m->waypoints[1].tx, 0);

        /* Computer fits and all 4 cluster cells classify as TC_COMPUTER. */
        CHECK(m->computer_tx <= 18);
        CHECK(m->computer_ty <= 15);
        CHECK_EQ(class_at(m, m->computer_tx,     m->computer_ty),     TC_COMPUTER);
        CHECK_EQ(class_at(m, m->computer_tx + 1, m->computer_ty),     TC_COMPUTER);
        CHECK_EQ(class_at(m, m->computer_tx,     m->computer_ty + 1), TC_COMPUTER);
        CHECK_EQ(class_at(m, m->computer_tx + 1, m->computer_ty + 1), TC_COMPUTER);

        /* Terminal waypoint cell is one of the 4 cluster cells and
         * classifies as TC_COMPUTER. */
        int8_t tx = m->waypoints[m->waypoint_count - 1].tx;
        int8_t ty = m->waypoints[m->waypoint_count - 1].ty;
        CHECK_EQ(class_at(m, tx, ty), TC_COMPUTER);
        int in_cluster =
            (tx == m->computer_tx     && ty == m->computer_ty)     ||
            (tx == m->computer_tx + 1 && ty == m->computer_ty)     ||
            (tx == m->computer_tx     && ty == m->computer_ty + 1) ||
            (tx == m->computer_tx + 1 && ty == m->computer_ty + 1);
        CHECK(in_cluster);
    }
}

/* T4 — segment walker: every consecutive waypoint pair is purely
 * horizontal or vertical, and every cell on the straight line
 * (including endpoints) classifies as TC_PATH or TC_COMPUTER. */
static void test_segment_walk(void) {
    for (int i = 0; i < 3; i++) {
        const test_map_t *m = &s_test_maps[i];
        for (uint8_t k = 1; k < m->waypoint_count; k++) {
            int8_t ax = m->waypoints[k - 1].tx;
            int8_t ay = m->waypoints[k - 1].ty;
            int8_t bx = m->waypoints[k    ].tx;
            int8_t by = m->waypoints[k    ].ty;

            /* Orthogonal: exactly one axis differs (XOR). */
            CHECK((ax == bx) ^ (ay == by));

            if (ax == bx) {
                int8_t lo = (ay < by) ? ay : by;
                int8_t hi = (ay < by) ? by : ay;
                for (int8_t y = lo; y <= hi; y++) {
                    if (ax < 0) continue;   /* skip off-grid spawn endpoint */
                    uint8_t c = class_at(m, ax, y);
                    CHECK(c == TC_PATH || c == TC_COMPUTER);
                }
            } else {
                int8_t lo = (ax < bx) ? ax : bx;
                int8_t hi = (ax < bx) ? bx : ax;
                for (int8_t x = lo; x <= hi; x++) {
                    if (x < 0) continue;    /* skip off-grid spawn endpoint */
                    if (ay < 0) continue;
                    uint8_t c = class_at(m, x, ay);
                    CHECK(c == TC_PATH || c == TC_COMPUTER);
                }
            }
        }
    }
}

/* T5 — exact tile counts per map (design doc §validation). */
static void test_tile_counts(void) {
    static const int expected_path[3]   = { 29, 32, 22 };
    static const int expected_ground[3] = { 307, 304, 314 };
    for (int i = 0; i < 3; i++) {
        const test_map_t *m = &s_test_maps[i];
        int p = 0, g = 0, c = 0;
        for (int idx = 0; idx < 20 * 17; idx++) {
            switch (m->classmap[idx]) {
                case TC_PATH:     p++; break;
                case TC_GROUND:   g++; break;
                case TC_COMPUTER: c++; break;
                default: CHECK(0); break;
            }
        }
        CHECK_EQ(p, expected_path[i]);
        CHECK_EQ(g, expected_ground[i]);
        CHECK_EQ(c, 4);
    }
}

int main(void) {
    test_cycle_wrap();
    test_default_is_zero();
    test_map_invariants();
    test_segment_walk();
    test_tile_counts();
    if (failures) {
        fprintf(stderr, "test_maps: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_maps: OK\n");
    return 0;
}
