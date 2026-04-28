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
- **OAM allocation (iter-2)**: 0 cursor / 1..14 menu / 15..16 reserved / 17..30 enemies (14) / 31..38 projectiles (8) / 39 reserved. Max 23 simultaneous (non-menu) or 15 (menu).
- **Per-frame BG-write budget**: ≤ 16 (worst case 15 = HUD 10 + computer-damaged 4 + tower place-or-clear 1). `towers_render` performs at most 1 place AND at most 1 sell-clear per frame.
- **Menu mode is modal**: `game.c::playing_update` returns early on the frame `menu_open()` is called, then on subsequent frames gates `cursor_update`, `towers_update`, `enemies_update`, `projectiles_update`, `waves_update`. Only `economy_tick` and `audio_tick` run during menu mode.
- **Sprite-bank glyph reuse**: any FONT-dict glyph in `gen_assets.py` may be mirrored into the sprite VRAM bank via `glyph_to_sprite(ch)`. Pixel-identical with HUD digits.
- **Bounty capture rule**: any code calling `enemies_apply_damage` MUST capture the bounty/type-derived state BEFORE the call, since the call may free the slot for same-frame re-use by `enemies_spawn`.
- **Module init in state transitions**: `enter_playing()` calls every module's `_init()` including `menu_init()` (added in iter-2). The MVP's per-`_init` OAM-hide convention extends to menu's slot range.
