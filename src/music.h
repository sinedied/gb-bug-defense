#ifndef GBTD_MUSIC_H
#define GBTD_MUSIC_H

/* Iter-3 #16 — custom mini music engine (D-MUS-1).
 *
 * Channel ownership (D-MUS-2):
 *   CH3 (wave/PCM) — music melody, never preempted by SFX.
 *   CH4 (noise)    — music percussion, preemptable by SFX_ENEMY_HIT/DEATH.
 *   CH1/CH2        — SFX-only; music NEVER touches them.
 *
 * Include direction (spec §"Include direction"): audio.c may include
 * music.h to forward ch4 notify edges. music.c MUST NOT include audio.h
 * (no circular deps; audio is the leaf).
 *
 * Synchronous arm: music_play(id) writes row 0 NR3x/NR4x BEFORE returning,
 * so a stinger's first note plays even when the caller blocks the main
 * loop for several frames (e.g. enter_gameover()'s DISPLAY_OFF + redraw).
 * Mirrors audio_play -> start_note in audio.c. */

#include <stdint.h>

enum { MUS_TITLE = 0, MUS_PLAYING, MUS_WIN, MUS_LOSE, MUS_COUNT };
/* No MUS_NONE. Use music_stop() for idle. */

void music_init(void);            /* init-once: load wave RAM (DAC-off->write->on) */
void music_reset(void);           /* silence ch3+ch4, clear state (called by audio_reset) */
void music_play(uint8_t song_id); /* synchronous arm: programs row 0 NRxx now */
void music_stop(void);            /* silence ch3+ch4, idle */
void music_tick(void);            /* advance one row tick; called from audio_tick */
void music_duck(uint8_t on);      /* on=1: NR50=0x33; on=0: NR50=0x77 */
void music_notify_ch4_busy(void); /* unconditional from audio_play(SFX_ENEMY_HIT/DEATH) */
void music_notify_ch4_free(void); /* unconditional from audio_tick edge-detect */

/* ---- Pure helpers (host-testable, <stdint.h>-only) -------------------- */

/* Compute the next row index after the current row finishes.
 *  - row_count : total rows in the song (>= 1)
 *  - loop_idx  : 0xFF for a stinger (one-shot), else the row to jump to
 *  - cur       : the row that just finished (0..row_count-1)
 * Returns either:
 *  - the next valid row index (0..row_count-1) to continue with, OR
 *  - 0xFF to indicate "song ended; engine should go idle".
 *
 * Stinger end:        loop_idx == 0xFF AND cur+1 == row_count -> 0xFF.
 * Loop wrap:          else if cur+1 == row_count            -> loop_idx.
 * Plain advance:      else                                   -> cur+1.
 */
static inline uint8_t music_next_row(uint8_t row_count,
                                     uint8_t loop_idx,
                                     uint8_t cur) {
    uint8_t nxt = (uint8_t)(cur + 1);
    if (nxt < row_count) return nxt;
    if (loop_idx == 0xFFu) return 0xFFu;     /* stinger end */
    return loop_idx;                          /* loop wrap */
}

#endif
