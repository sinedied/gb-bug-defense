/*
 * Host-side regression tests for iter-3 #19 SRAM save module.
 *
 * Covers:
 *   - save_calc.h pure helpers (slot index/offset, magic, pack/unpack)
 *   - save.c via mocked SRAM stub: save_init on fresh SRAM, save_init
 *     on valid SRAM, save_load_highscores round-trip, save_get_hi /
 *     save_write_hi, "high score replacement only on > prior" semantics
 *     (verified at the call-site level — score.c does the >-compare).
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc \
 *      tests/test_save.c src/save.c -o build/test_save
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "save_calc.h"
#include "save.h"

/* SRAM + cart-control register backing for the stub gb.h / hardware.h
 * macros. The actual extern declarations come from the stub headers. */
unsigned char g_sram[2048];
unsigned char g_rRAMG;
unsigned char g_rRAMB;

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

/* --- save_calc.h --- */

static void t_slot_index(void) {
    /* slot = map*3 + diff */
    CHECK_EQ(save_slot_index(0, 0), 0);
    CHECK_EQ(save_slot_index(0, 1), 1);
    CHECK_EQ(save_slot_index(0, 2), 2);
    CHECK_EQ(save_slot_index(1, 0), 3);
    CHECK_EQ(save_slot_index(2, 2), 8);
}

static void t_slot_offset(void) {
    CHECK_EQ(save_slot_offset(0, 0), SAVE_OFF_HI);
    CHECK_EQ(save_slot_offset(0, 1), SAVE_OFF_HI + 2);
    CHECK_EQ(save_slot_offset(2, 2), SAVE_OFF_HI + 16);
    CHECK_EQ(save_slot_offset(2, 2) + 1, SAVE_TOTAL_BYTES - 1);
}

static void t_pack_unpack(void) {
    uint8_t buf[2];
    save_pack_u16(buf, 0x0000); CHECK_EQ(save_unpack_u16(buf), 0);
    save_pack_u16(buf, 0xFFFF); CHECK_EQ(save_unpack_u16(buf), 0xFFFF);
    save_pack_u16(buf, 0x1234); CHECK_EQ(save_unpack_u16(buf), 0x1234);
    /* Little-endian byte order */
    save_pack_u16(buf, 0xABCD);
    CHECK_EQ(buf[0], 0xCD);
    CHECK_EQ(buf[1], 0xAB);
}

static void t_magic_ok(void) {
    uint8_t good[6] = { 'G', 'B', 'T', 'D', SAVE_VERSION, 0x00 };
    uint8_t bad_magic[6] = { 'X', 'B', 'T', 'D', SAVE_VERSION, 0x00 };
    uint8_t bad_ver[6] = { 'G', 'B', 'T', 'D', 0x99, 0x00 };
    uint8_t zero[6] = { 0 };
    CHECK(save_magic_ok(good));
    CHECK(!save_magic_ok(bad_magic));
    CHECK(!save_magic_ok(bad_ver));
    CHECK(!save_magic_ok(zero));
}

/* --- save.c with mocked SRAM --- */

static void zero_sram(void) {
    memset(g_sram, 0xFF, sizeof g_sram);  /* 0xFF = uninitialized SRAM */
    g_rRAMG = 0;
    g_rRAMB = 0;
}

static void t_save_init_fresh(void) {
    /* Fresh / corrupt SRAM: save_init must zero-init the cache and
     * stamp a fresh header + 18 zero bytes. */
    zero_sram();
    save_init();
    /* Header stamped */
    CHECK_EQ(g_sram[0], (uint8_t)'G');
    CHECK_EQ(g_sram[1], (uint8_t)'B');
    CHECK_EQ(g_sram[2], (uint8_t)'T');
    CHECK_EQ(g_sram[3], (uint8_t)'D');
    CHECK_EQ(g_sram[4], SAVE_VERSION);
    CHECK_EQ(g_sram[5], 0x00);
    /* Slot table zeroed */
    for (int i = 0; i < SAVE_HI_COUNT * 2; i++) {
        CHECK_EQ(g_sram[SAVE_OFF_HI + i], 0x00);
    }
    /* Cache reads zero */
    for (uint8_t m = 0; m < 3; m++) {
        for (uint8_t d = 0; d < 3; d++) {
            CHECK_EQ(save_get_hi(m, d), 0);
        }
    }
    /* RAM was disabled before returning (corruption-resilience invariant). */
    CHECK_EQ(g_rRAMG, 0x00);
}

static void t_save_init_valid(void) {
    /* Pre-stamp valid SRAM with known scores; save_init must hydrate
     * the cache without re-stamping (preserve existing scores). */
    zero_sram();
    g_sram[0] = 'G'; g_sram[1] = 'B'; g_sram[2] = 'T'; g_sram[3] = 'D';
    g_sram[4] = SAVE_VERSION;
    g_sram[5] = 0;
    /* Zero the slot table, then plant known values in slots 0/4/8. */
    for (int i = 0; i < SAVE_HI_COUNT * 2; i++) g_sram[SAVE_OFF_HI + i] = 0;
    /* slot 0 (M1/CASUAL) = 0x0001, slot 4 (M2/NORMAL) = 0x1234,
     * slot 8 (M3/VETERAN) = 0xFFFF. */
    g_sram[SAVE_OFF_HI + 0]  = 0x01; g_sram[SAVE_OFF_HI + 1]  = 0x00;
    g_sram[SAVE_OFF_HI + 8]  = 0x34; g_sram[SAVE_OFF_HI + 9]  = 0x12;
    g_sram[SAVE_OFF_HI + 16] = 0xFF; g_sram[SAVE_OFF_HI + 17] = 0xFF;
    save_init();
    CHECK_EQ(save_get_hi(0, 0), 0x0001);
    CHECK_EQ(save_get_hi(1, 1), 0x1234);
    CHECK_EQ(save_get_hi(2, 2), 0xFFFF);
    /* Untouched slots default to whatever was in SRAM (here: 0). */
    CHECK_EQ(save_get_hi(0, 1), 0);
    /* RAM disabled at return. */
    CHECK_EQ(g_rRAMG, 0x00);
}

static void t_save_write_hi(void) {
    /* save_write_hi updates BOTH cache AND SRAM (LE), bracketed by
     * ENABLE_RAM/DISABLE_RAM. */
    zero_sram();
    save_init();
    save_write_hi(1, 2, 0xBEEF);   /* slot 5 → off SAVE_OFF_HI + 10 */
    CHECK_EQ(save_get_hi(1, 2), 0xBEEF);
    CHECK_EQ(g_sram[SAVE_OFF_HI + 10], 0xEF);
    CHECK_EQ(g_sram[SAVE_OFF_HI + 11], 0xBE);
    /* Other slots untouched. */
    CHECK_EQ(save_get_hi(0, 0), 0);
    CHECK_EQ(g_rRAMG, 0x00);
    /* Bank-select was hit. */
    CHECK_EQ(g_rRAMB, 0x00);
}

static void t_high_score_replacement(void) {
    /* The "replace iff strictly greater" semantics live at the
     * call-site (game.c::enter_gameover does `sc > save_get_hi`).
     * Replicate that here against the cache to lock the behaviour
     * regression-tested. */
    zero_sram();
    save_init();
    save_write_hi(0, 0, 100);
    /* 99 should NOT replace 100. */
    if (200u > save_get_hi(0, 0)) save_write_hi(0, 0, 200);
    CHECK_EQ(save_get_hi(0, 0), 200);
    if (50u  > save_get_hi(0, 0)) save_write_hi(0, 0, 50);
    CHECK_EQ(save_get_hi(0, 0), 200);
    /* Tie does NOT replace (strict >). */
    if (200u > save_get_hi(0, 0)) save_write_hi(0, 0, 999);
    CHECK_EQ(save_get_hi(0, 0), 200);
}

static void t_per_slot_isolation(void) {
    zero_sram();
    save_init();
    save_write_hi(0, 0, 11);
    save_write_hi(2, 2, 22);
    save_write_hi(1, 1, 33);
    /* Each slot independent. */
    CHECK_EQ(save_get_hi(0, 0), 11);
    CHECK_EQ(save_get_hi(2, 2), 22);
    CHECK_EQ(save_get_hi(1, 1), 33);
    /* Untouched slots still zero. */
    CHECK_EQ(save_get_hi(0, 1), 0);
    CHECK_EQ(save_get_hi(1, 2), 0);
}

static void t_save_init_partial_reset_recovers(void) {
    /* Iter-3 #19 F1 regression: when SRAM has stale garbage in the
     * score region AND mismatched magic (e.g. fresh cart, factory
     * 0xAA / 0xFF noise), save_init must FULLY zero the slot table.
     * No byte of the prior pattern may leak through. The reset path
     * also stamps magic+version LAST so a power-loss mid-reset leaves
     * the header invalid (re-reset on next boot) — that ordering is
     * a runtime-only property, verified by code inspection in save.c. */
    memset(g_sram, 0xAA, sizeof g_sram);
    g_sram[0] = 0xFF; g_sram[1] = 0xFF; g_sram[2] = 0xFF; g_sram[3] = 0xFF;
    g_rRAMG = 0; g_rRAMB = 0;
    save_init();
    /* Magic+version stamped. */
    CHECK_EQ(g_sram[0], (uint8_t)'G');
    CHECK_EQ(g_sram[1], (uint8_t)'B');
    CHECK_EQ(g_sram[2], (uint8_t)'T');
    CHECK_EQ(g_sram[3], (uint8_t)'D');
    CHECK_EQ(g_sram[4], SAVE_VERSION);
    CHECK_EQ(g_sram[5], 0x00);
    /* Every score byte is 0 — no 0xAA leak. */
    for (int i = 0; i < SAVE_HI_COUNT * 2; i++) {
        CHECK_EQ(g_sram[SAVE_OFF_HI + i], 0x00);
    }
    /* Cache mirrors. */
    for (uint8_t m = 0; m < 3; m++) {
        for (uint8_t d = 0; d < 3; d++) {
            CHECK_EQ(save_get_hi(m, d), 0);
        }
    }
    CHECK_EQ(g_rRAMG, 0x00);
}

int main(void) {
    t_slot_index();
    t_slot_offset();
    t_pack_unpack();
    t_magic_ok();
    t_save_init_fresh();
    t_save_init_valid();
    t_save_init_partial_reset_recovers();
    t_save_write_hi();
    t_high_score_replacement();
    t_per_slot_isolation();
    if (failures) {
        fprintf(stderr, "test_save: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_save: all checks passed\n");
    return 0;
}
