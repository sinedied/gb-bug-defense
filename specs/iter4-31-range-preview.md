# Tower Range Preview

## Problem
Players have no way to visualize a tower's attack range during gameplay. When deciding tower placement strategy or checking existing tower coverage, they must memorize range values. Feature #31 adds a visual preview: when the cursor rests on an existing tower for ≥15 frames, 8 dot sprites are placed in a circle at the tower's range radius, providing instant feedback on coverage.

## Architecture

### Module structure
New module `src/range_preview.{h,c}` owns the preview lifecycle. A pure helper `src/range_calc.h` (header-only, `<stdint.h>` only) contains the circle-point computation — testable on the host without GBDK linkage.

### Data flow
```
playing_update()
  └─ cursor_update()
  └─ range_preview_update(cursor_tx(), cursor_ty())
       ├─ towers_index_at(tx, ty) → tower idx or -1
       ├─ dwell timer: increment if on same tower, reset if moved/no tower
       ├─ if dwell >= 15 && !visible:
       │    ├─ towers_get_type(idx) → type → towers_stats(type)->range_px
       │    ├─ compute center: tx*8+4, (ty+HUD_ROWS)*8+4
       │    ├─ range_calc_dot(cx, cy, range_px, i) for i=0..7
       │    └─ set_sprite_tile + move_sprite for OAM 1..8
       └─ if moved off tower || no tower: hide immediately
```

### OAM allocation
OAM slots 1..8 (a subset of the menu/pause shared range 1..16). These are idle during normal gameplay. Mutual exclusion is enforced by:
- `menu_open()` calls `range_preview_hide()` — menu paints its glyphs over slots 1..16
- `pause_open()` calls `range_preview_hide()` — pause paints its glyphs over slots 1..16
- `range_preview_update()` is only called in the non-modal gameplay path (structurally unreachable when menu/pause is open)

### Circle-point math
8 dots at 45° intervals. Lookup table scaled by 64:
```c
cos64[8] = { 64, 45, 0, -45, -64, -45, 0, 45 }
sin64[8] = { 0, 45, 64, 45, 0, -45, -64, -45 }
dx = ((int16_t)range_px * cos64[i]) / 64
dy = ((int16_t)range_px * sin64[i]) / 64
```
Division by 64 (not `>> 6`) is used because signed right-shift is implementation-defined in C. Division truncates toward zero on both host (GCC/Clang) and target (SDCC), guaranteeing identical results in host tests and ROM. The computation runs only once on show-transition (not per-frame), so the Z80 software-division cost is negligible.

### Coordinate contract
`range_calc_dot()` returns **screen-pixel center coordinates** — the point where the dot should visually appear. Hidden dots return `(255, 255)` which is a safe sentinel: the GB screen is 160×144, so no valid pixel coordinate can be 255. (Originally `(0, 0)` was used but that is a valid screen pixel — see bugfix in review F3-post.)

`range_preview.c` converts pixel centers to GB OAM positions:
```c
// SPR_PROJ dot visual is centered in the 8×8 tile.
// Sprite top-left = dot_center - 4; OAM = top_left + (8, 16)
move_sprite(id, dot.x + 4, dot.y + 12);
```
This matches the projectile rendering convention (both use SPR_PROJ positioned by center).

### Rationale
- **Separate module (not inline in game.c)**: Range preview has its own state (dwell timer, visibility flag, last tile) and OAM management — too much for a helper. game.c is already 360 lines. Follows the pattern of `pause.c`, `menu.c`.
- **Pure header for circle math**: Enables host-testing without GBDK stubs. Follows the established `gate_calc.h`, `towers_anim.h`, `map_anim.h` pattern.
- **Recompute dot positions only on show transition**: Dots are static while visible (tower doesn't move). No per-frame position updates needed while visible.
- **SPR_PROJ tile reuse**: The projectile dot sprite (tile index 7) is visually appropriate for range indicators and avoids adding new tiles to the sprite bank.

## Subtasks

1. ⬜ **Create `src/range_calc.h` pure helper** — Header-only file with `<stdint.h>` types only. Contains:
   - `range_dot_t` struct: `{ uint8_t x; uint8_t y; }` (screen-pixel center coordinates; (0,0) = hidden sentinel)
   - `static const int8_t _range_cos64[8]` and `_range_sin64[8]` lookup tables
   - `static inline range_dot_t range_calc_dot(uint8_t cx_px, uint8_t cy_px, uint8_t range_px, uint8_t dot_idx)` — computes one dot's pixel center position with off-screen clipping (px < 0 || px > 159 || py < 0 || py > 143 → returns (0,0)). Uses `/ 64` (NOT `>> 6`) for portability of signed division.
   
   Done when: file compiles standalone with `cc -std=c99 -Isrc -c` and returns correct pixel positions for all 8 dots at radius 24 from center (80, 76).

2. ⬜ **Create `tests/test_range_calc.c`** — Host-side test exercising the pure helper. Test cases:
   - T1: 8 dots at center (80, 76), R=24 — all visible, verify symmetry (e.g., dot 0 at pixel (104, 76), dot 2 at (80, 100), dot 4 at (56, 76), dot 6 at (80, 52))
   - T2: center at (4, 12), R=40 — dots at angles 135°/180°/225° should be hidden (pixel x < 0)
   - T3: center at (156, 140), R=40 — dots at angles 315°/0°/45° should be hidden (pixel x > 159 or y > 143)
   - T4: R=0 — all 8 dots at center pixel (all visible, all same position)
   - T5: dot_idx > 7 — safe (masked to 3 bits internally, produces same result as idx & 7)
   - T6: Division correctness — verify negative offsets: center (40, 40), R=40, dot at angle 180° should give pixel x = 40 + (40*-64)/64 = 0 (hidden sentinel boundary; actually 0 IS the result but since we clip at < 0, x=0 passes → verify dot IS visible at (0, 40))
   
   Done when: `cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_range_calc.c -o build/test_range_calc && build/test_range_calc` passes.

3. ⬜ **Create `src/range_preview.h`** — Public API header. Includes `gtypes.h`. Declares:
   - `void range_preview_init(void);` — hides OAM 1..8, resets state
   - `void range_preview_update(u8 tx, u8 ty);` — per-frame dwell + show/hide logic
   - `void range_preview_hide(void);` — immediate hide + state reset
   
   Done when: header compiles without errors when included by game.c.

4. ⬜ **Create `src/range_preview.c`** — Implementation. Static state:
   - `u8 s_dwell` — frame counter (0..15+)
   - `u8 s_last_tx, s_last_ty` — tile cursor was on last frame (0xFF = invalid sentinel)
   - `u8 s_visible` — 1 if dots are currently shown
   
   `range_preview_init()`: sets s_dwell=0, s_last_tx=0xFF, s_last_ty=0xFF, s_visible=0, hides OAM 1..8 via `move_sprite(OAM_RANGE_BASE + i, 0, 0)` for i=0..7.
   
   `range_preview_update(tx, ty)`:
   1. `idx = towers_index_at(tx, ty)`. If idx < 0: if visible → hide; s_dwell=0; s_last_tx=tx; s_last_ty=ty; return.
   2. If tx != s_last_tx || ty != s_last_ty: if visible → hide; s_dwell=0; s_last_tx=tx; s_last_ty=ty; return. (Moved to different tower — dwell restarts next frame.)
   3. If s_dwell < RANGE_DWELL_FRAMES: increment s_dwell.
   4. If s_dwell >= RANGE_DWELL_FRAMES && !s_visible: compute and show 8 dots:
      - type = towers_get_type(idx), range_px = towers_stats(type)->range_px
      - cx = tx * 8 + 4, cy = (ty + HUD_ROWS) * 8 + 4
      - For i=0..7: dot = range_calc_dot(cx, cy, range_px, i)
        - If dot.x == 0 && dot.y == 0: move_sprite(OAM_RANGE_BASE+i, 0, 0) [hidden]
        - Else: set_sprite_tile(OAM_RANGE_BASE+i, SPR_PROJ), move_sprite(OAM_RANGE_BASE+i, dot.x + 4, dot.y + 12) [center the 8×8 sprite over the pixel]
      - s_visible = 1
   5. s_last_tx = tx; s_last_ty = ty. (Only reached if steps 1/2 did not return.)
   
   `range_preview_hide()`: move_sprite(OAM_RANGE_BASE + i, 0, 0) for i=0..7. s_visible=0, s_dwell=0.
   
   Done when: compiles with GBDK (`lcc -Isrc -Ires -c src/range_preview.c`).
   
   Includes required: `"range_preview.h"`, `"range_calc.h"`, `"towers.h"`, `"assets.h"`, `<gb/gb.h>`.

5. ⬜ **Add `RANGE_DWELL_FRAMES` to `src/tuning.h`** — Add at end (before `#endif`):
   ```c
   /* Iter-4 #31: range preview dwell threshold (frames). */
   #define RANGE_DWELL_FRAMES  15
   ```
   `range_preview.c` uses this constant instead of a magic `15`. Done when: constant defined and referenced.

6. ⬜ **Add OAM constants to `src/gtypes.h`** — Add after OAM_PAUSE_COUNT:
   ```c
   #define OAM_RANGE_BASE  1   /* 1..8 — range preview dots; shares menu/pause OAM */
   #define OAM_RANGE_COUNT 8
   ```
   Done when: constants are defined and used by range_preview.c.

7. ⬜ **Wire `range_preview_init()` into `enter_playing()`** — In `src/game.c`, add `range_preview_init()` after `pause_init()` in `enter_playing()`. Also add it in `enter_title()` after `pause_init()`.
   
   Done when: range preview OAM is hidden on game start and title return.

8. ⬜ **Wire `range_preview_update()` into `playing_update()`** — In `src/game.c`, add `range_preview_update(cursor_tx(), cursor_ty())` immediately after `cursor_update()` in the non-modal branch of `playing_update()`. Add `#include "range_preview.h"` at the top.
   
   Done when: range preview logic runs once per gameplay frame, only when no modal is open.

9. ⬜ **Add `range_preview_hide()` to `menu_open()`** — In `src/menu.c`, add `#include "range_preview.h"` and call `range_preview_hide()` after `projectiles_hide_all()` inside `menu_open()`.
   
   Done when: opening the upgrade/sell menu immediately clears any visible range dots.

10. ⬜ **Add `range_preview_hide()` to `pause_open()`** — In `src/pause.c`, add `#include "range_preview.h"` and call `range_preview_hide()` after `projectiles_hide_all()` inside `pause_open()`.
    
    Done when: opening pause immediately clears any visible range dots.

11. ⬜ **Wire test into justfile** — Add to the `test` recipe in `justfile`:
    ```bash
    cc -std=c99 -Wall -Wextra -O2 -Isrc \
       tests/test_range_calc.c -o "{{BUILD}}/test_range_calc"
    "{{BUILD}}/test_range_calc"
    ```
    Done when: `just test` runs the new test and it passes.

12. ⬜ **ROM build verification** — `just build` succeeds. ROM size stays ≤65536 bytes. `just check` passes (header bytes unchanged).
    
    Done when: full `just check` passes with no regressions.

## Acceptance Scenarios

### Setup
1. `just build` succeeds
2. `just test` passes (all existing + new test_range_calc)
3. Launch with `just run`
4. Place at least one tower of each type (AV, FW, EMP) to verify range differences

### Scenarios

| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Dwell shows dots | Move cursor to an existing AV tower. Hold still for ~0.25s (15 frames of dwell after the arrival frame). | 8 dot sprites appear in a circle around the tower at radius 24px (3 tiles). |
| 2 | Move off hides immediately | With dots visible on a tower, move cursor 1 tile away (any direction). | All 8 dots disappear instantly (same frame as cursor move). |
| 3 | Different tower types show different radii | Place AV (R=24), FW (R=40), EMP (R=16). Dwell on each. | Each shows dots at correct radius: AV~3 tiles, FW~5 tiles, EMP~2 tiles. |
| 4 | Move between two towers resets dwell | Dwell on tower A until dots show. Move to adjacent tower B. | Tower A dots hide immediately. After another ~0.25s dwell on tower B, tower B dots appear. |
| 5 | Menu open hides dots | Dwell on a tower until dots visible. Press A (opens upgrade/sell menu). | Dots disappear immediately. Menu renders normally with no stale sprites. |
| 6 | Pause open hides dots | Dwell on a tower until dots visible. Press START (opens pause). | Dots disappear immediately. Pause menu renders normally. |
| 7 | No dots during modal | While menu is open, dots do not appear (even though cursor is on a tower). | No spurious sprites. Menu/pause glyphs use OAM 1..16 uncontested. |
| 8 | Cursor on empty tile = no dots | Rest cursor on a ground tile (no tower) for 60+ frames. | No dots appear; no OAM corruption. |
| 9 | Edge-of-screen tower clips dots | Place FW tower at tx=0 (left edge, R=40). Dwell. | Dots that would be off-screen left are hidden. Remaining dots visible at correct positions. |
| 10 | Fast mode does not double-show | Enable 2× speed (SELECT). Dwell on a tower. | Dots appear normally after 15 gameplay frames (not 7.5). No visual glitch. |
| 11 | Dots survive tower upgrade | Dwell until dots show. Open menu, upgrade, close menu. Dwell again. | After re-dwell, dots show at same radius (range is level-independent). |
| 12 | Sell tower while preview active | Dwell on tower. Open menu → sell. Close menu. | Dots hidden on menu_open. After close, cursor is on empty tile — no dots. |

## Constraints

- **OAM per-scanline limit**: 8 dots + 1 cursor = 9 sprites. If all 9 share a scanline, 1 would flicker on real DMG. Acceptable: the dot circle has vertical spread, so at most 3-4 dots share a scanline in practice.
- **CPU budget**: Circle computation runs once on the show-transition (not per-frame). The per-frame cost is 1 `towers_index_at()` call (iterates ≤16 towers) + dwell counter bookkeeping. Negligible.
- **ROM growth**: ~200 bytes (range_preview.c) + ~80 bytes (range_calc.h tables are `static const` so only one copy in the including .c). Well within the 64KB cart.
- **WRAM growth**: 4 bytes (s_dwell + s_last_tx + s_last_ty + s_visible).
- **No new sprite tiles**: Reuses SPR_PROJ (tile 7) already in VRAM.
- **Dwell threshold**: 15 frames = 0.25s at 60fps. Defined as `RANGE_DWELL_FRAMES` in `tuning.h`. Fast enough to feel responsive, slow enough to avoid flicker when moving cursor across towers.

## Decisions

| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| Module location | Inside game.c; new range_preview.{h,c} | New module | Own state + OAM management is too much for game.c. Follows pause.c/menu.c pattern. |
| Circle table scale factor | ÷256 (full precision); ÷128 (i8-safe); ÷64 (shift-friendly) | ÷64 with `/64` division | Division (not shift) is portable for signed values. ÷64 gives <1px error at max range. One-time cost on show-transition is negligible. |
| Off-screen dot behavior | Wrap around; clamp to edge; hide at (255,255) | Hide at (255,255) | Wrapped/clamped dots would be visually confusing. Hidden = cleanest UX. Sentinel (255,255) is always off-screen (GB is 160×144). |
| Dwell reset on `range_preview_hide()` | Keep dwell; reset to 0 | Reset to 0 | After modal close, fresh dwell is appropriate — player's attention has shifted. |
| Show transition trigger | Every frame while visible (reposition); once on first show | Once on show transition | Dots are static (tower doesn't move). Avoids 8× move_sprite per frame. |
| Dot positions: center of tower tile | Top-left of tile; center (tx*8+4, cy+4) | Center | Radius measured from center matches gameplay targeting (towers.c uses center for range check). |
| range_calc_dot return type | OAM coordinates; pixel center coordinates | Pixel center | Keeps pure helper GB-agnostic. range_preview.c applies the +4/+12 OAM centering transform. Easier to test. |
| Arrival frame counting | Count arrival frame in dwell; exclude (return early) | Exclude (step 2 returns) | Cleaner separation between transition detection and dwell counting. 1-frame difference is imperceptible. |

## Review

### First review (Claude Sonnet)
- **F1 (medium): Step 2 missing return causes 14-frame dwell instead of 15** — Fixed: added explicit `return` to step 2. Arrival frame is excluded from dwell count. Dots appear after 15 frames of continuous rest (not counting the transition frame).
- **F2 (medium): Missing `#include "assets.h"` for SPR_PROJ** — Fixed: added explicit includes list to subtask 4.

### Cross-model validation (GPT-5.4)
- **F1 (high): Coordinate system inconsistency — OAM vs pixel** — Fixed: `range_calc_dot()` now returns screen-pixel center coordinates (GB-agnostic). `range_preview.c` applies the `+4/+12` OAM centering transform (`move_sprite(id, dot.x+4, dot.y+12)`). Documented the coordinate contract explicitly in Architecture section.
- **F2 (medium): Dwell timing 15 vs 16 frames** — Resolved: the arrival frame IS excluded (step 2 returns). Cursor rests for 15 frames (frames T+1 through T+15), dots show on T+15. Acceptance scenarios updated to use "~0.25s" rather than exact frame counts (QA verifies visually, not by counting frames).
- **F3 (medium): Signed right-shift portability** — Fixed: changed from `>> 6` to `/ 64`. Division truncation is well-defined for negative values in C99. Both SDCC and host compilers produce identical results. One-time computation cost on show-transition is negligible.
