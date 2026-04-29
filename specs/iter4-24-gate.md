# First-Tower Gate (Iter-4 Feature #24)

## Problem
New players can start a game and lose early because enemies spawn before any towers are placed. Wave 1's `FIRST_GRACE` timer (60 frames ≈ 1 second) begins immediately on `enter_playing()`, and if the player takes time to read the HUD or survey the map, bugs may reach the computer before a single tower is down. The fix: block wave progression until the player places at least one tower, with a blinking on-screen prompt.

## Architecture

### Module ownership: `game.c` (no new `.c` file)
Gate state lives in `game.c` alongside `playing_update()` / `playing_render()`. The gate is a transient gameplay-start condition — not a modal, not a subsystem — so a dedicated `.c` module would be over-engineering. A pure helper `src/gate_calc.h` holds the blink-phase computation for host testing (following the `game_modal.h` precedent).

### Zero changes to `waves.c`
`waves_init()` already sets `s_timer = FIRST_GRACE`. The gate simply **skips calling `waves_update()`** while active. When the gate lifts, `waves_update()` resumes and the timer counts down from `FIRST_GRACE` naturally. No new API, no new state in waves.c.

### New accessor: `map_tile_at()`
A new `u8 map_tile_at(u8 tx, u8 ty)` in `map.h` / `map.c` reads the original map tilemap data for a play-field-local position. This enables restoring the 12 BG tiles to their original map artwork when the gate text blinks off or when the gate lifts. Follows the existing `map_class_at()` pattern.

### Gate state variables (in `game.c`)
```c
static u8 s_gate_active;   /* 1 while waiting for first tower placement */
static u8 s_gate_blink;    /* frame counter for blink timing (wrapping u8) */
static u8 s_gate_vis;      /* last-painted blink phase: 1=text, 0=map tiles */
static u8 s_gate_dirty;    /* 1 = need BG restore on gate-lift frame */
```

### BG text layout (12 tiles)
Two 6-character lines centered on the 20-column play field:

| Content | Screen cols | Screen row | Play-field row |
|---------|-------------|------------|----------------|
| `PLACEA` | 7–12 | 8 | 7 |
| `TOWER!` | 7–12 | 9 | 8 |

Centering: `(PF_COLS - 6) / 2 = 7`. Total: 12 tile writes per blink transition.

The text reads naturally as "PLACE A / TOWER!" across two lines. The line break between "PLACE" and "A" provides visual word separation; the 'A' reads as the article "a" given the game context. No space character tile is needed, avoiding the visual mismatch between font-space (solid white) and ground tile (white with dot texture).

### Blink timing
Toggle every 30 frames = 1 Hz (title screen "PRESS START" precedent). Computed by `gate_blink_visible()` in `gate_calc.h`. Counter freezes during pause/menu (incremented only in the entity-update branch of `playing_update`).

### Initial paint during `DISPLAY_OFF`
The gate text is painted inside `enter_playing()`'s existing `DISPLAY_OFF` bracket, before `DISPLAY_ON`. This eliminates a one-frame flash of bare map tiles. `s_gate_vis` is initialized to `1` (text visible) and `s_gate_blink` to `0`.

### Gate lift trigger
In `playing_update()`, the existing `towers_try_place()` call's return value (currently discarded) is captured. On success while `s_gate_active == 1`:
- `s_gate_active = 0`
- `s_gate_dirty = 1` (schedule BG restore in next `playing_render`)

### Render order
`playing_render()` inserts `gate_render()` **before** `towers_render()`:
```
hud_update() → map_render() → gate_render() → towers_render() → menu_render() → pause_render()
```
This ensures that if the player places a tower on a tile covered by gate text, `towers_render()` overwrites the just-restored map tile with the tower tile. Tower tile wins.

### `gate_render()` logic
```
if s_gate_dirty:
    restore all 12 map tiles; clear s_gate_dirty; return
if !s_gate_active: return
if game_is_modal_open(): return          // no ambient BG writes during modal
want = gate_blink_visible(s_gate_blink)
if want == s_gate_vis: return            // no phase change
s_gate_vis = want
if want: paint 12 text tiles
else:    paint 12 map tiles (from map_tile_at)
```

### BG-write budget analysis

| Scenario | HUD | Gate | Map corruption | Towers | Total | OK? |
|----------|-----|------|----------------|--------|-------|-----|
| Gate steady (no blink edge) | 0 | 0 | 0 | 0 | 0 | ✅ |
| Gate blink edge | 3 (passive income coincidence worst case) | 12 | 0 | 0 | 15 | ✅ |
| Gate lift frame | 3 (E from tower purchase) | 12 (restore) | 0 | 1 (place) | 16 | ✅ |
| Normal play (after gate) | 10 | 0 | 4 | 1 | 15 | ✅ |

Gate-lift frame hits exactly 16 (the budget cap). This is safe because:
- HUD HP: clean (no enemies have spawned yet — waves were gated)
- HUD W: clean (wave counter unchanged during gate)
- HUD T: clean **only if B-button is gated** during the gate (see mitigation below)

**A+B same-frame race**: `J_B` and `J_A` are checked in sequence in `playing_update()`, not as `if/else`. Both can fire simultaneously. If B fires → HUD T dirty (+1 write). Then A fires → tower placed → HUD E dirty (+3 writes). Plus gate restore (12) + tower render (1) = **17 > 16**.

**Mitigation**: Gate the `J_B` (cycle tower type) handler on `!s_gate_active`. During the gate, only one tower type matters for the first placement:

```c
if (!s_gate_active && input_is_pressed(J_B)) {
    cycle_tower_type();
}
```

With this guard, the gate-lift frame budget is: 3 (E) + 12 (gate) + 1 (tower) = 16. ✅

### Path tile overlap is acceptable
All three current maps have path tiles passing through the gate text area (PF rows 7–8, cols 7–12). This is not a problem: each `set_bkg_tile_xy` call replaces the BG tile wholesale with an ASCII font glyph (opaque white background with black foreground). The text block appears as a solid white rectangle with black characters — fully readable regardless of what map tiles were underneath. When the text blinks off, the original map tiles (path, ground, etc.) are restored from `map_tile_at()`.

The conventions.md "1 empty tile of padding on every side" rule targets permanent HUD text, not temporary BG overlays that replace tiles. The gate text is a blinking overlay that swaps tiles; the font glyph's built-in opaque background provides its own visual separation.

### Pure helper: `src/gate_calc.h`
```c
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
```

Joins the `<stdint.h>`-only family (`tuning.h`, `game_modal.h`, `*_anim.h`, `difficulty_calc.h`).

### Files changed

| File | Change |
|------|--------|
| `src/gate_calc.h` | **New** — pure helper, blink phase computation |
| `src/game.c` | Gate state, `enter_playing()` init + initial paint, `playing_update()` gate logic, `playing_render()` gate render, `J_B` gate guard |
| `src/map.h` | Add `map_tile_at()` declaration |
| `src/map.c` | Add `map_tile_at()` implementation |
| `tests/test_gate.c` | **New** — host tests for `gate_calc.h` |
| `justfile` | Add `test_gate` to `test` recipe |
| `memory/decisions.md` | D-GATE-1 through D-GATE-3 |
| `memory/conventions.md` | Gate convention block |

## Subtasks

1. ✅ **Add `map_tile_at()` to `map.h` / `map.c`** — Declare `u8 map_tile_at(u8 tx, u8 ty);` in `map.h`. Implement in `map.c` following the `map_class_at()` pattern: bounds-check `tx < PF_COLS && ty < PF_ROWS`, index into `s_maps[s_active_map_id].tilemap[ty * PF_COLS + tx]`, return `TILE_GROUND` for out-of-bounds. Done when `just build` compiles without warnings and the function is callable from `game.c`.

2. ✅ **Create `src/gate_calc.h`** — Header-only pure helper with include guard (`GBTD_GATE_CALC_H`), `#ifndef bool` portability block (matching `game_modal.h`), and `gate_blink_visible(uint8_t frame_counter)` returning `uint8_t` (1 or 0). Uses `<stdint.h>` only (no GBDK headers, no `gtypes.h`). Exact content specified in the Architecture section. Done when `cc -std=c99 -fsyntax-only -Isrc src/gate_calc.h` compiles on the host AND double-include (`#include "gate_calc.h"` twice in a `.c` file) compiles without redefinition errors.

3. ✅ **Create `tests/test_gate.c` and wire into `justfile`** — Test cases for `gate_calc.h`:
   - T1: `gate_blink_visible(0)` returns 1 (visible at frame 0)
   - T2: `gate_blink_visible(29)` returns 1 (still visible at frame 29)
   - T3: `gate_blink_visible(30)` returns 0 (hidden at frame 30)
   - T4: `gate_blink_visible(59)` returns 0 (still hidden at frame 59)
   - T5: `gate_blink_visible(60)` returns 1 (visible again at frame 60)
   - T6: `gate_blink_visible(255)` returns expected value ((255/30=8, 8&1=0) → 1)
   - T7: boundary sweep — verify that transitions happen exactly at multiples of 30
   
   Add compilation + run to the `just test` recipe:
   ```
   cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_gate.c -o "${BUILD}/test_gate"
   "${BUILD}/test_gate"
   ```
   Done when `just test` passes including the new test binary.
   - Depends on: subtask 2

4. ✅ **Add gate state and initial paint to `enter_playing()` in `game.c`** — Add state variables `s_gate_active`, `s_gate_blink`, `s_gate_vis`, `s_gate_dirty` (all `static u8`) at file scope. Add `#include "gate_calc.h"`. Define gate text constants:
   ```c
   #define GATE_COL    7
   #define GATE_ROW1   8   /* screen row = PF row 7 + HUD_ROWS */
   #define GATE_ROW2   9   /* screen row = PF row 8 + HUD_ROWS */
   #define GATE_W      6
   ```
   Add two static helper functions:
   - `gate_paint_text()`: writes "PLACEA" at `(GATE_COL, GATE_ROW1)` and "TOWER!" at `(GATE_COL, GATE_ROW2)` using `set_bkg_tile_xy()` with ASCII char casts.
   - `gate_restore_tiles()`: for each of the 12 positions, calls `map_tile_at(col - 0, pf_row)` (converting screen coords back to PF-local: col=7..12, pf_row = screen_row - HUD_ROWS = 7 or 8) and writes the result via `set_bkg_tile_xy()`.
   
   In `enter_playing()`, after `hud_init()` and before `DISPLAY_ON`:
   ```c
   s_gate_active = 1;
   s_gate_blink  = 0;
   s_gate_vis    = 1;
   s_gate_dirty  = 0;
   gate_paint_text();   /* safe: display is off */
   ```
   Done when `just build` compiles without warnings and the gate text is visible on the first frame after starting a new game in mGBA.
   - Depends on: subtasks 1, 2

5. ✅ **Wire gate into `playing_update()`** — Three changes in the entity-update branch (the `else` block that currently runs cursor/towers/enemies/projectiles/waves):
   
   **5a. Gate the B-button handler:**
   ```c
   if (!s_gate_active && input_is_pressed(J_B)) {
       cycle_tower_type();
   }
   ```
   (Prevents A+B same-frame BG-write budget overflow on the gate-lift frame.)
   
   **5b. Capture `towers_try_place()` return value and detect gate lift:**
   ```c
   if (towers_try_place(cursor_tx(), cursor_ty(), s_selected_type)) {
       if (s_gate_active) {
           s_gate_active = 0;
           s_gate_dirty  = 1;
       }
   }
   ```
   (Replaces the existing bare `towers_try_place(...)` call.)
   
   **5c. Conditionally skip `waves_update()` and increment blink counter:**
   Replace the existing `waves_update();` call (and ONLY that call — the `waves_just_cleared_wave()` score-drain block on the lines immediately below MUST be preserved verbatim) with:
   ```c
   if (s_gate_active) {
       s_gate_blink++;
   } else {
       waves_update();
   }
   ```
   The iter-3 #19 score-drain block that follows (`u8 cleared = waves_just_cleared_wave(); if (cleared) score_add_wave_clear(cleared);`) remains unconditional — it runs on every entity-update frame regardless of gate state. This is safe because `waves_just_cleared_wave()` returns 0 when no wave has cleared (which is always the case while the gate is active, since waves haven't started).
   
   Done when: (a) waves do not spawn until a tower is placed, (b) placing a tower lifts the gate and FIRST_GRACE countdown begins, (c) B-button tower cycling is blocked during the gate, (d) completing a wave after the gate is lifted still correctly fires `score_add_wave_clear()` (verify wave-clear score accumulates during normal play).
   - Depends on: subtask 4

6. ✅ **Add `gate_render()` to `playing_render()`** — Implement static `gate_render()` per the Architecture section's pseudocode. Insert the call in `playing_render()` between `map_render()` and `towers_render()`:
   ```c
   static void playing_render(void) {
       hud_update();
       map_render();
       gate_render();      /* ← NEW: before towers so tower tile wins on overlap */
       towers_render();
       menu_render();
       pause_render();
   }
   ```
   Done when: (a) gate text blinks at ~1 Hz during the gate period, (b) blink freezes during pause, (c) on gate lift the 12 tiles are restored to map artwork, (d) if tower is placed on a gate-text tile, the tower tile is visible (not overwritten by restore).
   - Depends on: subtask 5

7. ✅ **Update `memory/decisions.md` and `memory/conventions.md`** — Append:
   - `decisions.md`: D-GATE-1 (gate lives in game.c), D-GATE-2 (skip waves_update, no waves.c changes), D-GATE-3 (B-button gated during gate)
   - `conventions.md`: Iter-4 #24 gate conventions block (gate state, blink timing, render order, BG-write budget with gate, map_tile_at accessor)
   
   Done when both files are updated and consistent with the implementation.
   - Depends on: subtasks 1–6

8. ⬜ **Integration test in mGBA** — Verify all acceptance scenarios below. Done when all scenarios pass.

## Acceptance Scenarios

### Setup
```bash
just test    # All host tests pass (including new test_gate)
just build   # ROM builds without warnings
just run     # Launch in mGBA
```

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Gate text appears on new game | Start a new game (any map, any difficulty). Do not press any button. | Centered "PLACEA" / "TOWER!" text blinks on play-field rows 7–8 at ~1 Hz. No enemies spawn. Wave counter stays at W:01/10. |
| 2 | Gate lifts on first tower placement | During the gate, move cursor to a valid ground tile, press A to place a tower. | Gate text disappears (map tiles restored). FIRST_GRACE countdown begins (enemies spawn ~1 second later). |
| 3 | FIRST_GRACE starts after gate lift | Place first tower, then count frames until first enemy appears. | First enemy spawns at approximately 60 frames (1 second) after tower placement, not after game start. |
| 4 | Subsequent towers don't re-trigger gate | After the first tower, place a second tower. | No gate text reappears. Waves continue normally. |
| 5 | Pause during gate | During the gate (before placing a tower), press START to pause. | Pause overlay appears. Gate text is visible behind pause sprites (BG layer). Gate text does not blink while paused. |
| 6 | Resume from pause during gate | While paused during the gate, press A on RESUME. | Pause overlay disappears. Gate text resumes blinking from where it left off. Still no enemies spawning. |
| 7 | B-button blocked during gate | During the gate, press B. | Nothing happens (tower type does not cycle). HUD tower indicator stays at initial value. |
| 8 | B-button works after gate | After placing first tower (gate lifted), press B. | Tower type cycles (A→F→E→A). HUD indicator updates. |
| 9 | Tower on gate-text tile | During the gate, move cursor to a tile within the gate text area (play-field row 7 or 8, cols 7–12), press A. | Tower is placed. Gate text disappears. Tower tile is visible at that position (not overwritten by map restore). |
| 10 | Gate on every new game | Complete a game (win or lose), return to title, start a new game. | Gate text appears again. Waves gated until first tower placement. |
| 11 | Host tests pass | Run `just test`. | All test binaries pass, including `test_gate` (7+ checks for blink phase computation). |
| 12 | BG-write budget on gate-lift frame | Place tower on a ground tile (not in the gate text area) while HUD E is the only dirty field. | No visual glitch or tile corruption. (Budget: 3 HUD + 12 restore + 1 tower = 16 ≤ 16.) |

## Constraints
- BG-write budget: ≤16 writes/frame. Gate-lift frame is the tightest at exactly 16.
- Gate text is exactly 12 tiles (6 per line). Do not exceed this — it directly affects the budget ceiling.
- Blink rate: 1 Hz (30-frame half-period). Must not exceed 2 Hz per UI text convention.
- `gate_calc.h` uses `<stdint.h>` only — no GBDK headers, no `gtypes.h`.
- `map.c` must NOT include `game.h` (decision D8). The new `map_tile_at()` follows this constraint.
- Gate state resets in `enter_playing()` only — not in `enter_title()`, not on pause-resume.
- The B-button tower-cycle handler must be gated on `!s_gate_active` to prevent A+B same-frame budget overflow.

## Decisions
| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| D-GATE-1: Gate state location | A) New `gate.c` module; B) Inside `waves.c`; C) Inside `game.c` with pure helper | C) `game.c` + `gate_calc.h` | Gate is a transient start condition, not a subsystem. `game.c` already owns `playing_update()` flow control and the `waves_update()` call site. A new `.c` module adds include complexity with no benefit. Pure helper keeps blink logic testable. |
| D-GATE-2: How to defer FIRST_GRACE | A) Add a `waves_pause()` / `waves_resume()` API; B) Reset `s_timer` via new `waves_arm_grace()`; C) Skip `waves_update()` calls while gated | C) Skip `waves_update()` | Zero changes to waves.c. `waves_init()` already sets `s_timer = FIRST_GRACE`. Skipping the update call freezes the timer. When calls resume, the countdown proceeds naturally. Simplest possible approach. |
| D-GATE-3: B-button during gate | A) Allow tower cycling during gate; B) Block B-button during gate | B) Block | Prevents A+B same-frame BG-write budget overflow (17 writes without the guard). During the gate, the player hasn't placed any tower yet, so cycling the type indicator has low value. The guard is a single `if` condition. |
| D-GATE-4: Gate text content | A) "PLACE A" (7) + "TOWER!" (6) = 13 tiles; B) "PLACEA" (6) + "TOWER!" (6) = 12 tiles; C) "PLACE" (5) + "TOWER!" (6) = 11 tiles | B) 12 tiles | Exactly matches the 12-tile precedent from title screen "PRESS START" blink. Keeps gate-lift frame at 16 writes (3 HUD + 12 restore + 1 tower). 13 tiles would hit 17 on a worst-case gate-lift frame. The concatenated "PLACEA" reads naturally as "PLACE A" given the line break and game context. |
| D-GATE-5: Initial paint timing | A) Paint during DISPLAY_OFF in `enter_playing()`; B) Paint on first `gate_render()` call (frame 1) | A) During DISPLAY_OFF | Eliminates a one-frame flash of bare map tiles. No BG-write budget concern since display is off. |
| D-GATE-6: Render order | A) `gate_render()` after `towers_render()`; B) `gate_render()` before `towers_render()` | B) Before towers | If tower is placed on a gate-text tile, `towers_render()` writes the tower tile AFTER `gate_render()` restores the map tile. Tower tile wins — correct behavior. |

## Review

### Adversarial review (Decker — Sonnet)
Three findings, all resolved:

1. **`gate_calc.h` missing include guard** — The initial architecture code sample omitted `#ifndef GBTD_GATE_CALC_H` guards. **Resolved**: Updated the code sample in Architecture to include full guards + `#endif`, and added a double-include compilation check to subtask 2's done criteria.

2. **Gate text violates 1-tile padding convention** — The text at PF rows 7–8 has no guaranteed blank padding tiles around it. Investigation confirmed all 3 maps have path tiles passing through the gate text area. **Resolved**: Added an "Path tile overlap is acceptable" section to Architecture explaining that font glyph tiles are opaque (white background, black text) so the text block reads as a solid white rectangle — fully legible regardless of underlying map tiles. The padding convention targets permanent HUD text, not temporary BG overlays that swap tiles wholesale. Expanding to include padding would require 32 tile writes per blink (12 text + 20 padding), exceeding the 16-write budget.

3. **Subtask 5c risks breaking iter-3 #19 score tracking** — The `waves_just_cleared_wave()` / `score_add_wave_clear()` block sits immediately below `waves_update()` in `playing_update()`. A coder could accidentally replace or move it. **Resolved**: Updated subtask 5c to explicitly state the score-drain block must be preserved verbatim, explain why it's safe to run unconditionally (returns 0 during gate), and added a fourth done criterion: *(d) wave-clear score still accumulates during normal play.*

### Cross-model validation (GPT-5.4)
Passed with no findings. Confirmed: `game_is_modal_open()` correctly unchanged; all architectural elements traceable to subtasks; acceptance scenarios testable; implementation complete without ambiguity.
