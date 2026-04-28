#ifndef GBTD_TOWER_SELECT_H
#define GBTD_TOWER_SELECT_H

/* Iter-3 #18: pure helper for tower-type cycle.
 * <stdint.h>-only — host-testable without GBDK stubs. Joins the
 * tuning.h / difficulty_calc.h / game_modal.h / *_anim.h family. */
#include <stdint.h>

/* Cycle to the next tower type: AV → FW → EMP → AV (for count=3).
 * Adding a new tower type automatically extends the cycle. */
static inline uint8_t tower_select_next(uint8_t cur, uint8_t count) {
    return (uint8_t)((cur + 1u) % count);
}

#endif /* GBTD_TOWER_SELECT_H */
