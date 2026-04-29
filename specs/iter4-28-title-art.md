# Title Screen Art

## Problem
The title screen is text-only: "BUG DEFENDER" in the standard 5×7 font with flavour text and a copyright line. It communicates the game's identity through small-font text alone. Replacing the top area (rows 0–9) with a BG-art tilemap — a large block-letter logo, decorative circuit-trace divider, and a computer/bug/tower motif scene — transforms the first impression without touching any runtime selector or blink logic.

## Architecture

### Tile budget
| Category | Current | After |
|----------|---------|-------|
| Font tiles (0–127) | 128 | 128 (unchanged) |
| Map tiles (128–162) | 35 | 52 (+17 title art) |
| **Total BG tiles** | **163** | **180** |
| Available (of 256) | 93 | 76 |

17 new tiles ≤ 40 budget. Indices 163–179 (MAP_TILE_BASE + 35 … + 51).

### Approach
All changes live in **`tools/gen_assets.py`** (asset generator). No C source changes.

1. Add 17 `design_tile([…])` definitions for the art tiles.
2. Append 17 entries to the `map_tiles` list.
3. Add corresponding Python index constants and C `#define`s to the `assets_h` template (bump `MAP_TILE_COUNT 35 → 52`).
4. Rewrite `title_map()` to compose the new 20×18 layout using these indices plus existing font and COMP tiles.
5. Run `just assets` to regenerate `res/assets.{c,h}`.

`gfx_init()` already loads `MAP_TILE_COUNT` tiles via `set_bkg_data(MAP_TILE_BASE, MAP_TILE_COUNT, map_tile_data)` — no code change needed; the count auto-scales from the `#define`. `title_enter()` continues to write the 20×18 tilemap during a `DISPLAY_OFF` bracket; runtime draw functions (`draw_diff_now`, `draw_map_now`, `draw_prompt_now`, `draw_focus_now`, `draw_hi_now`) overwrite their cells at rows 10–15 exactly as before.

### Visual layout (20 columns × 18 rows)

```
Row 0:  (blank)
Row 1:  ······ B_TL B_TR · U_T  U_T  · G_TL G_TR ······
Row 2:  ······ B_BL B_BR · U_BL U_BR · G_BL G_BR ······
Row 3:  ······  D    E    F    E    N    D    E    R  ······
Row 4:  ·· NODE HLINE×14 NODE ··
Row 5:  (blank)
Row 6:  ···· BUG_T HL HL HL · COMP_TL COMP_TR · HL HL HL TW_T ····
Row 7:  ···· BUG_B ·········· COMP_BL COMP_BR ·········· TW_B ····
Row 8:  (blank)
Row 9:  (blank)
Row 10: (diff selector — overwritten at runtime)
Row 11: (blank)
Row 12: (map selector — overwritten at runtime)
Row 13: PRESS START (overwritten by blink)
Row 14: (blank)
Row 15: (HI line — overwritten at runtime)
Row 16: (C) 2025 GBTD   (font tiles, static)
Row 17: (blank)
```

**Rows 0–9** are the art area (logo + scene). **Rows 10–17** are unchanged UI.

The computer monitor (2×2) at the scene centre reuses the existing `TILE_COMP_TL/TR/BL/BR` tiles (indices 130–133) — these are already in BG VRAM. Font characters for "DEFENDER", "PRESS START", and the copyright line use the existing ASCII-indexed font tiles (0–127).

### New tile inventory (17 tiles)

| # | Name | Index | Purpose |
|---|------|-------|---------|
| 1 | `TITLE_B_TL` | 163 | Block "B" — top-left quadrant |
| 2 | `TITLE_B_TR` | 164 | Block "B" — top-right quadrant |
| 3 | `TITLE_B_BL` | 165 | Block "B" — bottom-left quadrant |
| 4 | `TITLE_B_BR` | 166 | Block "B" — bottom-right quadrant |
| 5 | `TITLE_U_T` | 167 | Block "U" — top half (shared by left & right, see note) |
| 6 | `TITLE_U_BL` | 168 | Block "U" — bottom-left quadrant |
| 7 | `TITLE_U_BR` | 169 | Block "U" — bottom-right quadrant |
| 8 | `TITLE_G_TL` | 170 | Block "G" — top-left quadrant |
| 9 | `TITLE_G_TR` | 171 | Block "G" — top-right quadrant |
| 10 | `TITLE_G_BL` | 172 | Block "G" — bottom-left quadrant |
| 11 | `TITLE_G_BR` | 173 | Block "G" — bottom-right quadrant |
| 12 | `TITLE_HLINE` | 174 | Horizontal circuit trace (dark grey, tiles seamlessly) |
| 13 | `TITLE_NODE` | 175 | Circuit junction node (dark grey circle) |
| 14 | `TITLE_BUG_T` | 176 | Bug icon — top half (8×8, front-facing) |
| 15 | `TITLE_BUG_B` | 177 | Bug icon — bottom half |
| 16 | `TITLE_TW_T` | 178 | Tower icon — top half (satellite dish) |
| 17 | `TITLE_TW_B` | 179 | Tower icon — bottom half |

**Shared tile note:** The U letter is horizontally symmetric at the tile boundary. Both the left arm (cols 1–3 of tile at col 9) and right arm (cols 1–3 of tile at col 10) produce an identical 8×8 pattern, so `TITLE_U_T` appears twice in tilemap row 1 at positions 9 and 10. This saves one tile slot.

### Pixel art for each tile

Palette: `.` = white (0), `,` = light (1), `o` = dark (2), `#` = black (3). All art follows the convention: black for high-contrast foreground, dark grey for secondary decorative traces.

Each design below is an 8-row × 8-column `design_tile()` art string (top to bottom, left to right; leftmost = pixel 7 in the tile byte).

#### Block letter "B" (16×16, 3-pixel stroke, 4 tiles)

Full composite:
```
           TL        TR
row 0  ........  ........
row 1  .#######  ####....
row 2  .#######  #####...
row 3  .###....  ..###...
row 4  .###....  ..###...
row 5  .#######  ####....
row 6  .#######  #####...
row 7  .###....  ..###...
           BL        BR
row 8  .###....  ..###...
row 9  .###....  ..###...
row10  .#######  #####...
row11  .#######  ####....
row12  ........  ........
row13  ........  ........
row14  ........  ........
row15  ........  ........
```

**TITLE_B_TL:**
```python
["........",
 ".#######",
 ".#######",
 ".###....",
 ".###....",
 ".#######",
 ".#######",
 ".###...."]
```

**TITLE_B_TR:**
```python
["........",
 "####....",
 "#####...",
 "..###...",
 "..###...",
 "####....",
 "#####...",
 "..###..."]
```

**TITLE_B_BL:**
```python
[".###....",
 ".###....",
 ".#######",
 ".#######",
 "........",
 "........",
 "........",
 "........"]
```

**TITLE_B_BR:**
```python
["..###...",
 "..###...",
 "#####...",
 "####....",
 "........",
 "........",
 "........",
 "........"]
```

#### Block letter "U" (16×16, 3-pixel stroke, 3 unique tiles)

Full composite (U_T is used twice in the top row):
```
           TL=U_T    TR=U_T
row 0  ........  ........
row 1  .###....  .###....
row 2  .###....  .###....
row 3  .###....  .###....
row 4  .###....  .###....
row 5  .###....  .###....
row 6  .###....  .###....
row 7  .###....  .###....
           U_BL      U_BR
row 8  .###....  .###....
row 9  .####...  ####....
row10  ..######  ###.....
row11  ...#####  ##......
row12  ........  ........
row13  ........  ........
row14  ........  ........
row15  ........  ........
```

**TITLE_U_T** (shared top-left and top-right):
```python
["........",
 ".###....",
 ".###....",
 ".###....",
 ".###....",
 ".###....",
 ".###....",
 ".###...."]
```

**TITLE_U_BL:**
```python
[".###....",
 ".####...",
 "..######",
 "...#####",
 "........",
 "........",
 "........",
 "........"]
```

**TITLE_U_BR:**
```python
[".###....",
 "####....",
 "###.....",
 "##......",
 "........",
 "........",
 "........",
 "........"]
```

#### Block letter "G" (16×16, 3-pixel stroke, 4 tiles)

Full composite:
```
           TL        TR
row 0  ........  ........
row 1  ....####  ###.....
row 2  ...#####  ####....
row 3  ..###...  ..###...
row 4  .###....  ........
row 5  .###....  ........
row 6  .###....  ####....
row 7  .###....  ####....
           BL        BR
row 8  .###....  ..###...
row 9  ..###...  ..###...
row10  ...#####  ####....
row11  ....####  ###.....
row12  ........  ........
row13  ........  ........
row14  ........  ........
row15  ........  ........
```

**TITLE_G_TL:**
```python
["........",
 "....####",
 "...#####",
 "..###...",
 ".###....",
 ".###....",
 ".###....",
 ".###...."]
```

**TITLE_G_TR:**
```python
["........",
 "###.....",
 "####....",
 "..###...",
 "........",
 "........",
 "####....",
 "####...."]
```

**TITLE_G_BL:**
```python
[".###....",
 "..###...",
 "...#####",
 "....####",
 "........",
 "........",
 "........",
 "........"]
```

**TITLE_G_BR:**
```python
["..###...",
 "..###...",
 "####....",
 "###.....",
 "........",
 "........",
 "........",
 "........"]
```

#### Decorative traces (2 tiles, dark grey)

**TITLE_HLINE** — horizontal circuit trace (tiles seamlessly):
```python
["........",
 "........",
 "........",
 "oooooooo",
 "oooooooo",
 "........",
 "........",
 "........"]
```

**TITLE_NODE** — circuit junction node (rows 3–4 full-width to connect seamlessly to adjacent HLINE tiles):
```python
["........",
 "...oo...",
 "..oooo..",
 "oooooooo",
 "oooooooo",
 "..oooo..",
 "...oo...",
 "........"]
```

#### Bug icon (8×16, 2 tiles)

Front-facing symmetric bug with antennae, body, and legs.

**TITLE_BUG_T** — top half:
```python
["..#..#..",
 "...##...",
 "..####..",
 ".##..##.",
 "########",
 ".##..##.",
 "#.####.#",
 ".######."]
```

**TITLE_BUG_B** — bottom half:
```python
[".#.##.#.",
 "########",
 ".##..##.",
 "..####..",
 "...##...",
 "........",
 "........",
 "........"]
```

#### Tower / satellite-dish icon (8×16, 2 tiles)

Symmetric tower with dish, shaft, and base.

**TITLE_TW_T** — top half:
```python
["...##...",
 "..####..",
 ".######.",
 ".#.##.#.",
 "...##...",
 "..####..",
 ".#....#.",
 ".######."]
```

**TITLE_TW_B** — bottom half:
```python
[".######.",
 ".##..##.",
 "..####..",
 "..####..",
 ".######.",
 ".######.",
 "........",
 "........"]
```

### Complete tilemap (title_map)

Below is the exact `title_map()` replacement. `S` = 32 (space). It references the `TITLE_*_IDX` Python constants (defined in subtask 2a) and existing `TILE_COMP_*_IDX` constants to avoid silent index mismatches.

```python
def title_map():
    S = 32
    # Use the Python index constants (defined alongside map_tiles).
    # These MUST match the append order in map_tiles.
    B_TL = TITLE_B_TL_IDX;  B_TR = TITLE_B_TR_IDX
    B_BL = TITLE_B_BL_IDX;  B_BR = TITLE_B_BR_IDX
    U_T  = TITLE_U_T_IDX
    U_BL = TITLE_U_BL_IDX;  U_BR = TITLE_U_BR_IDX
    G_TL = TITLE_G_TL_IDX;  G_TR = TITLE_G_TR_IDX
    G_BL = TITLE_G_BL_IDX;  G_BR = TITLE_G_BR_IDX
    HL   = TITLE_HLINE_IDX
    ND   = TITLE_NODE_IDX
    BG_T = TITLE_BUG_T_IDX
    BG_B = TITLE_BUG_B_IDX
    TW_T = TITLE_TW_T_IDX
    TW_B = TITLE_TW_B_IDX
    CT = TILE_COMP_TL_IDX       # 130 — existing computer tiles
    CR = TILE_COMP_TR_IDX       # 131
    CB = TILE_COMP_BL_IDX       # 132
    CX = TILE_COMP_BR_IDX       # 133

    rows = [blank_row() for _ in range(18)]
    # --- Art area (rows 0-9) ---
    rows[1]  = [S,S,S,S,S,S, B_TL,B_TR, S, U_T,U_T, S, G_TL,G_TR, S,S,S,S,S,S]
    rows[2]  = [S,S,S,S,S,S, B_BL,B_BR, S, U_BL,U_BR, S, G_BL,G_BR, S,S,S,S,S,S]
    rows[3]  = text_row("      DEFENDER      ")
    rows[4]  = [S,S, ND, HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL, ND, S,S]
    rows[6]  = [S,S,S,S, BG_T, HL,HL,HL, S, CT,CR, S, HL,HL,HL, TW_T, S,S,S,S]
    rows[7]  = [S,S,S,S, BG_B, S,S,S,    S, CB,CX, S, S,S,S,    TW_B, S,S,S,S]
    # --- UI area (rows 10-17, selectors overwritten at runtime) ---
    rows[13] = text_row("    PRESS START     ")
    rows[16] = text_row("    (C) 2025 GBTD   ")
    return rows
```

Row 3 uses `text_row("      DEFENDER      ")` which maps to font indices (6 spaces + DEFENDER + 6 spaces = 20). The block "BUG" letters (rows 1–2) and "DEFENDER" (row 3) are both centred at cols 6–13.

### Files changed

| File | Change |
|------|--------|
| `tools/gen_assets.py` | Add 17 tile `design_tile` definitions, append to `map_tiles`, add index constants, add `#define`s to `assets_h` template, bump `MAP_TILE_COUNT`, rewrite `title_map()` |
| `res/assets.c` | Regenerated (larger `map_tile_data[]`, new `title_tilemap` values) |
| `res/assets.h` | Regenerated (17 new `#define`s, `MAP_TILE_COUNT 52`) |

**No changes** to `src/title.c`, `src/title.h`, `src/gfx.c`, or any other C source. The title screen logic is entirely data-driven.

## Subtasks

1. ✅ **Add title art tile pixel data to gen_assets.py** — Insert 17 `design_tile([…])` definitions (matching the pixel art above) in the map-tiles section of `gen_assets.py`, after the existing L2 tower tiles and before `map_tiles = [`. Append 17 `('TILE_TITLE_*', …)` entries to the `map_tiles` list. Done when `len(map_tiles) == 52` and `python3 tools/gen_assets.py` runs without error.

2. ✅ **Add tile index constants and #defines** — (a) Below the existing `TILE_COMP_*_IDX` constants (around line 1020), add Python constants `TITLE_B_TL_IDX = MAP_TILE_BASE + 35` through `TITLE_TW_B_IDX = MAP_TILE_BASE + 51`, one per tile, in the exact order they appear in the inventory table above. (b) In the `assets_h` template string, add 17 `#define TILE_TITLE_B_TL (MAP_TILE_BASE + 35)` through `#define TILE_TITLE_TW_B (MAP_TILE_BASE + 51)` before the `MAP_TILE_COUNT` line. (c) Change `#define MAP_TILE_COUNT 35` to `#define MAP_TILE_COUNT 52`. **Critical:** the append order in `map_tiles` (subtask 1), the Python `*_IDX` constants, and the C `#define`s must all use the same sequential indices 35–51. The `title_map()` function references the Python `*_IDX` constants, so a mismatch will cause wrong tiles to display. Done when the generated `res/assets.h` contains all 17 new defines and `MAP_TILE_COUNT` is 52.

3. ✅ **Rewrite title_map()** — Replace the body of `title_map()` with the exact implementation shown in the Architecture section (block letters on rows 1–2, "DEFENDER" text on row 3, circuit divider on row 4, scene on rows 6–7, PRESS START on row 13, copyright on row 16). Done when `python3 tools/gen_assets.py` produces a `title_tilemap` array that references the new tile indices in the art rows and spaces/font in the UI rows.

4. ✅ **Regenerate assets and build** — Run `just assets && just build`. Verify: (a) `res/assets.h` has `MAP_TILE_COUNT 52` and all 17 new defines, (b) `res/assets.c` `map_tile_data` array has 52×16 = 832 bytes of tile data (was 35×16 = 560), (c) `title_tilemap` rows 1–2 contain values ≥ 163, (d) ROM builds without error, (e) `just check` passes. Done when all five checks pass.

5. ⬜ **Visual verification in emulator** — Run `just run`. On the title screen: (a) "BUG" is displayed in large block letters (rows 1–2, cols 6–13), (b) "DEFENDER" is displayed in standard font below (row 3), (c) a dark-grey horizontal divider line spans the screen width (row 4), (d) a bug icon, circuit traces, a computer monitor (the existing COMP tiles with smiley face), more traces, and a tower icon are arranged at rows 6–7, (e) difficulty/map selectors, focus chevron, PRESS START blink, and HI line all function normally at rows 10–15, (f) copyright "(C) 2025 GBTD" appears at row 16. Done when all six visual elements are confirmed. If any art tile looks wrong, tweak the `design_tile` art in gen_assets.py and re-run `just assets && just run`.

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
| 1 | Block logo visible | Launch ROM, observe rows 1–2 | Large "BUG" in 2-tile-high block letters, centred at cols 6–13 |
| 2 | DEFENDER text below logo | Observe row 3 | "DEFENDER" in standard font, centred under the block letters |
| 3 | Circuit divider | Observe row 4 | Dark-grey horizontal line spanning cols 2–17 with circle nodes at each end |
| 4 | Scene motif | Observe rows 6–7 | Bug icon (left) connected by dark-grey traces to a computer monitor (centre, existing COMP tiles) connected by traces to a tower icon (right) |
| 5 | Difficulty selector works | Press LEFT/RIGHT on title | Selector at row 10 cycles CASUAL/NORMAL/VETERAN, label updates correctly |
| 6 | Map selector works | Press DOWN then LEFT/RIGHT | Focus chevron moves to row 12; map cycles MAP 1/2/3 |
| 7 | PRESS START blinks | Wait ~1 second | "PRESS START" at row 13 toggles on/off every 30 frames |
| 8 | HI line renders | Observe row 15 | Shows "HI: 00000" (or actual saved score) in standard font at row 15, cols 5–13 |
| 9 | Game starts | Press START | Transitions to gameplay screen normally |
| 10 | Copyright visible | Observe row 16 | "(C) 2025 GBTD" in standard font |
| 11 | Build succeeds | `just check` | ROM produced, header checksum valid, host tests pass |

## Constraints

- **Tile budget**: ≤ 40 new BG tiles. Actual: 17. Remaining headroom: 23.
- **BG VRAM**: 180 / 256 tiles after this change. 76 slots remain.
- **Row positions**: `DIFF_ROW=10`, `MAP_ROW=12`, `PROMPT_ROW=13`, `HI_ROW=15` — must NOT change.
- **No C code changes**: the title screen is fully data-driven; only gen_assets.py and the regenerated asset files change.
- **Existing COMP tiles**: reused at indices 130–133; no modification to their pixel data.
- **ROM size impact**: +272 bytes of tile data (17 tiles × 16 bytes). Negligible within 64 KB MBC1 ROM.
- **DMG palette**: block letters are solid black (#) on white for maximum contrast on real hardware. Decorative traces use dark grey (o) as secondary elements per the palette convention.

## Decisions

| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| Logo style | (A) Custom block-letter "BUG" in 2-tile-high tiles + font "DEFENDER" below; (B) All font text with decorative border frame; (C) Full custom block letters for both words | A — block "BUG" + font "DEFENDER" | Best visual impact within budget. "BUG" in large type grabs attention; "DEFENDER" in font is readable and costs 0 tiles. Full custom for both words (C) would need ~24 letter tiles, leaving only 16 for everything else. |
| U tile reuse | (A) 4 unique tiles for U (no reuse); (B) 3 tiles (share top half due to symmetry) | B — shared TITLE_U_T | DMG BG tiles cannot be flipped, but the U was designed with both arms at the same relative position within their tiles. Saves 1 tile slot. |
| Reuse existing COMP tiles for scene | (A) Create new computer tiles for title; (B) Reuse gameplay COMP_TL/TR/BL/BR | B — reuse existing | Saves 4 tile slots. The COMP tiles already depict a recognizable computer monitor with a smiley face — perfect for the title theme. They're loaded in BG VRAM at boot. |
| Trace colour | (A) Black traces; (B) Dark grey traces | B — dark grey | Traces are secondary decorative elements. Dark grey avoids visual competition with the black logo and icons, per the palette convention ("reserve black for things the player must locate quickly"). |
| Scene composition | (A) Two bugs flanking computer (needs left + right facing variants = more tiles); (B) Bug + computer + tower in a line connected by traces | B — three-element line | Tells the game's story (bug attacks computer, tower defends) with minimal tiles. A symmetric dual-bug design would need mirrored tile variants (no BG flip on DMG), doubling the icon tile count. |
| Divider width | (A) Match logo width (cols 6–13); (B) Match scene width (cols 4–15); (C) Full screen width (cols 2–17) | C — full width | Provides the strongest visual separation between logo and scene areas. |
| PRESS START in tilemap | (A) Include in tilemap (matches pre-existing pattern); (B) Leave as spaces (draw_prompt_now overwrites it during DISPLAY_OFF) | A — include | Consistent with the pre-existing title_map() which has it at row 13. Costs 0 extra tiles (uses existing font). |

## Review

### Adversarial review (Claude Sonnet)
1. **NODE/HLINE 1-pixel gap** (medium): TITLE_NODE had `.oooooo.` at trace rows 3–4, leaving a 1-pixel gap against adjacent HLINE tiles. **Fixed:** extended NODE rows 3–4 to `oooooooo` so the circle seamlessly joins the trace.

### Cross-model validation (GPT-5.4)
1. **HI-line acceptance scenario** (medium): Original scenario expected HI score to visibly change when cycling difficulty/map, but on fresh SRAM all scores are 0. **Fixed:** changed scenario 8 to verify the HI line renders correctly (shows "HI: 00000" or actual score) rather than requiring a visible change.
2. **Tile-index synchronization** (medium): `title_map()` used raw `MAP_TILE_BASE + N` offsets instead of the Python `*_IDX` constants, creating a silent mismatch risk if tiles are reordered. **Fixed:** `title_map()` now references `TITLE_*_IDX` constants directly. Added explicit synchronization warning to subtask 2.
