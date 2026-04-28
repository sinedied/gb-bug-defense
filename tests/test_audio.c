/*
 * Host-side regression tests for the iter-2 SFX state machine in
 * src/audio.c. Compiles audio.c directly against host stubs of
 * <gb/gb.h> and <gb/hardware.h> (see tests/stubs/) which route every
 * NRxx_REG write to a global byte array we can inspect.
 *
 * What this locks in:
 *   1. audio_init() writes NR52=0x80 BEFORE any other audio register
 *      (DMG quirk: writes to NR10..NR51 are ignored while NR52 bit 7=0).
 *   2. audio_init() fires SFX_BOOT so a silent emulator/device is
 *      diagnosable on first boot (root cause of the "no audio" report).
 *   3. audio_play() sets per-channel priority and a non-zero envelope
 *      (NRx2 top nibble != 0 — required or the DAC is off and the trigger
 *      is a no-op).
 *   4. audio_play() refuses a strictly lower-priority preempt on the same
 *      channel and accepts an equal-or-higher one.
 *   5. audio_tick() decrements frames_left and silences the channel
 *      (NRx2 = 0x00 = DAC off — F2 fix) at end of single-note SFX.
 *   6. audio_tick() advances multi-note SFX (win/lose) and only silences
 *      after the LAST note completes.
 *   7. audio_reset() clears per-channel state without touching NR50/51/52
 *      (so master volume/routing survives across game-state transitions).
 *   8. SFX_TOWER_PLACE no longer self-destructs via NR10 sweep (the
 *      original bug: sweep=0x16 dropped the channel within ~30 ms).
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc \
 *      tests/test_audio.c src/audio.c -o build/test_audio
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* The stub <gb/hardware.h> declares this; audio.c expects it as lvalues. */
unsigned char g_audio_regs[32];

#include "audio.h"

/* Re-include the stub so REG_* enum is in scope here too. */
#include <gb/hardware.h>

/* Storage for the stub's write-order log (declared extern in the stub). */
unsigned char g_write_log[G_WRITE_LOG_CAP];
unsigned int  g_write_log_len;

static void reset_regs(void) {
    memset(g_audio_regs, 0, sizeof g_audio_regs);
    memset(g_write_log, 0, sizeof g_write_log);
    g_write_log_len = 0;
}

/* Index of the FIRST log entry for register `reg`, or -1 if never written. */
static int first_write_idx(unsigned char reg) {
    unsigned int i;
    for (i = 0; i < g_write_log_len; i++) if (g_write_log[i] == reg) return (int)i;
    return -1;
}

/* Number of times `reg` was written. */
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
    /* NR50/51 must be set (max volume both terminals, all channels routed). */
    CHECK(g_audio_regs[REG_NR50] == 0x77);
    CHECK(g_audio_regs[REG_NR51] == 0xFF);
    /* SFX_BOOT plays on ch2 with a non-zero envelope top nibble (DAC on)
     * and NR24 trigger bit (0x80) set. Without these the boot chime is
     * inaudible and the original "no audio" diagnosis is impossible. */
    CHECK((g_audio_regs[REG_NR22] & 0xF0) != 0);
    CHECK((g_audio_regs[REG_NR24] & 0x80) != 0);

    /* ORDERING (DMG quirk: writes to NR10..NR51 are dropped while NR52
     * bit 7 = 0). NR52 must be the FIRST audio register written. Without
     * this assertion the previous test would still pass even if
     * audio_init() regressed to write NR50 before NR52 (the stub stores
     * only the final value). The write log captures order. */
    int i_nr52 = first_write_idx(REG_NR52);
    int i_nr50 = first_write_idx(REG_NR50);
    int i_nr51 = first_write_idx(REG_NR51);
    int i_nr22 = first_write_idx(REG_NR22);
    int i_nr24 = first_write_idx(REG_NR24);
    CHECK(i_nr52 == 0);                    /* NR52 is the very first write */
    CHECK(i_nr52 < i_nr50);
    CHECK(i_nr52 < i_nr51);
    CHECK(i_nr52 < i_nr22);
    CHECK(i_nr52 < i_nr24);

    /* audio_reset() must run between the master-reg setup and the
     * SFX_BOOT trigger so per-channel state for ch1/ch2/ch4 is forced
     * silent BEFORE the boot chime arms ch2. If audio_reset() were
     * removed, NR12 and NR42 would never be written by audio_init()
     * (SFX_BOOT only touches ch2). Anchoring this catches regressions
     * that delete or reorder the reset call. */
    int i_nr12 = first_write_idx(REG_NR12); /* silence_channel(1) -> NR12=0 */
    int i_nr42 = first_write_idx(REG_NR42); /* silence_channel(4) -> NR42=0 */
    CHECK(i_nr12 >= 0);                    /* audio_reset wrote it */
    CHECK(i_nr42 >= 0);
    CHECK(i_nr12 < i_nr24);                /* reset before boot trigger */
    CHECK(i_nr42 < i_nr24);
    /* And NR22 must have been written at least twice: once to 0x00 by
     * audio_reset's silence_channel(2), then to the boot envelope by
     * SFX_BOOT's start_note(). */
    CHECK(write_count(REG_NR22) >= 2);
}

static void test_play_sets_priority_and_envelope(void) {
    reset_regs();
    audio_init();         /* fires SFX_BOOT on ch2 */
    audio_play(SFX_TOWER_PLACE);
    /* ch1 envelope (NR12) top nibble must be non-zero or the DAC is off. */
    CHECK((g_audio_regs[REG_NR12] & 0xF0) != 0);
    /* Trigger bit on NR14. */
    CHECK((g_audio_regs[REG_NR14] & 0x80) != 0);
    /* SFX_TOWER_PLACE no longer programs an aggressive sweep — the previous
     * 0x16 value killed the channel within ~30 ms (root-cause fix). */
    CHECK(g_audio_regs[REG_NR10] == 0x00);
}

static void test_priority_preempt_rules(void) {
    reset_regs();
    audio_init();
    /* Win jingle (priority 3) on ch1 must not be preempted by tower-place
     * (priority 2). This was the F1 bug-class: leaked priority blocking
     * subsequent SFX. We assert the WIN trigger registers stay intact. */
    audio_play(SFX_WIN);
    unsigned char nr14_after_win = g_audio_regs[REG_NR14];
    unsigned char nr12_after_win = g_audio_regs[REG_NR12];
    int writes_before = (int)g_write_log_len;
    audio_play(SFX_TOWER_PLACE); /* lower prio — must be rejected */
    CHECK(g_audio_regs[REG_NR12] == nr12_after_win);
    CHECK(g_audio_regs[REG_NR14] == nr14_after_win);
    /* Belt-and-braces: a rejected play must not write any audio register. */
    CHECK((int)g_write_log_len == writes_before);

    /* Equal-or-higher priority on the SAME channel must be accepted.
     * Use two distinct ch2/prio-1 SFX (SFX_BOOT then SFX_TOWER_FIRE) so
     * the second play changes observable register state — otherwise
     * playing the same SFX twice writes identical bytes and we cannot
     * tell whether the second call was accepted or silently rejected. */
    reset_regs();
    audio_init();                            /* fires SFX_BOOT on ch2 */
    /* Sanity: after init, NR22 reflects SFX_BOOT envelope (0xF0). */
    CHECK(g_audio_regs[REG_NR22] == 0xF0);
    audio_play(SFX_TOWER_FIRE);              /* same ch, equal prio */
    /* If the second play was accepted, NR22 is now SFX_TOWER_FIRE's
     * envelope (0xA1) and NR23 is PITCH_FIRE low byte (0x83). If it
     * was rejected, NR22 would still be 0xF0. */
    CHECK(g_audio_regs[REG_NR22] == 0xA1);
    CHECK(g_audio_regs[REG_NR23] == 0x83);
    CHECK((g_audio_regs[REG_NR24] & 0x80) != 0);

    /* Same-priority on a DIFFERENT channel must NOT preempt — both SFX
     * stay active on their own channels. SFX_TOWER_FIRE (ch2 prio 1)
     * + SFX_ENEMY_HIT (ch4 prio 1) is the cleanest pair. */
    reset_regs();
    audio_init();
    audio_reset();                           /* clear the boot chime */
    audio_play(SFX_TOWER_FIRE);              /* ch2 prio 1 */
    audio_play(SFX_ENEMY_HIT);               /* ch4 prio 1 — different ch */
    CHECK(g_audio_regs[REG_NR22] == 0xA1);   /* ch2 envelope intact */
    CHECK((g_audio_regs[REG_NR24] & 0x80) != 0);
    CHECK(g_audio_regs[REG_NR42] == 0xC1);   /* ch4 envelope set */
    CHECK((g_audio_regs[REG_NR44] & 0x80) != 0);
}

static void test_tick_silences_single_note_sfx(void) {
    reset_regs();
    audio_init();
    audio_reset();                 /* clear the boot chime so it doesn't tick */
    audio_play(SFX_TOWER_FIRE);    /* duration = 4 frames on ch2 */
    CHECK((g_audio_regs[REG_NR22] & 0xF0) != 0);
    /* Tick exactly `frames_per_note` times — channel must silence on the
     * last tick (NRx2 = 0x00 = DAC truly off, per F2 fix). */
    audio_tick(); audio_tick(); audio_tick(); audio_tick();
    CHECK(g_audio_regs[REG_NR22] == 0x00);
    /* A subsequent play on the same channel re-arms cleanly. */
    audio_play(SFX_TOWER_FIRE);
    CHECK((g_audio_regs[REG_NR22] & 0xF0) != 0);
    CHECK((g_audio_regs[REG_NR24] & 0x80) != 0);
}

static void test_tick_advances_multi_note_sfx(void) {
    reset_regs();
    audio_init();
    audio_reset();
    audio_play(SFX_WIN);           /* 4 notes × 10 frames */
    /* After 10 ticks we should have advanced to note 2, NOT silenced. */
    int i;
    for (i = 0; i < 10; i++) audio_tick();
    CHECK(g_audio_regs[REG_NR12] != 0x00); /* still active */
    CHECK((g_audio_regs[REG_NR14] & 0x80) != 0); /* re-triggered */
    /* After all 4 notes (40 ticks total), channel must be silent. */
    for (i = 10; i < 40; i++) audio_tick();
    CHECK(g_audio_regs[REG_NR12] == 0x00);
}

static void test_reset_preserves_master_regs(void) {
    reset_regs();
    audio_init();
    audio_play(SFX_TOWER_PLACE);
    g_audio_regs[REG_NR50] = 0x77;
    g_audio_regs[REG_NR51] = 0xFF;
    g_audio_regs[REG_NR52] = 0x80;
    audio_reset();
    /* Master regs untouched (so NR50/51 setup from audio_init survives). */
    CHECK(g_audio_regs[REG_NR50] == 0x77);
    CHECK(g_audio_regs[REG_NR51] == 0xFF);
    CHECK(g_audio_regs[REG_NR52] == 0x80);
    /* Per-channel envelopes silenced. */
    CHECK(g_audio_regs[REG_NR12] == 0x00);
    CHECK(g_audio_regs[REG_NR22] == 0x00);
    CHECK(g_audio_regs[REG_NR42] == 0x00);
    /* And state is idle: a brand-new low-priority SFX is accepted. */
    audio_play(SFX_TOWER_PLACE);
    CHECK((g_audio_regs[REG_NR12] & 0xF0) != 0);
}

int main(void) {
    test_init_writes_nr52_first_and_fires_boot_chime();
    test_play_sets_priority_and_envelope();
    test_priority_preempt_rules();
    test_tick_silences_single_note_sfx();
    test_tick_advances_multi_note_sfx();
    test_reset_preserves_master_regs();
    if (failures) {
        fprintf(stderr, "test_audio: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_audio: all checks passed\n");
    return 0;
}
