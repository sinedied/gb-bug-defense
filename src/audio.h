#ifndef GBTD_AUDIO_H
#define GBTD_AUDIO_H

#include "gtypes.h"

enum {
    SFX_TOWER_PLACE = 0,
    SFX_TOWER_FIRE,
    SFX_ENEMY_HIT,
    SFX_ENEMY_DEATH,
    /* SFX_WIN / SFX_LOSE removed in iter-3 #16 (D-MUS-3). Replaced by
     * MUS_WIN / MUS_LOSE stinger songs in src/music.c. */
    SFX_BOOT,                   /* one-shot chime fired from audio_init() so
                                 * the user immediately knows whether audio
                                 * reaches the emulator. See decisions.md
                                 * "Iter-2 audio diag: boot chime". */
    SFX_COUNT
};

void audio_init(void);          /* call once after gfx_init; also calls
                                 * music_init() (D-MUS-1 init-once). */
void audio_reset(void);         /* MASTER reset (D-MUS-5): zero per-channel
                                 * SFX state, silence ch1/2/4, AND call
                                 * music_reset() (silence ch3+ch4 music).
                                 * Does NOT touch NR50/51/52 master regs
                                 * (preserves F1 — duck state survives
                                 * reset; quit-to-title restores NR50
                                 * explicitly via music_duck(0)). */
void audio_play(u8 sfx_id);
void audio_tick(void);          /* once per frame; also drives music_tick()
                                 * (single audio entry point). */

#endif
