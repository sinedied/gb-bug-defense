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

---

## Iteration 2 — manual checks

`just check` covers the host-side iter-2 regression suite (sell refund,
passive income tick, wave-event indexing, tower stats lookup, enemy
bounty, sell-then-place de-dup, FW range squared distance). The on-DMG
checks below cover the audible / visual / interactive surfaces that no
host harness can verify.

### SFX audibility (#15)
Run with audio enabled: `just run`. With mGBA's audio panel open and
volume up, every action below must produce its blip:

1. Place a tower (A on a buildable tile) — short low-blip on ch1
   (`SFX_TOWER_PLACE`). Same blip plays on upgrade and sell.
2. Idle while a tower fires at a bug — periodic mid-square blip on ch2
   (`SFX_TOWER_FIRE`).
3. A projectile hits a robot (does not kill) — short noise click on ch4
   (`SFX_ENEMY_HIT`).
4. A projectile kills any enemy — longer noise rumble on ch4
   (`SFX_ENEMY_DEATH`); priority preempts a same-frame `SFX_ENEMY_HIT`.
5. Survive all 10 waves — ascending C-E-G-C jingle on ch1 (`SFX_WIN`).
6. Let HP reach 0 — descending jingle on ch1 (`SFX_LOSE`).
7. mGBA `Tools → View channels`: confirm only channels 1, 2, 4 ever
   become active. Channel 3 (wave) stays silent throughout (reserved
   for iter-3 music).

### Upgrade / sell menu UX (#12)
1. Place an AV tower; park cursor on it; press A. A 14-sprite widget
   appears anchored beside the tower:
       `>UPG:15`
       ` SEL:05`
2. Enemies and projectiles freeze; cursor stops blinking.
3. Press DOWN — the `>` glyph moves to row 1 (`SEL`). UP returns it.
4. Press B — menu closes; the next frame, enemies and projectiles
   resume motion exactly where they were.
5. Re-open menu, press A on UPG — energy drops by 15; tower visibly
   fires faster (cooldown 60 → 40 frames); menu closes.
6. Re-open menu — UPG line now reads `UPG:--`; pressing A on UPG is
   silently ignored (menu stays open). Pressing DOWN + A sells the
   tower; energy is refunded by `spent / 2`; the tower's BG tile
   reverts to `TILE_GROUND` within one frame.
7. mGBA `Tools → View sprites`: while menu is open, OAM slots 1..14
   hold the menu glyphs and slots 17..38 are hidden (Y=0). After
   close, slots 1..14 are hidden and 17..38 repopulate as enemies /
   projectiles tick.
8. Place a tower, sell it, immediately place another tower on the same
   tile — the new tower's BG tile is visible; no one-frame `TILE_GROUND`
   flash (de-dup of pending clear in `towers_try_place`).
9. **F3 (iter-2 review):** Open the upgrade/sell menu on a tower. The
   cursor sprite must remain visible (steady on-phase, NOT blank) for
   the entire menu session, regardless of the blink phase at the moment
   the menu opened. After closing the menu (B), the cursor's blink
   resumes within one frame.

### Tower-type cycle (#11)
1. Press B from gameplay — HUD col 19 toggles `A` ↔ `F`. No other
   state change (cursor and entities are unaffected).
2. Cycle to F, place a firewall on a buildable tile — distinct
   brick-pattern BG tile appears; energy drops by 15. The firewall
   reaches further (5-tile = 40 px range) and fires more slowly than
   AV; a single hit deals 3 damage (kills a bug in one shot).

### Robot enemy (#10)
1. Reach W3 — robots appear among the bugs. They are visually distinct:
   vertical humanoid silhouette with antenna spike, vs. the bug's
   horizontal centipede outline.
2. Robots traverse the path noticeably faster (1.5× bug speed).
3. Killing a robot awards 5 energy (vs. 3 for a bug) — verify HUD `E:`
   ticks accordingly.

### Passive income (#13)
1. From a known energy snapshot, idle for 180 frames (3 s in mGBA frame
   counter): energy increments by 1.
2. Idle 1080 frames: energy increments by exactly 6.

### Full 10-wave playthrough
1. `just run`, START. HUD reads `HP:5 E:030 W:01/10 A`.
2. Survive each wave; verify HUD wave counter advances `W:01/10` …
   `W:10/10` in order. W3 is the first to mix robots; W10 is the
   densest mix (28 enemies, 16 bugs + 12 robots).
3. After W10's last enemy is killed or reaches the computer, the win
   or lose screen appears with its corresponding SFX.
4. From either gameover screen, press START — game resets to title;
   then START again to play; HP/energy/wave/tower-pool/menu state and
   selected tower type all reset to defaults. No stale menu sprites
   linger on the title screen even if the menu was open at the moment
   the lose state triggered.
