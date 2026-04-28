#include "audio.h"
#include "music.h"
#include <gb/gb.h>
#include <gb/hardware.h>

/* SFX  see specs/iter2.8.md engine 
 *
 * Channels used: 1 (square + sweep), 2 (square), 4 (noise). Channel 3 (wave)
 * is owned by the music engine (src/music.c, D-MUS-2).
 *
 * Iter-3 #16 simplifications (D-MUS-3):
 *   - SFX_WIN / SFX_LOSE removed (replaced by MUS_WIN / MUS_LOSE).
 *   - All remaining SFX are single-note. The sfx_def_t multi-note fields
 *     (note_count, frames_per_note, multi-element pitches) and the
 *     audio_tick advance branch were deleted as dead code. Each SFX now
 *     has exactly one pitch held for `duration` frames.
 *
 * Iter-3 #16 ch4 arbitration (D-MUS-2):
 *   - audio_play(SFX_ENEMY_HIT|DEATH) calls music_notify_ch4_busy()
 *     UNCONDITIONALLY (no-op when music  keeps audio leaf-free ofidle 
 *     music state).
 *   - audio_tick() edge-detects "ch4 SFX active at frame start, idle now"
 *     and calls music_notify_ch4_free() BEFORE music_tick(). The notify
 *     only sets a latch; music_tick clears ch4_blocked at the END of
 *     the current tick (after the arm decision) and fires any deferred
 *     arm at the TOP of the FOLLOWING tick. Net guarantee: ≥1 full
 *     music_tick of percussion silence between SFX end and ch4 re-arm.
 *
 * DMG audio gotchas (unchanged):
 *  - NRx4 bit 7 is the trigger and is write-only: writing 0 to NRx4 does NOT
 *    silence a channel. To silence we write NRx2 = 0x00 (DAC truly off).
 *  - NRx2 must have a non-zero top nibble before trigger or DAC is off.
 *  - NR41 has no duty bits (noise channel); only the bottom 6 bits as length.
 */

typedef struct {
    u8  channel;        /* 1, 2, or 4 */
    u8  priority;       /* higher preempts on same channel */
    u8  nrx1;           /* duty + length (square: 0x80 = 50%; noise: 0x3F) */
    u8  envelope;       /* NRx2: top nibble must be non-zero */
    u8  duration;       /* total frames the single note is held */
    u8  sweep;          /* NR10 (ch1 only); 0 if unused */
    u16 pitch;          /* GB 11-bit freq for ch1/2; NR43 byte for ch4 */
} sfx_def_t;

static const sfx_def_t S_SFX[SFX_COUNT] = {
    [SFX_TOWER_PLACE] = {
        /* No NR10 sweep: see iter-2 audio diag. Fixed pitch keeps it as a
         * clean ~265 ms beep. */
        .channel = 1, .priority = 2, .nrx1 = 0x80, .envelope = 0xF1,
        .duration = 16, .sweep = 0x00, .pitch = 0x06A4,
    },
    [SFX_TOWER_FIRE] = {
        .channel = 2, .priority = 1, .nrx1 = 0x80, .envelope = 0xA1,
        .duration = 4, .pitch = 0x0783,
    },
    [SFX_ENEMY_HIT] = {
        .channel = 4, .priority = 1, .nrx1 = 0x3F, .envelope = 0xC1,
        .duration = 3, .pitch = 0x0050,
    },
    [SFX_ENEMY_DEATH] = {
        .channel = 4, .priority = 2, .nrx1 = 0x3F, .envelope = 0xC2,
        .duration = 8, .pitch = 0x0070,
    },
    [SFX_BOOT] = {
        /* Diagnostic chime fired from audio_init(). See iter-2 audio diag. */
        .channel = 2, .priority = 1, .nrx1 = 0x80, .envelope = 0xF0,
        .duration = 12, .pitch = 0x0700,
    },
    [SFX_EMP_FIRE] = {
        /* Iter-3 #18: descending square sweep, distinct from PLACE (shorter
         * duration=8 vs 16). CH1 prio=2, does not conflict with music CH4. */
        .channel = 1, .priority = 2, .nrx1 = 0x80, .envelope = 0xF1,
        .duration = 8, .sweep = 0x73, .pitch = 0x07A0,
    },
};

typedef struct {
    const sfx_def_t *cur;
    u8  prio;          /* 0 = idle */
    u8  frames_left;
} channel_state_t;

/* Indexed 0..2 for channels 1, 2, 4 respectively. */
static channel_state_t s_ch[3];

static u8 chan_to_idx(u8 ch) {
    /* 1->0, 2->1, 4->2 */
    return (ch == 4) ? 2 : (u8)(ch - 1);
}

static void start_note(const sfx_def_t *def, u16 pitch) {
    if (def->channel == 1) {
        NR10_REG = def->sweep;
        NR11_REG = def->nrx1;
        NR12_REG = def->envelope;
        NR13_REG = (u8)(pitch & 0xFF);
        NR14_REG = (u8)(0x80 | ((pitch >> 8) & 0x07));
    } else if (def->channel == 2) {
        NR21_REG = def->nrx1;
        NR22_REG = def->envelope;
        NR23_REG = (u8)(pitch & 0xFF);
        NR24_REG = (u8)(0x80 | ((pitch >> 8) & 0x07));
    } else { /* ch4 noise */
        NR41_REG = def->nrx1;
        NR42_REG = def->envelope;
        NR43_REG = (u8)(pitch & 0xFF);
        NR44_REG = 0x80;
    }
}

static void silence_channel(u8 channel) {
    /* NRx2 = 0x00: DAC off (F2 fix). */
    if      (channel == 1) NR12_REG = 0x00;
    else if (channel == 2) NR22_REG = 0x00;
    else                   NR42_REG = 0x00;
}

void audio_init(void) {
    NR52_REG = 0x80;   /* sound master ON (must be set BEFORE other writes) */
    NR50_REG = 0x77;   /* both stereo terminals at max volume */
    NR51_REG = 0xFF;   /* all 4 channels routed to L+R */
    audio_reset();     /* (now also calls music_reset, but ch3/4 silence is
                        * already implied by power-on state, safe). */
    /* Boot chime - see iter-2 audio diag. */
    audio_play(SFX_BOOT);
    /* music_init AFTER NR52 is on AND AFTER the boot chime arms ch2.
     * Wave RAM is loaded once here; the engine stays idle until the
     * first music_play() call (from enter_title()). Order matters: if
     * music_init ran before NR52, the wave-RAM writes would be dropped
     * by the DMG quirk. */
    music_init();
}

void audio_reset(void) {
    /* Force every used SFX channel back to a known-silent, idle state. Also
     * (D-MUS-5) reset music state. Safe to call at any state transition;
     * does NOT touch NR50/51/52 master regs (so duck state survives). */
    silence_channel(1);
    silence_channel(2);
    silence_channel(4);
    s_ch[0].cur = 0; s_ch[0].prio = 0; s_ch[0].frames_left = 0;
    s_ch[1].cur = 0; s_ch[1].prio = 0; s_ch[1].frames_left = 0;
    s_ch[2].cur = 0; s_ch[2].prio = 0; s_ch[2].frames_left = 0;
    music_reset();
}

void audio_play(u8 sfx_id) {
    if (sfx_id >= SFX_COUNT) return;
    const sfx_def_t *def = &S_SFX[sfx_id];
    u8 idx = chan_to_idx(def->channel);
    /* Preempt rule: if channel idle OR new prio >= cur prio, take the slot. */
    if (s_ch[idx].prio != 0 && def->priority < s_ch[idx].prio) return;

    s_ch[idx].cur         = def;
    s_ch[idx].prio        = def->priority;
    s_ch[idx].frames_left = def->duration;
    start_note(def, def->pitch);

    /* Iter-3 #16 D-MUS-2: ch4 SFX preempts music's percussion row.
     * Unconditional notify (no-op when music idle). The "is music ch4
     * row armed" check lives in music.c so audio.c stays leaf-free of
     * music state. */
    if (def->channel == 4) {
        music_notify_ch4_busy();
    }
}

void audio_tick(void) {
    /* Snapshot ch4 SFX activity at frame  needed for the "ch4start 
     * SFX just ended this frame" edge detect (D-MUS-2 within-frame
     * order step 2). */
    u8 ch4_was_active = (s_ch[2].prio != 0);

    u8 i;
    for (i = 0; i < 3; i++) {
        if (s_ch[i].prio == 0) continue;
        if (s_ch[i].frames_left) s_ch[i].frames_left--;
        if (s_ch[i].frames_left == 0) {
            const sfx_def_t *def = s_ch[i].cur;
            silence_channel(def->channel);
            s_ch[i].cur = 0;
            s_ch[i].prio = 0;
        }
    }

    /* Edge-detect ch4 SFX completion -> notify music it can re-arm at the
     * next row boundary. */
    if (ch4_was_active && s_ch[2].prio == 0) {
        music_notify_ch4_free();
    }

    /* Single audio entry point: drive music last so any ch4 re-arm at a
     * row boundary lands AFTER the SFX-end notify. */
    music_tick();
}
