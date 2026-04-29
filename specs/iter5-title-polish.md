# Title Menu Polish

## Problem
The title menu has three UX issues reported by user feedback:
1. A copyright line ("(C) 2025 GBTD" at row 16) is unnecessary and adds clutter.
2. Text is not nicely centered — specifically the selectors with their `> < LABEL >` layout create an asymmetric, visually noisy presentation.
3. The `> <` selector indicators are confusing: a `>` focus chevron at col 3, then `< LABEL >` brackets around each option, making three directional symbols visible simultaneously.

## Architecture

### Current title screen layout (rows 10–17, the runtime UI area)

| Row | Content | Col range | Width | Notes |
|-----|---------|-----------|-------|-------|
| 10 | `< LABEL >` difficulty selector | 5–15 | 11 | Overwritten by `draw_diff_now()` |
| 11 | (blank) | — | — | |
| 12 | `< LABEL >` map selector | 5–15 | 11 | Overwritten by `draw_map_now()` |
| 13 | `PRESS START` prompt | 4–15 | 12 | Blinked by `draw_prompt_now()` |
| 14 | (blank) | — | — | |
| 15 | `HI: NNNNN` | 5–13 | 9 | Overwritten by `draw_hi_now()` |
| 16 | `(C) 2025 GBTD` | 4–16 | 13 | Static in tilemap |
| 17 | (blank) | — | — | |

Focus chevron `>` at col 3 of the focused row (row 10 or 12).

### New title screen layout (rows 10–17)

| Row | Content | Col range | Width | Centering math |
|-----|---------|-----------|-------|----------------|
| 10 | `▶ LABEL__` difficulty | 5–13 | 9 (arrow 1 + gap 1 + label 7) | floor((20-9)/2)=5 ✓ |
| 11 | (blank) | — | — | |
| 12 | `▶ LABEL__` map | 5–13 | 9 | floor((20-9)/2)=5 ✓ |
| 13 | `PRESS START` prompt | 4–15 | 12 | floor((20-12)/2)=4 ✓ (unchanged) |
| 14 | (blank) | — | — | |
| 15 | `HI: NNNNN` | 5–13 | 9 | floor((20-9)/2)=5 ✓ (unchanged) |
| 16 | (blank — copyright removed) | — | — | |
| 17 | (blank) | — | — | |

The arrow (▶) at col 5 appears only on the focused row; the unfocused row shows a space at col 5.

### Centering verification for all text rows

| Row | Text | Length | floor((20-len)/2) | Current col | New col | Change? |
|-----|------|--------|--------------------| ------------|---------|---------|
| 3 | "DEFENDER" | 8 | 6 | 6 | 6 | No |
| 10 | selector group (arrow+gap+label) | 9 | 5 | 3/5 (split) | 5 | **Yes** |
| 12 | selector group (arrow+gap+label) | 9 | 5 | 3/5 (split) | 5 | **Yes** |
| 13 | "PRESS START " (blink region) | 12 | 4 | 4 | 4 | No |
| 15 | "HI: NNNNN" | 9 | 5 | 5 | 5 | No |
| 16 | "(C) 2025 GBTD" | 13 | 3 | 4 | — | **Removed** |

### New arrow tile

A solid right-pointing filled triangle, used as a BG tile. Added at `MAP_TILE_BASE + 52` (tile index 180).

**Pixel art (8×8, `#` = black/palette 3, `.` = white/palette 0):**
```
........
.##.....
.####...
.#####..
.#####..
.####...
.##.....
........
```

6 active rows (rows 1–6), max 5px wide, 1px left indent. Vertically symmetric. Uses black (palette 3) per convention for UI elements the player must quickly locate.

**Tile budget impact:** MAP_TILE_COUNT 52 → 53. Total BG VRAM: 128 (font) + 53 (map) = 181 / 256. Headroom: 75 tiles remaining.

### Selector logic change

**Before:**
- `draw_selector(col, row, label)` writes 11 tiles: `<` + ` ` + label[0..6] + ` ` + `>` at cols 5–15.
- `draw_focus_now()` writes `>` (ASCII 62) at col 3 of focused row, space at col 3 of unfocused row.

**After:**
- `draw_selector(col, row, label)` writes 7 tiles: just `label[0..6]` at cols 7–13.
- `draw_focus_now()` writes `TILE_ARROW` (tile 180) at col 5 of focused row, space (tile 32) at col 5 of unfocused row.
- Col 6 remains space (from initial tilemap blit; never overwritten by any draw function).
- Cols 14–15 remain space (no longer written; start as space in tilemap).

### BG-write budget (per-frame, during VBlank)

| Dirty flag | Old writes | New writes | Δ |
|-----------|-----------|-----------|---|
| s_diff_dirty | 11 | 7 | −4 |
| s_map_dirty | 11 | 7 | −4 |
| s_focus_dirty | 2 | 2 | 0 |
| s_hi_dirty | 9 | 9 | 0 |
| s_dirty (blink) | 12 | 12 | 0 |

**Worst case per frame: 12** (prompt blink). Budget is ≤16. ✓ Margin improved.

### Files changed

| File | Change |
|------|--------|
| `tools/gen_assets.py` | Add arrow tile pixel art, append to `map_tiles`, add `TILE_ARROW_IDX` constant, add `#define TILE_ARROW`, bump `MAP_TILE_COUNT` 52→53, remove `rows[16]` from `title_map()` |
| `src/title.c` | Change `DIFF_COL` 5→7, `DIFF_W` 11→7, `MAP_COL` 5→7, `MAP_W` 11→7, `FOCUS_COL` 3→5; rewrite `draw_selector()` (7 chars only); change `draw_focus_now()` to use `TILE_ARROW` |
| `res/assets.c` | Regenerated |
| `res/assets.h` | Regenerated (new `#define TILE_ARROW`, `MAP_TILE_COUNT 53`) |

No changes to `title.h`, `gfx.c`, or any other source file.

## Subtasks

1. ✅ **Add arrow tile to gen_assets.py** — Add a `design_tile([…])` definition for the right-pointing triangle (pixel art from Architecture section). Append `('TILE_ARROW', arrow_tile)` to the `map_tiles` list. Add Python constant `TILE_ARROW_IDX = MAP_TILE_BASE + 52` immediately after the existing `TITLE_TW_B_IDX` constant (around line 1254, in the tile-index-constants block). Note: `title_map()` does not reference this constant (the arrow is written at runtime by C code), but maintaining the convention of a Python `_IDX` for every map tile ensures consistency. In the `assets_h` template, add `#define TILE_ARROW (MAP_TILE_BASE + 52)` before the `MAP_TILE_COUNT` line. Change `MAP_TILE_COUNT` from 52 to 53. Done when `python3 tools/gen_assets.py` runs without error and the generated `res/assets.h` contains `#define TILE_ARROW` and `MAP_TILE_COUNT 53`.

2. ✅ **Remove copyright from title_map()** — In `tools/gen_assets.py`, delete the line `rows[16] = text_row("    (C) 2025 GBTD   ")` from `title_map()`. Done when the regenerated `title_tilemap` row 16 is all spaces (all bytes = 32).

3. ✅ **Regenerate assets** — Run `just assets`. Verify `res/assets.h` has `TILE_ARROW` and `MAP_TILE_COUNT 53`; verify `res/assets.c` `map_tile_data` array is 53×16=848 bytes; verify `title_tilemap` row 16 is all 32s. Done when all three checks pass.

4. ✅ **Update selector rendering in title.c** — (a) Change `#define DIFF_COL 5` → `7`, `#define DIFF_W 11` → `7`, `#define MAP_COL 5` → `7`, `#define MAP_W 11` → `7`, `#define FOCUS_COL 3` → `5`. Note: `DIFF_W` and `MAP_W` are documentation-only constants (not referenced in runtime code); update them to keep the header comment accurate. (b) Rewrite `draw_selector()` body to write only the 7-char label: remove the `<`, spaces, and `>` bracket writes; the loop writes `label[i]` at `col + i` for `i = 0..6`. With `DIFF_COL=7`, this produces writes at cols 7–13. (c) In `draw_focus_now()`, change `(u8)'>'` to `TILE_ARROW`. (d) `#include "assets.h"` is already present at line 3 of title.c — no include change needed. Done when `just build` succeeds and the resulting binary has no references to the old bracket/chevron logic.

5. ✅ **Build and verify** — Run `just build && just check`. Done when ROM compiles without error and host tests pass.

6. ✅ **Visual verification in emulator** — Run `just run`. On the title screen verify: (a) No copyright text visible on row 16; (b) Difficulty selector shows a filled triangle arrow at col 5 when focused, with label "NORMAL " at cols 7–13; (c) Map selector shows no arrow (space at col 5) when unfocused, with label " MAP 1 " at cols 7–13; (d) UP/DOWN moves the arrow between rows 10 and 12; (e) LEFT/RIGHT cycles values within the focused selector; (f) PRESS START blink and HI line still render correctly; (g) No stale `<` or `>` characters visible anywhere. Done when all seven checks pass visually.

## Acceptance Scenarios

### Setup
```sh
just assets   # regenerate res/assets.{c,h}
just build    # compile ROM
just run      # launch in mGBA
```

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Copyright removed | Launch ROM, observe row 16 | Row 16 is entirely blank — no "(C) 2025 GBTD" |
| 2 | Arrow on focused selector | Observe title screen on entry | Row 10 (difficulty) shows solid ▶ arrow at col 5 followed by label at cols 7–13. Row 12 (map) shows blank col 5 + label at cols 7–13. |
| 3 | No angle brackets | Observe selectors | No `<` or `>` characters visible anywhere on the title screen |
| 4 | Focus toggle (UP/DOWN) | Press DOWN | Arrow moves from row 10 to row 12. Row 10 col 5 becomes blank. |
| 5 | Focus toggle back | Press UP | Arrow moves back to row 10. |
| 6 | Difficulty cycle | With focus on difficulty, press RIGHT | Label at row 10 cols 7–13 changes (e.g. NORMAL→VETERAN). Arrow remains. |
| 7 | Map cycle | Press DOWN then RIGHT | Focus moves to map; pressing RIGHT cycles MAP 1→MAP 2. |
| 8 | PRESS START blink | Wait ~1 second | "PRESS START" at row 13 toggles on/off every ~0.5s |
| 9 | HI line correct | Observe row 15 | Shows "HI: 00000" (or saved high score) at cols 5–13 |
| 10 | Game starts | Press START | Transitions to gameplay normally |
| 11 | No stale tiles after cycling | Cycle difficulty through all 3 options | No leftover characters at cols 14–15; all labels display cleanly |
| 12 | Title art unaffected | Observe rows 0–9 | Block "BUG" letters, "DEFENDER" text, circuit divider, and scene icons unchanged |

## Constraints

- **BG-write budget**: ≤16 writes/frame. Worst case is now 12 (unchanged, prompt blink). Selector writes reduced from 11→7.
- **Tile budget**: 181/256 BG tiles after adding 1 arrow tile. 75 slots remain.
- **DMG only**: No color features. Arrow tile uses palette index 3 (black) for maximum contrast on real DMG hardware.
- **No row position changes**: `DIFF_ROW=10`, `MAP_ROW=12`, `PROMPT_ROW=13`, `HI_ROW=15` remain unchanged. Only column positions change.
- **Selector label format unchanged**: `s_diff_labels` and `s_map_labels` arrays keep their 7-char padded format.
- **Priority chain preserved**: The 5-flag dirty priority (`s_diff_dirty > s_map_dirty > s_focus_dirty > s_hi_dirty > s_dirty`) is unchanged.

## Decisions

| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| Arrow tile vs font glyph reuse | (A) Use existing `>` font glyph (ASCII 62); (B) New dedicated BG tile with filled triangle | B — new tile | The existing `>` is a thin 1px chevron that the user already finds confusing. A solid filled triangle reads clearly as "selection indicator" and is visually distinct from text. Costs only 1 tile slot (75 remaining). |
| Arrow placement | (A) Col 3 (same as old chevron); (B) Col 5 (adjacent to label); (C) Col 4 | B — col 5 | Placing the arrow 1 tile left of the label gap creates a tight visual group. Col 3 would leave a 3-tile gap between indicator and label. The 9-tile group (arrow+gap+label) is perfectly floor-centered at col 5. |
| Selector brackets | (A) Keep `< >` brackets but replace focus chevron only; (B) Remove all brackets, use arrow only | B — remove all brackets | The user specifically called out `> <` as confusing. Removing both the brackets AND the old chevron, replacing with a single clean arrow, eliminates all directional symbol confusion. LEFT/RIGHT cycling is discoverable via standard Game Boy UI conventions. |
| Right-side indicator | (A) Keep a right-side symbol (e.g. `◀`); (B) Remove entirely | B — remove | User feedback specifically objects to the dual `> <` pattern. A single left-side arrow is standard for menu selection on Game Boy games (Pokémon, Zelda, etc.). |
| PRESS START centering | (A) Move to col 5 (right-biased by 0.5); (B) Keep at col 4 (left-biased by 0.5, floor-centered) | B — keep col 4 | Floor-centering (left-bias for odd widths) is the standard convention. Moving to col 5 would be ceil-centering and introduce inconsistency with HI line. No user complaint targets PRESS START specifically. |
| Copyright removal approach | (A) Delete from tilemap only; (B) Delete from tilemap and add blank row explicitly | A — just delete the line | `title_map()` initializes all rows as blank. Removing `rows[16] = ...` reverts it to blank automatically. |

## Test Strategy

- **Host tests**: Title rendering writes directly to GB BG VRAM via `set_bkg_tile_xy()` — there are no host-testable functions for visual output. Confirmed: no existing host test covers title rendering. No new host test needed.
- **Build verification**: `just build && just check` confirms compilation succeeds and existing tests pass (no regressions in game logic).
- **Visual verification**: Manual emulator check per acceptance scenarios above.

## Risk / Regressions

- **Col 6 never written**: After removing the `< ` from `draw_selector`, col 6 at rows 10 and 12 is only set by the initial tilemap blit (which is all spaces). If any future code writes a non-space at col 6, it would persist. Risk is theoretical — no current code path touches it.
- **Cols 14–15 stale tiles**: The old `draw_selector` wrote ` >` at cols 14–15. The new version doesn't touch them. They start as spaces from the tilemap blit in `title_enter()`. If `title_enter()` is called (it always is on title entry), these are clean. No risk.
- **TILE_ARROW index**: The arrow tile must be referenced by its numeric tile index (180) in the BG layer. The `#define TILE_ARROW (MAP_TILE_BASE + 52)` evaluates to 180. `draw_focus_now()` passes this directly to `set_bkg_tile_xy()`. This is identical to how other map tiles work. No risk.
- **Priority chain**: Unchanged structure (5 flags, one serviced per frame). The only change is fewer writes per selector update (7 vs 11), which reduces VBlank pressure. No risk.

## Out of Scope

- Color/CGB features (DMG only).
- Repositioning rows vertically (e.g., moving HI line to fill copyright gap).
- New gameplay, balance, or performance changes.
- Changes to win/lose screens.
- Title art modifications (rows 0–9 unchanged).

## Review

### Adversarial review (code-review agent)
1. **Python constant placement ambiguity** (medium): Subtask 1 didn't specify WHERE in gen_assets.py to place `TILE_ARROW_IDX`, and noted the constant is unused by `title_map()` since the arrow is runtime-only. **Fixed:** Added explicit placement instruction ("immediately after `TITLE_TW_B_IDX`") and clarified the constant exists for convention consistency, not runtime use.

### Cross-model validation (GPT-5.4)
1. **`DIFF_W`/`MAP_W` are documentation-only** (low): These constants are defined but never referenced in runtime logic — the actual draw width is determined by the loop bounds in `draw_selector()`. **Fixed:** Added explicit note in subtask 4 that these are documentation-only constants updated for accuracy, not behavioral impact.
