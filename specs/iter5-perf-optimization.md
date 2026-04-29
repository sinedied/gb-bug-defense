# Performance Optimization (Iter-5)

## Problem
The game drops to ~30 FPS on DMG hardware during gameplay, audible as music playing at half speed. Three distinct symptoms:
1. **Instant, severe slowdown when upgrade/sell menu is open** — `menu_render()` in `src/menu.c:160-225` rewrites all 14 OAM sprite cells (28 GBDK calls: `set_sprite_tile` + `move_sprite` × 14) every frame, plus recomputes anchor position, tower stats, and a division. Content only changes on open or on selection change.
2. **Slowdown with many entities** — `towers_update()` in `src/towers.c:268-350` performs 16-bit software multiplications (u16 × u16) for range checks. Worst case: every EMP tower at cooldown=0 with no target in range rescans all 14 enemies every frame (each scan = two 16-bit multiplies per alive enemy, each multiply ~100-200 cycles on the Z80 software routine).
3. **General per-frame overhead** — `range_preview_update()` (`src/range_preview.c:40-44`) and `cursor_on_valid_tile()` (`src/cursor.c:22-26`) both call `towers_at()` or `towers_index_at()` (16-iteration loops) every frame regardless of cursor movement.

The 2× fast-mode (iter-4 #30, `src/game.c:288-289`) compounds all entity-tick issues by calling `entity_tick()` twice per frame.

## Architecture

### Strategy: reduce per-frame CPU work without changing behavior
All optimizations are internal — no gameplay changes, no new modules, no changes to game state or outcomes. One small internal cross-module API addition (`cursor_invalidate_cache()`) is needed for cache correctness. The approach:
- **Eliminate redundant work** — don't repaint static OAM content per-frame
- **Early-exit before expensive math** — reject impossible range matches with cheap u8 comparisons before 16-bit multiplies
- **Gate on state-change** — skip work when inputs haven't changed since last frame

### Modules affected
| Module | File | Change type |
|--------|------|-------------|
| menu | `src/menu.c` | Dirty-flag to skip OAM rewrite |
| towers | `src/towers.c` | Manhattan early-reject in range scan; add `#include "cursor.h"` |
| range_preview | `src/range_preview.c` | Early return when cursor stationary + dots visible |
| cursor | `src/cursor.c`, `src/cursor.h` | Cache valid-tile result; new `cursor_invalidate_cache()` |
| projectiles | `src/projectiles.c` | Eliminate redundant enemy position reads |

### Hot spot analysis (code references)

**menu_render per-frame OAM rewrite** (`src/menu.c:160-225`):
```c
void menu_render(void) {
    // ... BG save/restore (one-shot, fine) ...
    if (!s_open) return;
    compute_anchor(&mx, &my);  // recomputed EVERY frame
    // ... 14 × place_cell() calls ...
    // Each place_cell = set_sprite_tile + move_sprite = 2 GBDK calls
}
```
All 14 cells are repainted even when nothing changed. This is ~28 function calls + anchor math + stat lookups + division per frame.

**towers_update inner loop** (`src/towers.c:272-286` and `317-328`):
```c
u16 adx = dx < 0 ? (u16)-dx : (u16)dx;
u16 ady = dy < 0 ? (u16)-dy : (u16)dy;
u16 d2 = adx * adx + ady * ady;  // TWO 16-bit multiplies here
if (d2 > range_sq) continue;
```
With range values 16/24/40 px and a 20×17 tile play field (160×136 px), most enemies are well outside range. The multiply is wasted.

**range_preview unconditional lookup** (`src/range_preview.c:40-44`):
```c
void range_preview_update(u8 tx, u8 ty) {
    i8 idx;
    idx = towers_index_at(tx, ty);  // 16-iteration loop EVERY frame
```

## Subtasks

1. ✅ **menu_render: paint-on-change only** — Add `static u8 s_menu_dirty;` to `menu.c`. Set to `1` in `menu_open()` (after assigning `s_sel=0`). In `menu_update()`, change the D-pad handlers to only set `s_menu_dirty = 1` when `s_sel` actually changes:
   ```c
   // Before (unconditional write):
   if (input_is_repeat(J_UP))   s_sel = 0;
   if (input_is_repeat(J_DOWN)) s_sel = 1;

   // After (dirty only on change):
   if (input_is_repeat(J_UP)   && s_sel != 0) { s_sel = 0; s_menu_dirty = 1; }
   if (input_is_repeat(J_DOWN) && s_sel != 1) { s_sel = 1; s_menu_dirty = 1; }
   ```
   In `menu_render()`, after the BG save/restore phases (lines 164-185 — these stay unconditional with their own one-shot flags) and after the `if (!s_open) return;` guard (line 187), add:
   ```c
   if (!s_menu_dirty) return;
   ```
   At the end of the OAM paint block (after line 224), add:
   ```c
   s_menu_dirty = 0;
   ```
   Initialize `s_menu_dirty = 0;` in `menu_init()` (alongside the other state resets).

   - Done when: With menu open and no D-pad input, `set_sprite_tile` and `move_sprite` for OAM 1-14 are NOT called. Opening menu still paints correctly. UP/DOWN selection change still repaints. Closing menu (B or A→action) still works.
   - Dependencies: none.
   - Tests affected: none (menu_render is not host-tested; verified via manual smoke).

2. ✅ **towers_update: Manhattan early-reject** — In both range-scan inner loops, add a u8 component check BEFORE the 16×16 multiply.

   **Step 2a**: Change `acquire_target` signature from `(u8 cx_px, u8 cy_px, u16 range_sq)` to `(u8 cx_px, u8 cy_px, u16 range_sq, u8 range_px)`. Update the single call site (line 341) to pass `st->range_px` as the 4th argument.

   **Step 2b**: In `acquire_target()`, after computing `adx` and `ady` (lines 276-277), add before the multiply:
   ```c
   if (adx > (u16)range_px || ady > (u16)range_px) continue;
   ```

   **Step 2c**: In the TKIND_STUN inner loop (lines 320-321), after computing `adx` and `ady`, add before the multiply:
   ```c
   if (adx > (u16)st->range_px || ady > (u16)st->range_px) continue;
   ```

   **Correctness proof**: `range_sq = range_px × range_px`. If `adx > range_px`, then `adx² > range_px² = range_sq`. Since `ady² ≥ 0`, `d2 = adx² + ady² > range_sq`. Therefore any target rejected by the early check would also be rejected by the `d2 > range_sq` check. The optimization is strictly conservative (never rejects a valid target).

   - Done when: `just test` passes (including test_towers.c which exercises tower targeting). Tower targeting behavior unchanged — enemies within range still acquired, enemies outside range still rejected.
   - Dependencies: none.
   - Tests affected: `tests/test_towers.c` — the stub `enemies_x_px`/`enemies_y_px` values place enemies within EMP range for existing tests. The early-reject won't filter them. No test changes needed.

3. ✅ **range_preview_update: skip when visible + cursor stationary** — Add a single early-return at the top of `range_preview_update()`:
   ```c
   void range_preview_update(u8 tx, u8 ty) {
       /* Optimization: if dots are already displayed and cursor hasn't moved,
        * there is nothing to update — positions are static. */
       if (tx == s_last_tx && ty == s_last_ty && s_visible) return;

       i8 idx;
       // ... existing code unchanged ...
   ```
   This short-circuits the entire function (including the `towers_index_at` call) ONLY when the cursor is stationary AND dots are already visible. All other paths work exactly as before:
   - Dwell counter still increments frame-by-frame while cursor is on a tower but dots not yet shown (`s_visible=0` → early return skipped)
   - Moving off a tower still hides dots (cursor moved → `tx != s_last_tx || ty != s_last_ty` → early return skipped)
   - `towers_index_at` still called on every frame during dwell accumulation (needed to detect tower presence)

   - Done when: With cursor stationary on a tower and dots already visible, `towers_index_at` is not called. All other range preview behaviors unchanged.
   - Dependencies: none.
   - Tests affected: none (range_preview is not host-tested; dot math covered by test_range_calc).

4. ✅ **cursor_update: cache valid-tile result** — Add state to `src/cursor.c`:
   ```c
   static u8   s_cache_tx;     /* tile x where validity was last computed */
   static u8   s_cache_ty;     /* tile y where validity was last computed */
   static bool s_cache_valid;  /* cached result of cursor_on_valid_tile() */
   ```
   In `cursor_init()`, add: `s_cache_tx = 0xFF; s_cache_ty = 0xFF; s_cache_valid = false;`
   (0xFF is an impossible PF coordinate — PF_COLS=20, PF_ROWS=17 — so the first `cursor_update()` will always mismatch and recompute.)

   In `cursor_update()`, replace `bool valid = cursor_on_valid_tile();` (line 45) with:
   ```c
   bool valid;
   if (s_tx == s_cache_tx && s_ty == s_cache_ty) {
       valid = s_cache_valid;
   } else {
       valid = cursor_on_valid_tile();
       s_cache_tx = s_tx;
       s_cache_ty = s_ty;
       s_cache_valid = valid;
   }
   ```

   Add to `src/cursor.h`:
   ```c
   void cursor_invalidate_cache(void);
   ```
   Implementation in `src/cursor.c`:
   ```c
   void cursor_invalidate_cache(void) { s_cache_tx = 0xFF; }
   ```

   Call `cursor_invalidate_cache()` from two places in `src/towers.c`:
   - At the end of `towers_try_place()`, after `audio_play(SFX_TOWER_PLACE);` (line 159)
   - At the end of `towers_sell()`, after `audio_play(SFX_TOWER_PLACE);` (line 209)

   Add `#include "cursor.h"` to `src/towers.c` (new include; towers.c already has 8 includes).

   - Done when: `towers_at()` (inside `cursor_on_valid_tile()`) is only called when cursor moves or when cache is invalidated. First frame after init computes correctly. Placing/selling a tower on the cursor's current tile triggers cache invalidation and correct blink feedback next frame.
   - Dependencies: none (edits different functions than subtask 2 in towers.c — coordinate diffs).
   - Tests affected: `tests/test_towers.c` — must add a stub: `void cursor_invalidate_cache(void) {}` in the stubs section (since towers.c now includes cursor.h which declares it).

5. ✅ **step_proj: eliminate redundant enemy position reads** — In `src/projectiles.c::step_proj()`, the function calls `enemies_x_px(p->target)` and `enemies_y_px(p->target)` twice each: once at line 60-61 (for movement direction computation) and again at lines 68-69 (for hit detection). Cache into locals at the top after the alive/gen check:

   Replace lines 60-74 with:
   ```c
   u8 ex_px = enemies_x_px(p->target);
   u8 ey_px = enemies_y_px(p->target);
   fix8 tx = (fix8)((i16)ex_px << 8);
   fix8 ty = (fix8)((i16)ey_px << 8);
   i16 ddx = tx - p->x;
   i16 ddy = ty - p->y;

   /* Hit check (squared pixel distance, unsigned). */
   i16 px = (i16)FIX8_INTU(p->x);
   i16 py = (i16)FIX8_INTU(p->y);
   i16 dxp = (i16)ex_px - px;
   i16 dyp = (i16)ey_px - py;
   u16 adxp = dxp < 0 ? (u16)-dxp : (u16)dxp;
   u16 adyp = dyp < 0 ? (u16)-dyp : (u16)dyp;
   u16 d2 = adxp * adxp + adyp * adyp;
   ```

   - Done when: `enemies_x_px` and `enemies_y_px` are each called exactly once per `step_proj` invocation. Projectile behavior (hit detection, movement) unchanged.
   - Dependencies: none.
   - Tests affected: none (projectiles.c has no host-side test).

6. ✅ **Validation: full test suite + manual smoke** — Run `just test` (all host-side tests pass, including the new `cursor_invalidate_cache` stub in test_towers.c). Run `just check` (ROM builds, header validates). Perform manual playtest covering all acceptance scenarios.
   - Done when: All automated tests pass. All 10 acceptance scenarios confirmed via manual playtest.
   - Dependencies: subtasks 1-5.

## Acceptance Scenarios

### Setup
1. `just build` succeeds without warnings
2. `just test` — all host-side test binaries pass
3. `just run` — launch in mGBA

### Measurement method
Music tempo is the objective proxy for frame rate on DMG. The music engine (`music_tick()` in `src/music.c`) advances one row-duration countdown per `audio_tick()` call, which fires exactly once per `game_update()` frame. If frames are being dropped (game_update takes >1 VBlank), `wait_vbl_done()` returns late and music plays at half speed — this is immediately, unambiguously audible as a pitch/tempo drop. No subjective judgment is needed: either the melody sounds correct or it doesn't.

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Music tempo normal gameplay | Start NORMAL, place 3 AV towers near path, let W3+ spawn 12+ enemies | Music plays at consistent tempo — no audible slowdown |
| 2 | Music tempo with menu open | With enemies on screen, open upgrade/sell menu (A on tower). Hold open for 5+ seconds. | Music tempo unchanged while menu is open |
| 3 | Music tempo in fast mode | Toggle fast mode (SELECT), play through W8+ (14 enemies, 8 projectiles) | Music at correct tempo with full entity pools |
| 4 | Menu open + paint | Open menu on tower → menu appears with correct text/prices | Initial paint works |
| 5 | Menu selection change | Open menu → DOWN (">SEL" highlighted) → UP (">UPG" highlighted) | Selection cursor moves correctly |
| 6 | Menu upgrade action | Open menu → A (upgrade) → menu closes, tower upgrades | Upgrade works as before |
| 7 | Menu sell action | Open menu → DOWN → A (sell) → menu closes, tower sold | Sell works as before |
| 8 | Menu cancel | Open menu → B → menu closes | Cancel works as before |
| 9 | Tower targeting | Place AV tower at range edge, send mixed wave | "First" targeting (highest wp_idx) within range |
| 10 | EMP stun | Place EMP near path corner | Stun fires, cooldown respects 3-outcome rule |
| 11 | Range preview dwell | Cursor → tower → wait → dots appear. Move away → dots disappear. Return → wait → dots reappear. | Identical to pre-optimization |
| 12 | Cursor blink feedback | Move: ground=valid (slow), path=invalid (fast), tower=invalid (fast) | Blink rates correct |
| 13 | Cursor validity after placement | On ground (valid). Place tower (A). Now on tower = invalid blink. | Cache invalidation triggers correct feedback |
| 14 | Combined stress test | 16 towers (AV+FW+EMP), fast mode, W10, pools at max | Music at correct tempo throughout |

## Constraints
- Frame budget: ~70k cycles/frame on DMG (4.19 MHz / 59.73 Hz)
- BG-write budget: ≤16 writes/frame — **unchanged** (no subtask adds BG writes)
- OAM allocation: unchanged — only the *frequency* of OAM writes changes
- All optimizations MUST be behavior-preserving — identical game outcomes given identical inputs
- `just test` must pass (only change: add one stub to test_towers.c)
- New WRAM: +4 bytes total (3 cursor cache + 1 menu dirty flag)
- New include: `towers.c → cursor.h` (for `cursor_invalidate_cache`)
- Out of scope: anything that changes gameplay, balance, or visual appearance

## Decisions
| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| menu_render approach | A) Dirty flag skips full OAM paint; B) Incremental per-cell paint (only changed cells); C) Timer-based repaint | A — dirty flag | Menu has 2 visual states (sel=0/1). Full paint on dirty is cheap (~14 calls, once). Per-cell tracking adds complexity for negligible additional savings. Timer introduces latency. |
| Manhattan reject formula | A) `adx > range_px \|\| ady > range_px`; B) `adx + ady > range_px`; C) Lookup table of range_sq per range tier | A — axis component check | Mathematically proven conservative (never rejects valid target). Cheapest: 2 u8 compares + 1 branch, vs 16-bit multiply. Option B over-rejects valid targets at 45° angles. Option C wastes ROM and requires maintenance. |
| range_preview scope | A) Full restructure with cached tower-present flag; B) Single early-return when visible + unchanged | B — minimal early-return | Zero risk of dwell-timing regression. Covers the dominant steady-state case. Full restructure (option A) had a proven dwell-timing bug identified in review. |
| cursor cache invalidation | A) Explicit `cursor_invalidate_cache()` from towers.c; B) Per-frame "any tower changed" flag from game.c; C) Version counter on tower pool | A — explicit call | One new include, two call sites. Minimal coupling. B requires game.c to know cursor internals. C adds per-frame comparison overhead. |
| acquire_target signature change | A) Add `u8 range_px` param; B) Compute `range_px` from `sqrt(range_sq)` inside; C) Only pass `range_px`, compute `range_sq` inside | A — add param | Both values already available at call site. No sqrt (which is extremely expensive on Z80). Minimal signature delta. |

## Review
Adversarial review findings and resolutions:

1. **F1 (HIGH): range_preview dwell timing regression** — Original subtask 3 proposed a full restructure that would have incorrectly accumulated dwell on tower-less tiles. **Resolved**: reduced scope to a single early-return line that only fires when `s_visible=1 && cursor unchanged`. All non-visible paths (dwell accumulation, no-tower reset) run exactly as before.

2. **F2 (MED): range_preview underspecified state machine** — The full restructure required a decision table for {moved × visible × tower_present}. **Resolved**: by choosing the minimal single-line approach, no state machine redesign is needed. Existing logic handles all edge cases unchanged.

3. **F3 (MED): cursor cache missing init sentinel** — Without a sentinel, the first frame would use an uninitialized cache. **Resolved**: `s_cache_tx = 0xFF` in cursor_init() guarantees mismatch (0xFF > PF_COLS) on first update, forcing recomputation.

4. **F4 (MED): subtask 4 dependency on subtask 2** — Incorrectly stated. They edit different functions in towers.c. **Resolved**: dependency removed; subtasks can proceed in parallel.

5. **F5 (MED): "no API changes" vs cursor_invalidate_cache** — Architecture claimed no API changes but subtask 4 adds a cross-module function. **Resolved**: architecture text updated to accurately state "one small internal cross-module API addition for cache correctness."

6. **F6 (MED): acceptance scenario impossible step sequence** — Scenario 4 had sell+cancel in sequence (sell closes menu, so B is unreachable). **Resolved**: split into separate scenarios (4-8) covering open, select, upgrade, sell, cancel independently.

7. **F7 (MED): performance scenarios not objectively testable** — Music tempo is the objective proxy: music_tick advances per frame, so half-speed = audibly wrong pitch. **Resolved**: added measurement method section explaining why this is unambiguous, not subjective.
