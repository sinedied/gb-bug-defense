# Upgraded Tower Visual

## Problem
L1 (upgraded) towers display the same BG tile as L0, giving no visual feedback that an upgrade occurred. Players cannot distinguish L0 from L1 towers at a glance. Feature #26 adds 6 new tile designs (main + idle-alt for each of the 3 tower types) and wires `towers_upgrade()` to trigger a tile repaint.

## Architecture

### Struct extension
Add `bg_tile_l1` and `bg_tile_alt_l1` fields to `tower_stats_t` (in `towers.h`), placed after the existing `bg_tile_alt` field. This keeps tile fields grouped together. The struct grows from 13 to 15 bytes (ROM-resident `static const`, no WRAM impact).

### Tile selection logic
`towers_render()` Phase 2 (placement/dirty) and Phase 3 (idle LED) branch on `s_towers[i].level` to select `bg_tile`/`bg_tile_alt` (L0) vs `bg_tile_l1`/`bg_tile_alt_l1` (L1). No new phases; no budget change (still ≤1 write/frame from towers).

### Upgrade trigger
`towers_upgrade()` sets `dirty = 1` and `idle_phase = 0` after incrementing `spent`. Phase 2 writes the L1 base tile on the next VBlank. The `idle_phase = 0` reset prevents a stale phase: without it, if the tower was displaying the L0 alt tile (`idle_phase = 1`) when upgraded, Phase 3 would compare `want` against the stale `idle_phase = 1`, see a match, and skip the write — leaving the base L1 tile painted but `idle_phase` claiming alt. The stale display persists for up to 32 frames (one full LED half-period) until `want` flips.

### Asset additions
6 new BG tiles appended to `map_tiles` in `gen_assets.py`. MAP_TILE_COUNT grows 23 → 29.

### BG write budget impact
The upgrade-close frame now always produces exactly 16 writes (HUD energy 3 + menu BG restore 12 + tower Phase 2 dirty 1). This is at the hard cap with zero margin. Prior to this feature, towers contributed 0–1 on this path (idle Phase 3 was conditional); now Phase 2 fires unconditionally. Future changes that add BG writes to the upgrade-close render frame must audit this path first.

## L1 Tile Designs

All designs use `design_tile()` encoding: `.`=White, `,`=Light grey, `o`=Dark grey, `#`=Black.

### Design principle
L1 tiles retain each tower type's identity (dish, wall, rings) but add two upgrade markers: (a) **filled corners** and (b) **denser/brighter center**. Each alt tile has a 1-pixel LED diff. The LED direction may differ from L0 (L0 adds brightness; L1 may dim it) — this is intentional and acceptable since L0 and L1 tiles are visually distinct enough that same-screen mixed direction does not confuse.

### AV L1 — "Hardened Scanner"
Preserves the AV's distinctive spoke pattern (#o#). Black corner pixels + light-grey center glow.

Main:
```
#,o##o,#
,o####o,
o##oo##o
#o#,,#o#    ← center now light-grey (was white in L0)
#o#..#o#
o##oo##o
,o####o,
#,o##o,#
```
Alt (blink) — LED at [3][4]: `,` → `.`
```
#,o##o,#
,o####o,
o##oo##o
#o#,.#o#
#o#..#o#
o##oo##o
,o####o,
#,o##o,#
```

### FW L1 — "Fortified Wall"
Preserves the FW's brick-mortar pattern. Sealed corners + wider 2-pixel ember glow.

Main:
```
########    ← corners filled (was .######.)
#o##o##o
########
o##,,#oo    ← double ember (was single ,)
########
#o##o##o
########
########    ← corners filled
```
Alt (blink) — LED at [3][3]: `,` → `.`
```
########
#o##o##o
########
o##.,#oo
########
#o##o##o
########
########
```

### EMP L1 — "Supercharged Coil"
Preserves the EMP's concentric-ring motif. Light-grey corner field + solid inner ring + light-grey charged center.

Main:
```
.,o##o.,    ← corner light-grey (was white in L0)
.o####o.
o######o    ← inner ring filled solid (was oo in L0)
##o,,o##    ← center now light-grey (was white in L0)
##o,,o##
o######o
.o####o.
.,o##o.,
```
Alt (blink) — LED at [3][3]: `,` → `.`
```
.,o##o.,
.o####o.
o######o
##o.,o##
##o,,o##
o######o
.o####o.
.,o##o.,
```

## Subtasks

1. ✅ **Add L1 tile designs to gen_assets.py** — Define 6 new `design_tile()` variables (`TOWER_L1_BG`, `TOWER_L1_BG_B`, `TOWER2_L1_BG`, `TOWER2_L1_BG_B`, `TOWER3_L1_BG`, `TOWER3_L1_BG_B`) after the existing tower tile definitions (~line 516). Append 6 entries to `map_tiles` list (~line 544). Update the hardcoded `assets_h` string (~line 1070) with 6 new `#define TILE_TOWER_*_L1*` constants and `MAP_TILE_COUNT 29`. Run `just assets` to regenerate `res/assets.{c,h}`. Done when `res/assets.h` contains all 6 `TILE_TOWER_*_L1*` defines and `MAP_TILE_COUNT` equals 29.

2. ✅ **Extend tower_stats_t with L1 tile fields** — Add `u8 bg_tile_l1` and `u8 bg_tile_alt_l1` to `tower_stats_t` in `towers.h`, placed between `bg_tile_alt` and `hud_letter`. Update the 3 entries in `s_tower_stats[]` in `towers.c` to include `.bg_tile_l1` and `.bg_tile_alt_l1` with the new tile constants. Done when `just build` compiles with no warnings.
   - Depends on: subtask 1

3. ✅ **Wire towers_upgrade() to trigger tile repaint** — In `towers_upgrade()` in `towers.c`, add `s_towers[idx].dirty = 1;` and `s_towers[idx].idle_phase = 0;` after `s_towers[idx].spent += cost;` (before the cooldown-reset block). Done when upgrading a tower sets the dirty flag and resets idle_phase.
   - Depends on: subtask 2

4. ✅ **Update towers_render() tile selection** — In Phase 2, replace the direct `s_tower_stats[s_towers[i].type].bg_tile` with a level branch (`level ? st->bg_tile_l1 : st->bg_tile`). In Phase 3, replace the `base`/`alt` assignments with level branches. Done when L1 towers display L1 tiles in both placement and idle-animation phases.
   - Depends on: subtask 2

## Exact Code Changes

### gen_assets.py — new tile definitions (after ~line 516)

```python
# Iter-4 #26: L1 (upgraded) tower BG tiles.
# Design: L0 identity preserved + filled corners + denser/brighter center.
# Each alt has a 1-pixel LED diff matching the L0 blink convention.

TOWER_L1_BG = design_tile([
    "#,o##o,#",
    ",o####o,",
    "o##oo##o",
    "#o#,,#o#",
    "#o#..#o#",
    "o##oo##o",
    ",o####o,",
    "#,o##o,#",
])

TOWER_L1_BG_B = design_tile([
    "#,o##o,#",
    ",o####o,",
    "o##oo##o",
    "#o#,.#o#",
    "#o#..#o#",
    "o##oo##o",
    ",o####o,",
    "#,o##o,#",
])

TOWER2_L1_BG = design_tile([
    "########",
    "#o##o##o",
    "########",
    "o##,,#oo",
    "########",
    "#o##o##o",
    "########",
    "########",
])

TOWER2_L1_BG_B = design_tile([
    "########",
    "#o##o##o",
    "########",
    "o##.,#oo",
    "########",
    "#o##o##o",
    "########",
    "########",
])

TOWER3_L1_BG = design_tile([
    ".,o##o.,",
    ".o####o.",
    "o######o",
    "##o,,o##",
    "##o,,o##",
    "o######o",
    ".o####o.",
    ".,o##o.,",
])

TOWER3_L1_BG_B = design_tile([
    ".,o##o.,",
    ".o####o.",
    "o######o",
    "##o.,o##",
    "##o,,o##",
    "o######o",
    ".o####o.",
    ".,o##o.,",
])
```

### gen_assets.py — append to map_tiles list (after the TILE_TOWER_3_B entry)

```python
    # Iter-4 #26: L1 tower tiles.
    ('TILE_TOWER_L1',      TOWER_L1_BG),
    ('TILE_TOWER_L1_B',    TOWER_L1_BG_B),
    ('TILE_TOWER_2_L1',    TOWER2_L1_BG),
    ('TILE_TOWER_2_L1_B',  TOWER2_L1_BG_B),
    ('TILE_TOWER_3_L1',    TOWER3_L1_BG),
    ('TILE_TOWER_3_L1_B',  TOWER3_L1_BG_B),
```

### gen_assets.py — update assets_h string

Replace `#define MAP_TILE_COUNT  23` with:
```c
#define TILE_TOWER_L1     (MAP_TILE_BASE + 23)
#define TILE_TOWER_L1_B   (MAP_TILE_BASE + 24)
#define TILE_TOWER_2_L1   (MAP_TILE_BASE + 25)
#define TILE_TOWER_2_L1_B (MAP_TILE_BASE + 26)
#define TILE_TOWER_3_L1   (MAP_TILE_BASE + 27)
#define TILE_TOWER_3_L1_B (MAP_TILE_BASE + 28)
#define MAP_TILE_COUNT  29
```

### towers.h — struct extension

```c
typedef struct {
    u8  cost;
    u8  upgrade_cost;
    u8  cooldown;       /* level 0 */
    u8  cooldown_l1;
    u8  damage;         /* only for TKIND_DAMAGE */
    u8  damage_l1;      /* only for TKIND_DAMAGE */
    u8  range_px;       /* level-independent */
    u8  bg_tile;        /* TILE_TOWER / TILE_TOWER_2 / TILE_TOWER_3 */
    u8  bg_tile_alt;    /* idle-blink tile per type (iter-3 #18) */
    u8  bg_tile_l1;     /* L1 BG tile per type (iter-4 #26) */
    u8  bg_tile_alt_l1; /* L1 idle-blink tile per type (iter-4 #26) */
    u8  hud_letter;     /* 'A', 'F', or 'E' */
    u8  kind;           /* TKIND_DAMAGE or TKIND_STUN */
    u8  stun_frames;    /* only for TKIND_STUN — level 0 */
    u8  stun_frames_l1; /* only for TKIND_STUN — level 1 */
} tower_stats_t;
```

### towers.c — s_tower_stats[] initializer

Add `.bg_tile_l1` and `.bg_tile_alt_l1` after `.bg_tile_alt` for each type:
```c
/* AV */  .bg_tile_l1 = TILE_TOWER_L1, .bg_tile_alt_l1 = TILE_TOWER_L1_B,
/* FW */  .bg_tile_l1 = TILE_TOWER_2_L1, .bg_tile_alt_l1 = TILE_TOWER_2_L1_B,
/* EMP */ .bg_tile_l1 = TILE_TOWER_3_L1, .bg_tile_alt_l1 = TILE_TOWER_3_L1_B,
```

### towers.c — towers_upgrade()

Add after `s_towers[idx].spent += cost;`:
```c
    s_towers[idx].dirty = 1;         /* iter-4 #26: schedule L1 tile repaint */
    s_towers[idx].idle_phase = 0;    /* matches the about-to-be-painted L1 base tile */
```

### towers.c — towers_render() Phase 2

Replace:
```c
            set_bkg_tile_xy(s_towers[i].tx,
                            s_towers[i].ty + HUD_ROWS,
                            s_tower_stats[s_towers[i].type].bg_tile);
```
With:
```c
            const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
            set_bkg_tile_xy(s_towers[i].tx,
                            s_towers[i].ty + HUD_ROWS,
                            s_towers[i].level ? st->bg_tile_l1 : st->bg_tile);
```

### towers.c — towers_render() Phase 3

Replace:
```c
        u8 base = s_tower_stats[s_towers[i].type].bg_tile;
        u8 alt  = s_tower_stats[s_towers[i].type].bg_tile_alt;
```
With:
```c
        const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
        u8 base = s_towers[i].level ? st->bg_tile_l1     : st->bg_tile;
        u8 alt  = s_towers[i].level ? st->bg_tile_alt_l1  : st->bg_tile_alt;
```

## Acceptance Scenarios

### Setup
`just setup && just build && just run`. Place towers, upgrade them, observe tile changes.

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Upgrade shows new tile | Place AV tower. Open menu, select upgrade. Close menu. | Tower tile changes to L1 design within 1 frame of menu close. |
| 2 | All 3 types have distinct L1 tiles | Place and upgrade one of each type. | Each L1 tower is visually distinct from each other and from its L0 version. |
| 3 | L1 idle animation works | Upgrade a tower and wait 3+ seconds. | L1 tower blinks with its L1 alt tile (1-pixel LED diff), same 64-frame period as L0. |
| 4 | L0 towers unaffected | Place a tower but do not upgrade. | Tower displays the same L0 tile and blink as before. |
| 5 | Sell L1 tower restores ground | Upgrade a tower, then sell it. | Ground tile restored via Phase 1 sell-clear; no L1 tile remnant. |
| 6 | Mixed L0/L1 idle coexist | Place 4+ towers. Upgrade only 2. Wait. | L0 towers blink L0 tiles, L1 towers blink L1 tiles. No cross-contamination. |
| 7 | Upgrade during alt-phase repaint | Wait until a tower is visibly in its blink/alt phase, then open menu and upgrade. | On menu close, tower repaints immediately to L1 base tile. Subsequent idle toggles are normal with no stale mismatch (no 32-frame stuck period). |

## Constraints
- **BG write budget**: ≤16 writes/frame cap. Tower contribution stays ≤1 write/frame. The upgrade-close frame now always produces exactly 16 writes (HUD energy 3 + menu BG restore 12 + tower Phase 2 dirty 1), eliminating the prior 0–1 margin from conditional idle. Future features adding BG writes to the upgrade-close path must audit this.
- **Tile budget**: 6 new BG tiles. MAP_TILE_COUNT 23 → 29. Well within the ~105 remaining slots.
- **ROM growth**: ~110 bytes (96 tile data + 6 struct bytes + ~8 code bytes). Negligible.
- **No WRAM growth**: struct is `static const` ROM; no per-instance tower state added.
- **No new test file**: no pure-helper logic added; changes are rendering + struct data.

## Decisions
| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| L1 design principle | (A) Completely different shapes per level; (B) Same shape + corner/center accents | B — same shape + filled corners + denser center | Preserves tower identity (dish/wall/ring) at a glance; corners are the clearest "upgraded" marker on 8×8 pixels. |
| idle_phase reset on upgrade | (A) Reset to 0; (B) Leave as-is | A — reset to 0 | Phase 2 writes the base L1 tile; idle_phase must match (0) to prevent Phase 3 from misinterpreting the current tile as the alt. Without reset, up to 32 frames of stale display possible (one full LED half-period). |
| L1 LED blink direction | (A) Match L0 direction; (B) Allow reversal | B — allow reversal | L1 tiles have a light-grey center glow (was white in L0), so the "off" phase naturally dims to white. Since L0 and L1 tiles are visually distinct, the opposite blink direction does not confuse. Forcing same direction would require reworking the center design. |
| New field placement in struct | (A) After bg_tile_alt; (B) At end of struct | A — after bg_tile_alt | Groups all tile fields together for readability and natural extension to L2 (#27). |
| Tile constant naming | (A) TILE_TOWER_L1 / TILE_TOWER_2_L1; (B) TILE_TOWER_AV_L1 | A — TILE_TOWER_*_L1 | Consistent with existing pattern (TILE_TOWER, TILE_TOWER_2, TILE_TOWER_3). |

## Review
- **F1 (MEDIUM): Budget margin eliminated on upgrade-close frame.** Addressed: Constraints section and Architecture §"BG write budget impact" now explicitly document that the upgrade-close frame is always exactly 16 writes (zero margin) and future features must audit this path.
- **F2 (MEDIUM, cross-model): Missing acceptance scenario for idle_phase stale edge case.** Addressed: Added scenario #7 that explicitly exercises upgrading during the alt/blink phase.
- **Note: idle_phase stale window magnitude.** Fixed: rationale now correctly states "up to 32 frames" (one full LED half-period), not 16.
- **Note: AV/EMP L1 blink direction reversed from L0.** Addressed: documented as an intentional design choice in the Decisions table with rationale (L1 center glow naturally reverses the toggle direction; L0/L1 are distinct enough visually).
- **Note: Feature #27 (L2) forward compatibility.** The binary `level ? L1 : L0` branch will naturally extend to a three-way branch when L2 is added. Struct field placement (tile fields grouped together) supports clean extension.
