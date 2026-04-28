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
  `enum { DIFF_EASY=0, DIFF_NORMAL=1, DIFF_HARD=2, DIFF_COUNT=3 };`. `game.h`
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
