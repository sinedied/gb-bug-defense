#include "map.h"
#include "map_anim.h"
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

/* Iter-3 #21: computer corruption is a 5-state HP-keyed progression
 * (see specs/iter3-21-animations.md §3.2). LUT is indexed
 * [state][quadrant] where quadrant order is TL, TR, BL, BR. State 4
 * reuses the existing _D tile set. */
static const u8 s_comp_tiles[5][4] = {
    /* state 0 - pristine */
    { (u8)TILE_COMP_TL,    (u8)TILE_COMP_TR,    (u8)TILE_COMP_BL,    (u8)TILE_COMP_BR    },
    /* state 1 - single stuck pixel (TL only) */
    { (u8)TILE_COMP_TL_C1, (u8)TILE_COMP_TR,    (u8)TILE_COMP_BL,    (u8)TILE_COMP_BR    },
    /* state 2 - horizontal scanline tear across TL+TR */
    { (u8)TILE_COMP_TL_C2, (u8)TILE_COMP_TR_C2, (u8)TILE_COMP_BL,    (u8)TILE_COMP_BR    },
    /* state 3 - diagonal crack + dead pixels across all 4 */
    { (u8)TILE_COMP_TL_C3, (u8)TILE_COMP_TR_C3, (u8)TILE_COMP_BL_C3, (u8)TILE_COMP_BR_C3 },
    /* state 4 - heavy static (HP=1 and HP=0 share this) */
    { (u8)TILE_COMP_TL_D,  (u8)TILE_COMP_TR_D,  (u8)TILE_COMP_BL_D,  (u8)TILE_COMP_BR_D  },
};

static u8 s_state;
static u8 s_state_dirty;

void map_load(void) {
    u8 row;
    /* Reset corruption tracking so a quit-from-lost-run -> new-game does
     * NOT redundantly repaint the freshly-pristine cluster on the first
     * post-enter_playing frame. The gameplay tilemap already encodes
     * state 0. */
    s_state = 0;
    s_state_dirty = 0;
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

void map_set_computer_state(u8 hp) {
    u8 ns = map_hp_to_corruption_state(hp);
    if (ns == s_state) return;
    s_state = ns;
    s_state_dirty = 1;     /* deferred to map_render() (VBlank-safe) */
}

void map_render(void) {
    if (!s_state_dirty) return;
    s_state_dirty = 0;
    /* Computer occupies pf rows 4..5, cols 18..19 -> screen rows 5..6 */
    u8 sr = HUD_ROWS + 4;
    set_bkg_tile_xy(18, sr,     s_comp_tiles[s_state][0]);
    set_bkg_tile_xy(19, sr,     s_comp_tiles[s_state][1]);
    set_bkg_tile_xy(18, sr + 1, s_comp_tiles[s_state][2]);
    set_bkg_tile_xy(19, sr + 1, s_comp_tiles[s_state][3]);
}
