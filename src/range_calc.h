#ifndef GBTD_RANGE_CALC_H
#define GBTD_RANGE_CALC_H

/* Pure helper: circle-dot position computation for tower range preview.
 * Header-only, <stdint.h> only — testable on host without GBDK linkage.
 * Follows the gate_calc.h / towers_anim.h / map_anim.h pattern. */

#include <stdint.h>

typedef struct {
    uint8_t x;  /* screen-pixel center x; (255,255) = hidden sentinel */
    uint8_t y;  /* screen-pixel center y */
} range_dot_t;

/* 8 dots at 45° intervals. cos/sin scaled by 64.
 * Index 0 = 0°, 1 = 45°, 2 = 90°, ... 7 = 315°. */
static const int8_t _range_cos64[8] = {  64,  45,   0, -45, -64, -45,   0,  45 };
static const int8_t _range_sin64[8] = {   0,  45,  64,  45,   0, -45, -64, -45 };

/* Compute one dot's pixel center position on a circle.
 * cx_px, cy_px: screen-pixel center of the tower.
 * range_px: radius in pixels.
 * dot_idx: 0..7 (masked to 3 bits internally for safety).
 * Returns screen-pixel center; (255,255) if the dot would be off-screen. */
static inline range_dot_t range_calc_dot(uint8_t cx_px, uint8_t cy_px,
                                         uint8_t range_px, uint8_t dot_idx)
{
    range_dot_t result;
    int16_t dx, dy, px, py;
    uint8_t i = dot_idx & 7u;

    dx = ((int16_t)range_px * _range_cos64[i]) / 64;
    dy = ((int16_t)range_px * _range_sin64[i]) / 64;

    px = (int16_t)cx_px + dx;
    py = (int16_t)cy_px + dy;

    /* Off-screen clipping: GB screen is 160×144. */
    if (px < 0 || px > 159 || py < 0 || py > 143) {
        result.x = 255;
        result.y = 255;
        return result;
    }

    result.x = (uint8_t)px;
    result.y = (uint8_t)py;
    return result;
}

#endif /* GBTD_RANGE_CALC_H */
