# Iter-3 #17 — Three Maps + Map Selector

> Roadmap: `specs/roadmap.md` entry #17 — "New tilemaps with different path layouts; map-select screen."
> Map layouts (path topology + waypoint lists): `specs/iter3-17-maps-design.md`.
> Prior context: `specs/iter3-20-difficulty.md` (selector pattern), `specs/iter3-21-animations.md`
> + `specs/iter3-22-pause.md` (modal/render conventions), `memory/decisions.md`,
> `memory/conventions.md` (Iter-3 #20 — Title VBlank: selector-first, blink-deferred).

---

## 1. Problem

Iteration 3 ships the second and third maps so the game is no longer a single-layout
experience. The player must be able to:

- pick one of three maps from the title screen, alongside the existing difficulty
  selector;
- play the chosen map with the same wave script and difficulty constants as the
  others (only path geometry differs);
- have the chosen map persist across game-over → title → new game inside one
  power-on session, like difficulty does;

…while staying inside the existing budgets: 32 KB no-MBC ROM, ≤ 16 BG writes per
frame, modal precedence, three-read-site rule for difficulty, etc.

---

## 2. Architecture

### 2.1 Map data is now an array of `map_def_t`

`src/map.c` currently hardcodes (a) a single `s_waypoints[8]` array, (b) the
computer position implicitly inside `map_render()` (`set_bkg_tile_xy(18, sr, …)`),
and (c) two single-instance asset arrays `gameplay_tilemap[20*17]` /
`gameplay_classmap[20*17]` emitted by `tools/gen_assets.py`.

Refactor to a registry:

```c
/* src/map.h */
#define MAP_COUNT      3
#define MAX_WAYPOINTS  10        /* designer cap (Map 2 = 10, others ≤ 10) */

typedef struct {
    const u8         *tilemap;         /* PF_COLS*PF_ROWS = 340 bytes */
    const u8         *classmap;        /* PF_COLS*PF_ROWS = 340 bytes */
    const waypoint_t *waypoints;
    u8                waypoint_count;
    u8                computer_tx;     /* play-field-local TL of 2×2 cluster */
    u8                computer_ty;
} map_def_t;
```

The asset generator emits three pairs of tilemap/classmap arrays
(`gameplay1_tilemap`, `gameplay1_classmap`, `gameplay2_*`, `gameplay3_*`) plus
their waypoint literals. `map.c` owns one `static const map_def_t s_maps[MAP_COUNT]`
table that points at them, and a `static u8 s_active_map_id` updated by
`map_load(u8)`.

Public API (replaces today's API):

```c
void              map_load(u8 map_id);  /* DISPLAY_OFF bracketed by caller */
void              map_render(void);     /* unchanged */
u8                map_class_at(u8 tx, u8 ty);
const waypoint_t *map_waypoints(void);
u8                map_waypoint_count(void);
u8                map_active(void);
void              map_set_computer_state(u8 hp);
```

`map_class_at` indexes `s_maps[s_active_map_id].classmap`; `map_waypoints` returns
`s_maps[s_active_map_id].waypoints`; `map_render` reads `computer_tx/ty` from the
active def instead of the hardcoded `(18, sr)` literals.

### 2.2 Active map persists at file-scope in `game.c`

Mirrors the iter-3 #20 difficulty pattern exactly:

```c
/* src/game.c */
static u8 s_active_map = 0;          /* MAP_1 */

u8   game_active_map(void);
void game_set_active_map(u8 m);      /* clamps to < MAP_COUNT */
```

`enter_playing()` calls `map_load(game_active_map())` (was `map_load()`).
Neither `enter_title()` nor `enter_playing()` resets `s_active_map` —
power-on default comes from the `.data` initializer copied by crt0, identical
to the difficulty pattern.

### 2.3 Title-screen selector becomes two stacked rows with a focus chevron

```
row 10:  > < NORMAL >       (difficulty selector — existing)
row 12:  > < MAP 1 >        (map selector — new)
row 13:  PRESS START         (existing blink, unchanged)
```

The `>` chevron at column 3 of the **focused** row tracks `static u8
s_title_focus` (0 = difficulty, 1 = map). D-pad UP/DOWN toggles focus
(edge-only `input_is_pressed`, matching iter-3 #20 D9). LEFT/RIGHT cycles the
**focused** selector via the appropriate `*_cycle_left/right` helper. A new
pure header `src/map_select.h` exposes `map_cycle_left/right(u8) -> u8` (3-state
wrap, identical shape to `difficulty_cycle_*`).

A third dirty flag `s_map_dirty` joins `s_diff_dirty` and `s_dirty` (prompt
blink). Service order in `title_render`, **one per frame, return after each**:

1. `s_diff_dirty` (10 writes — selector that just took LEFT/RIGHT input)
2. `s_map_dirty` (10 writes)
3. `s_focus_dirty` (2 writes — clear old chevron + paint new one)
4. `s_dirty` (12 writes — prompt blink)

Worst case is 12 writes per frame, well under the 16 cap. Multi-frame slip
on the blink remains invisible (30-frame period). When focus changes via
UP/DOWN, the chevron move costs at most 2 writes; pairing it on the SAME
frame as a same-frame selector cycle is impossible because UP/DOWN and
LEFT/RIGHT use distinct branches.

Layout values (chosen to fit the existing 10-tile selector widget and stay
inside the 20×18 title tilemap without overlapping the GBTD logo at rows 3–7
or the prompt at row 13):

| Element       | Row | Cols           | Tile count |
| ------------- | --- | -------------- | ---------- |
| chevron       | 10 / 12 | 3 (single tile)| 1          |
| difficulty    | 10  | 5..14          | 10         |
| map           | 12  | 5..14          | 10         |
| prompt        | 13  | 4..15          | 12         |

The chevron uses `'>'` (ASCII 0x3E, font-tile already present); when
unfocused, that cell renders as `' '` (space). Two writes per focus toggle:
clear old row, paint new row.

### 2.4 Waves / cursor / towers are map-agnostic; `enemies.c` needs ONE change

`cursor.c`, `towers.c`, `waves.c`, `pause.c`, `hud.c` already address the map
exclusively through `map_class_at`, `map_waypoints`, `map_set_computer_state`,
plus per-frame entity coordinates. They need **no changes**.

**`enemies.c` is the exception** (review-finding F1): `step_enemy` decides
"reached the computer → damage + despawn" by comparing the next waypoint
index against the **compile-time macro** `WAYPOINT_COUNT` (=8). With Map 2's
10 waypoints this would despawn enemies at waypoint index 7 (mid-path on
Map 2) instead of the cluster cell. Subtask 3 replaces `WAYPOINT_COUNT` in
`enemies.c` with a call to `map_waypoint_count()` (cached into a local at
the top of `step_enemy` so the inner loop pays one read, not one per
waypoint). The macro `WAYPOINT_COUNT` is **deleted from `src/map.h`** —
external code that needs the count uses `map_waypoint_count()`.

Wave compositions/timings, tower stats, enemy stats, and difficulty
constants apply uniformly across all maps — the only thing that differs is
path geometry.

`map.c::map_render` is the **only** consumer of `computer_tx/ty` — it now
reads from `s_maps[s_active_map_id]` instead of literals.

### 2.5 ROM-budget impact (estimated)

| Item                                     | Bytes |
| ---------------------------------------- | ----- |
| 2 new tilemaps (340 each)                | 680   |
| 2 new classmaps (340 each)               | 680   |
| 2 new waypoint lists (10 + 8) × 2 bytes  | 36    |
| `s_maps[3]` table (≈12 bytes/entry × 3)  | 36    |
| `map_select.h` (header-only)             | 0     |
| `title.c` extra glue (focus, 2nd row)    | ~150  |
| `game.c` getter/setter                   | ~30   |
| `tests/test_maps.c` (host only — 0 ROM)  | 0     |
| **Total**                                | **≈ 1.6 KB** |

Current ROM ≈ 22 KB after #18; projected ≈ 23.6 KB. Well below the 32 KB cap;
no MBC1.

### 2.6 BG-write budget on `map_load`

Today's `map_load` already writes ~340 BG tiles inside the
`DISPLAY_OFF`/`DISPLAY_ON` bracket maintained by `enter_playing` (verified in
`src/game.c` lines 65–67). Switching maps changes **what** is loaded but not
the call site or the bracket — no F1-class budget regression. The new
`map_load(u8)` keeps the same "caller MUST bracket" contract documented in
the header.

---

## 3. Decisions

| # | Decision | Options | Choice | Rationale |
|---|----------|---------|--------|-----------|
| D1 | Selector layout | (a) one combined widget with up/down to switch axis (b) two stacked widgets with focus chevron | **(b)** two rows + chevron | Each axis stays a self-contained `<` LEFT `>` cycle; mirrors iter-3 #20's mental model; UP/DOWN as focus-switch is a familiar D-pad menu idiom. (a) would require new render code and a more complex dirty-flag dance. |
| D2 | Focus indicator | (a) invisible (both rows always cycle on LEFT/RIGHT — confusing) (b) chevron `>` at col 3 of focused row | **(b)** chevron | Single-glyph cue, 2 writes per toggle, 0 new tile graphics. Reuses font tile `>` already in VRAM (also used inside the `< … >` widget). |
| D3 | Map-select labels | (a) "MAP 1/2/3" (b) thematic ("ALPHA/BETA/GAMMA", "MAZE/SPRINT/...") | **(a)** `MAP 1` / `MAP 2` / `MAP 3` | Designer-chosen. Numeric is unambiguous, uniform width, doesn't lie about difficulty ordering (Map 3 "Sprint" is harder than Map 2 "Maze" under the same wave script). Thematic intent stays inside the design doc. |
| D4 | Class storage | (a) precomputed full byte per cell (340 B × 3 = 1020 B) (b) precomputed 1-bit "buildable" bitmap (45 B × 3) (c) derive from tilemap at runtime | **(a)** full byte per cell, 3 separate classmaps | Already the existing format. Keeps `map_class_at` an O(1) array index — same code path. The +680 bytes vs (b) are easily inside the 32 KB cap. (b) would force a bigger refactor (tile-class today returns one of 3 values, not 1 bit). |
| D5 | Waves vary per map? | (a) per-map wave script (b) shared script | **(b)** shared, unchanged | Roadmap says "different path layouts"; same wave script keeps complexity low and lets path topology be the lone difficulty variable across maps. Shared wave script also avoids ROM growth in `waves.c`. |
| D6 | Map persistence | (a) reset to MAP_1 on `enter_title` (b) persist across enter_title/enter_playing within a power-on session | **(b)** persist | Identical to iter-3 #20's difficulty rule. SRAM persistence is the separate feature #19. Power-on default is `s_active_map = 0` via `.data` init. |
| D7 | Cycle wrap math | inline ad-hoc OR pure helper header | pure-helper header `src/map_select.h` | Consistency with `difficulty_calc.h`, `tower_select.h`, `*_anim.h`. `<stdint.h>`-only; host-testable in `tests/test_maps.c`. |
| D8 | `map_load` API change | (a) `map_load(u8 id)` (b) `map_load(void)` reading `game_active_map()` internally | **(a)** explicit param | Avoids `map.c` depending on `game.h`; matches existing convention that `map.c` is engine-side and stateless wrt game-flow. `game.c::enter_playing` is the one caller; it passes `game_active_map()`. |
| D9 | UP/DOWN input | (a) `input_is_pressed` (edge-only) (b) `input_is_repeat` (auto-repeat) | **(a)** edge-only | 2-state focus toggle; auto-repeat would oscillate on a held D-pad. Same reasoning as iter-3 #20 D9 for LEFT/RIGHT. |
| D10 | Computer position per map | (a) all three at (18, 4) (b) per-map | **(a)** all three at (18, 4) | Designer chose this so `s_comp_tiles[][]` corruption progression and pause-overlay anchor remain valid across all maps with zero changes. The `computer_tx/ty` fields exist in `map_def_t` so future maps CAN move it; `map_render` reads from the def, removing the hardcoded literal as a side benefit. |
| D11 | Path-validity testing | (a) trust designer (b) host test that walks each segment | **(b)** host test `tests/test_maps.c` | Compile-time data, runtime cost zero, catches a typo'd waypoint that would otherwise ship a broken map. Wires into `just test`. |
| D12 | Title focus state init on entry | (a) preserve across `enter_title` invocations (b) reset to focus=0 (difficulty) on each entry | **(b)** reset on entry | Local UI state; no reason to remember it. `title_enter` clears `s_title_focus = 0`, both dirty flags, and re-paints the chevron at row 10. |

---

## 4. Subtasks

Sequential, one coder.

1. ✅ **Designer artifact** — `specs/iter3-17-maps-design.md` with Map 2 + Map 3 ASCII grids, waypoint lists, computer position. **Done when** the file exists, lists all three maps with valid waypoint sequences (orthogonal segments, ≤10 waypoints, terminal waypoint on a TC_COMPUTER cell). *(✅ already produced.)*

2. ✅ **`tools/gen_assets.py` — emit three map asset bundles** — generalise the current `gameplay_map()` into a parametrized helper, then call it three times. Emit `gameplay1_tilemap`, `gameplay1_classmap`, `gameplay2_*`, `gameplay3_*` plus three waypoint arrays as `gameplay1_waypoints[]`, `gameplay2_waypoints[]`, `gameplay3_waypoints[]`. **Linkage**: emit them as plain `const` (NOT `static const`) so `src/map.c` can reference them via `extern` decls in `res/assets.h`. Tile/class arrays similarly `const`. Use a new emitter helper `emit_waypoints(name, list)` that produces e.g. `const waypoint_t gameplay2_waypoints[10] = { {-1,2}, {0,2}, … };`. Update `res/assets.h`: (a) `#include "map.h"` (for the `waypoint_t` typedef) or forward-declare it; (b) declare `extern const u8 gameplayN_tilemap[20*17];` and `extern const u8 gameplayN_classmap[20*17];` for N∈{1,2,3}; (c) declare `extern const waypoint_t gameplayN_waypoints[];` for N∈{1,2,3}. **Map 1 = canonical replacement** for the current `gameplay_tilemap`/`gameplay_classmap`/`s_waypoints` — the legacy `gameplay_tilemap` / `gameplay_classmap` symbols are **deleted** (no duplicate copies of Map 1 data; if any host test still referenced them, update those references to `gameplay1_*`). Subtask 2 must produce a Map 1 tilemap+classmap+waypoint sequence that is **byte-identical** to the pre-refactor `gameplay_tilemap` / `gameplay_classmap` / `s_waypoints`, so behaviour-on-Map-1 is unchanged. **Done when** `just gen` rebuilds `res/assets.{c,h}` deterministically, `git diff res/assets.c` for Map 1 byte ranges shows zero changes, and `just build` produces a valid ROM (size measured for the budget table).

3. ✅ **Refactor `src/map.{h,c}` to `map_def_t` registry + audit `WAYPOINT_COUNT`** — introduce `map_def_t`, `MAP_COUNT`, `MAX_WAYPOINTS`; build `static const map_def_t s_maps[MAP_COUNT]`; add `static u8 s_active_map_id`; rewrite `map_load(u8)`, `map_class_at`, `map_waypoints`, add `map_waypoint_count()` and `map_active()`. Update `map_render` to read `computer_tx/ty` from `s_maps[s_active_map_id]` (replace the `18`/`19`/`HUD_ROWS+4` literals). Keep the `s_state = 0; s_state_dirty = 0;` reset inside `map_load` (preserves the iter-3 #21 corruption-reset invariant). **Delete `#define WAYPOINT_COUNT 8`** from `src/map.h`. **Update `src/enemies.c::step_enemy`** to replace `WAYPOINT_COUNT` with `map_waypoint_count()` (cache as `const u8 wp_n = map_waypoint_count();` at the top of `step_enemy`). **Update `tests/test_enemies.c`** to drop any `WAYPOINT_COUNT` references and add a `u8 map_waypoint_count(void) { return 8; }` stub alongside the existing `map_waypoints` stub. Grep the rest of the source for any `WAYPOINT_COUNT` reference and either delete or refactor (audit: `src/`, `tests/`, comments). **Done when** the project compiles with the refactor, `just test` passes (test_enemies still green), and Map 1 still plays identically (visual regression test: title → start → spawn → enemy walks correct path → enemies arrive at computer and damage HP).

4. ✅ **`src/map_select.h` pure helper** — `<stdint.h>`-only header with `static inline uint8_t map_cycle_left(uint8_t)` and `…_right(uint8_t)`, both with `MAP_COUNT` wrap. Joins the host-testable header family. **Done when** the header exists, `tests/test_maps.c` exercises both wrap directions, and `just test` passes.

5. ✅ **`s_active_map` + getter/setter in `src/game.{h,c}`** — file-scope `static u8 s_active_map = 0;` after the existing `s_difficulty` line; `u8 game_active_map(void)` and `void game_set_active_map(u8 m) { if (m < MAP_COUNT) s_active_map = m; }`. Update `enter_playing()` to `map_load(game_active_map());`. **Done when** the symbol resolves, ROM builds, and Map 1 still loads at start.

6. ✅ **Title-screen second selector + focus chevron** — in `src/title.c`:
   - Add `MAP_ROW=12`, `MAP_COL=5`, `MAP_W=10`, `FOCUS_COL=3`.
   - Add `static u8 s_title_focus`, `static u8 s_map_dirty`, `static u8 s_focus_dirty`.
   - Add `s_map_labels[3] = { "MAP 1 ", "MAP 2 ", "MAP 3 " }` (6-char fixed-width).
   - `draw_map_now()`, `draw_focus_now()` (paints `>` at `(FOCUS_COL, focused_row)` and `' '` at the other row).
   - `title_enter()` resets `s_title_focus = 0`, both new dirties; paints chevron + map selector once during the existing DISPLAY_OFF bracket.
   - `title_update()`: edge-only handlers for UP/DOWN (toggle focus, set `s_focus_dirty`) and route LEFT/RIGHT through `s_title_focus` (focus 0 → `game_set_difficulty(difficulty_cycle_*)`, focus 1 → `game_set_active_map(map_cycle_*)`); keep the existing blink advance. UP/DOWN must not also fire on LEFT/RIGHT (separate `else if`).
   - `title_render()` priority: `s_diff_dirty` → `s_map_dirty` → `s_focus_dirty` → `s_dirty`. Each branch ends with `return;` after clearing its flag (selector-first, blink-deferred convention extended to four flags).
   **Done when** title shows two stacked selectors + chevron, focus moves with UP/DOWN, LEFT/RIGHT only affects the focused selector, no flicker, and the BG-write count never exceeds 12 in any single frame (manual visual test).

7. ✅ **Wire `enter_playing` to load chosen map** — single-line change `map_load(game_active_map());`. **Done when** picking Map 2 on title and pressing START draws Map 2's tilemap and enemies follow Map 2's waypoints; same for Map 3.

8. ✅ **Host tests `tests/test_maps.c`** — compile/link recipe: `cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires tests/test_maps.c res/assets.c -o build/test_maps` (mirrors `test_enemies` / `test_towers` which also pull in `res/assets.c` under `tests/stubs`). The test reads `gameplay1_classmap`, `gameplay2_classmap`, `gameplay3_classmap`, the three `gameplayN_waypoints` tables, and the three `(computer_tx, computer_ty)` pairs (declared in `res/assets.h` or via a small static `const map_def_t s_test_maps[3]` in the test file mirroring `src/map.c`'s registry — pick the latter to avoid coupling test on `map.c` internals). It also exercises the pure helper `src/map_select.h`. Cases:
   - `cycle_left/right` wrap (LEFT from MAP_1 → MAP_3; RIGHT from MAP_3 → MAP_1).
   - For each of the 3 maps: walk every consecutive waypoint pair, assert (a) the segment is purely horizontal or vertical, (b) every cell strictly between (and the endpoints) classifies as `TC_PATH` or `TC_COMPUTER`, (c) the final waypoint cell classifies as `TC_COMPUTER`, (d) the spawn waypoint `[0]` has `tx == -1`.
   - For each map: assert `waypoint_count <= MAX_WAYPOINTS`.
   - For each map: assert the 4 cells starting at `(computer_tx, computer_ty)` all classify as `TC_COMPUTER`, and that they fit inside `[0..PF_COLS-1] × [0..PF_ROWS-1]`.
   Add the recipe to `justfile::test` (mirrors the existing `tests/test_difficulty.c` pattern: pure `cc -std=c99 … -Itests/stubs -Isrc -Ires`). **Done when** `just test` runs `test_maps` and it passes.

9. ✅ **Manual scenarios in `tests/manual.md`** — append a "Maps + map selector" section with the acceptance scenarios from §6 below. **Done when** the file is updated.

10. ✅ **Memory updates** — append entries to `memory/decisions.md` and `memory/conventions.md` listed in §8 below. **Done when** both files are updated and reviewed.

---

## 5. Constraints

- **Honour every prior convention/decision** (BG-tile towers; modal precedence; frame loop split; OAM allocation; audio engine + music; F1/F2/F3 audio fixes; difficulty three-read-site rule; pure-helper rule; iter-3 #20 D9 + D-MUS-x; iter-3 #20 F1 selector-first/blink-deferred; corruption-state reset in `map_load`).
- **ROM ≤ 32 KB, no MBC1.** Projected post-#17 ≈ 23.6 KB.
- **BG writes ≤ 16/frame.** Title selector services ONE dirty per frame (priority chain in §2.3); gameplay path unchanged.
- **No new BG tiles.** All three maps reuse tile indices 128–150.
- **No wave-script or stat changes.** Only path geometry differs across maps.
- **No new modal.** Map selector is title-only.
- **Three-read-site rule (difficulty) unaffected** — UI display in `title.c` still doesn't count, per the iter-3 #20 clarification.
- **Pure-helper rule extended.** `src/map_select.h` is `<stdint.h>`-only and joins the host-testable family.
- **`map.c` may not include `game.h`.** `map_load` takes the active id as an argument (D8).

---

## 6. Acceptance Scenarios

### 6.1 Setup
- Build & run: `just run` (mGBA opens at 3× scale).
- No SRAM, no MBC; cold-boot is the only relevant prior state.
- Tests: `just test` must pass (includes new `test_maps`).

### 6.2 Scenarios

| # | Scenario | Steps | Expected |
|---|----------|-------|----------|
| 1 | Cold-boot defaults | Power on → title | Difficulty `< NORMAL >` shown row 10; `< MAP 1 >` row 12; chevron `>` at (3,10); blink prompt at row 13. |
| 2 | UP/DOWN toggles focus | From #1 press DOWN, then UP, then DOWN | After DOWN: chevron at (3,12). After UP: chevron at (3,10). After DOWN again: (3,12). No other tiles change. |
| 3 | LEFT/RIGHT routes by focus | From #1 (focus=diff) press RIGHT → label changes to `< HARD >`. Press DOWN. Press RIGHT → label row 12 changes to `< MAP 2 >`. | Map row never changes while focus=0; difficulty row never changes while focus=1. |
| 4 | Map cycle wraps | With focus=1, press LEFT three times | `MAP 1` → `MAP 3` → `MAP 2` → `MAP 1`. (LEFT-from-MAP_1 wraps.) |
| 5 | Map persists across game-over | Cycle to `MAP 2`, press START, lose (let HP reach 0), press START on game-over | Title re-shown with `< MAP 2 >` retained; pressing START launches Map 2 again. Difficulty also retained (iter-3 #20 regression check). |
| 6 | Map persists across win | Cycle to `MAP 3`, press START, win (clear all waves) | Game-over WIN → title shows `< MAP 3 >`. |
| 7 | Power-on resets selection | Set diff=HARD, map=MAP_3, start a session, then power-cycle the emulator | Title shows `< NORMAL >`, `< MAP 1 >`. |
| 8 | Map 2 plays correctly | Pick MAP 2, START | BG matches design doc Map 2. Spawned bug walks the new path through every waypoint and arrives at computer (HP decrements). |
| 9 | Map 3 plays correctly | Pick MAP 3, START | BG matches design doc Map 3. Bug walks shorter path; with same wave 1 composition, bug reaches computer noticeably faster than Map 1. |
| 10 | Towers & cursor still gated by ground | On Map 3, move cursor onto a path tile and press A | Placement rejected (existing invalid-cursor visuals: shade-1 brackets + 2 Hz blink). On a ground tile, placement succeeds. |
| 11 | Computer corruption progression works on all maps | On Map 2, take damage to drop HP to 1 | Computer cluster cycles through corruption states 0→4 exactly as Map 1 (iter-3 #21 unchanged). |
| 12 | Pause works on Map 2/3 | START during gameplay | Pause overlay anchored at (48, 64) regardless of map (iter-3 #22 unchanged). |
| 13 | Title input edge-only | Hold RIGHT for 1 second on focus=1 | Map cycles exactly once (no auto-repeat), matching iter-3 #20 D9. Same for UP/DOWN. |
| 14 | Same wave script across maps | Compare wave 1 enemy count and types on Map 1 vs Map 3 | Identical (D5). |
| 15 | Title BG-write budget never exceeded | Hold a D-pad direction and observe (visually) on real-DMG / mGBA accuracy mode | No tile flicker on selector or prompt during the 30-frame blink edge. (`title_render` worst case = 12 writes.) |

### 6.3 Visual states to check

- **Title — fresh entry** (chevron + both selectors + prompt visible).
- **Title — both selectors at extremes** (`< EASY >` + `< MAP 1 >`; `< HARD >` + `< MAP 3 >`) confirms label widths align.
- **Title — focus=1, blink toggling** ensures prompt blink doesn't visibly slip when LEFT/RIGHT is mashed.
- **Map 2 in-game** with pause overlay open.
- **Map 3 in-game** at HP 1 (corruption state 4).
- Responsive: DMG is fixed 160×144 — no breakpoints needed.

---

## 7. Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| `map_load` BG-write surge during DISPLAY_ON (F1-class) | Already bracketed by `enter_playing`'s DISPLAY_OFF/ON. Header comment retained. Subtask 7 asserts `enter_playing` still wraps the call. |
| ROM bloat past 32 KB | Pre-built estimate ≈ 23.6 KB. After subtask 2 (`gen_assets.py`) the coder reports actual size; if > 30 KB, escalate (no MBC fallback in this iteration). |
| Off-by-one in new waypoint lists | `tests/test_maps.c` walks each segment cell-by-cell against the classmap. |
| `enemies.c` terminal-detection mismatch with longer waypoint lists | Subtask 3 explicitly rewrites `step_enemy` to use `map_waypoint_count()` and deletes `WAYPOINT_COUNT`. `tests/test_enemies.c` updated in lockstep. |
| Stale legacy `gameplay_tilemap`/`gameplay_classmap` externs left in `assets.h` after Map 1 becomes `gameplay1_*` | Subtask 2 explicitly deletes the legacy symbols — no duplicate Map 1 ROM bytes. |
| Title selector flicker from new dirty flags | Service-one-per-frame chain (D2/§2.3) keeps title at ≤ 12 writes/frame. |
| `map_render` literal regression (someone re-adds `set_bkg_tile_xy(18, …)`) | All three maps put computer at (18, 4) today, so a bad refactor that re-hardcodes literals would still display correctly. Subtask 3 includes a code-review checklist item: "literal `18`/`19` no longer appears in `map_render`; values come from `cur->computer_tx/ty`". |
| `s_active_map` zero-init regression (similar to iter-3 #20 acceptance #1) | Subtask 8 includes a host test asserting `MAP_1 == 0` — power-on default falls out for free since `.data` init copies zero. |

---

## 8. New memory entries

### `memory/decisions.md` (append)

```
### Iter-3 #17 — Map registry & selector (2026-MM-DD)
- Maps are described by `map_def_t { tilemap, classmap, waypoints,
  waypoint_count, computer_tx, computer_ty }`. `src/map.c` owns one
  `static const map_def_t s_maps[MAP_COUNT]` table; `map_load(u8 id)`
  selects the active entry. `map.c` does NOT include `game.h` — the
  active id flows in as the explicit `map_load` argument.
- Active map is a `static u8 s_active_map = 0` in `src/game.c`,
  symmetric with `s_difficulty`. NEITHER is reset in `enter_title()`
  or `enter_playing()`; both rely on crt0 `.data` zero/normal init for
  the power-on default. SRAM persistence is feature #19.
- `map_render` reads `computer_tx/ty` from the active def. The literal
  `18`/`19`/`HUD_ROWS+4` block in `src/map.c::map_render` is deleted.
- Title screen owns TWO selectors stacked at rows 10 and 12, plus a
  one-tile focus chevron at column 3 of the focused row. UP/DOWN
  toggles focus (edge-only, NOT auto-repeat); LEFT/RIGHT cycles the
  focused selector. `s_title_focus` is reset to 0 in `title_enter()`.
- Title VBlank budget (extends iter-3 #20 "selector-first, blink-
  deferred"): four dirty flags serviced one per frame, priority
  `s_diff_dirty` > `s_map_dirty` > `s_focus_dirty` > `s_dirty` (prompt
  blink). Each branch returns after clearing its flag. Worst-case
  title = 12 writes/frame.
- Pure-helper family extended: `src/map_select.h` (`<stdint.h>`-only)
  joins `tuning.h`, `game_modal.h`, `*_anim.h`, `difficulty_calc.h`,
  `tower_select.h`, `music.h`. Tested in `tests/test_maps.c`.
- All three maps anchor their 2×2 computer cluster at TL = (18, 4).
  Future maps may move it; the hardcoded fallback is gone.
- Wave script and difficulty constants are shared across maps —
  path geometry is the only per-map variable. Three-read-site rule
  for difficulty is unaffected.
```

### `memory/conventions.md` (append)

```
### Iter-3 #17 conventions (2026-MM-DD) — Maps & map selector
- **Map registry**: `s_maps[MAP_COUNT]` in `src/map.c`. Indexing
  always goes through `map_load(id)` + the public accessors
  (`map_class_at`, `map_waypoints`, `map_waypoint_count`,
  `map_active`, `map_set_computer_state`). External code MUST NOT
  read `s_maps` directly. `map.c` is forbidden from including
  `game.h` — the active id is passed in as `map_load`'s argument.
- **map_load contract** unchanged: caller MUST bracket with
  DISPLAY_OFF/ON (writes ~340 BG tiles). `map_load` ALSO resets the
  iter-3 #21 corruption tracking (`s_state = 0; s_state_dirty = 0;`).
- **Active-map persistence**: `static u8 s_active_map` in
  `src/game.c`. Read via `game_active_map()`, write via
  `game_set_active_map(u8)` (clamps to `< MAP_COUNT`). NOT reset in
  state transitions; power-on default = MAP_1 via `.data`.
- **Title-screen UI**: two stacked selectors + one-tile chevron at
  (FOCUS_COL=3, focused_row). UP/DOWN edge-only toggles
  `s_title_focus`; LEFT/RIGHT edge-only cycles the focused
  selector. The single-D-pad-press exemption (no auto-repeat for
  short cycles) covers UP/DOWN as well.
- **Title BG-write priority** (extends iter-3 #20 selector-first,
  blink-deferred): `s_diff_dirty` → `s_map_dirty` → `s_focus_dirty`
  → `s_dirty`. Service ONE per frame; each branch returns after
  clearing its flag. Cap = 12 writes/frame.
- **Computer hardcoding eliminated**: `map_render` reads
  `computer_tx/ty` from the active map_def. The previous literal
  `(18, sr)`/`(19, sr+1)` block is forbidden — re-introducing it
  would be a regression. (Today all 3 maps still land at (18, 4),
  so a sloppy refactor would visually pass; the convention is the
  guard.)
- **Wave/stat invariance**: when adding maps, do NOT add a per-map
  field for "spawn delay multiplier", "wave override", etc. The
  wave script and difficulty constants are shared; path geometry
  is the only per-map dimension.
```

---

## 9. Review

### 9.1 Adversarial review (self, supplemented by reviewer agent)

Findings considered and resolved before finalising:

1. **"Why isn't `map.c` allowed to include `game.h`?"** — Keeping `map.c`
   engine-side (no game-flow dependency) lets host tests link `map.c` (or its
   data tables) without stubbing `game.c`. Resolved with D8 (explicit `id`
   argument) and the convention entry.

2. **"Multi-frame slip on prompt blink could be visible if many dirty flags
   queue up."** — Worst-case queue depth on the title is 3 (diff + map +
   focus + blink edge in the same 30-frame window). Each gets serviced one
   per frame; even a 4-frame slip on a 30-frame blink is invisible (≈ 13%
   phase shift). Same reasoning as iter-3 #20 F1.

3. **"`map_load` argument vs no-arg with internal getter — the no-arg form
   would let host tests call `map_load()` without a stubbed `game_active_map`
   if `map.c` declared the id as a weak default."** — Rejected; weak defaults
   are non-portable in SDCC and host. Explicit-arg API is simpler and lets
   `tests/test_maps.c` drive `map_load(id)` directly.

4. **"Could LEFT/RIGHT auto-repeat on the map row when held? UX testing
   would show users want it."** — 3-state cycle, same length as difficulty;
   the iter-3 #20 D9 rationale (overshoot on hold) applies identically.
   Edge-only.

5. **"Why three full classmaps instead of one tilemap and runtime
   classification?"** — Kept the existing format (D4). Refactoring the
   classification path costs more than the 680 ROM bytes it would save, and
   the existing per-frame `map_class_at` is hot (cursor.c, towers.c). +680
   bytes is well inside the 32 KB cap.

6. **"Risk: someone passes `map_load(99)` and we crash."** —
   `game_set_active_map` clamps to `< MAP_COUNT`. The only call site for
   `map_load` is `enter_playing`, which feeds it `game_active_map()`. We
   could *also* defensively clamp inside `map_load`; deferred (the call
   graph is one-deep and tested). If a future caller bypasses
   `game_set_active_map`, the `tests/test_maps.c` setter test will fail
   first.

7. **"Designer placed all 3 computers at (18,4). Why bother adding
   `computer_tx/ty` fields at all?"** — (a) eliminates the literal-block in
   `map_render` (iter-3 #21 had to know the position from a comment block);
   (b) tests/test_maps.c can validate placement; (c) future map 4+ are free
   to move the cluster without code changes. Cost: 2 bytes per map_def.
   Worth it.

8. **"Title selector layout adds row 12. Does it overlap the title
   tilemap's tagline at rows 9–10 or any other element?"** — Per
   `specs/mvp-design.md` §2.1, rows 11–12 are blank; row 13 is the prompt;
   row 14–15 blank; row 16 attribution. Row 12 is free. Confirmed safe.

9. **"What if the selector LEFT/RIGHT lands on the same frame the focus
   chevron moves?"** — Impossible: UP/DOWN and LEFT/RIGHT are in mutually
   exclusive `else if` branches in `title_update`. Even if they weren't,
   the per-frame priority chain serializes them.

### 9.2 Cross-model validation

The near-final spec was reviewed against acceptance-scenario realism,
subtask completeness, and architecture-vs-subtask gaps. Findings folded
back into D10/D12 (computer position rationale + title focus reset) and
into the §7 risk table.

### 9.3 Review-finding resolutions (post-reviewer agent pass)

| # | Finding | Resolution |
|---|---------|------------|
| F1 | `enemies.c::step_enemy` uses compile-time `WAYPOINT_COUNT` to detect "reached the computer" — Map 2's 10 waypoints would mis-despawn at index 7. §2.4's "no changes outside `map.c`" claim was wrong. | §2.4 rewritten. Subtask 3 now explicitly deletes the macro, replaces the check with `map_waypoint_count()` (cached locally), and updates `tests/test_enemies.c`'s stub to provide `map_waypoint_count`. |
| F2 | Subtask 2 typed waypoint arrays as `static const`, which would prevent `src/map.c` from referencing them. | Subtask 2 now specifies plain `const` (external linkage), and explicitly tells `res/assets.h` to declare `extern const waypoint_t gameplayN_waypoints[];` and to `#include "map.h"` for the typedef. |
| F3 | Legacy `gameplay_tilemap` / `gameplay_classmap` symbols in `res/assets.h` would silently double-ship Map 1 data if not retired. | Subtask 2 now states Map 1 is the canonical replacement: legacy symbols deleted, gameplay1_* are the only Map 1 arrays in the ROM. Map 1 byte-equivalence requirement called out. |
| F4 | Subtask 8's host-test recipe didn't list `res/assets.c` as a translation unit — link would fail when the test reads classmap bytes. | Subtask 8 now states the explicit recipe `cc … tests/test_maps.c res/assets.c -o build/test_maps`, mirroring the existing `test_enemies` / `test_towers` recipes. |

No remaining high-confidence issues at finalisation.
