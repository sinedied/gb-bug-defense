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
Run with audio enabled: `just run` (passes `-C mute=0 -C volume=0x100`
so Qt mGBA's mute toggle and saved-volume can't silence the run).
With the emulator's audio panel open, every action below must produce
its blip:

0. **Boot chime** — the moment the ROM starts (before the title screen
   logic settles), a single ~200 ms square tone plays on ch2
   (`SFX_BOOT`, fired from `audio_init`). If you don't hear this, the
   ROM is fine; the issue is host-side (Qt mGBA `Audio > Mute` enabled,
   no audio backend selected in `Tools > Settings > Audio`, or system
   output muted). See README "Audio".
1. Place a tower (A on a buildable tile) — clean ~265 ms low beep on
   ch1 (`SFX_TOWER_PLACE`). Same blip plays on upgrade and sell.
   (NB: pre-fix this was a near-inaudible pop because NR10 sweep killed
   the channel within ~30 ms; sweep is now disabled and duration is
   16 frames.)
2. Idle while a tower fires at a bug — periodic mid-square blip on ch2
   (`SFX_TOWER_FIRE`).
3. A projectile hits a robot (does not kill) — short noise click on ch4
   (`SFX_ENEMY_HIT`).
4. A projectile kills any enemy — longer noise rumble on ch4
   (`SFX_ENEMY_DEATH`); priority preempts a same-frame `SFX_ENEMY_HIT`.
5. Survive all 10 waves — `MUS_WIN` victory stinger on ch3+ch4 (replaces
   the iter-2 `SFX_WIN` ch1 jingle, removed in iter-3 #16 / D-MUS-3).
6. Let HP reach 0 — `MUS_LOSE` defeat stinger on ch3+ch4 (replaces the
   iter-2 `SFX_LOSE` ch1 jingle).
7. mGBA `Tools → View channels`: ch1/ch2 stay SFX-only (place/fire/boot);
   ch3 (wave) is owned by music; ch4 (noise) is shared (music percussion +
   SFX_ENEMY_HIT/DEATH preempt).

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

### Pause menu (# iter-3)22 
See `specs/iter3-22-pause.md` for the full scenario 9). Thelist (
checks below cover behaviors that no host harness can verify (modal
precedence, same-frame ordering, held-button edge detection, no-SFX
bleed, full reset on QUIT, OAM positions on real OAM).

1. **Open / resume ( From PLAYING (W1+), press START. Withinhappy)** 
   1 frame: `PAUSE` header + `>RESUME` + `QUIT` overlay appears at
   screen `(48, 64)`. Enemies/projectiles freeze (no flicker). The
   placement cursor disappears (OAM 0 moved to `(0,0)`). Press START,
 overlay disappears; entities resume; cursor reappears.
2. **Selection  Open pause, press DOWN: chevron snaps to QUITwrap** 
   row (y=96). DOWN again: wraps to RESUME (y=88). UP from RESUME:
   wraps to QUIT.
3. **Quit-to-title (full  Open pause, DOWN, A. Title screenreset)** 
   appears with no leftover sprites (no enemy/projectile/menu/pause
 new game with HP=5, E=030, W:01/10.
 the
   upgrade/sell menu opens. Press START. Upgrade/sell menu remains
   open and unchanged; pause does NOT open. (Tests the
   `if (menu_is_open())` arm running before the START handler.)
5. **Gameover wins over pause ( Place no towers; on theLOSE)** 
   frame the first bug enters the computer tile, mash START. LOSE
   screen appears; pause overlay never visible. Approximate timing
   acceptable.
6. **Gameover wins over pause ( Reach W10; as the last enemyWIN)** 
   dies, mash START. WIN screen appears; pause overlay never visible.
   *This is the regression test for the iter-2 missing `return;` after
   `enter_gameover( without the fix, pause glyphs would painttrue);` 
   over the WIN screen.*
7. **START on TITLE / WIN /  START at title still starts a newLOSE** 
   game; START at win/lose still returns to title. Pause is local to
   GS_PLAYING.
8. **Held START across  From paused, press A (resume). Keepresume** 
   holding START for 1 s. Pause does NOT immediately re-open. Release
 pause opens. (Edge-detection sanity.)
9. **No SFX  While paused, no tower-fire, enemy-hit, orbleed** 
   enemy-death SFX. Any in-flight stinger from before pause drains
   naturally over its remaining frames.
10. **Passive income during  Open pause, note E. Wait ~4 s.pause** 
    Resume. E increased by ~1 per 180 frames (`economy_tick` runs
    during pause; entity ticks do not).
  While paused, slots 1..16OAM)** 
    sit at the screen pixels documented in 4. Slot 0 (cursor)spec 
    and slots 17..38 are at `(0, 0)`.

## Iter-3 # animated tiles & sprite polish21 

Spec: `specs/iter3-21-animations.md`. Run `just check`, then `just run`,
press START, and walk through the scenarios below. Each maps to one of
5 acceptance scenarios.the 

1. ** pristine (state  Start a new game; observe the0)** Corruption 
   computer cluster (top-right of the play field) before any enemy
   reaches it. All 4 quadrants render the iter-2 pristine art.

 Place no towers;4)** 
   let bugs reach the computer one at a time. After each impact the
   cluster transitions: HP4 = single stuck pixel (TL only), HP3 =
   horizontal scanline tear across TL+TR, HP2 = diagonal cracks +
   "dead pixels" across all 4 quadrants, HP1 = heavy static (the
   existing `_D` art). Each transition lands on the same frame the
   HP digit drops (no visible lag).

3. **Corruption  After scenario 2, idle for ~2 s. Thepersists** 
   cluster stays in the new state with no flicker / no repaint.

4. **HP=0 (single-bug  From HP=1 (state 4 visible), letarrival)** 
   one more bug through. Game-over screen appears; the last play-field
   frame shows heavy-static. (Documented limitation: a same-frame
0 may skip from state 3 directly to gameover 
3.2/D18.)   

5. **Hit  bug, non- Place an AV tower (damage 1) sokilling** flash 
   bugs (HP 3) take 2 hits before dying. The bug sprite visibly flips
   to the "ghost outline" `SPR_BUG_FLASH` for ~3 frames after the
   first hit, then resumes its A/B walk anim.

6. **Hit   Reach W2; place AV+FW. Same flash on robotrobot** flash 
   sprites for partial-damage hits.

7. **No flash on killing  Watch a bug die from a hit. Spritehit** 
   disappears immediately with no inverted-ghost frame.

8. **Flash does not desync walk  Multi-hit a robot 3Anim over a** 
   path traversal. Flash frames render and clear cleanly; the A/B walk
   cycle resumes between flashes without obvious skipping.

9. **Tower idle  single  Place one AV tower; idle 60tower** blink 
   frames. The central LED pixel toggles dark/light ~once per second
   (32-frame ON, 32-frame OFF).

10. **Tower  4 towers anti- Place an AV tower (slot 0,phase** idle 
    -10E), another adjacent (slot 1, -10E); kill 1 bug to refund 3E
    and let 1E/3 s) tick 20E; place slots 2 + 3.to income (
    Observe ~4 s. LEDs are visibly out-of-phase: slot 0 vs slot 1
    are exactly anti-phase (one ON while the other OFF, swapping
    every  0.5 s) per the bitrev4 stagger formula.frames 

11. **Tower idle freezes during  Place a tower, press STARTpause** 
    to pause, idle 4 s. LED frozen, no BG flicker; resume restores
    blinking from the same phase-stride.

12. **Tower idle freezes during upgrade  Place a tower; pressmenu** 
    A on it. LED frozen for the duration the menu is open.

13. **Idle does not break tower place/ Open upgrade menu, SELLsell** 
    the tower. Tile clears within 1-2 frames (sell-clear preempts
    idle); no leftover LED pixel painted afterwards.

14. **All-pool  Reach W8+, fill the tower pool untilstress** 
    `towers_try_place` returns false (16 placements). Idle 10 s. All
    16 LEDs blink, the pattern shifts each 32-frame half-period, no
    HUD glitches, no visible BG-write tearing.

15. **HUD + corruption + idle simultaneous ( Best-exploratory)** 
    effort: place a tower near the path during W3 with HP=2 so the
    next hit drops HUD HP digit + paints corruption + scans idle on
    the same frame. Pass = "no visible artefact in 30 s of play".
    Non-gating.

16. **Build &  `just check` from a clean tree builds, all 5cap** 
    host tests pass,  32 KB, cart type 0x00.ROM

17. **Hit-flash audio/visual sync (F1 regression check)** —
    During any wave, allow a tower to hit (but not kill) an enemy.
    Pass = the white flash sprite is visible on the SAME frame as
    the SFX_ENEMY_HIT chirp — no perceptible audio-before-visual
    lag. A regression where the audio plays one frame before the
    flash appears indicates `enemies_set_flash()` is no longer
    writing the flash tile immediately (visual would otherwise be
    delayed by one frame because `enemies_update()` runs before
    `projectiles_update()` in `playing_update()`). Not host-testable:
    update ordering plus PPU/audio timing make this a manual-only
    check. 

## Iter-3 # Difficulty modes (EASY / NORMAL / HARD)20 

Setup: `just build && just run`. mGBA controls: arrows = D-pad,
`Z` = A, `X` = B, `Enter` = START.

| #  | Scenario                            | Steps                                                                                                                | Expected                                                                                                |
|----|-------------------------------------|----------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------|
| D1 | Default on cold boot                | Power on → title.                                                                                                    | Selector shows `< NORMAL >` centered on row 10. PRESS START blinks below.                               |
| D2 | Cycle right with wrap               | RIGHT, RIGHT, RIGHT.                                                                                                 | After 1st `< HARD >`; after 2nd wraps to `< EASY >`; after 3rd `< NORMAL >`.                            |
| D3 | Cycle left with wrap                | From default NORMAL: LEFT, LEFT, LEFT.                                                                               | After 1st `< EASY >`; after 2nd wraps to `< HARD >`; after 3rd `< NORMAL >`.                            |
| D4 | A / B inert on title                | Press A; press B.                                                                                                    | No visual change; selector stays put; no state transition.                                              |
| D5 | START commits — starting energy     | Set EASY → press START.                                                                                              | Game enters PLAYING. HUD shows `E:045` immediately.                                                     |
| D6 | HARD spawn faster + lower wallet    | Title → HARD → START → wait through W1 grace, place 1 antivirus on a path-adjacent tile, then watch W1 spawns.       | HUD `E:024` right after START. Inter-spawn gap visibly tighter than NORMAL (~38 f vs 50 f over 5 bugs). |
| D7 | EASY HUD energy                     | Title → EASY → START.                                                                                                | HUD `E:045` immediately after START.                                                                    |
| D8 | Quit-to-title preserves selection   | Title → HARD → START → pause → QUIT → back at title.                                                                 | Selector still `< HARD >` (not reset to NORMAL).                                                        |
| D9 | Game-over preserves selection       | Title → EASY → START → lose deliberately → press START on lose screen.                                               | Back at title; selector still `< EASY >`.                                                               |
| D10| Win-over preserves selection        | Title → NORMAL → win all 10 waves → press START on win screen.                                                       | Back at title; selector still `< NORMAL >`.                                                             |
| D11| Power-off resets                    | Power off mGBA, power on.                                                                                            | Selector defaults to `< NORMAL >` (no SRAM).                                                            |
| D12| Pause works on every difficulty     | Repeat D5 / D6 / D7 with one pause+resume during gameplay.                                                           | Pause modal opens/closes identically. No SFX or visual difference between modes.                        |
| D13| Visual / audio parity               | Compare W1 on EASY vs HARD.                                                                                          | Same bug sprites, same audio cues, same HUD layout. Only HP, spawn timing, and starting `E:` differ.    |
| D14| EASY beatable, HARD beatable        | Play through all 10 waves on EASY (casual) and HARD (focused).                                                       | Both runs reach the WIN screen.                                                                         |

**Note on HARD late waves**: by design, HARD wave 10's scaled spawn delay
 37 frames) outpaces path traversal so the `MAX_ENEMIES = 14` pool
saturates. `waves.c` falls back to retry every 8 frames (existing iter-2
back-pressure). Expected, not a regression.

### Iter-3 #20 — F1 (MEDIUM) regression: title VBlank BG-write budget

On the title screen, hold LEFT or RIGHT briefly (about 2 seconds) then
release. The selector and PRESS START prompt areas must remain pristine
— no garbled tiles — regardless of the timing of releases relative to
the 30-frame blink edge.

**Automated:** none. Frame-ordering / VBlank PPU timing is not observable
from a host harness. Pure-helper extraction was not warranted (the fix is
a single early `return;`).

**Manual on-device check (mGBA / real DMG):**
1. Boot to title; selector reads `< NORMAL >`, PRESS START blinks.
2. Hold LEFT for ~2 s, then release. While held, the selector cycles
   continuously through EASY/HARD/NORMAL.
3. Repeat with RIGHT held for ~2 s.
4. Pass = the difficulty selector row (row 10, cols 5..14) AND the
   PRESS START prompt row (row 13, cols 4..15) remain pristine — no
   garbled / half-drawn / "stuck blank" tiles, regardless of where the
   30-frame blink edge lands relative to the input. Pre-fix, the same
   action would intermittently corrupt ~6 tiles per second of holding
   because `title_render` emitted 22 BG writes (10 selector + 12 prompt)
   in a single VBlank, past the ~16-write budget.
5. Press START — gameplay enters cleanly; HUD digits render without
   tearing (sanity check that the title-side fix did not regress the
   transition).

### Music (#16) - iter-3
Setup: launch with `just run` (mGBA `-C mute=0 -C volume=0x100`). Audio
unmuted on host.

1. **Boot chime + title theme** - Power on. Hear the ~200 ms `SFX_BOOT`
   ch2 chime, IMMEDIATELY followed by `MUS_TITLE` looping on ch3+ch4
   (calm arpeggio, ~9.6 s loop). No silence gap > 1 s between them.
2. **Title -> Playing transition** - Press START on title. `MUS_TITLE`
   stops; `MUS_PLAYING` begins within 1 frame (driving 16-row loop on
   ch3 with kick/snare/hat on ch4). No stuck wave note (a single click
   is acceptable).
3. **In-game SFX over music** - Place a tower (ch1 SFX_TOWER_PLACE),
   fire (ch2 SFX_TOWER_FIRE), let an enemy take damage (ch4
   SFX_ENEMY_HIT/DEATH). All play AUDIBLY OVER the ch3 melody. The ch4
   percussion line goes momentarily silent during ENEMY_HIT/DEATH and
   resumes at the next row boundary (no double-trigger click).
4. **Win stinger** - Survive all waves. `MUS_PLAYING` stops; `MUS_WIN`
   plays once (~1.5 s ascending C-E-G-C with kick/snare); silence
   afterwards (no loop).
5. **Lose stinger** - Let HP reach 0. `MUS_PLAYING` stops; `MUS_LOSE`
   plays once (~1.6 s descending C-A-F-D-C with kick); silence after.
6. **Pause ducking** - During `MUS_PLAYING`, press START. NR50 drops
   from 0x77 to 0x33; music + any subsequent SFX are audibly quieter
   (~half loudness). Closing pause (A on RESUME) restores full volume.
7. **Quit-to-title restores volume** - During `MUS_PLAYING`, START to
   pause, DOWN, A on QUIT. Returns to title with `MUS_TITLE` at full
   NR50=0x77 volume.
8. **Loop point** - Sit on title screen for 60 s. `MUS_TITLE` loops
   seamlessly. No audible discontinuity at the row 23 -> row 0
   boundary (the snare hit on row 23 telegraphs the loop).
9. **Idempotent play** - Internal: re-entering an already-active state
   does not restart `MUS_TITLE` row 0. Verified by host test_music
   case (e); manually not directly observable.
10. **Upgrade/sell menu does NOT duck** - Open the upgrade menu on a
    placed tower. Volume stays at full 0x77 (D-MUS-4). Closing the
    menu also leaves volume unchanged.
11. **mGBA channel viewer** - `Tools -> View channels`. CH3 (wave)
    is active whenever music is playing; CH4 (noise) is active mostly
    continuously (alternating music percussion and SFX). CH1/CH2 are
    only active for SFX events.

## Music engine: F1 / F2 cross-cutting smoke (MANUAL-REQUIRED)

These two paths are not host-testable (they require simultaneous
SFX playback, music transitions and NRxx state inspection on real
DMG / mGBA). Run them after any change to `src/music.c` or
`src/audio.c`.

### F1: song switch preserves SFX-owned CH4

1. Start a wave; let an enemy reach low HP.
2. Trigger the killing shot so `SFX_ENEMY_DEATH` (8 frames on CH4)
   fires on the LAST enemy of the wave (so the win transition runs
   while the death SFX is still audible).
3. Expected:
   - The death SFX completes  no truncation.naturally 
   - `MUS_WIN`'s CH3 melody starts immediately on the win transition.
   - `MUS_WIN`'s CH4 percussion starts at the first row boundary
     after the death SFX 1 tick of percussion silence betweenends (
     SFX end and music ch4 re- F2 latch).arm 
4. Failure mode (regression):
   - Death SFX cut short the moment the transition fires (F1 broken),
     or audible click on the SFX-end frame (F2 broken).

### F2: simultaneous SFX-end + music row boundary

1. Start a long wave to maximize CH4 SFX activity.
2. Listen for any CH4 click/pop coinciding with a percussion hit
   immediately after `SFX_ENEMY_HIT` (3 frames) or `SFX_ENEMY_DEATH`
   (8 frames) ends.
3. Expected: no  there is always at least one tick of silenceclick 
   between the SFX envelope ending and the next music percussion
   trigger.
