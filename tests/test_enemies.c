/*
 * Host-side tests for iter-3 #18 enemy stun API + flash>stun>walk
 * priority. Compiles src/enemies.c against host stubs.
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires \
 *      tests/test_enemies.c src/enemies.c -o build/test_enemies
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "gtypes.h"
#include "enemies.h"
#include "assets.h"

/* ---------------------------------------------------------------- */
/* Stubs for collaborators enemies.c calls.                          */
/* ---------------------------------------------------------------- */

/* OAM tracking */
#define OAM_TRACK 64
typedef struct { u8 x, y, tile; int move_count; int tile_count; } oam_slot_t;
static oam_slot_t s_oam[OAM_TRACK];

void move_sprite(unsigned char nb, unsigned char x, unsigned char y) {
    if (nb >= OAM_TRACK) return;
    s_oam[nb].move_count++;
    s_oam[nb].x = x;
    s_oam[nb].y = y;
}

void set_sprite_tile(unsigned char nb, unsigned char tile) {
    if (nb >= OAM_TRACK) return;
    s_oam[nb].tile_count++;
    s_oam[nb].tile = tile;
}

/* map.h stubs */
#include "map.h"

#define TEST_WP_COUNT 8
static waypoint_t s_waypoints[TEST_WP_COUNT] = {
    {-1, 2}, {0, 2}, {8, 2}, {8, 9}, {15, 9}, {15, 5}, {17, 5}, {18, 5}
};

const waypoint_t *map_waypoints(void) { return s_waypoints; }
u8 map_waypoint_count(void) { return TEST_WP_COUNT; }

/* economy.h stubs */
static int s_damage_count;
void economy_damage(u8 n) { s_damage_count += n; (void)n; }

/* game.h stubs */
u8 game_difficulty(void) { return 1; } /* NORMAL */
u8 game_anim_frame(void) { return 0; }
bool game_is_modal_open(void) { return false; }

/* ---------------------------------------------------------------- */
/* Test harness                                                      */
/* ---------------------------------------------------------------- */
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

/* ---------------------------------------------------------------- */
/* Tests                                                             */
/* ---------------------------------------------------------------- */

static void reset_all(void) {
    memset(s_oam, 0, sizeof s_oam);
    s_damage_count = 0;
    enemies_init();
}

/* T1: enemies_try_stun returns true on fresh enemy, false on already stunned. */
static void test_try_stun_basic(void) {
    reset_all();
    enemies_spawn(ENEMY_BUG, 1);
    CHECK(enemies_alive(0));
    CHECK(!enemies_is_stunned(0));

    /* First stun succeeds. */
    CHECK(enemies_try_stun(0, 60) == true);
    CHECK(enemies_is_stunned(0));

    /* Second stun on already-stunned enemy fails (no-stack). */
    CHECK(enemies_try_stun(0, 90) == false);
    CHECK(enemies_is_stunned(0));
}

/* T2: enemies_try_stun returns false on dead enemy. */
static void test_try_stun_dead(void) {
    reset_all();
    enemies_spawn(ENEMY_BUG, 1);
    /* Kill it. */
    enemies_apply_damage(0, 255);
    CHECK(!enemies_alive(0));
    CHECK(enemies_try_stun(0, 60) == false);
}

/* T3: enemies_try_stun with out-of-range index. */
static void test_try_stun_oob(void) {
    reset_all();
    CHECK(enemies_try_stun(MAX_ENEMIES, 60) == false);
    CHECK(enemies_try_stun(255, 60) == false);
}

/* T4: stun_timer counts down to 0 via enemies_update. */
static void test_stun_timer_decrement(void) {
    reset_all();
    enemies_spawn(ENEMY_BUG, 1);
    enemies_try_stun(0, 5);
    CHECK(enemies_is_stunned(0));

    /* Tick 5 frames. After 5 calls to enemies_update, stun should expire. */
    u8 i;
    for (i = 0; i < 5; i++) {
        CHECK(enemies_is_stunned(0));
        enemies_update();
    }
    /* After 5 ticks, stun_timer should have gone 5→4→3→2→1→0. */
    CHECK(!enemies_is_stunned(0));
}

/* T5: movement is frozen while stunned, resumes after. */
static void test_stun_freezes_movement(void) {
    reset_all();
    enemies_spawn(ENEMY_BUG, 1);
    /* Record initial position. */
    u8 x0 = enemies_x_px(0);
    u8 y0 = enemies_y_px(0);

    /* Stun for 3 frames. */
    enemies_try_stun(0, 3);
    enemies_update();
    enemies_update();
    enemies_update();
    /* Position should not have changed. */
    CHECK_EQ(enemies_x_px(0), x0);
    CHECK_EQ(enemies_y_px(0), y0);

    /* Stun expired. Next update should move. */
    CHECK(!enemies_is_stunned(0));
    enemies_update();
    /* BUG_SPEED = 0x0080 = 0.5 px/frame. After 1 frame of movement,
     * the sub-pixel position changed but the integer pixel might not.
     * Run a few more frames to confirm movement resumes. */
    for (i8 j = 0; j < 10; j++) enemies_update();
    /* After ~11 frames at 0.5 px/frame, should have moved ~5-6 px. */
    u8 x1 = enemies_x_px(0);
    CHECK(x1 != x0 || enemies_y_px(0) != y0);
}

/* T6: flash > stun priority — flash tile shows even when stunned. */
static void test_flash_over_stun(void) {
    reset_all();
    enemies_spawn(ENEMY_BUG, 1);

    /* Stun the enemy. */
    enemies_try_stun(0, 60);
    enemies_update();
    /* Should be showing stun tile. */
    CHECK_EQ(s_oam[OAM_ENEMIES_BASE].tile, SPR_BUG_STUN);

    /* Now flash it (simulating a hit while stunned). */
    enemies_set_flash(0);
    /* enemies_set_flash writes flash tile immediately. */
    CHECK_EQ(s_oam[OAM_ENEMIES_BASE].tile, SPR_BUG_FLASH);

    /* On the next update, flash_timer=3-1=2, still active → flash tile wins. */
    enemies_update();
    CHECK_EQ(s_oam[OAM_ENEMIES_BASE].tile, SPR_BUG_FLASH);

    /* Tick through remaining flash frames. enemies_flash_step returns 1
     * for each decrement (3→2, 2→1, 1→0), so flash persists for 3
     * update calls total. On the 4th update, flash_step sees 0 and
     * returns 0, allowing the stun tile to show. */
    enemies_update();  /* flash_timer: 2→1 (still flashing) */
    CHECK_EQ(s_oam[OAM_ENEMIES_BASE].tile, SPR_BUG_FLASH);
    enemies_update();  /* flash_timer: 1→0 (last flash frame) */
    CHECK_EQ(s_oam[OAM_ENEMIES_BASE].tile, SPR_BUG_FLASH);
    enemies_update();  /* flash_timer: 0→0 (flash done, stun still active) */
    CHECK_EQ(s_oam[OAM_ENEMIES_BASE].tile, SPR_BUG_STUN);
}

/* T7: armored enemy spawns, walks, takes damage with flash. */
static void test_armored_basic(void) {
    reset_all();
    CHECK(enemies_spawn(ENEMY_ARMORED, 1));
    CHECK(enemies_alive(0));
    CHECK_EQ(enemies_bounty(0), ARMORED_BOUNTY);

    /* Flash should use armored flash tile. */
    enemies_set_flash(0);
    CHECK_EQ(s_oam[OAM_ENEMIES_BASE].tile, SPR_ARMORED_FLASH);
}

/* T8: is_stunned reads back correctly for all enemy types. */
static void test_is_stunned_all_types(void) {
    reset_all();
    enemies_spawn(ENEMY_BUG, 1);
    enemies_spawn(ENEMY_ROBOT, 1);
    enemies_spawn(ENEMY_ARMORED, 1);

    CHECK(!enemies_is_stunned(0));
    CHECK(!enemies_is_stunned(1));
    CHECK(!enemies_is_stunned(2));

    enemies_try_stun(0, 10);
    enemies_try_stun(1, 20);
    enemies_try_stun(2, 30);

    CHECK(enemies_is_stunned(0));
    CHECK(enemies_is_stunned(1));
    CHECK(enemies_is_stunned(2));
}

int main(void) {
    test_try_stun_basic();
    test_try_stun_dead();
    test_try_stun_oob();
    test_stun_timer_decrement();
    test_stun_freezes_movement();
    test_flash_over_stun();
    test_armored_basic();
    test_is_stunned_all_types();

    if (failures) {
        fprintf(stderr, "test_enemies: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_enemies: OK\n");
    return 0;
}
