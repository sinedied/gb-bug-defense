# Iteration 3 #20 — Difficulty Modes (EASY / NORMAL / HARD)

## Problem
Roadmap entry #20: *"Difficulty modes — Easy / Normal / Hard scaling HP + spawn rate."*
Players currently get a single fixed balance (NORMAL, locked in by iter‑2). Skilled players want a sterner test (HARD) and newcomers want a gentler ramp (EASY) without us redesigning all 10 wave tables. The selection must be made on the title screen and survive a quit‑to‑title within the same power‑on session, but must not require SRAM (that is feature #19, a separate spec).

## Architecture

### State + scope
- A single `u8 s_difficulty` static lives in `src/game.c`, initialised to `DIFF_NORMAL` at file‑scope (so it survives `enter_title()` / `enter_playing()` cycles without being touched).
- The enum `enum { DIFF_EASY = 0, DIFF_NORMAL = 1, DIFF_HARD = 2, DIFF_COUNT = 3 };` lives in **`src/difficulty_calc.h`** (the pure `<stdint.h>`‑only header — see below). `game.h` `#include`s `difficulty_calc.h` so on‑device callers get the enum transitively, and host tests can include `difficulty_calc.h` directly without pulling `gtypes.h` / `<gb/gb.h>`.
- Public API in `game.h`: `u8 game_difficulty(void);` and `void game_set_difficulty(u8 d);` (setter validates `d < DIFF_COUNT`, otherwise no‑op).
- Difficulty is **read** at three points (enemy spawn, wave timer arming, economy reset) and **written** only from the title screen.

### Pure helper header (host‑testable)
New header `src/difficulty_calc.h`, following the iter‑3 #21 convention (`<stdint.h>` only, no GBDK pull‑in, joins `tuning.h` / `game_modal.h` / `*_anim.h`):

```c
#ifndef GBTD_DIFFICULTY_CALC_H
#define GBTD_DIFFICULTY_CALC_H
#include <stdint.h>
#include "tuning.h"

/* Difficulty enum — lives here (NOT in game.h) so host tests can include
 * this header without pulling gtypes.h / <gb/gb.h>. game.h #includes this
 * header so on-device callers see the enum transitively. */
enum { DIFF_EASY = 0, DIFF_NORMAL = 1, DIFF_HARD = 2, DIFF_COUNT = 3 };

/* Per-(difficulty, enemy-type) HP table — 6 bytes ROM.
 * Indexed [difficulty][enemy_type]. Order matches DIFF_* and ENEMY_*. */
static const uint8_t DIFF_ENEMY_HP[3][2] = {
    /* EASY   */ { 2, 4 },                    /* bug, robot */
    /* NORMAL */ { BUG_HP, ROBOT_HP },        /* identity — symbolic to fail-fast on tuning.h drift */
    /* HARD   */ { 5, 9 },
};

/* Spawn-interval scaler: result = max(floor, (base * num) >> 3).
 * NORMAL is identity (num=8). */
#define DIFF_SPAWN_NUM_EASY    12u  /* x1.5 — slower */
#define DIFF_SPAWN_NUM_NORMAL   8u  /* x1.0 */
#define DIFF_SPAWN_NUM_HARD     6u  /* x0.75 — faster */
#define DIFF_SPAWN_FLOOR       30u  /* absolute minimum spawn interval (frames) */

/* Starting energy lookup. */
#define DIFF_START_ENERGY_EASY    45u
#define DIFF_START_ENERGY_NORMAL  30u   /* matches START_ENERGY (iter-2) */
#define DIFF_START_ENERGY_HARD    24u

static inline uint8_t difficulty_enemy_hp(uint8_t enemy_type, uint8_t diff) {
    if (diff >= 3) diff = 1;            /* clamp to NORMAL on garbage */
    if (enemy_type >= 2) enemy_type = 0;
    return DIFF_ENEMY_HP[diff][enemy_type];
}

static inline uint8_t difficulty_scale_interval(uint8_t base, uint8_t diff) {
    uint8_t num =
        (diff == 0) ? DIFF_SPAWN_NUM_EASY :
        (diff == 2) ? DIFF_SPAWN_NUM_HARD :
                      DIFF_SPAWN_NUM_NORMAL;
    uint16_t scaled = ((uint16_t)base * num) >> 3;     /* fits in u16 */
    if (scaled < DIFF_SPAWN_FLOOR) scaled = DIFF_SPAWN_FLOOR;
    if (scaled > 255u) scaled = 255u;                  /* spawn_event_t.delay is u8 */
    return (uint8_t)scaled;
}

static inline uint8_t difficulty_starting_energy(uint8_t diff) {
    if (diff == 0) return DIFF_START_ENERGY_EASY;
    if (diff == 2) return DIFF_START_ENERGY_HARD;
    return DIFF_START_ENERGY_NORMAL;
}

static inline uint8_t difficulty_cycle_left(uint8_t diff) {
    return (diff == 0) ? 2u : (uint8_t)(diff - 1u);
}
static inline uint8_t difficulty_cycle_right(uint8_t diff) {
    return (diff >= 2) ? 0u : (uint8_t)(diff + 1u);
}

#endif
```

Why a 2‑D table for HP (instead of a single multiplier per difficulty)?
- The user's target values (BUG: 2 / 3 / 5, ROBOT: 4 / 6 / 9) can't be expressed by any single `(base * num) >> 3` scalar — bug‑HARD ratio is 5/3 = 1.67, robot‑HARD is 9/6 = 1.50. A table delivers exact designer intent for **6 bytes of ROM** with zero arithmetic ambiguity.
- Spawn intervals are scaled by formula because each wave has many distinct delay values (50–90 frames); per‑(wave,event) tables would explode.
- Starting energy is a 3‑entry lookup because there are only 3 numbers.

### Module impact map

| Module | Change |
|---|---|
| `src/game.h` | Add `#include "difficulty_calc.h"` and the `game_difficulty()` / `game_set_difficulty()` prototypes. (Enum lives in `difficulty_calc.h`, not here.) |
| `src/game.c` | Add `static u8 s_difficulty = DIFF_NORMAL;` (file scope; **NOT** reset by `enter_title()` / `enter_playing()`). Add getter/setter. |
| `src/difficulty_calc.h` | New pure header (above). |
| `src/tuning.h` | Unchanged — all difficulty constants live in `difficulty_calc.h` to keep the helper self‑contained and host‑testable in isolation. (`tuning.h` is included by it for `BUG_HP` / `ROBOT_HP` cross‑checks in tests.) |
| `src/enemies.c` | At line 71, change `s_enemies[i].hp = s_enemy_stats[type].hp;` to `s_enemies[i].hp = difficulty_enemy_hp(type, game_difficulty());`. Add `#include "difficulty_calc.h"` and `#include "game.h"`. |
| `src/waves.c` | At line 152, wrap the raw delay read with `difficulty_scale_interval(...)`. Same include additions. |
| `src/economy.c` | At line 11, replace `s_energy = START_ENERGY;` with `s_energy = difficulty_starting_energy(game_difficulty());`. Same includes. |
| `src/title.{h,c}` | Add the difficulty selector (state, input, render — see UI section). New code calls `input_is_pressed`, `game_difficulty()`, `game_set_difficulty()`, and the cycle helpers, so add `#include "input.h"`, `#include "game.h"`, `#include "difficulty_calc.h"` to `title.c`. |
| `src/hud.c` | **Unchanged** — see decision D6 (no HUD indicator). |
| `tools/gen_assets.py` | Add `<` and `>` BG‑font glyphs so `text_row("…<…>…")` renders correctly. |
| `res/assets.{c,h}` | Regenerated by `gen_assets.py`. |
| `tests/test_difficulty.c` | New host test. Wired into `justfile` `test` recipe. |

### Why pause / menu / SRAM are untouched
- Pause modal works identically across difficulties (it doesn't read difficulty).
- Tower stats are unchanged across difficulties (per scope: changing both attack and defense doubles complexity for marginal value).
- SRAM persistence is feature #19 — out of scope. Difficulty resets to `DIFF_NORMAL` at power‑on because `s_difficulty` is a non‑zero‑initialised file‑scope static; SDCC/GBDK‑2020 places it in `.data` and crt0 copies the initial byte from ROM into RAM at boot. (It is **NOT** a `.bss` zero‑init — the value `DIFF_NORMAL = 1` could not survive zero‑init. Acceptance scenario #1 is the regression guard for this.)

### Title‑screen UI (selector)

Layout (20 × 18 BG grid):

```
row  2  |        GBTD        |
row  4  |   TOWER  DEFENSE   |
row  6  | A RUNAWAY AI IS    |
row  7  | SENDING THE BUGS   |
row 10  |     < NORMAL >     |   ← NEW: difficulty selector (10 BG tiles)
row 11  |       EASY         |   ← NEW: helper hint, dim — see design notes
row 13  |    PRESS START     |
row 16  |  (C) 2025 GBTD MVP |
```

Wait — keep it simple, **single‑row** selector:

```
row 10  |     < NORMAL >     |   ← centered, 10 tiles wide
row 13  |    PRESS START     |
```

- Selector width is a fixed 10‑tile window at cols 5..14 to fit the longest label "NORMAL" (6 chars) flanked by `< ` and ` >`.
- Labels are space‑padded to 6 chars so the chevrons stay put: `" EASY "`, `"NORMAL"`, `" HARD "`.
- Rendered as **BG tiles** (no sprites) — title currently calls `gfx_hide_all_sprites()` and uses BG only. This keeps OAM untouched (0 new slots) and matches the existing PRESS‑START blink mechanism.
- Input: D‑pad LEFT / RIGHT, **edge‑only** via `input_is_pressed` (NOT `input_is_repeat`) — see decision D9. The 3‑option cycle is short enough that auto‑repeat would over‑cycle on a held D‑pad; edge detection is the right granularity here, even though the iter‑1 convention permits repeat for menu navigation. Cycles with **wrap** (`difficulty_cycle_left/right`). Re‑renders the selector row only on change (dirty flag, identical pattern to the PRESS‑START blink).
- A and B do **nothing** on the title screen (preserve iter‑2 behaviour). START commits the current difficulty (already stored) and calls `enter_playing()`.

### Glyph audit
BG font (`tools/gen_assets.py` FONT dict) already has every letter we need for `EASY`, `NORMAL`, `HARD`: E, A, S, Y, N, O, R, M, L, H, D — all present. Two glyphs are missing from the BG font and must be added:

| Char | ASCII | Status | Action |
|---|---|---|---|
| `<` | 0x3C | **Missing** in FONT dict | Add 8×8 glyph |
| `>` | 0x3E | Defined as a sprite glyph but **NOT** in FONT dict; `text_row` writes BG tile 62 which is currently blank | Add to FONT dict (mirror existing sprite‑bank chevron) |

Sprite‑bank `SPR_GLYPH_*` set is unchanged — the title selector uses BG, not sprites.

### Resource budget impact

| Resource | Delta | Notes |
|---|---|---|
| ROM (code)   | ≈ +250 B | Title selector logic (≈ 150 B), enemies/waves/economy hooks (≈ 30 B), 6‑byte HP table + 3‑entry energy lookup. |
| ROM (tiles)  | +32 B   | Two new 16‑byte BG glyphs (`<`, `>`). |
| WRAM         | +1 B    | `s_difficulty` static. |
| OAM          | +0      | BG‑only render. |
| BG‑write budget | unchanged | Title‑screen frame loop is not under the gameplay 16‑writes/frame cap; the selector writes ≤10 tiles only on the frame the selection changes. |
| Total ROM growth | ≈ 0.3 KB | Well within the available headroom in the 32 KB cart. |

## Subtasks

1. ✅ **Add chevron BG glyphs** — In `tools/gen_assets.py`:
   - Add a `'<'` 5×7 ASCII‑art glyph to the main `FONT` dict literal (style: same as `'>'`, mirrored horizontally; lives between `';'`/`'='`-equivalent neighbours alphabetically).
   - **Move** the existing late `FONT['>'] = [...]` assignment (around line 641, *after* `font_tiles[]` is built) into the main `FONT` dict literal next to the new `'<'` entry, and **delete** the late assignment. This ensures `font_tiles[60]` (`<`) and `font_tiles[62]` (`>`) are populated at table‑build time, and prevents the BG bank and `SPR_GLYPH_GT` from drifting if the chevron is ever tweaked.
   Done when `just build` regenerates `res/assets.c` containing non‑zero pixel data at BG tile indices 60 and 62, and `SPR_GLYPH_GT` still resolves (sprite‑bank glyph cross‑references the same FONT entry it always did).

2. ✅ **Pure helper + tuning constants** — Create `src/difficulty_calc.h` exactly as specified above. Done when the file compiles standalone with `cc -std=c99 -Itests/stubs -Isrc -Ires -c src/difficulty_calc.h` (header‑only smoke), and `tests/test_difficulty.c` (subtask 7) links it without GBDK stubs.

3. ✅ **Difficulty enum + getter/setter in `game.{h,c}`** — In `game.h`: add `#include "difficulty_calc.h"` (enum comes from there — do **not** redeclare in `game.h`), then declare `u8 game_difficulty(void);` and `void game_set_difficulty(u8 d);`. In `game.c`: add `static u8 s_difficulty = DIFF_NORMAL;` at file scope (NOT inside any function). Implement `u8 game_difficulty(void) { return s_difficulty; }` and `void game_set_difficulty(u8 d) { if (d < DIFF_COUNT) s_difficulty = d; }`. Done when ROM still builds and difficulty defaults to NORMAL on cold boot. Do NOT touch `enter_title()` / `enter_playing()`.

4. ✅ **Title‑screen selector** — Extend `src/title.c`:
   - Add includes: `#include "input.h"`, `#include "game.h"`, `#include "difficulty_calc.h"`.
   - Add `static u8 s_diff_dirty;` flag.
   - Add `#define DIFF_ROW 10`, `#define DIFF_COL 5`, `#define DIFF_W 10`.
   - Add `static const char *const s_diff_labels[3] = { " EASY ", "NORMAL", " HARD " };`.
   - Add `draw_diff_now(void)` that writes 10 BG tiles at row `DIFF_ROW`, cols `DIFF_COL..DIFF_COL+9`: `<`, ` `, label[0..5], ` `, `>`, where `label = s_diff_labels[game_difficulty()]`.
   - In `title_enter()`: after the existing `set_bkg_tiles(...)` full redraw and under the same `DISPLAY_OFF` bracket, call `draw_diff_now()`. Then set `s_diff_dirty = 0;` (mirror the existing `s_dirty = false;` reset for the PRESS‑START blink — same per‑screen‑state hygiene).
   - In `title_update()`, **before** the existing blink counter logic: on `input_is_pressed(J_LEFT)` call `game_set_difficulty(difficulty_cycle_left(game_difficulty()));` and set `s_diff_dirty = 1;`. Same for `J_RIGHT` with `difficulty_cycle_right`. Do NOT consume START — `game.c::game_update` still owns the START → `enter_playing()` transition.
   - In `title_render()`, before the existing PRESS‑START dirty branch: `if (s_diff_dirty) { draw_diff_now(); s_diff_dirty = 0; }`. Two independent dirty flags → two independent paint paths.
   Done when QA can press LEFT/RIGHT on the title and see the label cycle EASY ↔ NORMAL ↔ HARD with wrap, and re‑entering the title from gameover paints the persisted selection (no flicker, no stale value).

5. ✅ **Plumb scaling into gameplay** — Three minimal edits:
   - `src/enemies.c` line 71: `s_enemies[i].hp = difficulty_enemy_hp(type, game_difficulty());`
   - `src/waves.c` line 152: `s_timer = difficulty_scale_interval(s_waves[s_cur].events[s_event_idx].delay, game_difficulty());`
   - `src/economy.c` line 11: `s_energy = difficulty_starting_energy(game_difficulty());`
   Add the `#include "difficulty_calc.h"` and `#include "game.h"` to each. Done when bug HP at spawn matches the table for the active difficulty (verifiable via emulator + memory inspection or a host‑test mock that links the helper).

6. ✅ **Host tests** — `tests/test_difficulty.c`. Tests:
   - `T1` HP table identity for NORMAL: `difficulty_enemy_hp(0, DIFF_NORMAL) == BUG_HP` and `(1, DIFF_NORMAL) == ROBOT_HP` (catches drift between `tuning.h` and the table).
   - `T2` HP scaling values for EASY (2, 4) and HARD (5, 9) — every entry asserted.
   - `T3` HP defends against junk: `difficulty_enemy_hp(99, 99) == DIFF_ENEMY_HP[1][0]` (clamp behaviour).
   - `T4` Spawn interval identity for NORMAL: `difficulty_scale_interval(b, DIFF_NORMAL) == b` for b ∈ {50, 60, 75, 90} (every base used in the iter‑2 wave script). Catches `num != 8` regressions.
   - `T5` Spawn interval EASY > NORMAL > HARD ordering for each base ∈ {50, 60, 75, 90}.
   - `T6` Spawn interval HARD floor: `difficulty_scale_interval(30, DIFF_HARD) == DIFF_SPAWN_FLOOR` (30 * 6 / 8 = 22, clamps to 30).
   - `T7` Starting energy lookup for all 3 modes: 45 / 30 / 24, plus junk‑input clamp to NORMAL.
   - `T8` Cycle math: `difficulty_cycle_left(DIFF_EASY) == DIFF_HARD`, `difficulty_cycle_right(DIFF_HARD) == DIFF_EASY`, plus the four non‑wrap transitions.
   - `T9` Sanity: every entry in `DIFF_ENEMY_HP` is ≥ 1 (no zero‑HP enemy can ever be spawned).
   Wire into `justfile` `test` recipe (mirror `test_anim` / `test_pause` invocation). Done when `just test` runs all tests green.

7. ✅ **Manual scenarios** — Append to `tests/manual.md` the scenarios in §Acceptance Scenarios. Done when QA can execute them from the doc without asking questions.

8. ✅ **Memory updates** — Append the two new entries (one decision, one convention) to `memory/decisions.md` and `memory/conventions.md` per §Memory Updates below. Done in the same coder pass.

(No designer subtask — selector visual is fully specified above. Two new BG chevron glyphs follow the existing 5‑col / 7‑row punctuation style; designer involvement deferred unless QA flags a legibility issue.)

## Acceptance Scenarios

### Setup
- `just build && just run` from a clean working tree.
- mGBA controls: arrow keys = D‑pad, `Z` = A, `X` = B, `Enter` = START.
- For T2/T6 verification, mGBA's memory viewer is needed (or use the `tests/test_difficulty.c` proof for the table values).

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Default on cold boot | Power on → title screen | Selector shows `< NORMAL >` centered on row 10. PRESS START blinks below. |
| 2 | Cycle right with wrap | RIGHT, RIGHT, RIGHT | After 1st: `< HARD >` (with leading/trailing space). After 2nd: wraps to `< EASY >`. After 3rd: `< NORMAL >`. |
| 3 | Cycle left with wrap | LEFT (from default NORMAL), then LEFT, then LEFT | After 1st: `< EASY >`. After 2nd: wraps to `< HARD >`. After 3rd: `< NORMAL >`. |
| 4 | A / B inert on title | Press A; press B | No visual change; selector stays put; no state transition. |
| 5 | START commits — starting energy | Set EASY → press START | Game enters PLAYING. HUD shows `E:045`. (HP verification via host test T2 — see §Tests; QA does not need to count hits in‑emulator.) |
| 6 | HARD spawn faster + lower wallet | Title → HARD → START → wait through wave 1 grace, then place 1 antivirus on the southernmost path‑adjacent tile (cost 10, leaves `E:014`). Time the gap between the 1st and 2nd bug spawn against an mGBA reference NORMAL run timed the same way (or use mGBA's frame counter). | HUD shows `E:024` immediately after START. Inter‑spawn gap on HARD is ~38 frames vs ~50 on NORMAL (24% shorter — visible to the naked eye over 5 bugs). HP verification deferred to host tests T2/T6. |
| 7 | EASY HUD energy | Title → EASY → START | HUD shows `E:045` immediately after START. (Robot HP verification deferred to host test T2.) |
| 8 | Quit‑to‑title preserves selection | Title → HARD → START → pause → QUIT → back at title | Selector still shows `< HARD >` (not reset to NORMAL). |
| 9 | Game‑over preserves selection | Title → EASY → START → lose deliberately → press START on lose screen → back at title | Selector still shows `< EASY >`. |
| 10 | Win‑over preserves selection | Title → NORMAL → win all 10 waves → return to title | Selector still shows `< NORMAL >`. |
| 11 | Power‑off resets | Power off mGBA, power on | Selector defaults to `< NORMAL >` (no SRAM). |
| 12 | Pause works on every difficulty | Repeat scenarios 5/6/7 with a pause+resume during gameplay | Pause modal opens/closes identically. No SFX or visual difference. |
| 13 | Visual / audio parity | Compare wave 1 on EASY vs HARD | Same bug sprites, same audio cues, same HUD layout. Only HP, spawn timing, and starting `E:` differ. |
| 14 | EASY beatable, HARD beatable | Play through all 10 waves on EASY (casual) and HARD (focused) | Both runs complete with correct WIN screen. |

Visual states to check on title:
- Selector row legible at DMG contrast (BG tile shade 3 on shade 0, same as existing FONT).
- Chevron `<` and `>` align vertically with the label letters (5×7 glyphs in 8×8 cells).

## Constraints
- Respect every prior decision: BG‑tile towers, modal precedence (`game_modal.h` latch), `audio_reset()` placement (QUIT path only), frame‑loop split, OAM allocation (`0=cursor / 1..16=menu‑or‑pause / 17..30=enemies / 31..38=projectiles / 39=reserved`), pause is a flag inside PLAYING.
- ROM ≤ 32 KB (no MBC). Current build is well under; this feature adds ≈ 0.3 KB.
- WRAM growth ≤ 2 B (actual: +1 B).
- BG‑write budget on the gameplay screen is unchanged (≤16 writes/frame). The title selector lives on the title screen, which has no per‑frame budget cap.
- `difficulty_calc.h` MUST use `<stdint.h>` only (no `gtypes.h`, no GBDK header) — same rule as `game_modal.h` / `*_anim.h`.
- Tower stats unchanged across difficulties.
- Wave count and composition unchanged across difficulties (only spawn `delay` is scaled).
- Spawn‑interval floor of 30 frames is global (applies before the iter‑2 D12 50‑frame floor, since on HARD the scaling can drop below 50; the 50‑frame floor in D12 was a NORMAL‑balance number, not an engine invariant).
- **HARD wave 10 is enemy‑pool‑bounded by design.** With HARD scaling, W10's lowest base delay (50 → 37 frames) means events arrive faster than the path traversal time can drain `MAX_ENEMIES = 14`. When the pool fills, `enemies_spawn` returns false and `waves.c` retries every 8 frames (`s_timer = 8`, line 154 — the existing iter‑2 back‑pressure mechanism, untouched). Net effect: HARD wave 10 plateaus at "pool‑saturated" rather than scaling proportionally past wave 7‑ish. This is acceptable — the design intent (more enemies on screen at once, less recovery time between kills) is preserved, and no engine invariant breaks. QA scenario #14 ("HARD beatable") and the explicit "enemy pool may saturate on HARD late waves; retry stalls are expected, not a regression" note in `tests/manual.md` cover this.
- Inter‑wave delay (`s_waves[*].inter_delay`, 180 frames) is **not** scaled by difficulty (see D15) — uniform breathing room across all modes.
- `s_difficulty` MUST NOT be reset in `enter_title()` or `enter_playing()` — it persists for the lifetime of the program.

## Decisions

| # | Decision | Options Considered | Choice | Rationale |
|---|---|---|---|---|
| D1 | HP scaling shape | (a) Single `(base*num)>>3` per‑difficulty multiplier; (b) per‑(diff, type) 2‑D table | **(b)** 2‑D table (6 B) | User's HARD targets (bug=5, robot=9) imply non‑uniform ratios (1.67 vs 1.50) — no single multiplier hits both. Table gives exact designer control for negligible ROM. |
| D2 | Spawn‑interval scaling shape | (a) Per‑(diff, base‑value) lookup; (b) `(base*num)>>3` formula with floor | **(b)** formula | Wave script uses many distinct base delays; a table would explode. Formula is exact for clean multipliers (1.5, 0.75) and cheap. |
| D3 | EASY HP multiplier value | num=5 (×0.625), num=6 (×0.75), table | **table {2, 4}** (≈ ×0.67) | Hits the requested EASY targets cleanly while keeping bug HP ≥ 2 (1 would feel trivial). |
| D4 | HARD HP multiplier value | num=12 (×1.5), num=14 (×1.75), table | **table {5, 9}** | Matches user's HARD targets exactly. |
| D5 | Spawn floor | 50 (iter‑2 D12), 30, none | **30 frames global** | iter‑2's 50‑frame floor was a NORMAL‑balance choice; HARD must go below it to be meaningfully harder. 30 frames (~0.5 s) still leaves projectiles/towers a fighting chance. Documented in constraints. |
| D6 | HUD difficulty indicator | (a) Add E/N/H letter to HUD row; (b) skip | **(b) skip** | HUD row is fully occupied (cols 0..19, last col is the tower‑select indicator per iter‑2 convention). Reflowing the HUD is disproportionate to the value — title shows the choice clearly, and the difference is tactile within seconds of play. |
| D7 | Cycle wrap vs clamp | wrap, clamp | **wrap** | Matches "press LEFT/RIGHT to cycle" mental model; symmetric. Costs nothing (4‑line helper). |
| D8 | A/B button on title | A commits, B does nothing, both inert | **both inert** | Preserves the iter‑2 "START is the only commit" flow; avoids a second commit path that QA / muscle memory might surprise. |
| D9 | LEFT/RIGHT auto‑repeat on title | edge‑only, repeat (12+6 frames) | **edge‑only** | 3‑option selector doesn't need repeat; eliminates accidental over‑cycling on a held D‑pad. |
| D10 | Difficulty storage location | `title.c` (UI owns it), `game.c` (state owns it), new `difficulty.c` | **`game.c`** | It's cross‑module shared state; `game.c` already owns analogous statics (`s_selected_type`, `s_anim_frame`). A whole new module for one byte is over‑engineering. |
| D11 | Render selector via BG or sprites | BG tiles, sprite glyphs | **BG tiles** | Title is BG‑only today (`gfx_hide_all_sprites()` on enter); BG is cheaper (no OAM use, no per‑frame painting), and the existing PRESS‑START blink already proves the dirty‑flag BG pattern works. |
| D12 | Where to read difficulty | (a) cache into local statics on `enter_playing()`; (b) call `game_difficulty()` at every read site | **(b)** | Three reads per session (one per call site, not per frame): enemies on spawn, waves on event arming, economy on init. Caching adds bookkeeping for no win. |
| D13 | Scaling of tower stats | scale, leave fixed | **leave fixed** | Per scope ("changing both attack and defense doubles complexity for marginal value"). Difficulty curve comes entirely from enemy HP + spawn pressure + starting wallet. |
| D14 | Where the constants live | `tuning.h`, `difficulty_calc.h`, both | **`difficulty_calc.h` only** | Keeps the helper self‑contained and host‑testable in isolation. `tuning.h` only defines per‑entity base stats; difficulty‑specific numbers don't belong there. |
| D15 | Scale inter‑wave delay too? | scale, leave fixed | **leave fixed at 180 frames** | Inter‑wave delay is recovery / placement time between waves; making it shorter on HARD compounds the spawn‑rate scaling and risks an unwinnable HARD without commensurate economy buffs. Single‑axis scaling per wave is enough; uniform breather across difficulties keeps the user‑facing definition of difficulty narrow ("more / tougher enemies, less starting cash"). |

## Memory Updates

Append to `memory/decisions.md`:
```
### Iter-3 #20: Difficulty modes — 2-D HP table, formula spawn scaler, BG-only title selector
- **Date**: 2026-05-15
- **Context**: Roadmap #20 — Easy/Normal/Hard scaling HP + spawn rate.
- **Decision**: Difficulty is a `u8 s_difficulty` static at file scope in
  `src/game.c`, initialised to `DIFF_NORMAL`, never reset by `enter_title()` or
  `enter_playing()` (so quit-to-title preserves selection within a power-on
  session; SRAM is feature #19). Enum `{DIFF_EASY=0, DIFF_NORMAL=1, DIFF_HARD=2}`
  in `game.h`; getter `game_difficulty()` / setter `game_set_difficulty()`.
  Pure header `src/difficulty_calc.h` (joins `tuning.h`, `game_modal.h`,
  `*_anim.h` as `<stdint.h>`-only host-testable helpers) holds: a 2-D 6-byte
  `DIFF_ENEMY_HP[3][2]` table (EASY {2,4} / NORMAL {3,6} / HARD {5,9}) — chosen
  over a single multiplier because user's HARD targets (bug=5, robot=9) imply
  non-uniform ratios; a `(base*num)>>3` spawn-interval scaler with global 30-
  frame floor (HARD num=6 ×0.75, EASY num=12 ×1.5); and a 3-entry starting-
  energy lookup (EASY=45, NORMAL=30, HARD=24). Title screen renders selector
  `< LABEL >` at row 10 cols 5..14 in BG tiles (no OAM); LEFT/RIGHT edge-only
  cycle with wrap; A and B inert; START commits and calls `enter_playing()`
  unchanged. Tower stats and wave composition are difficulty-invariant; the
  iter-2 D12 50-frame spawn floor is now a NORMAL-balance number, superseded
  by the engine-wide 30-frame floor.
- **Rationale**: 2-D HP table = exact designer intent for 6 B; formula spawn
  scaler = O(1) over many base delays; BG selector = zero OAM/render cost
  reusing the existing dirty-flag pattern from PRESS-START blink. File-scope
  static persistence avoids touching `enter_*` and is the simplest possible
  "remember last selection within session".
- **Alternatives**: Per-difficulty single HP multiplier (rejected — can't
  match user's targets); per-(diff, base) spawn lookup table (rejected —
  combinatorial); HUD letter indicator (rejected — HUD row 0 fully occupied
  by iter-2 convention; reflow disproportionate); difficulty stored in
  `title.c` (rejected — it's cross-module state, `game.c` is the right
  owner); resetting `s_difficulty` in `enter_title` (rejected — would lose
  the selection on quit-to-title).
```

(Note: an earlier draft of this snippet placed the `DIFF_*` enum in
`game.h`. Authoritative location is `src/difficulty_calc.h`; `game.h` only
re‑exports it via `#include`. See conventions update below.)

Append to `memory/conventions.md`:
```
### Iter-3 #20 conventions (2026-05-15)
- **Difficulty is global, persistent within a power-on session**:
  `static u8 s_difficulty = DIFF_NORMAL;` at file scope in `src/game.c`. Read
  via `game_difficulty()`. Do NOT reset it in `enter_title()` or
  `enter_playing()`. SRAM persistence is feature #19 (separate spec).
- **Difficulty enum lives in `src/difficulty_calc.h`** (NOT `game.h`):
  `enum { DIFF_EASY=0, DIFF_NORMAL=1, DIFF_HARD=2, DIFF_COUNT=3 };`. `game.h`
  `#include`s `difficulty_calc.h` so on‑device callers see the enum
  transitively, and host tests can include `difficulty_calc.h` directly
  without pulling `gtypes.h` / `<gb/gb.h>`. Use the symbolic names; the
  integer values are also the indices into `DIFF_ENEMY_HP[diff][type]` —
  do not reorder.
- **Three difficulty read sites only**: enemies.c (HP at spawn), waves.c
  (delay at event arming), economy.c (starting energy at init). Adding a
  new read site requires updating the test coverage in
  `tests/test_difficulty.c`.
- **Pure-helper headers (extended)**: `src/difficulty_calc.h` joins the
  `<stdint.h>`-only family (`tuning.h`, `game_modal.h`, `*_anim.h`). It
  includes `tuning.h` only — never a GBDK header.
- **Spawn-interval engine floor is 30 frames** (was 50 under iter-2 D12,
  now superseded — D12's 50-frame floor was a NORMAL balance number, not
  an engine invariant). Enforced inside `difficulty_scale_interval`.
- **Title screen owns no game state**: the difficulty selector renders
  `game_difficulty()` directly and writes back via `game_set_difficulty()`.
  Title carries no shadow copy.
```

## Review

### Round 1 — adversarial review (`reviewer` agent, claude-sonnet-4.6)

| # | Severity | Finding | Resolution |
|---|---|---|---|
| R1 | HIGH | Subtask 4 didn't list the `#include`s `title.c` needs (`input.h`, `game.h`, `difficulty_calc.h`) — coder would hit unresolved symbols. | Subtask 4 now lists the three includes explicitly; module impact map row updated. |
| R2 | MED | `s_diff_dirty` not reset in `title_enter()` — breaks the existing per‑screen‑state hygiene pattern used by `s_dirty`. | Subtask 4 now spells out `s_diff_dirty = 0;` reset inside `title_enter()` after `draw_diff_now()`. |
| R3 | MED | `gen_assets.py` already has a late `FONT['>']` assignment after `font_tiles[]` is built; adding `'>'` to the dict literal would duplicate and risk silent divergence with `SPR_GLYPH_GT`. | Subtask 1 now instructs the coder to *move* the late `FONT['>']` into the main dict literal (and delete the late assignment), consolidating into one source of truth. |
| R4 | MED | "Zeroed BSS" wording was internally contradictory — `static u8 x = DIFF_NORMAL;` lives in `.data`, not `.bss`. Risk of masking a real boot‑time hazard if a future linker config zero‑inits the static. | Architecture note rewritten to "non‑zero‑initialised file‑scope static; relies on crt0's `.data` copy from ROM." Acceptance scenario #1 explicitly nominated as the regression guard. |
| R5 | MED | NORMAL row of `DIFF_ENEMY_HP` embedded literals `{3, 6}`, decoupled from `tuning.h` `BUG_HP` / `ROBOT_HP`. | Changed to `{ BUG_HP, ROBOT_HP }` symbolic — compile‑time identity. T1 retained as defense in depth. |
| R6 | MED | HARD wave‑10 enemy‑pool exhaustion not addressed — 37‑frame spawn vs `MAX_ENEMIES=14` will saturate; QA might mistake the back‑pressure stall for a regression. | Added explicit constraint documenting the pool‑saturation behaviour as intentional (uses existing iter‑2 retry mechanism); QA scenario / manual note required. |
| R7 | NOTE | Inter‑wave delay (180 frames) not scaled — defensible but not documented. | Added decision **D15** (don't scale) with rationale; added constraint line. |

### Round 2 — cross‑model validation (`reviewer` agent, gpt‑5.4)

| # | Severity | Finding | Resolution |
|---|---|---|---|
| V1 | HIGH | Acceptance scenarios 5/6/7 asked QA to verify enemy HP by counting tower hits, but never instructed them to place a tower or establish a controlled setup — the scenarios were not executable as written. | Scenarios 5 and 7 reduced to HUD energy check only (HP verification deferred to host tests T2/T6, which already cover every (mode × type) combination exhaustively). Scenario 6 expanded with explicit tower placement step + concrete frame‑gap target (~38f HARD vs ~50f NORMAL) and an mGBA frame‑counter / reference‑run methodology QA can actually execute. |
| V2 | MED | Cycle‑left wrap scenario (#3) had a math error: `NORMAL → LEFT → HARD → LEFT → NORMAL` skipped EASY. | Corrected to the canonical wrap order `NORMAL → LEFT → EASY → LEFT → HARD → LEFT → NORMAL`. |
| V3 | HIGH | Architecture line 134 said LEFT/RIGHT uses `input_is_repeat` with auto‑repeat, contradicting Subtask 4, decision D9, and the memory‑update snippet (all edge‑only via `input_is_pressed`). | Architecture line rewritten to explicitly say edge‑only via `input_is_pressed` and to reference D9 for rationale. All four locations now agree. |
| V4 | MED | Memory‑update snippets stated the `DIFF_*` enum lives in `game.h`, contradicting the architecture (which places it in `src/difficulty_calc.h` for host‑test isolation). Would have appended incorrect project memory. | Conventions snippet updated to name `src/difficulty_calc.h` as the canonical location with `game.h` re‑exporting via `#include`. A note also added under the decisions snippet flagging the earlier draft's mistake to prevent the original wording from leaking back. |

No further findings. Spec cleared for coder hand‑off.
