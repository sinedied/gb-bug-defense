# Conventions

<!-- Append new conventions at the end using this format:

### <Convention Name>
<Clear description with example if helpful>
-->

### DMG palette assignments (BGP indices)
- 0 = white: HUD background, text background, buildable ground (with subtle dot texture)
- 1 = light grey: tower/computer mid-tone shading, "ghost" (invalid) cursor variant
- 2 = dark grey: path tiles, computer chassis, tower base ring
- 3 = black: all text, enemy bodies, projectiles, cursor brackets, tower turret cap
Reserve black for "things the player must locate quickly" so they pop on real DMG hardware.

### HUD format (gameplay screen)
Single top row, exact layout: `HP:05  E:030  W:1/3` (17 chars + 3 trailing blank tiles).
Redraw only on the relevant event (HP on damage, E on spend/kill, W on wave change). Never per-frame.

### UI text rules
Use the GBDK default 8×8 ASCII font unmodified. No custom glyphs in MVP. Always render text as
shade 3 on shade 0 with at least 1 empty tile of padding on every side. No element blinks faster
than 2 Hz.

### Tile coordinate convention
All UI/map positions are in 8×8 tile coordinates `(col, row)` with origin at the screen
top-left. When working inside the play field, specify "play-field-local" coords (origin at
play-field top-left, i.e. screen row + HUD height).

### Cursor invalid-placement feedback (dual channel)
Convey invalid tiles by changing **two** properties simultaneously: lower the bracket shade
(black → light grey) AND double the blink rate (1 Hz → 2 Hz). Single-channel cues do not
survive DMG contrast loss reliably.

### Sprite/OAM caps for MVP
Pool sizes (authoritative): `MAX_ENEMIES = 12`, `MAX_PROJECTILES = 8`, `MAX_TOWERS = 16`. Worst-case OAM 37/40 (1 cursor + 12 enemies + 8 projectiles + 16 towers). OAM slot allocation: 0=cursor, 1..16=towers, 17..28=enemies, 29..36=projectiles, 37..39=reserved. Each module's `_init()` must hide every OAM slot it owns to prevent stale sprites across game sessions.

### Input auto-repeat timing
D-pad: 12-frame initial hold, then 6-frame auto-repeat. Applies to cursor movement (and any future menu navigation).

### Justfile recipe shape
Use `just`'s shebang recipes (`#!/usr/bin/env bash` + `set -euo pipefail`) for any recipe with multi-line shell logic. Plain recipe lines run in separate shells and break multi-line `if`/loops.

### Sprite/OAM caps for  towers moved to BG (2025-01-29 update)MVP 
Per F3 fix (see memory/decisions.md "Towers render as BG tiles"), towers
no longer occupy OAM. New worst-case OAM = 21/40 (1 cursor + 12 enemies
+ 8 projectiles). OAM slots `OAM_TOWERS_BASE..+15` are reserved unused
to keep the existing slot allocation stable; do not repurpose without
also re-checking the per-scanline budget.

### Frame loop split: update vs render
Modules with per-frame BG writes (`hud`, `map`, `towers`, `title`,
`gameover`) expose a `_render()` entry point that performs only small
VBlank-safe `set_bkg_*` calls. `_update()` does logic + sprite shadow-

 game_update()` so render lands inside the VBlank window.
Full-screen BG redraws bracket their writes with `DISPLAY_OFF`/`ON`.

### Iter-2 conventions (2025-01-30)
- **HUD col 19** is reserved for the selected-tower indicator (`A` = antivirus, `F` = firewall). `hud_mark_t_dirty()` triggers the redraw.
- **HUD wave field** widened to `W:NN/NN` (cols 11..17). Coders adding more waves past 99 must reflow.
- **OAM 1..14** are owned by `menu.c` while `menu_is_open()`. `menu_close()` MUST hide them. `menu_open()` MUST hide enemies (17..30) + projectiles (31..38) via `enemies_hide_all()` / `projectiles_hide_all()`.
- **OAM allocation (iter-2)**: 0 cursor / 1..14 menu / 15..16 reserved / 17..30 enemies (14) / 31..38 projectiles (8) / 39 reserved. Max 23 simultaneous (non-menu) or 15 (menu). *(Superseded for iter-3 — see the iter-3 entry below; menu range is now 1..16.)*
- **Per-frame BG-write budget**: ≤ 16 (worst case 15 = HUD 10 + computer-damaged 4 + tower place-or-clear 1). `towers_render` performs at most 1 place AND at most 1 sell-clear per frame.
- **Menu mode is modal**: `game.c::playing_update` returns early on the frame `menu_open()` is called, then on subsequent frames gates `cursor_update`, `towers_update`, `enemies_update`, `projectiles_update`, `waves_update`. Only `economy_tick` and `audio_tick` run during menu mode.
- **Sprite-bank glyph reuse**: any FONT-dict glyph in `gen_assets.py` may be mirrored into the sprite VRAM bank via `glyph_to_sprite(ch)`. Pixel-identical with HUD digits.
- **Bounty capture rule**: any code calling `enemies_apply_damage` MUST capture the bounty/type-derived state BEFORE the call, since the call may free the slot for same-frame re-use by `enemies_spawn`.
- **Module init in state transitions**: `enter_playing()` calls every module's `_init()` including `menu_init()` (added in iter-2). The MVP's per-`_init` OAM-hide convention extends to menu's slot range.

### Iter-3 conventions (2026-05-01)
- **Modal helper**: `game_is_modal_open()` returns true iff a modal is open (`menu_is_open() || pause_is_open()`). Use this anywhere code needs to ask "is gameplay frozen". Do NOT add new modals without updating the helper.
- **OAM allocation (iter-3)**: `0 = cursor, 1..16 = menu/pause (mutually exclusive), 17..30 = enemies, 31..38 = projectiles, 39 = reserved`. `OAM_MENU_COUNT` is 16 (was 14 in iter-2). The pause module hides the cursor (OAM 0) on open via `move_sprite(OAM_CURSOR, 0, 0)` in addition to `cursor_blink_pause(true)`. `menu.c::hide_menu_oam` zeroes all 16 slots even though upgrade/sell only paints 14.
- **Module init in state transitions (iter-3 update)**: `enter_title()` and `enter_playing()` both call `pause_init()` alongside the iter-2 set (`cursor/towers/enemies/projectiles/menu`). `audio_reset()` is **NOT** added to `enter_title()` (would race with `audio_init()` on boot); it is called from the QUIT path inside `playing_update` instead.
- **Frame-loop modal precedence**: `playing_update` checks `pause_is_open()` first, then `menu_is_open()`, then runs the normal entity-update path. Gameover checks (`economy_get_hp() == 0`, `waves_all_cleared() && enemies_count() == 0`) MUST run before the START → `pause_open()` handler at end-of-frame, and BOTH gameover branches MUST end with explicit `return;`.
- **Pause-overlay anchor is fixed** at screen `(48, 64)` — no contextual placement, unlike the iter-2 tower menu.

### Modal state must be latched at frame start
Any per-frame entry point that both (a) calls a modal-mutating function (e.g. `menu_update()`, `pause_update()`) AND (b) later gates behavior on "is a modal open" MUST snapshot the modal predicates into local `bool`s at the TOP of the function, before the first modal-mutating call. Re-querying `menu_is_open()` / `pause_is_open()` after a same-frame close leaks input (e.g. F1: A+START on the same frame opened pause). The canonical example is `src/game.c::playing_update`'s `menu_was_open` latch feeding `playing_modal_should_open_pause()` (`src/game_modal.h`).

### Modal-dispatch logic must be unit-testable on the host
Predicates that decide modal precedence / cross-modal gating belong in a header-only pure helper (see `src/game_modal.h`) so tests can link them without GBDK collaborators. Add a corresponding `tests/test_<helper>.c` and wire it into the `just test` recipe. Do NOT rely on isolation tests of individual modal modules to catch dispatch bugs in `playing_update`.

### Iter-3 #21 conventions (2026-05-08)
- **Global anim phase**: `game_anim_frame()` (in `game.h`) is the single source of truth for cross-module animation timing. `static u8 s_anim_frame` lives in `src/game.c`, incremented at top of `game_update()`, reset to 0 in `enter_playing()`. Wraps at 256; all phase divisors used (32 = LED half-period; bitrev4 stagger across 64-frame full period) divide it evenly.
- **BG-write budget (revised)**: ≤ 16 writes/frame remains the cap. `towers_render` contributes **at most 1** write/frame total — sell, place, and idle phases each `return;` after writing, so they are mutually exclusive within a single call. Updated worst-case: 10 (HUD) + 4 (corruption) + 1 (towers) = **15**. The idle scanner's 1-write/frame ceiling is non-negotiable; do not "drain all stale" in one frame.
- **Modal gating extended**: any new "ambient" BG-write source MUST gate on `!game_is_modal_open()` (towers idle is the precedent). Animation timers tied to entity ticks (e.g. `flash_timer` inside `enemies_update`) freeze automatically because their host `_update` is already gated. Do NOT add cross-module tick paths that bypass modal gating.
- **Pure-helper headers**: `src/map_anim.h`, `src/enemies_anim.h`, and `src/towers_anim.h` join `src/tuning.h` and `src/game_modal.h` as host-testable header-only modules. All three new headers MUST use `<stdint.h>` directly (NOT `gtypes.h`, which transitively pulls `<gb/gb.h>`). Tests in `tests/test_anim.c` link without GBDK stubs.

### Iter-3 #20 conventions (2026-05-15)
- **Difficulty is global, persistent within a power-on session**:
  `static u8 s_difficulty = DIFF_NORMAL;` at file scope in `src/game.c`. Read
  via `game_difficulty()`. Do NOT reset it in `enter_title()` or
  `enter_playing()`. SRAM persistence is feature #19 (separate spec).
- **Difficulty enum lives in `src/difficulty_calc.h`** (NOT `game.h`):
  `enum { DIFF_CASUAL=0, DIFF_NORMAL=1, DIFF_VETERAN=2, DIFF_COUNT=3 };`. `game.h`
  `#include`s `difficulty_calc.h` so on-device callers see the enum
  transitively, and host tests can include `difficulty_calc.h` directly
  without pulling `gtypes.h` / `<gb/gb.h>`. Use the symbolic names; the
  integer values are also the indices into `DIFF_ENEMY_HP[diff][type]` — do
  not reorder.
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
- **Title-screen selector input is edge-only**: D-pad LEFT/RIGHT on title
  uses `input_is_pressed` (NOT `input_is_repeat`). 3-state cycles are too
  short for auto-repeat to feel right (overshoot on a held D-pad). The
  iter-1 "D-pad auto-repeat for menu navigation" convention applies to
  cursor movement and to in-game menus with >3 items; short title-screen
  selectors are exempted.

### Iter-3 #20  clarification (2026-05-16)conventions 
- **"Three difficulty read sites only" applies to gameplay-scaling
  consumers** (enemies HP at spawn, waves spawn delay, economy starting
  energy). UI-display read  currently `src/title.c::draw_diff_now`sites 
  reading `game_difficulty()` for the `< LABEL >`  are NOTselector 
  counted and do NOT require updating `tests/test_difficulty.c`. New
  gameplay-scaling sites still require new test coverage; new
  UI-display sites do not.
- **Title BG-write VBlank budget**: `title_render` services at most ONE
  dirty region per frame (selector first, blink deferred). This caps
  title at 12 writes/frame (the  within the 16-write cap. Seeprompt) 
  decisions.md "Title VBlank: selector-first, blink-deferred".

### Music engine conventions (Iter-3 #16, 2026-05-22)
- Music lives in `src/music.{h,c}`; SFX in `src/audio.{h,c}`. `audio_tick()`
  calls `music_tick()` once per frame at the END of the tick - single
  audio entry point for the main loop.
- Music owns CH3 (always) and CH4 (when not preempted by SFX). CH1/CH2
  are SFX-exclusive. Music NEVER touches CH1/CH2.
- Wave RAM (0xFF30-0xFF3F) is written ONCE in `music_init()` with the
  DAC off (`NR30=0x00`), then DAC turned on per-row via the next
  `music_play()`/`music_tick()`. Never write wave RAM while CH3 is
  enabled.
- Song format = linear `mus_row_t` array with `loop_idx` (0xFF =
  stinger one-shot). Each row holds CH3 pitch (0=rest), CH4 noise
  byte (0=rest), and frame duration. End-of-stinger -> `music_stop()`.
- `music_play(id)` is idempotent for the active song; switching songs
  resets `row_idx` to 0 AND synchronously arms row-0 NR3x/NR4x BEFORE
  returning (so stingers play their first note even when the caller
  blocks the main loop).
- Pause ducking touches NR50 only (`0x77` <-> `0x33`). Affects all
  channels - intentional (D-MUS-4). Upgrade/sell menu does NOT duck.
- Include direction is one-way: `audio.c` may include `music.h`;
  `music.c` MUST NOT include `audio.h`.
- Pure-helper rule: row-advance arithmetic lives as `static inline
  music_next_row()` in `src/music.h` (`<stdint.h>`-only). Joins the
  `tuning.h` / `game_modal.h` / `*_anim.h` / `difficulty_calc.h` family
  of host-testable headers. Test coverage in `tests/test_music.c`
  (8 cases: synchronous-arm, row advance, loop wrap, stinger end,
  idempotency, song switch, ch4 arbitration, duck).
- Test-stub extension: `tests/stubs/gb/hardware.h` exposes NR30-NR34
  via the same `_NR_LOG` macro and `_AUD3WAVERAM` as a 16-byte global
  array (`g_wave_ram`) so wave-RAM writes are observable from host
  tests.

### Tower stats kind discriminator (iter-3 #18)
Each row in s_tower_stats[] declares `kind`:
  - TKIND_DAMAGE: reads .damage / .damage_l1 / .cooldown / .cooldown_l1, fires projectiles via projectiles_fire().
  - TKIND_STUN: reads .stun_frames / .stun_frames_l1 / .cooldown / .cooldown_l1, calls enemies_try_stun() (no projectile).
The unused field group is  do NOT cross-read.irrelevant 

### Enemy sprite-tile priority (iter-3 #18)
In step_enemy(), pick the sprite tile in this priority order:
 SPR_*_FLASH (3-frame override; per iter-3 #21)
 SPR_*_STUN  (movement also skipped)
 walk frame A/B by (anim >> 4) & 1
flash_timer and stun_timer both decrement every frame regardless of which one wins the tile slot. e->anim only increments on the walk branch.

### Tower-select cycle: pure helper in tower_select.h (iter-3 #18)
The B-button handler in game.c calls tower_select_next(cur, TOWER_TYPE_COUNT) from the pure header src/tower_select.h. Adding a new tower requires NO change to this code; the new type joins the cycle automatically. The pure header keeps host tests in test_math.c free of game-source dependencies.

### Stun encapsulation: enemies_try_stun() / enemies_is_stunned() (iter-3 #18)
enemy_t (including stun_timer) is private to enemies.c. Stun-applying code (currently towers.c, future projectile-stuns, debuffs, etc.) MUST go through the public APIs:
  - bool enemies_try_stun(u8 idx, u8 frames)  — returns true and sets timer iff alive and not already stunned
  - bool enemies_is_stunned(u8 idx)           — read-only
Direct enemy_t access from outside enemies.c is forbidden.

### Iter-3 #17 conventions (2026-04- Maps & map selector28) 
- **Map registry**: `s_maps[MAP_COUNT]` in `src/map.c`. External code reaches it ONLY via the public API (`map_load(id)`, `map_class_at`, `map_waypoints`, `map_waypoint_count`, `map_active`, `map_set_computer_state`). Direct reads of `s_maps` from outside `map.c` are forbidden. `map.c` may NOT include `game. the active id is passed in as `map_load`'s argument.h` 
- **map_load contract** (extended): caller MUST bracket with DISPLAY_OFF/ON (writes ~340 BG tiles, larger than a VBlank window). `map_load` ALSO resets the iter-3 #21 corruption tracking (`s_state = 0; s_state_dirty = 0;`).
- **Active-map persistence**: `static u8 s_active_map` in `src/game.c`. Read via `game_active_map()`, write via `game_set_active_map(u8)` (clamps `< MAP_COUNT`). NOT reset in state transitions; power-on default = MAP_1 via `.data` zero-init.
- **`WAYPOINT_COUNT` is deprecated**. Use `map_waypoint_count()`. Any new code that needs the count of the active map's waypoints MUST go through the accessor, never a compile-time macro. `MAX_WAYPOINTS` (=10) is a designer cap on the largest map; not a per-map count.
- **Title-screen UI**: two stacked selectors + one-tile focus chevron at (FOCUS_COL=3, focused_row). UP/DOWN edge-only toggles `s_title_focus`; LEFT/RIGHT edge-only cycles the focused selector. The single-D-pad-press exemption (no auto-repeat for short cycles) covers UP/DOWN as well as LEFT/RIGHT.
- **Title BG-write priority chain** (extends iter-3 #20): service ONE dirty per frame in priority order `s_diff_dirty` > `s_map_dirty` > `s_focus_dirty` > `s_dirty` (prompt blink). Each branch returns after clearing its flag. Cap = 12 writes/frame, well inside the 16-write budget.
- **Computer position is map-local**: `map_render` reads `computer_tx/ty` from the active map_def. The previous literal `(18, sr)`/`(19, sr+1)` block in `src/map.c::map_render` is forbidden. (Today all 3 maps still land at (18, 4), so a sloppy refactor would visually pass; this convention is the guard.)
- **Wave/stat invariance across maps**: when adding maps, do NOT add per-map fields for spawn delays, wave overrides, energy multipliers, etc. Wave script and difficulty constants are shared; path geometry is the only per-map dimension.
- **Asset emission for new map data** (gen_assets.py): tilemap/classmap/waypoint arrays are emitted as plain `const` (NOT `static const`) so `src/map.c` can reference them via `extern` decls in `res/assets.h`. `assets.h` `#include`s `src/map.h` for the `waypoint_t` typedef. Subsequent maps follow the `gameplayN_*` naming pattern.

- SRAM access protocol (iter-3 #19): every read or write transaction wraps `ENABLE_RAM; SWITCH_RAM(0); /* I/O */ DISABLE_RAM;` with `DISABLE_RAM` always called before returning, even on read paths. Save writes happen ONLY at `enter_gameover` — never per-frame, never mid-game.
- Single-owner save cache (iter-3 #19): the 9-slot high-score cache `static u16 s_hi[9]` lives ONLY in `save.c`. `score.c` has no local cache and reads via `save_get_hi(map, diff)`. UI code (title, gameover) also reads via `save_get_hi`. Three read sites total: title HI line, score commit-if-record, (future) any stats screen.
- Title BG-write priority chain extended to 5 flags (iter-3 #19): `s_diff_dirty > s_map_dirty > s_focus_dirty > s_hi_dirty > s_dirty (prompt)`. Each branch services one per frame and returns. `s_hi_dirty` is set on title enter and after any difficulty/map cycle (the same handlers that already set `s_diff_dirty`/`s_map_dirty`). Per-frame BG-write cap (12) preserved: HI line is 9 tiles.
- Pure-helper headers (iter-3 #19): `src/score_calc.h` and `src/save_calc.h` join the existing list (`tuning.h`, `game_modal.h`, `difficulty_calc.h`, `map_anim.h`, `enemies_anim.h`, `towers_anim.h`). Use `<stdint.h>` directly — never `gtypes.h`. All score-formula and save-serialization math lives in these headers for host testing.
- Cartridge configuration (iter-3 #19): MBC1+RAM+BATTERY, 64 KB / 4 banks, 2 KB SRAM. Build flags `-Wl-yt0x03 -Wm-yo4 -Wm-ya1` (split prefixes — all-`-Wl-` form is silently ignored for `-yo`/`-ya`). `just check` asserts header bytes 0x147=0x03, 0x148=0x01, 0x149=0x01 and ROM size 65536. No `__banked` annotations needed; current code fits banks 0+1; banks 2-3 reserved for future.

### Iter-4 #24 conventions (first-tower gate)
- **Gate state lives in `game.c`** (no new `.c` module): `s_gate_active`, `s_gate_blink`, `s_gate_vis`, `s_gate_dirty`. All reset in `enter_playing()` only — NOT on pause-resume, NOT in `enter_title()`.
- **Gate blocks waves by skipping `waves_update()`**: while `s_gate_active == 1`, `waves_update()` is not called and `s_gate_blink` increments instead. Zero changes to `waves.c`.
- **Gate text**: "PLACEA" / "TOWER!" — 12 tiles total at screen (7, 8) and (7, 9). Initial paint during `DISPLAY_OFF` in `enter_playing()`.
- **Blink rate**: 1 Hz (30-frame half-period), computed by `gate_blink_visible()` in the pure helper `src/gate_calc.h`. Counter freezes during pause/menu (incremented only in entity-update branch).
- **Render order**: `hud_update → map_render → gate_render → towers_render → menu_render → pause_render`. Gate before towers so tower tile wins on overlap at gate-lift.
- **BG-write budget with gate**: gate-lift frame = 3 (HUD E) + 12 (restore) + 1 (tower) = 16 (exactly at cap). B-button is gated on `!s_gate_active` to prevent A+B same-frame overflow to 17.
- **`map_tile_at(u8 tx, u8 ty)`**: new accessor in `map.h`/`map.c` reading original tilemap tiles for play-field-local positions. Returns `TILE_GROUND` for out-of-bounds. Follows `map_class_at()` pattern.
- **Pure-helper headers (extended)**: `src/gate_calc.h` joins the `<stdint.h>`-only family (`tuning.h`, `game_modal.h`, `*_anim.h`, `difficulty_calc.h`, `score_calc.h`, `save_calc.h`). Test coverage in `tests/test_gate.c`.
