#ifndef GBTD_MAP_ANIM_H
#define GBTD_MAP_ANIM_H

/* Pure helper for the iter-3 #21 computer-corruption animation.
 * Self-contained — uses <stdint.h> only (NOT gtypes.h, which transitively
 * pulls <gb/gb.h>) so tests/test_anim.c can include it without GBDK
 * stubs. See specs/iter3-21-animations.md §3.2 / D13. */
#include <stdint.h>

/* Map current player HP -> 5-state corruption index (0 = pristine,
 * 4 = heaviest static). HP=0 reuses HP=1's state because the game-over
 * screen takes over the same frame anyway. HP > 5 (defensive) clamps
 * to pristine. */
static inline uint8_t map_hp_to_corruption_state(uint8_t hp) {
    if (hp >= 5) return 0;
    if (hp == 0) return 4;
    return (uint8_t)(5 - hp);   /* 4->1, 3->2, 2->3, 1->4 */
}

#endif /* GBTD_MAP_ANIM_H */
