# Manual / on-device regression checks

The Game Boy ROM target has no on-device unit-test harness, so the
following findings are verified by inspection + a scripted boot smoke
test + the documented manual playthrough below. Run with:

    just check          # build + ROM size/header + host-side test_math
    timeout 5 mgba -3 build/gbtd.gb   # boot smoke test (kill after 5s)

## F1 — VRAM/BG writes during active scan
**Automated:** none (LCD timing not observable from outside the emulator).

**Inspection check:** every `set_bkg_*` call site is now either inside a
`DISPLAY_OFF` … `DISPLAY_ON` bracket (full-screen redraws in
`title_enter`, `gameover_enter`, `enter_playing` which calls `map_load`
+ `hud_init`) **or** runs from a `*_render()` function called by
`game_render()` immediately after `wait_vbl_done()` in `main.c`'s loop.

**Manual on-device check (mGBA / real DMG):**
1. Boot — title screen renders cleanly with no torn/garbage tiles.
2. Press START — gameplay map appears with no flicker; HUD digits render
   stably.
3. Place a tower (A on a buildable tile) — tower BG tile appears
   instantly with no half-redrawn glitch.
4. Take damage to drop HP to ≤ 2 — computer 2×2 tile swap to "damaged"
   variant occurs cleanly in one frame.
5. Reach win/lose — full-screen text appears with no tearing.

## F3 — 10-sprites-per-scanline limit
**Automated:** none.

**Inspection check:** `towers.c` no longer calls `set_sprite_tile` /
`move_sprite` for tower placement; placement writes `TILE_TOWER` to the
BG via `towers_render()`. OAM slots `OAM_TOWERS_BASE..+15` are reserved
unused (see `memory/conventions.md` appended note).

**Manual on-device check:**
1. Build a row of towers along screen row 7 (place 5+ towers on the same
   tile-row, all on `TC_GROUND` cells).
2. Trigger wave 3 so several enemies cross that row simultaneously.
3. Expected: enemies and projectiles render without per-row sprite
   flicker. Pre-fix, the same setup would drop sprites or flicker on
   real DMG because each scanline's 10-sprite budget was exceeded.

## F5 — D-pad auto-repeat reset on direction change
**Automated:** none (input layer reads `joypad()` from `<gb/gb.h>` —
extracting the repeat logic for host testing would require a
non-trivial refactor and a hardware-input shim).

**Manual on-device check:**
1. On the title screen (or in-game with the cursor visible), hold RIGHT
   for ≥ 1 second so the auto-repeat kicks in.
2. Without releasing, also press UP (now both held).
3. Release RIGHT but keep holding UP. Expected: the cursor moves UP
   exactly once and waits the full 12-frame initial delay before
   auto-repeating. Pre-fix, UP would immediately auto-repeat at the
   6-frame interval inherited from RIGHT's hold counter.
4. Equivalent test by tapping then holding a perpendicular direction: no
   "first step then instant repeat" jump.

## F6 — Projectile retargets new enemy in reused slot
**Automated:** none (cleanly extracting the projectile/enemy slot
lifecycle for host testing would require pulling fixed-point math,
waypoint tables, and economy from `<gb/gb.h>`-tied modules).

**Manual on-device check:**
- The window for the original bug is small (one bug killed and a new
  one spawned into the same slot before an in-flight projectile lands)
  but reachable in wave 3 with several towers concentrated near the
  spawn point. After the fix the in-flight projectile silently despawns
  on slot-reuse instead of warping and "free-killing" the new bug.
- Inspection: `projectiles.c::step_proj` checks
  `enemies_gen(p->target) != p->target_gen` and despawns if so;
  `enemies.c::enemies_spawn` increments `e->gen`.
