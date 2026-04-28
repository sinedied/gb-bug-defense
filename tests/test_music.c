/*
 * Host-side tests for src/music.c (iter-3 #16 — D-MUS-1..5).
 *
 * Compiled against the same NRxx-logging stubs as test_audio. Asserts:
 *   (a) music_play synchronously writes wave RAM (already loaded by
 *       music_init) AND row 0 NR3x BEFORE any music_tick — required so
 *       MUS_WIN/LOSE first note plays during the multi-frame
 *       enter_gameover() block.
 *   (b) advancing N frames lands on the next row.
 *   (c) loop wrap: hitting row_count with loop_idx=L jumps row_idx to L.
 *   (d) stinger end (loop_idx=0xFF): silence ch3+ch4, no underflow on
 *       further ticks.
 *   (e) music_play(current) is idempotent: row_idx unchanged, no extra
 *       trigger write logged.
 *   (f) music_play(other) resets row_idx to 0 AND writes new row 0 NRxx
 *       synchronously.
 *   (g) ch4 arbitration: notify_ch4_busy + 10 ticks -> NR4x not written;
 *       notify_ch4_free; mid-row tick -> still not written; tick across
 *       a row boundary with noise!=0 -> NR4x written exactly once.
 *   (h) music_duck(1) writes NR50=0x33; music_duck(0) writes NR50=0x77.
 *
 * The (b/c/d/g) cases reach into the engine via the public API only; they
 * use MUS_PLAYING / MUS_WIN / MUS_LOSE / MUS_TITLE row counts as defined
 * in the designer file. Update if the designer changes those.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

unsigned char g_audio_regs[32];
unsigned char g_wave_ram[16];

#include "music.h"
#include <gb/hardware.h>

unsigned char g_write_log[G_WRITE_LOG_CAP];
unsigned int  g_write_log_len;

static void reset_log(void) {
    memset(g_write_log, 0, sizeof g_write_log);
    g_write_log_len = 0;
}

static void reset_all(void) {
    memset(g_audio_regs, 0, sizeof g_audio_regs);
    memset(g_wave_ram,   0, sizeof g_wave_ram);
    reset_log();
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

static void boot(void) {
    /* Mimic boot order: master regs (NR52 first) then music_init.
     * Direct stub writes so the test isn't coupled to audio.c. */
    g_audio_regs[REG_NR52] = 0x80;
    g_audio_regs[REG_NR50] = 0x77;
    g_audio_regs[REG_NR51] = 0xFF;
    music_init();
    reset_log();
}

/* ---- (a) music_play synchronously arms row 0 ------------------------- */
static void test_play_synchronous_arm(void) {
    reset_all();
    boot();
    /* Wave RAM was loaded by music_init; assert pattern present. */
    CHECK(g_wave_ram[0]  == 0x01);
    CHECK(g_wave_ram[15] == 0x10);

    music_play(MUS_TITLE);
    /* No music_tick called — row 0 must already be programmed. */
    CHECK((g_audio_regs[REG_NR34] & 0x80) != 0);   /* trigger bit */
    CHECK(g_audio_regs[REG_NR30] == 0x80);          /* DAC on */
    CHECK(g_audio_regs[REG_NR32] == 0x20);          /* 100% output */
    /* Per designer: row 0 = C5 = 0x783, lo=0x83, hi=7. */
    CHECK(g_audio_regs[REG_NR33] == 0x83);
    CHECK((g_audio_regs[REG_NR34] & 0x07) == 0x07);
    /* CH4 row-0 noise is N_KICK = 0x55. */
    CHECK(g_audio_regs[REG_NR43] == 0x55);
    CHECK((g_audio_regs[REG_NR44] & 0x80) != 0);
}

/* ---- (b) tick advances rows ----------------------------------------- */
static void test_tick_advances_row(void) {
    reset_all();
    boot();
    music_play(MUS_TITLE);          /* row 0 = C5, duration 24 frames */
    reset_log();
    /* Advance exactly 24 ticks -> on the LAST tick we cross the
     * boundary into row 1 (E5 = 0x79D, lo=0x9D). */
    int i;
    for (i = 0; i < 24; i++) music_tick();
    CHECK(g_audio_regs[REG_NR33] == 0x9D);          /* E5 lo byte */
    CHECK((g_audio_regs[REG_NR34] & 0x80) != 0);    /* re-triggered */
}

/* ---- (c) loop wrap: end-of-song with loop_idx=0 jumps back ---------- */
static void test_loop_wrap(void) {
    reset_all();
    boot();
    music_play(MUS_PLAYING);        /* 16 rows × 16 frames; loop_idx = 0 */
    /* Total frames = 256. After 256 ticks we've consumed every row and
     * wrapped: row_idx should be back at 0 (C5 = 0x783, lo=0x83). */
    int i;
    for (i = 0; i < 256; i++) music_tick();
    CHECK(g_audio_regs[REG_NR33] == 0x83);          /* row 0 C5 again */
    CHECK((g_audio_regs[REG_NR34] & 0x80) != 0);
}

/* ---- (d) stinger ends and silences --------------------------------- */
static void test_stinger_ends(void) {
    reset_all();
    boot();
    music_play(MUS_LOSE);            /* loop_idx = 0xFF (stinger) */
    /* MUS_LOSE total = 14+14+14+14+16+24 = 96 frames. After exactly
     * 96 ticks the song must be idle: NR30 DAC off and NR42 = 0. */
    int i;
    for (i = 0; i < 96; i++) music_tick();
    CHECK(g_audio_regs[REG_NR30] == 0x00);
    CHECK(g_audio_regs[REG_NR42] == 0x00);
    /* Further ticks must be safe (no underflow / no register writes). */
    reset_log();
    for (i = 0; i < 50; i++) music_tick();
    CHECK(g_write_log_len == 0);
}

/* ---- (e) idempotent music_play(current) ---------------------------- */
static void test_play_idempotent(void) {
    reset_all();
    boot();
    music_play(MUS_PLAYING);
    /* Tick a few frames so row_idx advances + frames_left non-zero. */
    int i;
    for (i = 0; i < 16; i++) music_tick();    /* now at row 1 */
    reset_log();
    music_play(MUS_PLAYING);                  /* same song -> no-op */
    CHECK(g_write_log_len == 0);              /* zero NRxx writes */
}

/* ---- (f) music_play(other) resets to row 0 ------------------------- */
static void test_play_switches_song(void) {
    reset_all();
    boot();
    music_play(MUS_TITLE);
    int i;
    for (i = 0; i < 50; i++) music_tick();    /* somewhere mid-song */
    reset_log();
    music_play(MUS_PLAYING);                  /* different song */
    /* New row 0 NRxx written synchronously. MUS_PLAYING row 0 = C5
     * (same lo byte 0x83) but envelope differs (0xA1) — assert that. */
    CHECK((g_audio_regs[REG_NR44] & 0x80) != 0);
    CHECK(g_audio_regs[REG_NR42] == 0xA1);
    CHECK(g_audio_regs[REG_NR33] == 0x83);
}

/* ---- (g) ch4 arbitration ------------------------------------------- */
static void test_ch4_arbitration(void) {
    reset_all();
    boot();
    music_play(MUS_PLAYING);              /* row 0 noise = KICK 0x55 */
    /* SFX takes ch4: */
    music_notify_ch4_busy();
    reset_log();
    /* Tick 10 frames mid-row — NO NR4x writes from music while blocked. */
    int i;
    for (i = 0; i < 10; i++) music_tick();
    CHECK(write_count(REG_NR41) == 0);
    CHECK(write_count(REG_NR42) == 0);
    CHECK(write_count(REG_NR43) == 0);
    CHECK(write_count(REG_NR44) == 0);
    /* SFX done -> notify free; NO immediate re-arm (gated on next row
     * boundary). One mid-row tick -> still no writes. */
    music_notify_ch4_free();
    music_tick();
    CHECK(write_count(REG_NR44) == 0);
    /* Cross enough ticks to hit a row boundary with noise!=0. We're
     * inside row 0 (16-frame). After 10 blocked ticks + 1 unblocked
     * tick we've consumed 11 frames; 5 more crosses the boundary into
     * row 1 (HAT 0x21). NR43 should be written exactly once with the
     * new noise byte. */
    reset_log();
    for (i = 0; i < 5; i++) music_tick();
    CHECK(write_count(REG_NR43) == 1);
    CHECK(g_audio_regs[REG_NR43] == 0x21);
    CHECK(write_count(REG_NR44) == 1);
}

/* ---- (g2) ch4 arbitration: simultaneous unblock + row boundary ------ */
/* F3 regression for F2: SFX ends on the same frame the music row boundary
 * crosses. With the latch (ch4_just_freed promoted at END of music_tick +
 * ch4_arm_pending fired at TOP of next tick), the boundary tick must NOT
 * write NR4x (still blocked); the FOLLOWING tick must fire the deferred
 * arm. Without the F2 latch the boundary tick would fire NR4x with zero
 * gap from the SFX silence — audible click on DMG hardware. */
static void test_ch4_arbitration_boundary_on_unblock(void) {
    reset_all();
    boot();
    music_play(MUS_PLAYING);              /* row 0 KICK 0x55, 16 frames */
    music_notify_ch4_busy();              /* SFX takes ch4 */
    /* Bring frames_left to exactly 1 (the next tick crosses the row 0
     * boundary into row 1). Row 0 starts at frames_left=16 after
     * music_play; 15 ticks decrement it to 1. */
    int i;
    for (i = 0; i < 15; i++) music_tick();
    reset_log();
    /* Release ch4 — SFX just ended. */
    music_notify_ch4_free();
    /* THIS music_tick crosses the boundary AND is the SFX-end frame. The
     * latch keeps ch4_blocked=1 across the arm decision, so NR4x must
     * NOT be touched. Row 1 (HAT) is the new row; ch4_arm_pending is
     * set internally but no register writes have fired yet. */
    music_tick();
    CHECK(write_count(REG_NR41) == 0);
    CHECK(write_count(REG_NR42) == 0);
    CHECK(write_count(REG_NR43) == 0);
    CHECK(write_count(REG_NR44) == 0);
    /* NEXT tick: latch was promoted at end of previous tick → blocked=0.
     * Top-of-tick deferred arm fires program_ch4 for the current row
     * (row 1 = HAT 0x21). NR41-NR44 should all be written exactly once. */
    music_tick();
    CHECK(write_count(REG_NR41) == 1);
    CHECK(write_count(REG_NR42) == 1);
    CHECK(write_count(REG_NR43) == 1);
    CHECK(write_count(REG_NR44) == 1);
    CHECK(g_audio_regs[REG_NR43] == 0x21);     /* HAT */
    CHECK(g_audio_regs[REG_NR42] == 0xA1);     /* MUS_PLAYING ch4_env */
    CHECK((g_audio_regs[REG_NR44] & 0x80) != 0); /* trigger */
}

/* ---- (h) duck writes NR50 ------------------------------------------ */
static void test_duck(void) {
    reset_all();
    boot();
    music_duck(1);
    CHECK(g_audio_regs[REG_NR50] == 0x33);
    music_duck(0);
    CHECK(g_audio_regs[REG_NR50] == 0x77);
}

int main(void) {
    test_play_synchronous_arm();
    test_tick_advances_row();
    test_loop_wrap();
    test_stinger_ends();
    test_play_idempotent();
    test_play_switches_song();
    test_ch4_arbitration();
    test_ch4_arbitration_boundary_on_unblock();
    test_duck();
    if (failures) {
        fprintf(stderr, "test_music: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_music: all checks passed\n");
    return 0;
}
