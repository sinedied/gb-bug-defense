#include "map.h"
#include "assets.h"
#include <gb/gb.h>

/* Authoritative waypoints (verbatim from spec §6 / mvp-design.md §3). */
static const waypoint_t s_waypoints[WAYPOINT_COUNT] = {
    { -1,  2 },
    {  0,  2 },
    {  8,  2 },
    {  8,  9 },
    { 15,  9 },
    { 15,  5 },
    { 17,  5 },
    { 18,  5 },
};

static bool s_damaged;
static bool s_damaged_dirty;

void map_load(void) {
    u8 row;
    s_damaged = false;
    s_damaged_dirty = false;
    /* Caller MUST bracket with DISPLAY_OFF/ON — this writes ~340 BG tiles
     * which is far more than fits in a single VBlank window. */
    for (row = 0; row < PF_ROWS; row++) {
        set_bkg_tiles(0, row + HUD_ROWS, PF_COLS, 1,
                      &gameplay_tilemap[row * PF_COLS]);
    }
}

u8 map_class_at(u8 tx, u8 ty) {
    if (tx >= PF_COLS || ty >= PF_ROWS) return TC_PATH; /* off-field = invalid */
    return gameplay_classmap[ty * PF_COLS + tx];
}

const waypoint_t *map_waypoints(void) { return s_waypoints; }

void map_set_computer_damaged(bool damaged) {
    if (damaged == s_damaged) return;
    s_damaged = damaged;
    s_damaged_dirty = true;     /* deferred to map_render() (VBlank-safe) */
}

void map_render(void) {
    if (!s_damaged_dirty) return;
    s_damaged_dirty = false;
    /* Computer occupies pf rows 4..5, cols 18..19 -> screen rows 5..6 */
    u8 sr = HUD_ROWS + 4;
    if (!s_damaged) {
        set_bkg_tile_xy(18, sr,     (u8)TILE_COMP_TL);
        set_bkg_tile_xy(19, sr,     (u8)TILE_COMP_TR);
        set_bkg_tile_xy(18, sr + 1, (u8)TILE_COMP_BL);
        set_bkg_tile_xy(19, sr + 1, (u8)TILE_COMP_BR);
    } else {
        set_bkg_tile_xy(18, sr,     (u8)TILE_COMP_TL_D);
        set_bkg_tile_xy(19, sr,     (u8)TILE_COMP_TR_D);
        set_bkg_tile_xy(18, sr + 1, (u8)TILE_COMP_BL_D);
        set_bkg_tile_xy(19, sr + 1, (u8)TILE_COMP_BR_D);
    }
}
