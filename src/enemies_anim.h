#ifndef GBTD_ENEMIES_ANIM_H
#define GBTD_ENEMIES_ANIM_H

/* Pure helper for the iter-3 #21 enemy-hit-flash animation.
 * Self-contained — uses <stdint.h> only (NOT gtypes.h, which transitively
 * pulls <gb/gb.h>) so tests/test_anim.c can include it without GBDK
 * stubs. See specs/iter3-21-animations.md §3.3 / D13. */
#include <stdint.h>

/* Returns 1 iff a flash is in progress and decrements the timer in
 * place; otherwise returns 0 and leaves *timer untouched (already 0). */
static inline uint8_t enemies_flash_step(uint8_t *timer) {
    if (*timer == 0) return 0;
    (*timer)--;
    return 1;
}

#endif /* GBTD_ENEMIES_ANIM_H */
