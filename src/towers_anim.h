#ifndef GBTD_TOWERS_ANIM_H
#define GBTD_TOWERS_ANIM_H

/* Pure helpers for the iter-3 #21 tower idle animation.
 * Self-contained — uses <stdint.h> only (NOT gtypes.h, which transitively
 * pulls <gb/gb.h>) so tests/test_anim.c can include it without GBDK
 * stubs. See specs/iter3-21-animations.md §3.4 / D17. */
#include <stdint.h>

/* Bit-reverse the low 4 bits of v. Internal — used by
 * towers_idle_phase_for() to spread adjacent placement slots evenly
 * across the LED period. */
static inline uint8_t _towers_bitrev4(uint8_t v) {
    v = (uint8_t)(((v & 0x0Cu) >> 2) | ((v & 0x03u) << 2));
    v = (uint8_t)(((v & 0x0Au) >> 1) | ((v & 0x05u) << 1));
    return (uint8_t)(v & 0x0Fu);
}

/* Returns the desired LED phase (0 or 1) for the given tower at the
 * given global animation frame.
 *
 * 64-frame full period: 32 frames OFF, 32 frames ON. Per-tower offset
 * uses bit-reversed 4-bit indexing of the placement slot so adjacent
 * slots (the order placement fills) are spread evenly across the
 * period. bitrev4({0,1,...,15}) = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
 * shifted left 2 = {0,32,16,48,8,40,24,56,4,36,20,52,12,44,28,60}, so
 * slot 0 vs slot 1 are exactly anti-phase. (D17, satisfies QA #10.) */
static inline uint8_t towers_idle_phase_for(uint8_t frame, uint8_t tower_idx) {
    uint8_t off = (uint8_t)(_towers_bitrev4(tower_idx & 0x0Fu) << 2);
    return (uint8_t)(((frame + off) >> 5) & 1u);
}

#endif /* GBTD_TOWERS_ANIM_H */
