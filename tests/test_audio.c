/*
 * Host-side regression tests for the iter-2 SFX state machine in
 * src/audio.c. Compiles audio.c directly against host stubs of
 * <gb/gb.h> and <gb/hardware.h> (see tests/stubs/) which route every
 * NRxx_REG write to a global byte array we can inspect.
 *
 * Iter-3 #16 update:
 *   - SFX_WIN / SFX_LOSE removed (D-MUS-3); multi-note state-machine
 *     coverage migrated to test_music.c.
 *   - test_priority_preempt_rules rewritten to use ch4 (ENEMY_DEATH
 *     prio 2 preempts; ENEMY_HIT prio 1  preserves the F1rejected) 
 *     "lower-prio rejected" invariant on a still-existing channel.
 *   - audio.c now also calls music_init / music_reset / music_tick;
 *     test links src/music.c too. Music engine writes NR3x and the
 *     wave RAM at  the new ordering check verifies these landboot 
 *     AFTER NR52 (DMG quirk).
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

unsigned char g_audio_regs[32];
unsigned char g_wave_ram[16];

#include "audio.h"
#include "music.h"
#include <gb/hardware.h>

unsigned char g_write_log[G_WRITE_LOG_CAP];
unsigned int  g_write_log_len;

static void reset_regs(void) {
    memset(g_audio_regs, 0, sizeof g_audio_regs);
    memset(g_wave_ram,   0, sizeof g_wave_ram);
    memset(g_write_log,  0, sizeof g_write_log);
    g_write_log_len = 0;
}

static int first_write_idx(unsigned char reg) {
    unsigned int i;
    for (i = 0; i < g_write_log_len; i++) if (g_write_log[i] == reg) return (int)i;
    return -1;
}

static int write_count(unsigned char reg) {
    unsigned int i; int n = 0;
    for (i = 0; i < g_write_log_len; i++) if (g_write_log[i] == reg) n++;
    return n;
}

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void test_init_writes_nr52_first_and_fires_boot_chime(void) {
    reset_regs();
    audio_init();
    /* NR52 master-on must be set to 0x80. */
    CHECK(g_audio_regs[REG_NR52] == 0x80);
    CHECK(g_audio_regs[REG_NR50] == 0x77);
    CHECK(g_audio_regs[REG_NR51] == 0xFF);
    /* SFX_BOOT plays on ch2. */
    CHECK((g_audio_regs[REG_NR22] & 0xF0) != 0);
    CHECK((g_audio_regs[REG_NR24] & 0x80) != 0);

    /* ORDERING (DMG quirk: writes to NR10..NR51 are dropped while NR52
     * bit 7 = 0). NR52 must be the FIRST audio register written, AND
     * must precede every NR3x write made by music_init() (wave RAM
     * load via NR30). */
    int i_nr52 = first_write_idx(REG_NR52);
    int i_nr50 = first_write_idx(REG_NR50);
    int i_nr30 = first_write_idx(REG_NR30);
    CHECK(i_nr52 == 0);
    CHECK(i_nr52 < i_nr50);
    CHECK(i_nr30 > i_nr52);          /* music_init came AFTER NR52 */

    /* audio_reset() must run between master-reg setup and SFX_BOOT. */
    int i_nr12 = first_write_idx(REG_NR12);
    int i_nr42 = first_write_idx(REG_NR42);
    int i_nr24 = first_write_idx(REG_NR24);
    CHECK(i_nr12 >= 0);
    CHECK(i_nr42 >= 0);
    CHECK(i_nr12 < i_nr24);
    CHECK(i_nr42 < i_nr24);
    CHECK(write_count(REG_NR22) >= 2);
}

static void test_play_sets_priority_and_envelope(void) {
    reset_regs();
    audio_init();
    audio_play(SFX_TOWER_PLACE);
    CHECK((g_audio_regs[REG_NR12] & 0xF0) != 0);
    CHECK((g_audio_regs[REG_NR14] & 0x80) != 0);
    CHECK(g_audio_regs[REG_NR10] == 0x00);
}

static void test_priority_preempt_rules(void) {
    /* Iter-3 #16: rewritten on ch4 since SFX_WIN/LOSE are gone.
     * SFX_ENEMY_DEATH (prio 2) preempts; SFX_ENEMY_HIT (prio 1)
     * arriving WHILE death is active must be rejected. */
    reset_regs();
    audio_init();
    audio_reset();                          /* clear boot chime + music state */
    audio_play(SFX_ENEMY_DEATH);            /* ch4 prio 2 */
    unsigned char nr42_after = g_audio_regs[REG_NR42];
    unsigned char nr43_after = g_audio_regs[REG_NR43];
    unsigned char nr44_after = g_audio_regs[REG_NR44];
    int writes_before = (int)g_write_log_len;
    audio_play(SFX_ENEMY_HIT);              /* ch4 prio 1: must be rejected */
    CHECK(g_audio_regs[REG_NR42] == nr42_after);
    CHECK(g_audio_regs[REG_NR43] == nr43_after);
    CHECK(g_audio_regs[REG_NR44] == nr44_after);
    CHECK((int)g_write_log_len == writes_before);

    /* Equal-or-higher priority on the SAME channel must be accepted.
     * SFX_ENEMY_DEATH (prio 2) preempts ENEMY_HIT (prio 1). */
    reset_regs();
    audio_init();
    audio_reset();
    audio_play(SFX_ENEMY_HIT);               /* ch4 prio 1 (envelope 0xC1) */
    CHECK(g_audio_regs[REG_NR42] == 0xC1);
    audio_play(SFX_ENEMY_DEATH);             /* prio 2: accepted */
    CHECK(g_audio_regs[REG_NR42] == 0xC2);   /* env now reflects death */
    CHECK(g_audio_regs[REG_NR43] == 0x70);   /* NOISE_DEATH pitch */
    CHECK((g_audio_regs[REG_NR44] & 0x80) != 0);

    /* Same-priority on a DIFFERENT channel must NOT  both staypreempt 
     * active on their own channels. */
    reset_regs();
    audio_init();
    audio_reset();
    audio_play(SFX_TOWER_FIRE);              /* ch2 prio 1 */
    audio_play(SFX_ENEMY_HIT);               /* ch4 prio 1 */
    CHECK(g_audio_regs[REG_NR22] == 0xA1);
    CHECK((g_audio_regs[REG_NR24] & 0x80) != 0);
    CHECK(g_audio_regs[REG_NR42] == 0xC1);
    CHECK((g_audio_regs[REG_NR44] & 0x80) != 0);
}

static void test_tick_silences_single_note_sfx(void) {
    reset_regs();
    audio_init();
    audio_reset();
    audio_play(SFX_TOWER_FIRE);    /* duration = 4 frames on ch2 */
    CHECK((g_audio_regs[REG_NR22] & 0xF0) != 0);
    audio_tick(); audio_tick(); audio_tick(); audio_tick();
    CHECK(g_audio_regs[REG_NR22] == 0x00);
    audio_play(SFX_TOWER_FIRE);
    CHECK((g_audio_regs[REG_NR22] & 0xF0) != 0);
    CHECK((g_audio_regs[REG_NR24] & 0x80) != 0);
}

static void test_reset_preserves_master_regs(void) {
    reset_regs();
    audio_init();
    audio_play(SFX_TOWER_PLACE);
    g_audio_regs[REG_NR50] = 0x77;
    g_audio_regs[REG_NR51] = 0xFF;
    g_audio_regs[REG_NR52] = 0x80;
    audio_reset();
    CHECK(g_audio_regs[REG_NR50] == 0x77);
    CHECK(g_audio_regs[REG_NR51] == 0xFF);
    CHECK(g_audio_regs[REG_NR52] == 0x80);
    CHECK(g_audio_regs[REG_NR12] == 0x00);
    CHECK(g_audio_regs[REG_NR22] == 0x00);
    CHECK(g_audio_regs[REG_NR42] == 0x00);
    audio_play(SFX_TOWER_PLACE);
    CHECK((g_audio_regs[REG_NR12] & 0xF0) != 0);
}

int main(void) {
    test_init_writes_nr52_first_and_fires_boot_chime();
    test_play_sets_priority_and_envelope();
    test_priority_preempt_rules();
    test_tick_silences_single_note_sfx();
    test_reset_preserves_master_regs();
    if (failures) {
        fprintf(stderr, "test_audio: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_audio: all checks passed\n");
    return 0;
}
