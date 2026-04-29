#ifndef GBTD_GATE_CALC_H
#define GBTD_GATE_CALC_H

#include <stdint.h>

#ifndef bool
#define bool unsigned char
#define true 1
#define false 0
#endif

/* Returns 1 (text visible) or 0 (text hidden).
 * Toggle every 30 frames = 1 Hz blink. */
static inline uint8_t gate_blink_visible(uint8_t frame_counter) {
    return ((frame_counter / 30u) & 1u) == 0 ? 1 : 0;
}

#endif /* GBTD_GATE_CALC_H */
