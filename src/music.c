#include "music.h"
#include <gb/gb.h>
#include <gb/hardware.h>

/* Iter-3 #16 — music engine (D-MUS-1..5). See music.h for API contract.
 *
 * Row data is hand-authored from specs/iter3-16-music-design.md. Each
 * row holds:
 *   pitch    — CH3 11-bit freq (NR33+NR34 low 3 bits); 0 = rest (ch3 silent)
 *   noise    — CH4 NR43 poly byte; 0 = rest (ch4 left as-is, no re-arm)
 *   duration — frames this row is held; >0
 *
 * Per-row register sequence (CH3): NR30=0x80, NR32=song->ch3_vol,
 * NR33=lo, NR34=0x80|hi (trigger bit). For a rest, NR30=0x00 (DAC off).
 *
 * Per-row register sequence (CH4): NR41=0x3F, NR42=song->ch4_env,
 * NR43=row->noise, NR44=0x80 (trigger, length-enable OFF). Skipped while
 * ch4_blocked != 0 (SFX owns the channel).
 *
 * music_play() programs row 0 IMMEDIATELY (synchronous arm — review R1)
 * so stingers play their first note even when the caller blocks the
 * main loop for several frames.
 *
 * MUST NOT include audio.h (one-way dep — see spec §"Include direction").
 */

typedef struct {
    uint16_t pitch;     /* CH3 11-bit freq, OR 0 = rest */
    uint8_t  noise;     /* CH4 NR43 poly byte, OR 0 = rest */
    uint8_t  duration;  /* frames; 0 reserved (unused — row_count is the bound) */
} mus_row_t;

typedef struct {
    const mus_row_t *rows;
    uint8_t row_count;
    uint8_t loop_idx;   /* 0xFF = stinger (one-shot) */
    uint8_t ch3_vol;    /* NR32 byte (e.g. 0x20 = 100%) */
    uint8_t ch4_env;    /* NR42 byte (envelope) */
} mus_song_t;

/* ===== Wave pattern 0 (mellow triangle, 32 4-bit samples) =============== */
static const uint8_t WAVE0[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
};

/* ===== Pitch + noise constants (mirror designer doc) ==================== */
#define P_REST 0x000
#define P_C4   0x706
#define P_D4   0x721
#define P_E4   0x739
#define P_F4   0x744
#define P_G4   0x759
#define P_A4   0x76B
#define P_B4   0x77B
#define P_C5   0x783
#define P_D5   0x790
#define P_E5   0x79D
#define P_F5   0x7A2
#define P_G5   0x7AC
#define P_A5   0x7B5
#define P_C6   0x7C1

#define N_REST 0x00
#define N_KICK 0x55
#define N_SNAR 0x32
#define N_HAT  0x21

/* ===== Songs ============================================================ */

static const mus_row_t ROWS_TITLE[] = {
    { P_C5, N_KICK, 24 }, { P_E5, N_REST, 24 }, { P_G5, N_HAT,  24 }, { P_E5, N_REST, 24 },
    { P_C5, N_KICK, 24 }, { P_E5, N_REST, 24 }, { P_G5, N_HAT,  24 }, { P_E5, N_REST, 24 },
    { P_A4, N_KICK, 24 }, { P_C5, N_REST, 24 }, { P_E5, N_HAT,  24 }, { P_C5, N_REST, 24 },
    { P_A4, N_KICK, 24 }, { P_C5, N_REST, 24 }, { P_E5, N_HAT,  24 }, { P_C5, N_REST, 24 },
    { P_F4, N_KICK, 24 }, { P_A4, N_REST, 24 }, { P_C5, N_HAT,  24 }, { P_A4, N_REST, 24 },
    { P_G4, N_KICK, 24 }, { P_B4, N_REST, 24 }, { P_D5, N_HAT,  24 }, { P_B4, N_SNAR, 24 },
};

static const mus_row_t ROWS_PLAYING[] = {
    { P_C5, N_KICK, 16 }, { P_C5, N_HAT,  16 }, { P_E5, N_REST, 16 }, { P_C5, N_SNAR, 16 },
    { P_G4, N_KICK, 16 }, { P_G4, N_HAT,  16 }, { P_B4, N_REST, 16 }, { P_G4, N_SNAR, 16 },
    { P_A4, N_KICK, 16 }, { P_A4, N_HAT,  16 }, { P_C5, N_REST, 16 }, { P_A4, N_SNAR, 16 },
    { P_F4, N_KICK, 16 }, { P_F4, N_HAT,  16 }, { P_A4, N_REST, 16 }, { P_F4, N_SNAR, 16 },
};

static const mus_row_t ROWS_WIN[] = {
    { P_C5, N_KICK, 12 },
    { P_E5, N_HAT,  12 },
    { P_G5, N_HAT,  12 },
    { P_C6, N_SNAR, 18 },
    { P_G5, N_REST, 12 },
    { P_C6, N_SNAR, 24 },
};

static const mus_row_t ROWS_LOSE[] = {
    { P_C5, N_KICK, 14 },
    { P_A4, N_REST, 14 },
    { P_F4, N_KICK, 14 },
    { P_D4, N_REST, 14 },
    { P_C4, N_KICK, 16 },
    { P_C4, N_SNAR, 24 },
};

#define ROW_COUNT(arr) (uint8_t)(sizeof(arr) / sizeof((arr)[0]))

static const mus_song_t S_SONGS[MUS_COUNT] = {
    [MUS_TITLE]   = { ROWS_TITLE,   ROW_COUNT(ROWS_TITLE),   0x00, 0x20, 0x82 },
    [MUS_PLAYING] = { ROWS_PLAYING, ROW_COUNT(ROWS_PLAYING), 0x00, 0x20, 0xA1 },
    [MUS_WIN]     = { ROWS_WIN,     ROW_COUNT(ROWS_WIN),     0xFF, 0x20, 0xC1 },
    [MUS_LOSE]    = { ROWS_LOSE,    ROW_COUNT(ROWS_LOSE),    0xFF, 0x20, 0xC2 },
};

/* ===== Engine state ===================================================== */

typedef struct {
    const mus_song_t *song;   /* NULL = idle */
    uint8_t row_idx;
    uint8_t frames_left;
    uint8_t ch4_blocked;      /* nonzero while SFX owns ch4 */
    uint8_t ch4_just_freed;   /* F2 latch: SFX ended; clears ch4_blocked at the
                                 END of the current music_tick (after arm).
                                 Guarantees the boundary tick that coincides
                                 with the SFX-end frame still sees blocked=1
                                 and skips ch4 arm — no same-frame zero-gap
                                 re-arm, no DMG click. */
    uint8_t ch4_arm_pending;  /* F2 deferred arm: arm_current_row sets this
                                 when it skipped CH4 because of a block.
                                 Fired at the TOP of the NEXT music_tick once
                                 ch4_blocked has been cleared, so a row that
                                 started during a block still gets its
                                 percussion the moment the block lifts. */
} music_state_t;

static music_state_t s_state;

/* ===== Hardware helpers ================================================= */

static void ch3_silence(void) {
    NR30_REG = 0x00;          /* DAC off — silence wave channel */
}

static void ch4_silence(void) {
    NR42_REG = 0x00;          /* DAC off (F2 convention) */
}

static void program_ch3(uint16_t pitch, uint8_t vol) {
    if (pitch == 0) {
        ch3_silence();
        return;
    }
    NR30_REG = 0x80;                                /* DAC on */
    NR32_REG = vol;                                 /* output level */
    NR33_REG = (uint8_t)(pitch & 0xFF);             /* freq lo */
    NR34_REG = (uint8_t)(0x80 | ((pitch >> 8) & 0x07)); /* trigger | hi */
}

static void program_ch4(uint8_t noise, uint8_t env) {
    if (noise == 0) return;          /* rest = leave channel alone */
    NR41_REG = 0x3F;                 /* length 0; we don't use length counter */
    NR42_REG = env;                  /* envelope */
    NR43_REG = noise;                /* poly byte */
    NR44_REG = 0x80;                 /* trigger; length-enable OFF */
}

static void load_wave_ram_once(void) {
    /* DMG corrupts wave RAM if written while ch3 DAC is on.
     * Sequence: DAC off -> 16 byte writes -> DAC stays off (music_play
     * turns it back on per-row). Called once at boot from music_init. */
    uint8_t i;
    NR30_REG = 0x00;
    for (i = 0; i < 16; i++) {
        _AUD3WAVERAM[i] = WAVE0[i];
    }
    /* Leave DAC off — first program_ch3() will turn it on with a trigger. */
}

/* Arm the registers for the row currently pointed to by s_state.row_idx
 * and (re)load frames_left. Used by both music_play (synchronous arm)
 * and music_tick (row boundary). */
static void arm_current_row(void) {
    const mus_row_t *r = &s_state.song->rows[s_state.row_idx];
    s_state.frames_left = r->duration;
    program_ch3(r->pitch, s_state.song->ch3_vol);
    if (!s_state.ch4_blocked) {
        program_ch4(r->noise, s_state.song->ch4_env);
        s_state.ch4_arm_pending = 0;
    } else if (r->noise != 0) {
        /* Skipped this row's percussion because SFX still owns CH4.
         * Remember to fire it as soon as the block lifts (handled at the
         * top of music_tick once ch4_just_freed has been promoted). */
        s_state.ch4_arm_pending = 1;
    }
}

/* ===== Public API ======================================================= */

void music_init(void) {
    /* Init-once: wave RAM load. Engine stays idle until music_play(). */
    s_state.song = 0;
    s_state.row_idx = 0;
    s_state.frames_left = 0;
    s_state.ch4_blocked = 0;
    s_state.ch4_just_freed = 0;
    s_state.ch4_arm_pending = 0;
    load_wave_ram_once();
}

void music_reset(void) {
    /* Master reset hook: called from audio_reset() (D-MUS-5).
     * Silence both channels, clear state. Wave RAM is preserved
     * (it's already correct from music_init). */
    ch3_silence();
    ch4_silence();
    s_state.song = 0;
    s_state.row_idx = 0;
    s_state.frames_left = 0;
    s_state.ch4_blocked = 0;
    s_state.ch4_just_freed = 0;
    s_state.ch4_arm_pending = 0;
}

void music_stop(void) {
    ch3_silence();
    ch4_silence();
    s_state.song = 0;
    s_state.row_idx = 0;
    s_state.frames_left = 0;
    s_state.ch4_arm_pending = 0;
}

void music_play(uint8_t song_id) {
    if (song_id >= MUS_COUNT) return;
    const mus_song_t *next = &S_SONGS[song_id];
    /* Idempotent for same song (review R1). */
    if (s_state.song == next) return;
    /* Different song: silence prior channels, reset position, arm row 0.
     * F1: do NOT clobber CH4 if an SFX currently owns it — silencing
     * NR42 here would truncate the SFX mid-play, and arm_current_row()
     * below also skips CH4 while ch4_blocked, leaving CH4 fully silent.
     * music_notify_ch4_free() will hand control back at the next row
     * boundary after the SFX completes. */
    if (s_state.song != 0) {
        ch3_silence();
        if (!s_state.ch4_blocked) {
            ch4_silence();
        }
    }
    s_state.song = next;
    s_state.row_idx = 0;
    /* Synchronous arm: write row-0 NRxx NOW so callers that block the
     * main loop (enter_gameover -> DISPLAY_OFF + 360-tile redraw) still
     * hear the stinger's first note. */
    arm_current_row();
}

void music_tick(void) {
    /* F2 deferred arm: a previous boundary advanced into a row whose
     * percussion couldn't be programmed because SFX still owned CH4.
     * Fire it now that the block has lifted. Runs FIRST (before the
     * latch promotion below) so the SFX-end frame's music_tick still
     * sees ch4_blocked=1 and skips this branch — the deferred arm
     * fires on the FOLLOWING tick instead, preserving the
     * "≥1 tick of percussion silence" contract. */
    if (s_state.ch4_arm_pending && !s_state.ch4_blocked && s_state.song != 0) {
        const mus_row_t *r = &s_state.song->rows[s_state.row_idx];
        program_ch4(r->noise, s_state.song->ch4_env);
        s_state.ch4_arm_pending = 0;
    }
    if (s_state.song == 0) {
        /* Even when idle, promote the latch so a stale ch4_just_freed
         * doesn't survive across an idle period. */
        if (s_state.ch4_just_freed) {
            s_state.ch4_blocked = 0;
            s_state.ch4_just_freed = 0;
        }
        return;
    }
    if (s_state.frames_left) s_state.frames_left--;
    if (s_state.frames_left == 0) {
        /* Row boundary crossed THIS tick — advance. arm_current_row
         * will skip CH4 (and set ch4_arm_pending) if blocked is still
         * set this tick; the latch promotion below clears blocked
         * only AFTER the arm decision, so a boundary that coincides
         * with the SFX-end frame defers ch4 re-arm to the next tick. */
        uint8_t nxt = music_next_row(s_state.song->row_count,
                                     s_state.song->loop_idx,
                                     s_state.row_idx);
        if (nxt == 0xFFu) {
            /* Stinger end: silence and idle. */
            music_stop();
            /* Still promote the latch so the freed flag doesn't leak. */
            if (s_state.ch4_just_freed) {
                s_state.ch4_blocked = 0;
                s_state.ch4_just_freed = 0;
            }
            return;
        }
        s_state.row_idx = nxt;
        arm_current_row();
    }
    /* End-of-tick latch promotion: SFX-end notify happened earlier this
     * frame (or earlier). Clear the block now so the NEXT music_tick
     * can fire the deferred arm (or arm normally at a future boundary). */
    if (s_state.ch4_just_freed) {
        s_state.ch4_blocked = 0;
        s_state.ch4_just_freed = 0;
    }
}

void music_duck(uint8_t on) {
    NR50_REG = on ? 0x33 : 0x77;
}

void music_notify_ch4_busy(void) {
    /* Unconditional from audio.c. No-op when music idle (still set the
     * flag; reset when music_stop/play clears state via re-init). */
    s_state.ch4_blocked = 1;
}

void music_notify_ch4_free(void) {
    /* F2: do NOT clear ch4_blocked directly. Set the just-freed latch
     * instead — music_tick() promotes it at the END of the current
     * tick (after the arm decision). Together with the deferred-arm
     * path in music_tick(), this guarantees that a row boundary
     * coinciding with the SFX-end frame skips ch4 (still blocked) and
     * the missed arm fires on the FOLLOWING tick. Net effect: ≥1
     * full music_tick of percussion silence between SFX end and music
     * ch4 re-arm — no zero-gap re-arm, no DMG click. */
    s_state.ch4_just_freed = 1;
}
