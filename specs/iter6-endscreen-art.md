# End-Screen Art: Game Over & Victory

## Problem
The game-over and victory screens are text-only placeholders (`lose_map()` / `win_map()` in `gen_assets.py`) that display flavour text without any graphical art. The title screen demonstrates that tile-art composition (block letters, circuit traces, silhouette motifs) creates a strong visual identity. The end screens should match that quality and provide satisfying closure to each play session.

## Design

### Design Principles Applied
- **Match title screen art quality** — same silhouette style, same circuit-trace vocabulary
- **Tell the story visually** — Lose = "bugs overran defenses", Win = "towers protected the system"
- **High contrast between screens** — Lose is noticeably darker/heavier than Win
- **Maximize reuse** — existing title tiles carry the composition; only 6 new tiles needed
- **DMG-first** — all art reads clearly in 4-shade monochrome on real hardware

---

### Layout: GAME OVER Screen (Lose)

Theme: System corrupted. Digital static, broken circuits, swarming bugs.

```
     Col: 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
Row  0:  ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST
Row  1:  ·  ·  ·  ·  ·  G  A  M  E  ·  O  V  E  R  ·  ·  ·  ·  ·  ·
Row  2:  ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST
Row  3:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row  4:  ·  ·  ND BK BK BK BK BK BK BK BK BK BK BK BK BK BK ND ·  ·
Row  5:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row  6:  ·  ·  ·  ·  BT BK BK BK ·  DT DR ·  BK BK BK BT ·  ·  ·  ·
Row  7:  ·  ·  ·  ·  BB ·  ·  ·  ·  DB DX ·  ·  ·  ·  BB ·  ·  ·  ·
Row  8:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row  9:  ·  ·  ND HL HL HL HL HL HL HL HL HL HL HL HL HL HL ND ·  ·
Row 10:  ·  ·  ·  ·  ·  ·  BT ·  ·  SK ·  ·  BT ·  ·  ·  ·  ·  ·  ·
Row 11:  ·  ·  ·  ·  ·  ·  BB ·  ·  SB ·  ·  BB ·  ·  ·  ·  ·  ·  ·
Row 12:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row 13:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ← RESERVED
Row 14:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ← RESERVED
Row 15:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ← RESERVED
Row 16:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row 17:  ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST ST
```

**Legend:**
| Symbol | Tile | Source |
|--------|------|--------|
| `·` | Space (32) | Existing (font) |
| ST | `ENDSCR_STATIC` | **NEW** |
| ND | `TITLE_NODE` | Existing (title art) |
| BK | `ENDSCR_BROKEN_HL` | **NEW** |
| HL | `TITLE_HLINE` | Existing (title art) |
| BT | `TITLE_BUG_T` | Existing (title art) |
| BB | `TITLE_BUG_B` | Existing (title art) |
| DT | `TILE_COMP_TL_D` | Existing (gameplay) |
| DR | `TILE_COMP_TR_D` | Existing (gameplay) |
| DB | `TILE_COMP_BL_D` | Existing (gameplay) |
| DX | `TILE_COMP_BR_D` | Existing (gameplay) |
| SK | `ENDSCR_SKULL_T` | **NEW** |
| SB | `ENDSCR_SKULL_B` | **NEW** |
| G,A,M,E,O,V,R | Font glyphs | Existing (font 0-127) |

**Composition notes:**
- Rows 0, 2, 17: Full-width STATIC bands (3×20=60 dark tiles) create a "CRT failure" frame
- Row 1: "GAME OVER" (9 chars) at col 5 — `floor((20-9)/2)=5` — visible against white between noise
- Row 4: Broken circuit divider — NODE endpoints with BROKEN_HL fill (damaged trace)
- Rows 6–7: Corrupted scene — mirrors title layout exactly but with bugs on BOTH sides (tower is gone, replaced by second bug) and damaged computer at center, connected by broken traces
- Row 9: Intact circuit divider separates art from score area
- Rows 10–11: Death motif — skull flanked by bugs, centered — `floor((20-7)/2)=6`, width 7 (BUG+2blank+SKULL+2blank+BUG)
- Row 12: Blank breathing room before reserved score area
- Rows 13–15: RESERVED — left blank for code-drawn text overlays

---

### Layout: VICTORY Screen (Win)

Theme: System secured. Clean circuits, triumphant towers, bright and open.

```
     Col: 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
Row  0:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row  1:  ·  ·  ·  ·  ·  ·  V  I  C  T  O  R  Y  !  ·  ·  ·  ·  ·  ·
Row  2:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row  3:  ·  ·  ND HL HL HL HL HL HL HL HL HL HL HL HL HL HL ND ·  ·
Row  4:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row  5:  ·  ·  ·  ·  TT HL HL HL ·  CT CR ·  HL HL HL TT ·  ·  ·  ·
Row  6:  ·  ·  ·  ·  TB ·  ·  ·  ·  CB CX ·  ·  ·  ·  TB ·  ·  ·  ·
Row  7:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row  8:  ·  ·  ND HL HL HL HL HL HL HL HL HL HL HL HL HL HL ND ·  ·
Row  9:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row 10:  ·  ·  ·  ·  ·  ·  TT ·  ·  SH ·  ·  TT ·  ·  ·  ·  ·  ·  ·
Row 11:  ·  ·  ·  ·  ·  ·  TB ·  ·  SS ·  ·  TB ·  ·  ·  ·  ·  ·  ·
Row 12:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row 13:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ← RESERVED
Row 14:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ← RESERVED
Row 15:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ← RESERVED
Row 16:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
Row 17:  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
```

**Legend (additional to shared tiles):**
| Symbol | Tile | Source |
|--------|------|--------|
| TT | `TITLE_TW_T` | Existing (title art) |
| TB | `TITLE_TW_B` | Existing (title art) |
| CT | `TILE_COMP_TL` | Existing (gameplay) |
| CR | `TILE_COMP_TR` | Existing (gameplay) |
| CB | `TILE_COMP_BL` | Existing (gameplay) |
| CX | `TILE_COMP_BR` | Existing (gameplay) |
| SH | `ENDSCR_SHIELD_T` | **NEW** |
| SS | `ENDSCR_SHIELD_B` | **NEW** |
| V,I,C,T,O,R,Y,! | Font glyphs | Existing (font 0-127) |

**Composition notes:**
- Row 0: Blank — immediate brightness/openness contrast vs lose screen's static band
- Row 1: "VICTORY!" (8 chars) at col 6 — `floor((20-8)/2)=6` — same width as title's "DEFENDER"
- Rows 3, 8: Clean circuit dividers (identical to title row 4) — system integrity intact
- Rows 5–6: Victory scene — mirrors title layout exactly but with TOWERS on BOTH sides (bugs are vanquished) and healthy computer at center, connected by intact traces
- Rows 10–11: Triumph motif — shield flanked by towers, centered — `floor((20-7)/2)=6`, width 7 (TW+2blank+SHIELD+2blank+TW)
- Row 12: Blank breathing room before score area
- Rows 13–15: RESERVED for code-drawn overlays
- Rows 16–17: Blank — overall screen has 12 blank rows vs lose screen's 8 → measurably "lighter"

---

### Visual Contrast Analysis

| Property | Lose Screen | Win Screen |
|----------|-------------|------------|
| Dark-tile rows | 4 full rows (0,2,4,17) = 80 tiles | 0 full dark rows |
| White-space rows | 8 blank rows | 12 blank rows |
| Circuit style | 1 broken + 1 intact | 2 intact |
| Scene characters | Bugs (aggressors) | Towers (defenders) |
| Computer state | Damaged (glitch dots) | Healthy (smiley face) |
| Central motif | Skull (death) | Shield (protection) |
| Overall shade weight | ~80% dark fill in noise tiles | 100% white background |
| Emotional read | Dark, chaotic, oppressive | Bright, ordered, triumphant |

---

### Components: New Tile Inventory (6 tiles)

| # | Name | Index | Category | Purpose |
|---|------|-------|----------|---------|
| 1 | `ENDSCR_STATIC` | MAP_TILE_BASE + 53 | Reusable | Dark noise/corruption fill — TV static effect |
| 2 | `ENDSCR_BROKEN_HL` | MAP_TILE_BASE + 54 | Reusable | Broken circuit trace — fragmented HLINE variant |
| 3 | `ENDSCR_SKULL_T` | MAP_TILE_BASE + 55 | Screen-specific (lose) | Skull icon top half — death/game-over motif |
| 4 | `ENDSCR_SKULL_B` | MAP_TILE_BASE + 56 | Screen-specific (lose) | Skull icon bottom half |
| 5 | `ENDSCR_SHIELD_T` | MAP_TILE_BASE + 57 | Screen-specific (win) | Shield icon top half — defense/victory motif |
| 6 | `ENDSCR_SHIELD_B` | MAP_TILE_BASE + 58 | Screen-specific (win) | Shield icon bottom half |

**Tile budget: 53 + 6 = 59 total map tiles. BG VRAM: 128 + 59 = 187 / 256. Headroom: 69 tiles.**

---

### New Tile Pixel Art

Palette: `.` = White (shade 0), `,` = Light grey (shade 1), `o` = Dark grey (shade 2), `#` = Black (shade 3)

#### ENDSCR_STATIC — Digital noise/corruption fill

Design goal: Pseudo-random mix of predominantly dark pixels (~80% black+dark grey) evoking CRT static / digital corruption. Tiles seamlessly in all directions without visible grid seam.

```
o#,o##.#
#oo#.o#o
.#o#o##,
o#,##o.#
##o.#o#o
#.o#o#,#
o##,o#o.
#o#.#oo#
```

Shade distribution per tile: ~30 black, ~21 dark grey, ~8 white, ~5 light grey out of 64 pixels. Very dark overall — creates a "dead signal" atmosphere when 20 tiles fill a row.

**Tilability verification:** Rightmost column (`#,o,,,o,o,.,#`) differs sufficiently from leftmost column (`o,#,.,o,#,#,o,#`) — no vertical stripe artefact. Top row (`o#,o##.#`) and bottom row (`#o#.#oo#`) are visually distinct — no horizontal stripe artefact.

---

#### ENDSCR_BROKEN_HL — Broken circuit trace

Design goal: Fragmented horizontal trace at the same vertical position as TITLE_HLINE (rows 3–4) but with gaps/dislocations suggesting damage. Must NOT tile seamlessly (visual "brokenness" is the point).

```
........
.....o..
........
oo..oo.o
o.oo..oo
........
..o.....
........
```

Comparison with TITLE_HLINE:
- HLINE: solid dark-grey band at rows 3–4 (2px thick, full width)
- BROKEN_HL: scattered fragments at rows 3–4 with stray pixels at rows 1 and 6

When 14 of these tiles fill a row between NODEs, the irregular gaps create a distinctly "corrupted data bus" feel vs the clean solid HLINE.

---

#### ENDSCR_SKULL_T — Skull top half (8×8)

Design goal: Immediately recognizable skull silhouette (black on white). Key features: rounded dome, two white "eye socket" holes, solid brow/cheeks. Pairs with SKULL_B below for a 8×16 icon.

```
..####..
.######.
##.##.##
########
########
.##..##.
..####..
.##..##.
```

Feature breakdown:
- Row 0–1: Rounded cranium dome (4px then 6px wide)
- Row 2: **Eye sockets** — two single-pixel white holes at positions 2 and 5 (the key recognition feature)
- Row 3–4: Solid forehead/cheekbones (full 8px)
- Row 5: Nasal cavity — two white gaps create triangular nose
- Row 6: Jaw narrows
- Row 7: Teeth — alternating black/white suggesting tooth gaps

---

#### ENDSCR_SKULL_B — Skull bottom half (8×8)

```
..####..
...##...
...##...
........
........
........
........
........
```

- Row 0: Lower jaw (matching row 6 width of top tile)
- Rows 1–2: Neck/spine (narrow 2px column)
- Rows 3–7: Empty (allows breathing room below the icon)

---

#### ENDSCR_SHIELD_T — Shield top half (8×8)

Design goal: Heraldic pointed shield — flat/broad at top, solid black fill. Visually distinct from the skull (which has a rounded dome and eye holes). The flat top edge is the key differentiator.

```
.######.
########
########
########
########
########
.######.
..####..
```

Feature breakdown:
- Row 0: Nearly flat top (6px) — distinctly different from skull's 4px rounded dome
- Rows 1–5: Solid black mass (full 8px) — no internal holes (vs skull's eye sockets)
- Rows 6–7: Sides begin tapering inward toward the point

---

#### ENDSCR_SHIELD_B — Shield bottom half (8×8)

```
...##...
...##...
...##...
........
........
........
........
........
```

- Rows 0–2: Continues tapering to a point (2px column)
- Rows 3–7: Empty

The full 8×16 shield reads as: broad solid rectangle on top, tapering to a sharp point at bottom — classic heraldic/medieval shield silhouette. At a glance: "protection" / "defense" — thematically linked to the tower defense genre.

---

### User Flow

1. Player's computer HP reaches 0 → `gameover_enter(false, score, new_hi)`:
   - Sprites hidden, display off
   - `lose_tilemap` blitted (static bands + corrupted scene + skull)
   - Score/banner/prompt drawn at rows 13–15
   - Display on → player sees dark, oppressive "GAME OVER" screen
   
2. Player clears all waves → `gameover_enter(true, score, new_hi)`:
   - Sprites hidden, display off
   - `win_tilemap` blitted (clean scene + shield + towers)
   - Score/banner/prompt drawn at rows 13–15
   - Display on → player sees bright, triumphant "VICTORY!" screen

3. On both screens: "PRESS START" blinks at row 15 (30-frame toggle, existing logic unchanged)
4. Player presses START → returns to title screen

---

### Responsive Behavior

N/A — Game Boy has a fixed 160×144 resolution. Both screens are designed for exactly 20×18 tiles.

---

### Accessibility

- **Contrast**: All text is shade 3 (black) on shade 0 (white) — maximum DMG contrast. Art motifs are solid black silhouettes on white backgrounds.
- **Readability**: "GAME OVER" and "VICTORY!" headings use the standard 5×7 font at row 1 — large enough to read on original DMG screen size.
- **No color-dependent information**: Win/lose distinction is communicated through multiple channels: different heading text, different scene composition, different overall brightness, different central icon (skull vs shield). A player who can distinguish any shade pair can tell the screens apart.
- **Timing**: PRESS START blink rate (1 Hz) is unchanged and well above the 3 Hz seizure threshold.
- **Input**: Single START press to continue — same interaction on both screens, discoverable from the always-visible prompt.

---

## Architecture

### Tile budget impact

| Category | Before | After |
|----------|--------|-------|
| Map tiles (MAP_TILE_BASE+0..52) | 53 | 59 (+6) |
| Total BG VRAM (font+map) | 181 | 187 |
| Available (of 256) | 75 | 69 |

### Files changed

| File | Change |
|------|--------|
| `tools/gen_assets.py` | Add 6 `design_tile()` definitions, append to `map_tiles`, add `ENDSCR_*_IDX` Python constants, add C `#define`s, bump `MAP_TILE_COUNT` 53→59, rewrite `win_map()` and `lose_map()` |
| `res/assets.c` | Regenerated (larger `map_tile_data[]`, new `win_tilemap`/`lose_tilemap`) |
| `res/assets.h` | Regenerated (6 new `#define`s, `MAP_TILE_COUNT 59`) |

**No C source changes** — `gameover.c` already calls `set_bkg_tiles(0, 0, 20, 18, win ? win_tilemap : lose_tilemap)` and draws score/banner/prompt over rows 13–15. The tilemaps are purely data-driven.

### Implementation: win_map() replacement

```python
def win_map():
    S = 32
    HL   = TITLE_HLINE_IDX
    ND   = TITLE_NODE_IDX
    TT   = TITLE_TW_T_IDX
    TB   = TITLE_TW_B_IDX
    CT   = TILE_COMP_TL_IDX
    CR   = TILE_COMP_TR_IDX
    CB   = TILE_COMP_BL_IDX
    CX   = TILE_COMP_BR_IDX
    SH   = ENDSCR_SHIELD_T_IDX
    SS   = ENDSCR_SHIELD_B_IDX

    rows = [blank_row() for _ in range(18)]
    rows[1]  = text_row("      VICTORY!      ")
    rows[3]  = [S,S, ND, HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL, ND, S,S]
    rows[5]  = [S,S,S,S, TT, HL,HL,HL, S, CT,CR, S, HL,HL,HL, TT, S,S,S,S]
    rows[6]  = [S,S,S,S, TB, S,S,S,    S, CB,CX, S, S,S,S,    TB, S,S,S,S]
    rows[8]  = [S,S, ND, HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL, ND, S,S]
    rows[10] = [S,S,S,S,S,S, TT, S,S, SH, S,S, TT, S,S,S,S,S,S,S]
    rows[11] = [S,S,S,S,S,S, TB, S,S, SS, S,S, TB, S,S,S,S,S,S,S]
    return rows
```

### Implementation: lose_map() replacement

```python
def lose_map():
    S = 32
    HL   = TITLE_HLINE_IDX
    ND   = TITLE_NODE_IDX
    BK   = ENDSCR_BROKEN_HL_IDX
    BT   = TITLE_BUG_T_IDX
    BB   = TITLE_BUG_B_IDX
    DT   = TILE_COMP_TL_D_IDX
    DR   = TILE_COMP_TR_D_IDX
    DB   = TILE_COMP_BL_D_IDX
    DX   = TILE_COMP_BR_D_IDX
    ST   = ENDSCR_STATIC_IDX
    SK   = ENDSCR_SKULL_T_IDX
    SB   = ENDSCR_SKULL_B_IDX

    rows = [blank_row() for _ in range(18)]
    rows[0]  = [ST]*20
    rows[1]  = text_row("     GAME OVER      ")
    rows[2]  = [ST]*20
    rows[4]  = [S,S, ND, BK,BK,BK,BK,BK,BK,BK,BK,BK,BK,BK,BK,BK,BK, ND, S,S]
    rows[6]  = [S,S,S,S, BT, BK,BK,BK, S, DT,DR, S, BK,BK,BK, BT, S,S,S,S]
    rows[7]  = [S,S,S,S, BB, S,S,S,    S, DB,DX, S, S,S,S,    BB, S,S,S,S]
    rows[9]  = [S,S, ND, HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL, ND, S,S]
    rows[10] = [S,S,S,S,S,S, BT, S,S, SK, S,S, BT, S,S,S,S,S,S,S]
    rows[11] = [S,S,S,S,S,S, BB, S,S, SB, S,S, BB, S,S,S,S,S,S,S]
    rows[17] = [ST]*20
    return rows
```

### Python index constants (append after TILE_ARROW_IDX)

```python
ENDSCR_STATIC_IDX    = MAP_TILE_BASE + 53
ENDSCR_BROKEN_HL_IDX = MAP_TILE_BASE + 54
ENDSCR_SKULL_T_IDX   = MAP_TILE_BASE + 55
ENDSCR_SKULL_B_IDX   = MAP_TILE_BASE + 56
ENDSCR_SHIELD_T_IDX  = MAP_TILE_BASE + 57
ENDSCR_SHIELD_B_IDX  = MAP_TILE_BASE + 58
```

### C #defines (add before MAP_TILE_COUNT line)

```c
#define TILE_ENDSCR_STATIC    (MAP_TILE_BASE + 53)
#define TILE_ENDSCR_BROKEN_HL (MAP_TILE_BASE + 54)
#define TILE_ENDSCR_SKULL_T   (MAP_TILE_BASE + 55)
#define TILE_ENDSCR_SKULL_B   (MAP_TILE_BASE + 56)
#define TILE_ENDSCR_SHIELD_T  (MAP_TILE_BASE + 57)
#define TILE_ENDSCR_SHIELD_B  (MAP_TILE_BASE + 58)
#define MAP_TILE_COUNT  59
```

---

## Subtasks

1. ✅ **Add 6 end-screen tile pixel art to gen_assets.py** — Insert 6 `design_tile([…])` definitions (ENDSCR_STATIC, ENDSCR_BROKEN_HL, ENDSCR_SKULL_T, ENDSCR_SKULL_B, ENDSCR_SHIELD_T, ENDSCR_SHIELD_B) using the exact pixel art from this spec. Append 6 `('TILE_ENDSCR_*', …)` entries to `map_tiles` list (after TILE_ARROW). Done when `len(map_tiles) == 59`.

2. ✅ **Add tile index constants and #defines** — Add Python constants `ENDSCR_*_IDX = MAP_TILE_BASE + 53..58` after `TILE_ARROW_IDX`. In the `assets_h` template, add 6 `#define TILE_ENDSCR_*` before `MAP_TILE_COUNT`, change `MAP_TILE_COUNT` from 53 to 59. Done when `python3 tools/gen_assets.py` runs without error.

3. ✅ **Rewrite win_map() and lose_map()** — Replace both function bodies with the implementations from this spec. The new maps reference `ENDSCR_*_IDX` and existing `TITLE_*_IDX` / `TILE_COMP_*_IDX` constants. Done when the generated tilemaps contain the correct art tile indices at the specified rows/cols.

4. ✅ **Regenerate assets and build** — Run `just assets && just build`. Verify: (a) `res/assets.h` has `MAP_TILE_COUNT 59` and all 6 new defines, (b) `map_tile_data` is 59×16=944 bytes, (c) `win_tilemap` and `lose_tilemap` contain values ≥181 (new tile indices) at the art rows, (d) ROM builds without error, (e) `just check` passes.

5. ⬜ **Visual verification — lose screen** — Trigger game over in emulator. Verify: (a) Rows 0,2,17 show full-width dark noise bands (CRT static look), (b) "GAME OVER" is readable at row 1, (c) Row 4 has broken circuit trace between NODE endpoints, (d) Rows 6–7 show bugs flanking damaged computer connected by broken traces, (e) Row 9 has intact circuit divider, (f) Rows 10–11 show skull flanked by bugs, (g) Score/banner/prompt render correctly at rows 13–15, (h) "PRESS START" blinks normally.

6. ⬜ **Visual verification — win screen** — Win the game. Verify: (a) Screen feels noticeably brighter/cleaner than lose screen, (b) "VICTORY!" is readable at row 1, (c) Rows 3,8 have clean circuit dividers, (d) Rows 5–6 show towers flanking healthy computer connected by traces, (e) Rows 10–11 show shield flanked by towers, (f) Score/banner/prompt render correctly, (g) "PRESS START" blinks normally.

---

## Acceptance Scenarios

### Setup
```sh
just assets     # regenerate res/assets.{c,h}
just build      # compile ROM
just run        # launch in mGBA
```

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Lose screen — static bands | Let HP reach 0 | Rows 0, 2, 17 filled with dark noise tiles (pseudo-random dark grey/black pattern) |
| 2 | Lose screen — heading | Observe row 1 | "GAME OVER" in standard font, centered, clearly readable against white |
| 3 | Lose screen — broken circuit | Observe row 4 | NODE tiles at cols 2 & 17, broken/fragmented trace between (not solid) |
| 4 | Lose screen — corrupted scene | Observe rows 6–7 | Bug icons on both flanks (cols 4, 15), damaged computer center (cols 9–10), broken traces connecting |
| 5 | Lose screen — death motif | Observe rows 10–11 | Skull icon centered (col 9) with bug icons at cols 6, 12 |
| 6 | Lose screen — divider | Observe row 9 | Clean intact HLINE between NODEs (visual anchor separating art from score) |
| 7 | Lose screen — score area | Observe rows 13–15 | Score and PRESS START render correctly over blank tilemap |
| 8 | Win screen — brightness | Win the game | Screen is visibly brighter/cleaner than lose screen — mostly white tiles |
| 9 | Win screen — heading | Observe row 1 | "VICTORY!" in standard font, centered |
| 10 | Win screen — circuit dividers | Observe rows 3, 8 | Two clean circuit dividers (NODE + solid HLINE + NODE) |
| 11 | Win screen — victory scene | Observe rows 5–6 | Tower icons on both flanks, healthy computer center, intact traces |
| 12 | Win screen — triumph motif | Observe rows 10–11 | Shield icon centered (col 9) with tower icons at cols 6, 12 |
| 13 | Win screen — score area | Observe rows 13–15 | Score and PRESS START render correctly |
| 14 | NEW HIGH SCORE banner | Achieve a new high score on either screen | "NEW HIGH SCORE!" appears at row 13 without overlapping art |
| 15 | PRESS START blink | Wait on either screen | "PRESS START" toggles every ~0.5s |
| 16 | Return to title | Press START on either screen | Transitions to title screen normally |
| 17 | Build succeeds | `just check` | ROM produced, header checksum valid, host tests pass |

---

## Constraints

- **Tile budget**: 6 new tiles. Total 59/256 BG. 69 slots remain for future features.
- **No C source changes**: Both screens are entirely data-driven via `win_tilemap` / `lose_tilemap`. The `gameover.c` logic for score/banner/prompt overlay is unchanged.
- **Reserved rows**: Rows 13–15 MUST remain all-spaces (tile 32) in both tilemaps. Code writes: row 13 cols 2–16 (banner, conditional), row 14 cols 4–15 (score), row 15 cols 4–15 (prompt).
- **Existing tile reuse**: 10 existing tiles reused across both screens (BUG_T/B, TW_T/B, NODE, HLINE, COMP×4, COMP_D×4). No modifications to their pixel data.
- **ROM size impact**: +96 bytes tile data (6×16). Negligible.
- **DMG palette convention**: Skull and shield are solid black (#) for maximum contrast. STATIC uses all 4 shades for noise texture. BROKEN_HL uses dark grey (o) matching HLINE convention.

---

## Decisions

| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| Heading style | (A) Block-letter art tiles for "GAME OVER"/"WIN"; (B) Standard font with decorative framing | B — font + art framing | "GAME OVER" is 8 unique letters × 4 tiles each = 32+ tiles — far exceeds budget. Even "FAIL" (4 letters) would need 16 tiles. Standard font is free (0 tile cost); the visual impact comes from the art composition surrounding it. |
| Lose screen mood device | (A) Darker background shade via BGP palette swap; (B) Fill rows with dark-toned art tiles; (C) Invert all tiles | B — dark art tile fills | BGP swap would affect ALL tiles including font text (unreadable). Dark art fills (STATIC rows) achieve the "dark" mood while keeping text rows white and legible. |
| Scene composition | (A) Original design per screen; (B) Mirror title screen format with substitutions | B — mirror title | Creates visual continuity with the title screen. Players subconsciously recognize the composition and read the difference: bugs→tower becomes bugs→bugs (defeat) or tower→tower (victory). Tells the story without words. |
| Skull vs X motif | (A) Skull silhouette; (B) Large "X"; (C) Sad/angry face (text emoticon in tilemap) | A — skull | The skull is universally recognized as "death/game over" across all game genres. An X is ambiguous (could mean "wrong" or "close"). A text emoticon (the current X_X) lacks visual weight. |
| Shield vs checkmark vs trophy | (A) Shield; (B) Checkmark; (C) Trophy/star | A — shield | Shield = "defense" — directly thematically linked to tower defense. It's the conceptual opposite of the skull (protection vs death). A checkmark is generic. A trophy doesn't relate to the game's narrative. |
| Number of new tiles | (A) 6 tiles (minimal); (B) 15-20 tiles (richer art); (C) 25+ (block letters + art) | A — 6 tiles | The existing 10 reusable tiles from the title screen carry the composition. Adding 6 purpose-built tiles (2 fills + 4 motif icons) is sufficient for strong visual differentiation between screens while leaving 69 tile slots free for future features. |
| Static tile tilability | (A) Perfectly tileable (designed to repeat seamlessly); (B) Intentionally non-repeating (pseudo-random look) | B — pseudo-random | Perfect tilability would create visible patterns at 20× repetition. The pseudo-random approach makes 20 adjacent STATIC tiles look like a continuous noise band rather than a repeating pattern. |
| Bottom static band on lose | (A) Row 16 and 17; (B) Row 17 only | B — row 17 only | Row 16 needs to stay blank to provide breathing room below the PRESS START prompt (row 15). One bottom band is sufficient for framing. |

---

## Test Strategy

This feature is **presentation-only** — it changes tile data and tilemap arrays in `gen_assets.py`. No game logic, no new C code, no new pure helpers.

- **No new host tests required**: The existing 14+ host tests cover score calculation, save, audio, modal logic, etc. None of those are affected.
- **Verification is visual**: Use `just run` in mGBA emulator; manually trigger game-over (let HP reach 0) and victory (clear all waves). The acceptance scenarios above are the test plan.
- **Regression gate**: `just check` ensures the ROM still builds at 65536 bytes, header checksum is valid, and all existing host tests pass.

---

## Risk Assessment

| Subtask | Risk | Rationale |
|---------|------|-----------|
| 1. Add tile pixel art | LOW | Pure data entry — 6 tiles using the well-established `design_tile()` pattern |
| 2. Add constants/defines | LOW | Mechanical edits to the `assets_h` template string and Python constants |
| 3. Rewrite map functions | LOW | Self-contained function bodies with no external dependencies |
| 4. Regenerate + build | LOW | Existing `just assets && just build` pipeline; tile count auto-loads via `gfx_init()` |
| 5–6. Visual verification | LOW | If the build succeeds, the art is correct by construction (deterministic tilemap) |

**ROM size impact**: +96 B tile data (6×16) + ~0 B net tilemap change (same 20×18×2 arrays, just different values). Total ROM headroom remains >10 KB in 64 KB address space.

**Tile budget impact**: 53 → 59. 69 slots remain (more than all previous iterations combined have used).

---

## Out of Scope

- **Animation on end screens** — Only the existing "PRESS START" blink runs. No sprite animation, no tile cycling, no scroll effects.
- **Sound stings** — Existing MUS_WIN / MUS_LOSE stinger songs are retained as-is. No new audio.
- **Gameplay mechanic changes** — This feature is purely cosmetic.
- **New C source files or headers** — Everything is data-driven through `gen_assets.py`.
- **Block-letter art for headings** — Rejected due to tile budget (would need 16–32 tiles). Standard font is used instead.
- **BGP palette manipulation** — Both screens use the default `BGP=0xE4`; no runtime palette changes.

---

## Review

### Adversarial review (Claude Sonnet)
**Verdict: PASS** — No issues found. Verified:
- All pixel art grids are 8×8 with valid chars
- All tilemap rows contain exactly 20 elements
- Text centering math is correct
- "No C changes" claim validated against `gameover.c` line 69
- Tile budget arithmetic correct (53+6=59, 128+59=187 ≤ 256)

### Cross-model validation (GPT-5.4)
**Verdict: PASS** — No blocking issues. Confirmed:
- `gfx_init()` auto-loads `MAP_TILE_COUNT` tiles — bumping the #define is sufficient
- `gameover.c` consumes `win_tilemap`/`lose_tilemap` by extern reference — no code change needed
- All text_row strings are exactly 20 chars
- Row 10/11 arrays in both maps verified as 20 elements
- Reserved rows 13–15 remain blank in both maps

### Design quality checklist
- [x] All 6 tile pixel art grids are exactly 8×8
- [x] STATIC tile shade distribution is predominantly dark (~80%)
- [x] BROKEN_HL fragments are at rows 3–4 (same Y as HLINE) so NODE connections work
- [x] Skull has clearly visible eye sockets (white holes in black)
- [x] Shield has flat/broad top (distinct from skull's rounded dome)
- [x] Both tilemaps have all-blank rows 13–15
- [x] Win scene uses healthy COMP tiles (TL/TR/BL/BR)
- [x] Lose scene uses damaged COMP tiles (TL_D/TR_D/BL_D/BR_D)
- [x] Column positions match between lose scene and title scene (cols 4, 5-7/12-14, 9-10, 15)
- [x] "GAME OVER" centering: 9 chars, col 5 → `floor((20-9)/2)=5` ✓
- [x] "VICTORY!" centering: 8 chars, col 6 → `floor((20-8)/2)=6` ✓
- [x] Bottom motif centering: 7 tiles wide, col 6 → `floor((20-7)/2)=6` ✓
