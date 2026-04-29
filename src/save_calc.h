#ifndef GBTD_SAVE_CALC_H
#define GBTD_SAVE_CALC_H

/* Iter-3 #19 — pure helpers for the SRAM save format.
 *
 * Header-only, `<stdint.h>`-only. Joins the host-testable family
 * (`tuning.h`, `game_modal.h`, `difficulty_calc.h`, `*_anim.h`,
 * `score_calc.h`). NEVER include `gtypes.h` or any GBDK header here.
 *
 * Layout (24 bytes at 0xA000, little-endian):
 *   off 0..3  magic   = 'G','B','T','D' (0x47 0x42 0x54 0x44)
 *   off 4     version = 0x01
 *   off 5     pad     = 0x00
 *   off 6..23 hi[9]   = 9 × u16 little-endian, slot = map_id*3 + diff
 */

#include <stdint.h>

#define SAVE_MAGIC0   0x47   /* 'G' */
#define SAVE_MAGIC1   0x42   /* 'B' */
#define SAVE_MAGIC2   0x54   /* 'T' */
#define SAVE_MAGIC3   0x44   /* 'D' */
#define SAVE_VERSION  0x01

#define SAVE_OFF_MAGIC    0
#define SAVE_OFF_VERSION  4
#define SAVE_OFF_PAD      5
#define SAVE_OFF_HI       6
#define SAVE_HI_COUNT     9
#define SAVE_TOTAL_BYTES  24

/* slot = map_id * 3 + diff. Caller is responsible for valid ranges
 * (map ∈ {0..2}, diff ∈ {0..2}). */
static inline uint8_t save_slot_index(uint8_t map_id, uint8_t diff) {
    return (uint8_t)((uint8_t)(map_id * 3u) + diff);
}
static inline uint8_t save_slot_offset(uint8_t map_id, uint8_t diff) {
    return (uint8_t)(SAVE_OFF_HI + (uint8_t)(save_slot_index(map_id, diff) * 2u));
}

static inline uint8_t save_magic_ok(const uint8_t *buf) {
    return (uint8_t)(buf[0] == SAVE_MAGIC0
                  && buf[1] == SAVE_MAGIC1
                  && buf[2] == SAVE_MAGIC2
                  && buf[3] == SAVE_MAGIC3
                  && buf[SAVE_OFF_VERSION] == SAVE_VERSION);
}

static inline void save_pack_u16(uint8_t *out, uint16_t v) {
    out[0] = (uint8_t)(v & 0xFFu);
    out[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline uint16_t save_unpack_u16(const uint8_t *in) {
    return (uint16_t)((uint16_t)in[0] | ((uint16_t)in[1] << 8));
}

#endif /* GBTD_SAVE_CALC_H */
