#ifndef GBTD_AUDIO_H
#define GBTD_AUDIO_H

#include "gtypes.h"

enum {
    SFX_TOWER_PLACE = 0,
    SFX_TOWER_FIRE,
    SFX_ENEMY_HIT,
    SFX_ENEMY_DEATH,
    SFX_WIN,
    SFX_LOSE,
    SFX_COUNT
};

void audio_init(void);          /* call once after gfx_init */
void audio_reset(void);         /* zero per-channel state, silence ch1/2/4.
                                 * Call from each game-state transition that
                                 * starts a new gameplay session so the
                                 * win/lose jingle priority on ch1 doesn't
                                 * leak across sessions and block all
                                 * subsequent SFX. */
void audio_play(u8 sfx_id);
void audio_tick(void);          /* once per frame */

#endif
