#ifndef GBTD_MAP_SELECT_H
#define GBTD_MAP_SELECT_H

/* Iter-3 #17 — Map selector cycle helper.
 *
 * Pure helper header — `<stdint.h>` only, no GBDK pull-in. Joins the
 * `tuning.h` / `game_modal.h` / `*_anim.h` / `difficulty_calc.h` /
 * `tower_select.h` / `music.h` family so host tests can include this
 * header directly without `gtypes.h` / `<gb/gb.h>`.
 *
 * MAP_COUNT is duplicated as a literal here (keep in sync with
 * `src/map.h`) so this header stays free of `map.h` (which pulls in
 * `gtypes.h`/GBDK transitively for `i8`/`u8`). The cycle is a 3-state
 * wrap mirroring `difficulty_cycle_*`. */

#include <stdint.h>

#ifndef MAP_SELECT_COUNT
#define MAP_SELECT_COUNT 3u
#endif

static inline uint8_t map_cycle_left(uint8_t m) {
    return (m == 0) ? (uint8_t)(MAP_SELECT_COUNT - 1u) : (uint8_t)(m - 1u);
}
static inline uint8_t map_cycle_right(uint8_t m) {
    return (m + 1u >= MAP_SELECT_COUNT) ? 0u : (uint8_t)(m + 1u);
}

#endif /* GBTD_MAP_SELECT_H */
