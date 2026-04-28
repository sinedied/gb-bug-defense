#include "audio.h"
#include <gb/gb.h>
#include <gb/hardware.h>

/* SFX engine — see specs/iter2.md §8.
 *
 * Channels used: 1 (square + sweep), 2 (square), 4 (noise). Channel 3 (wave)
 * is reserved for iter-3 music.
 *
 * DMG audio gotchas:
 *  - NRx4 bit 7 is the trigger and is write-only: writing 0 to NRx4 does NOT
 *    silence a channel. To silence we write NRx2 = 0x00 (DAC truly off:
 *    DMG enables the DAC iff bits 7..3 of NRx2 are non-zero, so 0x00 also
 *    clears NR52's per-channel-active flag).
 *  - Re-enabling the DAC requires a subsequent NRx2 write with a non-zero
 *    top nibble AND a trigger (NRx4 bit 7). start_note() does both, so the
 *    next audio_play() after silence_channel() always re-arms cleanly.
 *  - NRx2 must have a non-zero top nibble before trigger or the DAC is off
 *    and the trigger is a no-op.
 *  - NR41 has no duty bits (noise channel); only the bottom 6 bits as length.
 */

typedef struct {
    u8        channel;        /* 1, 2, or 4 */
    u8        priority;       /* higher preempts on same channel */
    u8        nrx1;           /* duty + length (square: 0x80 = 50%; noise: 0x3F) */
    u8        envelope;       /* NRx2 — top nibble must be non-zero */
    u8        duration;       /* total frames (single-note path); 0 -> derived */
    u8        is_noise;       /* 1 = ch4 (poly counter byte in pitches[i]) */
    u8        sweep;          /* NR10 (ch1 only); 0 if unused */
    u8        note_count;     /* >=1 */
    u8        frames_per_note;
    const u16 *pitches;       /* GB 11-bit freq for ch1/2; NR43 byte for ch4 */
} sfx_def_t;

static const u16 PITCH_LO[]    = { 0x06A4 };  /* short low blip */
static const u16 PITCH_FIRE[]  = { 0x0783 };  /* mid square blip */
static const u16 NOISE_HIT[]   = { 0x0050 };  /* short sharp click */
static const u16 NOISE_DEATH[] = { 0x0070 };  /* longer rumble */
static const u16 WIN_NOTES[]   = { 0x06A4, 0x0721, 0x0783, 0x07C2 };  /* asc */
static const u16 LOSE_NOTES[]  = { 0x07C2, 0x0721, 0x06A4, 0x05F2 };  /* desc */

static const sfx_def_t S_SFX[SFX_COUNT] = {
    [SFX_TOWER_PLACE] = {
        .channel = 1, .priority = 2, .nrx1 = 0x80, .envelope = 0xF1,
        .duration = 8, .sweep = 0x16,
        .note_count = 1, .frames_per_note = 8, .pitches = PITCH_LO,
    },
    [SFX_TOWER_FIRE] = {
        .channel = 2, .priority = 1, .nrx1 = 0x80, .envelope = 0xA1,
        .duration = 4,
        .note_count = 1, .frames_per_note = 4, .pitches = PITCH_FIRE,
    },
    [SFX_ENEMY_HIT] = {
        .channel = 4, .priority = 1, .nrx1 = 0x3F, .envelope = 0x71,
        .duration = 3, .is_noise = 1,
        .note_count = 1, .frames_per_note = 3, .pitches = NOISE_HIT,
    },
    [SFX_ENEMY_DEATH] = {
        .channel = 4, .priority = 2, .nrx1 = 0x3F, .envelope = 0xC2,
        .duration = 8, .is_noise = 1,
        .note_count = 1, .frames_per_note = 8, .pitches = NOISE_DEATH,
    },
    [SFX_WIN] = {
        .channel = 1, .priority = 3, .nrx1 = 0x80, .envelope = 0xF3,
        .duration = 0,
        .note_count = 4, .frames_per_note = 10, .pitches = WIN_NOTES,
    },
    [SFX_LOSE] = {
        .channel = 1, .priority = 3, .nrx1 = 0x80, .envelope = 0xF3,
        .duration = 0,
        .note_count = 4, .frames_per_note = 10, .pitches = LOSE_NOTES,
    },
};

typedef struct {
    const sfx_def_t *cur;
    u8  prio;          /* 0 = idle */
    u8  frames_left;   /* in current note */
    u8  note_idx;
    u8  notes_left;
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
    /* Write NRx2 = 0x00: bits 7..3 = 0 -> DAC off -> NR52 per-channel-active
     * flag clears, no DC offset on real hardware. (Earlier 0x08 left bit 3
     * set, which kept the DAC enabled per pandocs even though volume was
     * zero — silent in practice but the channel-on flag stuck.) The
     * subsequent audio_play() restores a non-zero envelope and triggers
     * via NRx4 bit 7, so the channel re-arms cleanly. */
    if      (channel == 1) NR12_REG = 0x00;
    else if (channel == 2) NR22_REG = 0x00;
    else                   NR42_REG = 0x00;
}

void audio_init(void) {
    NR52_REG = 0x80;   /* sound master ON (must be set BEFORE other writes) */
    NR50_REG = 0x77;   /* both stereo terminals at max volume */
    NR51_REG = 0xFF;   /* all 4 channels routed to L+R */
    audio_reset();
}

void audio_reset(void) {
    /* Force every used channel back to a known-silent, idle state. Safe to
     * call at any state transition; does NOT touch NR50/51/52 master regs
     * so it is also safe to call repeatedly. */
    silence_channel(1);
    silence_channel(2);
    silence_channel(4);
    s_ch[0].cur = NULL; s_ch[0].prio = 0;
    s_ch[0].frames_left = 0; s_ch[0].note_idx = 0; s_ch[0].notes_left = 0;
    s_ch[1].cur = NULL; s_ch[1].prio = 0;
    s_ch[1].frames_left = 0; s_ch[1].note_idx = 0; s_ch[1].notes_left = 0;
    s_ch[2].cur = NULL; s_ch[2].prio = 0;
    s_ch[2].frames_left = 0; s_ch[2].note_idx = 0; s_ch[2].notes_left = 0;
}

void audio_play(u8 sfx_id) {
    if (sfx_id >= SFX_COUNT) return;
    const sfx_def_t *def = &S_SFX[sfx_id];
    u8 idx = chan_to_idx(def->channel);
    /* Preempt rule: if channel idle OR new prio >= cur prio, take the slot. */
    if (s_ch[idx].prio != 0 && def->priority < s_ch[idx].prio) return;

    s_ch[idx].cur         = def;
    s_ch[idx].prio        = def->priority;
    s_ch[idx].note_idx    = 0;
    s_ch[idx].notes_left  = def->note_count;
    s_ch[idx].frames_left = def->frames_per_note;
    start_note(def, def->pitches[0]);
}

void audio_tick(void) {
    u8 i;
    for (i = 0; i < 3; i++) {
        if (s_ch[i].prio == 0) continue;
        if (s_ch[i].frames_left) s_ch[i].frames_left--;
        if (s_ch[i].frames_left == 0) {
            const sfx_def_t *def = s_ch[i].cur;
            s_ch[i].note_idx++;
            if (s_ch[i].note_idx >= s_ch[i].notes_left) {
                /* End of SFX: stop this channel. */
                silence_channel(def->channel);
                s_ch[i].cur = NULL;
                s_ch[i].prio = 0;
            } else {
                /* Advance to next note, re-trigger. */
                s_ch[i].frames_left = def->frames_per_note;
                start_note(def, def->pitches[s_ch[i].note_idx]);
            }
        }
    }
}
