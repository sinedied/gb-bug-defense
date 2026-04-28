# Iteration 3 — Feature #18: Third Enemy + Third Tower

## Problem
The roadmap (#18) calls for a third enemy and a third tower to expand the tactical
vocabulary of the game: an **ARMORED bug** (high-HP "tank") and an **EMP tower**
that stuns instead of damaging. With only one offensive answer (damage) the player
has no way to *control* the battlefield; armored bugs make raw DPS towers feel
inadequate, and EMP gives a counter-mechanic (crowd control) that is uniquely
useful against fast/tanky targets. Together they unlock the first non-trivial
build decisions.

## Architecture

### Components touched
- **`src/enemies.{h,c}`** — new `ENEMY_ARMORED` row in stats table; new
  *private* `enemy_t.stun_timer` field (1 B) — `enemy_t` stays private to
  `enemies.c`. New public APIs in `enemies.h`:
  `bool enemies_try_stun(u8 idx, u8 frames)` (returns `true` if the
  enemy was alive and not already stunned, then sets the timer; `false`
  otherwise) and `bool enemies_is_stunned(u8 idx)` (used by host tests).
  Movement gated internally by `stun_timer == 0`; sprite tile selection
  priority **flash > stun > walk**.
- **`src/towers.{h,c}`** — new `TOWER_EMP` row in stats table; `tower_stats_t`
  gains a `kind` discriminator (`TKIND_DAMAGE` vs `TKIND_STUN`), a
  `bg_tile_alt` field (idle-blink tile per type), and a
  `stun_frames`/`stun_frames_l1` pair (D-IT3-18-1).
  `towers_update()` dispatches on `kind`: damage towers fire
  projectiles unchanged; stun towers iterate alive enemies and call
  `enemies_try_stun()` for each one in range (no projectile, no direct
  field access).
- **`src/projectiles.{h,c}`** — **unchanged**. EMP does not spawn projectiles.
- **`src/waves.{h,c}`** — `enemy_id_t` / `B`/`R` macros gain `A` for armored;
  waves 7–10 get a small armored sprinkle (no change to W1–W6).
- **`src/tuning.h`** — `ARMORED_*` constants and `TOWER_EMP_*` constants.
- **`src/difficulty_calc.h`** — `DIFF_ENEMY_HP` expands `[3][2] → [3][3]`;
  the bounds-clamp in `difficulty_enemy_hp()` updates from `>= 2` to `>= 3`.
- **`src/game.c`** — change `cycle_tower_type()` from XOR toggle to
  `(s + 1) % TOWER_TYPE_COUNT` so the cycle naturally extends to 3 (and any
  future `N`). Two-line diff.
- **`src/hud.c`** — no code change; the col-19 letter is read from
  `tower_stats[type].hud_letter`. EMP letter `'E'` already in the BG font.
- **`src/menu.c`** — no text change; the "UPG" / "SEL" labels are tower-
  agnostic and stay as-is. (See D-IT3-18-3 for why we don't add a "STUN+"
  label — keeps glyph budget untouched and screen layout identical.)
- **`src/audio.{h,c}`** — new `SFX_EMP_FIRE` on CH1, prio=2, descending
  square sweep ~8 frames (templated off `SFX_TOWER_PLACE`).
- **`tools/gen_assets.py`** — adds 3 sprite tiles (ARMORED A/B + flash),
  3 stun-overlay sprite tiles (one per enemy type), and 2 BG tiles
  (`TILE_TOWER_3`, `TILE_TOWER_3_B` for EMP idle blink).

### Stun mechanism (data flow)
```
EMP tower (cooldown==0)               step_enemy() each frame
   └─> for each alive enemy in range     ├─ if flash_timer > 0  → use FLASH tile (priority 1)
        call enemies_try_stun(idx, N):   ├─ else if stun_timer  → use STUN tile (priority 2)
          (sets timer if not already)    │     skip movement
   └─> if any returned true:             └─ else                → walk + advance
        audio_play(SFX_EMP_FIRE)            always: stun_timer-- (if > 0)
        cooldown = 120
```
**Modal gating**: enemy and tower update loops are already gated by
`playing_update()` so stun_timer naturally freezes during pause/menu.
**Three-read-site rule** for difficulty unaffected (still bug/robot/armored
HP read at one site in `enemies.c`).

### Why this architecture
- Keeping projectiles untouched preserves the bounty-capture invariant and
  the OAM 31–38 budget exactly as shipped.
- Putting the damage-vs-stun discriminator in tower stats (not in
  `tower_t`) keeps per-tower WRAM unchanged.
- Using `stun_timer` as a direct write from `towers_update()` (no projectile
  travel time) makes the stun *feel instant and AoE-y* and is the cheapest
  implementation. The visual feedback budget is spent on the per-enemy stun
  sprite, not on a projectile.

## Subtasks

1. ✅ **Sprite & tile assets (designer + coder)** — In `tools/gen_assets.py`
   add `SPR_ARMORED_A`, `SPR_ARMORED_B`, `SPR_ARMORED_FLASH`,
   `SPR_BUG_STUN`, `SPR_ROBOT_STUN`, `SPR_ARMORED_STUN`, `TILE_TOWER_3`,
   `TILE_TOWER_3_B`. Update `MAP_TILE_COUNT` to 23 and `SPRITE_TILE_COUNT` to
   43. Regenerate `res/assets.h`. *Done when* `just build` succeeds and the
   new IDs appear in `res/assets.h` with the correct base offsets. Pixel
   designs delegated to designer (see Design section below).
2. ✅ **Tuning constants** — Append to `src/tuning.h`:
   `ARMORED_HP=12`, `ARMORED_SPEED=0x0040`, `ARMORED_BOUNTY=8`;
   `TOWER_EMP_COST=18`, `TOWER_EMP_UPG_COST=12`,
   `TOWER_EMP_COOLDOWN=120`, `TOWER_EMP_COOLDOWN_L1=120`,
   `TOWER_EMP_STUN=60`, `TOWER_EMP_STUN_L1=90`, `TOWER_EMP_RANGE_PX=16`.
   *Done when* the file compiles host-side.
3. ✅ **Difficulty HP table extension** — In `src/difficulty_calc.h` change
   `DIFF_ENEMY_HP[3][2]` → `DIFF_ENEMY_HP[3][3]`; values
   `EASY={2,4,8}`, `NORMAL={BUG_HP, ROBOT_HP, ARMORED_HP}`,
   `HARD={5,9,16}`. Update the clamp in `difficulty_enemy_hp()`:
   `if (enemy_type >= 3) enemy_type = 0;`. *Done when*
   `tests/test_difficulty.c` is extended with 3 new asserts (one per
   difficulty for armored) and passes.
4. ✅ **Enemy type plumbing + stun API** — In `src/gtypes.h` add
   `ENEMY_ARMORED = 2; ENEMY_TYPE_COUNT = 3`. In `src/enemies.c` add
   the third row to `s_enemy_stats[]` and the **private** `u8
   stun_timer` field to `enemy_t`; zero it in `enemies_spawn()`.
   `enemy_t` stays in `enemies.c` (not in `enemies.h`). In `enemies.h`
   declare two new public functions:
   - `bool enemies_try_stun(u8 idx, u8 frames);` — returns `true` and
     sets the timer iff the enemy is alive and `stun_timer == 0`,
     `false` otherwise (already-stunned or dead are both no-ops).
   - `bool enemies_is_stunned(u8 idx);` — read-only accessor (used by
     host tests).
   In `step_enemy()`:
   - decrement `stun_timer` if non-zero (always, consistent with
     `flash_timer`);
   - skip the position-advance block when `stun_timer > 0`;
   - extend the tile-pick ternary to:
     `flash → SPR_*_FLASH; else if stun_timer → SPR_*_STUN; else walk`.
   The `e->anim` counter only increments on the walk branch (existing
   behavior), so it cleanly resumes phase after a stun. *Done when*
   armored bug spawns/walks/takes damage with flash, the stun API
   works (verified via `test_enemies_stun`).
5. ✅ **Tower kind discriminator + EMP type** — In `src/gtypes.h` add
   `TOWER_EMP=2; TOWER_TYPE_COUNT=3`. In `src/towers.h` extend
   `tower_stats_t` with `u8 kind` (enum `TKIND_DAMAGE=0, TKIND_STUN=1`),
   `u8 bg_tile_alt` (the idle-blink tile for this type), and
   `u8 stun_frames, stun_frames_l1` (only meaningful when
   `kind==TKIND_STUN`). Populate `bg_tile_alt` for **all three** rows:
   AV→`TILE_TOWER_B`, FW→`TILE_TOWER_2_B`, EMP→`TILE_TOWER_3_B`. In
   `src/towers.c::towers_render()` Phase 3 replace the hardcoded
   `(type==TOWER_AV) ? TILE_TOWER_B : TILE_TOWER_2_B` ternary with a
   single read of `s_tower_stats[type].bg_tile_alt` (so EMP blinks with
   its own alt tile, not the firewall's). Add the EMP row using
   `TILE_TOWER_3` and `hud_letter='E'`. In `towers_try_place()`,
   special-case EMP: after setting `cooldown = stats.cooldown`, override
   to `cooldown = 0` when `kind == TKIND_STUN` so a freshly-placed EMP
   stuns immediately if a target is already in range (consistent with
   D-IT3-18-7). In `towers_update()`, after the cooldown decrement and
   before the existing fire branch, dispatch on `kind`:
   - `TKIND_DAMAGE`: existing path (acquire nearest, `projectiles_fire`).
   - `TKIND_STUN`: scan **all** alive enemies; for each one in range
     call `enemies_try_stun(i, level ? stun_frames_l1 : stun_frames)`
     and OR the result into a local `bool any_stunned`. After the loop,
     if `any_stunned`, play `SFX_EMP_FIRE` (once) and reset
     `cooldown = TOWER_EMP_COOLDOWN`. Otherwise leave cooldown at 0
     so the tower keeps polling each frame.
   *Done when* placing an EMP and walking an enemy past it visibly freezes
   the enemy and plays the SFX; host test `test_towers_emp_scan` passes.
6. ✅ **Tower-select cycle to 3** — Add a new pure header
   `src/tower_select.h` (`<stdint.h>`-only, like `difficulty_calc.h`)
   exposing `static inline uint8_t tower_select_next(uint8_t cur,
   uint8_t count) { return (uint8_t)((cur + 1u) % count); }`. In
   `src/game.c::cycle_tower_type()` replace the `s_selected_type ^= 1;`
   line with `s_selected_type = tower_select_next(s_selected_type,
   TOWER_TYPE_COUNT);` (include the new header). `towers_try_place()`
   already validates `type < TOWER_TYPE_COUNT`. *Done when* B-button
   cycles AV→FW→EMP→AV in the running ROM; the host test in subtask 9
   covers the math via the new header.
7. ✅ **SFX_EMP_FIRE** — In `src/audio.h` add the enum entry. In
   `src/audio.c` add the `sfx_def_t`: `.channel=1, .priority=2,
   .nrx1=0x80, .envelope=0xF1, .duration=8, .sweep=0x73,
   .pitch=0x07A0` (descending sweep, distinct from PLACE which is
   .duration=16). Host audio test extends with one assert that the new
   index has channel=1.
8. ✅ **Wave script extension** — In `src/waves.c` add `#define A
   ENEMY_ARMORED`. Append/replace the W7–W10 event arrays:
   - **W7**: existing 14 events + 1 trailing `{A,90}` → count=15.
   - **W8**: existing 20 + 1 trailing `{A,90}` → count=21.
   - **W9**: existing 20 + 2 trailing `{A,80},{A,80}` → count=22.
   - **W10**: existing 28 + 3 trailing `{A,75},{A,75},{A,75}` → count=31.
   W1–W6 unchanged. Verify byte values: `ENEMY_ARMORED=2`, fits in u8.
   *Done when* armored sprites visibly appear in W7+ in the ROM.
9. ✅ **Host tests** — Add or extend:
   - `tests/test_difficulty.c` — armored HP at 3 difficulties.
   - `tests/test_enemies.c` (new) — compiles `src/enemies.c` against
     `tests/stubs/gb/`. Asserts: `enemies_try_stun` returns true on
     fresh enemy and false on already-stunned/dead; `enemies_is_stunned`
     reads back correctly; after N calls to `step_enemy` the timer
     reaches 0 and movement resumes; `flash_timer > 0` overrides the
     stun tile (priority test).
   - `tests/test_towers.c` (new) — compiles `src/towers.c` with
     stubbed `enemies_*` (record `try_stun` calls). Asserts: EMP scan
     calls `enemies_try_stun` for every alive enemy in range; ignores
     out-of-range; cooldown is **not** reset when zero stuns succeeded;
     cooldown **is** reset to 120 when ≥1 succeeded; SFX queued only on
     success; freshly-placed EMP has cooldown=0.
   - `tests/test_math.c` — include `src/tuning.h`, `src/tower_select.h`,
     and `src/gtypes.h`. Assert `TOWER_TYPE_COUNT==3`,
     `ENEMY_TYPE_COUNT==3`, `tower_select_next(0,3)==1`,
     `tower_select_next(1,3)==2`, `tower_select_next(2,3)==0`.
   Wire all new test binaries into `justfile`'s `test` recipe.
   *Done when* `just test` is green.
10. ✅ **Manual QA scenarios** — Append the scenarios in the Acceptance
    section below to `tests/manual.md` (or `qa/manual.md` per existing
    convention — match whatever already exists).

## Design

> Pixel art delegated to designer (Murdock). Constraints below are
> non-negotiable; visual choices within them are designer-owned.

- **`SPR_ARMORED_A` / `_B`** (8×8): visually heavier than BUG, distinct
  silhouette from ROBOT. Suggested motif: bug body with crosshatched
  metal carapace; A/B differ by leg position (same 32-frame walk cadence
  as bug/robot, `e->anim >> 4 & 1`).
- **`SPR_ARMORED_FLASH`** (8×8): inverted-palette variant of `_A` (matches
  iter-3 #21 convention for `SPR_BUG_FLASH` / `SPR_ROBOT_FLASH`).
- **`SPR_BUG_STUN` / `SPR_ROBOT_STUN` / `SPR_ARMORED_STUN`** (8×8 each):
  the enemy with a small lightning/`Z` overlay or visible "shock" lines.
  Must read as *frozen* at a glance from 3 tiles away on the DMG screen.
  Same outline as the walk-A frame so the silhouette is recognisable.
- **`TILE_TOWER_3`** (8×8 BG): EMP tower base. Distinct from
  `TILE_TOWER` (AV, satellite-dish motif) and `TILE_TOWER_2` (FW, brick
  motif). Suggested: capacitor / Tesla-coil motif with a central dot.
- **`TILE_TOWER_3_B`** (8×8 BG): idle blink variant of `TILE_TOWER_3` —
  the central dot lit / unlit, matching the iter-3 #21 LED-blink pattern
  (single-pixel diff so the eye reads it as "active").
- **HUD letter**: `'E'` in the BG font, written to col 19 by existing
  `hud.c` code. The energy label is `E:` at cols 5–6 — the lone `'E'` at
  col 19 is unambiguous in context (separated by 12 chars and the
  trailing space at col 18).

## Acceptance Scenarios

### Setup
1. `just build` produces `build/gbtd.gb` ≤ 32 768 bytes.
2. `just test` is green.
3. `just run` launches mGBA with the ROM. NORMAL difficulty default
   (energy 30, HP 5).
4. **Survival baseline** for any scenario that requires reaching wave N:
   place 1× AV-L0 at the first path bend on frame 0 (cost 10, leaves
   20 energy). This kills W1–W2 reliably; passive income + W3 bounty
   funds a second AV before W4. Every "play to W*" scenario assumes this
   baseline unless it says otherwise.
5. **EMP-test loadout** (used by scenarios 3–8, 11–12): start as in (4),
   add a second AV before W3 to keep clearing damage, then place the
   EMP under test in the position the scenario specifies. EMP itself
   does no damage, so it must always be paired with at least one
   damage tower.
6. Scenarios are independent unless explicitly chained — restart the
   ROM (Select+Start soft reset, or close/reopen mGBA) between them.

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | **Tower cycle wraps to 3** | New game. On the play screen, press B three times. Watch HUD col 19. | Letter cycles `A → F → E → A`. |
| 2 | **Place EMP, no error** | New game. Cycle to `E`. Move cursor to a buildable tile. Press A. | EMP tower BG tile appears; energy decreases by 18. `SFX_TOWER_PLACE` plays. |
| 3 | **EMP stuns enemies in range** | Apply Setup (5). At W3 spawn, place EMP within 16 px of the path. Watch a bug walk into range. | Bug visibly **stops** moving for ~60 frames (~1 s), shows `SPR_BUG_STUN` tile, then resumes walking. `SFX_EMP_FIRE` plays at the moment of stun. |
| 4 | **EMP is AoE** | Apply Setup (5). At W6, place EMP at a path bend so 2+ enemies are within 16 px when it fires. | All enemies within 16 px stun simultaneously on the same frame. |
| 5 | **EMP cooldown** | Continuation of scenario 3 (do not reset). After a successful pulse, watch the same EMP. | The same enemy is **not** re-stunned mid-stun; after its `stun_timer` expires it walks; the EMP fires again only after 120 frames have elapsed since the previous successful pulse. |
| 6 | **Freshly-placed EMP stuns immediately** | Apply Setup (5). Pause when a bug is at a known tile. Unpause and immediately place EMP adjacent so the bug is within 16 px. | The bug stuns on the **same** frame the tower appears (no 120-frame placement delay). After the pulse, normal 120-frame cooldown applies. If placed with an empty range, cooldown stays at 0 and the next entering enemy stuns immediately. |
| 7 | **Upgrade EMP → longer stun** | Continuation of scenario 3. Open menu on the EMP, choose UPG (cost 12). Watch the next pulse. | Stun visibly lasts ~50% longer (90 frames vs 60). Already-stunned enemies are not retroactively extended. |
| 8 | **Sell EMP refund** | Continuation of scenario 2 (L0 EMP, spent=18). Open menu, choose SEL. | Tile clears; energy increases by 9 (= 18/2). Any enemy currently stunned by that EMP keeps its existing `stun_timer` (timer is on the enemy, not the tower) and finishes the freeze normally. |
| 9 | **Armored bug appears in W7+** | Apply Setup (4) plus extra towers as needed. Play through to W7. | Final spawn of W7 is an `ENEMY_ARMORED` (visibly heavier sprite, slower walk). |
| 10 | **Armored HP scaling** | For each difficulty in {EASY, NORMAL, HARD}: restart, place **exactly one AV-L0** at a position where it is the only tower in range of the armored bug's path; play to W7; count shots to kill. | EASY: 8 shots. NORMAL: 12 shots. HARD: 16 shots. |
| 11 | **Flash > stun visual** | Continuation of scenario 4 (≥1 stunned enemy on screen). While the enemy is stunned, hit it with a tower projectile. | For 3 frames the FLASH tile shows; then it reverts to the STUN tile (stun_timer keeps counting under the flash). |
| 12 | **Pause freezes stun_timer** | Continuation of scenario 3. Stun an enemy, press START to pause for 5 s, unpause. | Enemy is still stunned for the same number of remaining frames as when paused (i.e. countdown didn't progress). |
| 13 | **Existing waves unchanged** | New game on NORMAL with Setup (4). Record W1–W6 spawn timestamps and counts (W1=5 bugs @90f; W2=8 bugs @75f; W3=7 mixed @75f; W4=12 @60f; W5=12 @60f; W6=16 @50f). | Spawn counts and inter-spawn delays match the values listed exactly. No `ENEMY_ARMORED` (sprite ID 2) appears. |
| 14 | **Host tests** | `just test`. | All test binaries pass, including the 3 new tests. |

## Edge cases (documented behavior)

- **14 enemies all stunned simultaneously**: AoE stun iterates the
  fixed 14-slot pool; no allocation, no overflow. SFX fires once per
  pulse regardless of count.
- **Selling an EMP while a stun is in progress**: the `stun_timer`
  lives on the *enemy*, not on the tower. Already-stunned enemies
  finish their freeze normally (asserted by scenario 8).
- **Upgrading EMP while an enemy is mid-stun**: the upgrade only
  affects *future* pulses (the upgraded `stun_frames_l1` is read at
  the next `enemies_try_stun` call). Already-set timers are not
  retroactively extended (asserted by scenario 7).
- **Multiple EMPs covering the same tile**: because
  `enemies_try_stun` returns false on already-stunned enemies, a
  second EMP whose range overlaps a first will *not* extend the stun.
  Stacking EMPs is therefore wasteful for the *same* enemy — the
  intended use is geographic coverage (different path segments). This
  is documented behavior, not a bug; an alternative ("extend on
  re-stun") was rejected because it would let a player perpetually
  freeze any enemy with 2 EMPs and trivialize tanks.


- ≤ 32 KB ROM (no MBC1 in this iteration).
- 14 enemy slots / 8 projectile slots / 16 tower slots — unchanged.
- ≤ 10 sprites per scanline (worst case unchanged: 1 cursor + 14 enemies
  + 8 projectiles, all path-distributed).
- ≤ 16 BG writes per VBlank (EMP tower idle blink obeys the existing
  ≤1-write/frame round-robin in `towers_render()`).
- All movement frozen during modal (existing gate).
- Three-read-site rule for difficulty preserved (no new read site:
  `enemies_spawn()` already calls `difficulty_enemy_hp(type, diff)`).
- Pure-helper rule: `difficulty_calc.h` and `enemies_anim.h` remain
  `<stdint.h>`-only.
- Bounty-capture rule: armored bug uses the same projectile-hit path,
  invariant unchanged.
- No SRAM (deferred to roadmap #19).

### Resource budget
| Resource | Cost | Notes |
|----------|------|-------|
| Sprite tiles | +6 (3 armored + 3 stun overlays) | 96 B VRAM |
| BG tiles | +2 (`TILE_TOWER_3` + `_B`) | 32 B |
| WRAM (`stun_timer`) | +14 B | 1 B × 14 enemy slots |
| Tower stats row | +12 B (added `kind` + 2 stun fields) | const ROM |
| Enemy stats row | +5 B | const ROM |
| Wave script | +14 B (7 new spawn events × 2 B) | const ROM |
| SFX entry | ~24 B | `SFX_EMP_FIRE` |
| Code | ~500–700 B | EMP scan, stun gating, cycle change |
| **Total ROM growth** | **~700–900 B** | Current ~20 KB → projected ~21 KB. Well under cap. |

## Decisions

| ID | Decision | Options | Choice | Rationale |
|----|----------|---------|--------|-----------|
| D-IT3-18-1 | EMP fields in `tower_stats_t` | (a) reuse `damage`/`damage_l1` slots semantically as stun frames; (b) add a `kind` enum + dedicated `stun_frames`/`stun_frames_l1` fields | **(b)** | Reusing the damage slot saved ~2 B but reading "damage = 60" for a stun tower is bug-bait at every read site. Explicit `kind` keeps host-test asserts honest and makes a future fourth tower trivial. |
| D-IT3-18-2 | EMP target selection | single nearest vs all-in-range AoE | **all-in-range AoE** | AoE is the EMP's defining identity; cost is a 14-iteration scan once per 120 frames — negligible. Single-target would feel like a worse AV. |
| D-IT3-18-3 | Upgrade-menu label | (a) keep `UPG`/`SEL`; (b) add a STUN+ glyph string | **(a) keep** | Adding "STUN+" requires `SPR_GLYPH_N` and `+` (neither exported), plus a tower-type branch in `menu.c` rendering. Existing `UPG` is tower-agnostic and still accurate. Zero glyph budget impact. |
| D-IT3-18-4 | EMP upgrade direction | shorter cooldown vs longer stun | **longer stun** (60 → 90 f) | Longer stun is *visibly* better — a player can see the freeze last longer. A shorter cooldown is invisible at human reaction time. Also keeps cooldown ≥ stun, so an upgraded EMP can't perpetually freeze a slow enemy (which would trivialize tanks). |
| D-IT3-18-5 | Stun visual approach | (a) reuse flash tile for first frame; (b) dedicated SPR_*_STUN per type | **(b) dedicated** | 3 tiles (96 B) is well within budget and gives unambiguous "frozen" feedback. Flash-reuse would be confusing because flash is already the *hit* signifier and a stun is not a hit. |
| D-IT3-18-6 | Flash + stun priority | flash first vs stun first | **flash > stun** | Flash is 3 frames and signals damage; stun is 60+ frames. Letting flash win for those 3 frames keeps the damage-feedback channel clean; stun_timer continues ticking underneath so the total stun duration is preserved. |
| D-IT3-18-7 | EMP cooldown reset when scan finds nobody | always reset vs only on success | **only on success** | "Always reset" would create a dead 120-frame window if an enemy entered range one frame after a fruitless scan — unfair and unreadable. "Only on success" makes the EMP feel responsive. Cost: one extra branch, no extra state. |
| D-IT3-18-8 | EMP cost | 15 (= FW) vs 18 (more) vs 20 (most) | **18** | More than FW (which already does damage) but not so much that the player can't afford one early. With NORMAL starting energy 30 and bug bounty 3, an 18-energy EMP is reachable after ~3 kills. |
| D-IT3-18-9 | EMP upgrade cost | 12 vs 18 (= other towers' upg) | **12** | Upgrade only *extends* stun by 30 f — a meaningful but not transformative buff. Cheaper upgrade encourages experimentation with the unique tower. |
| D-IT3-18-10 | ARMORED HP table | EASY/NORMAL/HARD | **8 / 12 / 16** | NORMAL=12 = 4× BUG and 2× ROBOT — clearly the "tank" tier. EASY scaled ⅔, HARD scaled 4/3 (matching BUG/ROBOT ratios within rounding). Kills on AV-L0 (1 dmg) take 8/12/16 shots — long but not absurd; Firewall (3 dmg) clears in 3/4/6. |
| D-IT3-18-11 | ARMORED speed | 0x40 (slowest) vs 0x60 vs 0x80 (= BUG) | **0x40 (0.25 px/f)** | Slow speed makes high HP *feel* like armor (player has time to whittle it). 0x40 is half BUG and a third of ROBOT — clearly the slowest enemy. |
| D-IT3-18-12 | ARMORED bounty | 6 vs 8 vs 10 | **8** | Reward proportional to effort: ARMORED ≈ 4× BUG HP and 1.6× ROBOT HP. Bounty 8 vs BUG 3 / ROBOT 5 keeps the ratio sensible without inflating economy. |
| D-IT3-18-13 | EMP audio channel | CH1 (square+sweep) vs CH2 (square) vs CH4 (noise) | **CH1, prio=2** | A descending square sweep "feels" electrical/zappy. CH4 would force a music-duck at every pulse (every 2 s) which is annoying. CH1 prio=2 preempts low-prio chatter (TOWER_FIRE on CH2 is unaffected, different channel). |
| D-IT3-18-14 | B-cycle code change | XOR vs modulo | **modulo TOWER_TYPE_COUNT** | XOR doesn't generalize past 2; modulo is one-line, future-proof, and makes the unit test trivial. |
| D-IT3-18-15 | Wave 7–10 armored counts | none / sparse (1-1-2-3) / heavy (2-3-4-5) | **sparse 1-1-2-3** | Preserves existing W7–W10 difficulty curve (player has already validated these counts in playtests). Armored is an *additional* late-game pressure, not a replacement. W10 bumped to 3 for boss feel. |

## Memory updates

Append to `memory/decisions.md`:

```
### Iter-3 #18: Tower kind discriminator (damage vs stun)
- Date: 2025-01-XX
- Context: EMP tower stuns instead of damaging; needed a clean way to keep
  damage and stun parameters in tower_stats_t without overloading fields.
- Decision: Add `u8 kind` (TKIND_DAMAGE/TKIND_STUN) and dedicated
  `stun_frames`/`stun_frames_l1` fields. Damage tower fields are unread
  for stun towers and vice versa. tower_t per-tower WRAM unchanged.
- Rationale: Explicit > clever. Future towers (debuff, slow, AoE-damage)
  slot in by adding a kind value and the relevant stat fields.
- Alternatives: Reuse damage_l0/l1 as stun_frames (rejected: bug-bait at
  every read site).

### Iter-3 #18: EMP cooldown only resets on a successful pulse
- Date: 2025-01-XX
- Decision: When an EMP scan finds zero stunnable targets, cooldown stays
  at 0 (the scan re-runs each frame). Cooldown resets only when ≥1 enemy
  was actually stunned that frame.
- Rationale: Avoids an unreadable 120-frame dead window after a fruitless
  scan. Keeps EMP feeling responsive.

### Iter-3 #18: Sprite priority — flash > stun > walk
- Date: 2025-01-XX
- Decision: When both flash_timer and stun_timer are >0 on the same
  enemy in the same frame, the FLASH tile renders. stun_timer continues
  to count down underneath. Flash is 3 frames; stun is 60+; total stun
  duration is preserved.
- Rationale: Flash is the damage-feedback channel and must not be
  silenced by stun. Stun is long enough that 3 lost frames are
  imperceptible.
```

Append to `memory/conventions.md`:

```
### Tower stats kind discriminator (iter-3 #18)
Each row in s_tower_stats[] declares `kind`:
  - TKIND_DAMAGE: reads .damage / .damage_l1 / .cooldown / .cooldown_l1,
    fires projectiles via projectiles_fire().
  - TKIND_STUN: reads .stun_frames / .stun_frames_l1 / .cooldown /
    .cooldown_l1, writes enemy.stun_timer directly (no projectile).
The unused field group is irrelevant — do NOT cross-read.

### Enemy sprite-tile priority (iter-3 #18)
In step_enemy(), pick the sprite tile in this priority order:
  1. flash_timer > 0   → SPR_*_FLASH (3-frame override; per iter-3 #21)
  2. stun_timer  > 0   → SPR_*_STUN  (movement also skipped)
  3. else              → walk frame A/B by (anim >> 4) & 1
flash_timer and stun_timer both decrement every frame regardless of
which one wins the tile slot.

### Tower-select cycle: modulo TOWER_TYPE_COUNT
The B-button handler in game.c uses
  s_selected_type = (s_selected_type + 1) % TOWER_TYPE_COUNT;
Adding a new tower requires NO change to this code; the new type joins
the cycle automatically.
```

## Review

### Adversarial review (self, planner) — round 1
- *"What if multiple enemies hit the in-range scan at exactly frame N+120 — could SFX_EMP_FIRE play more than once?"* — No: the SFX is played once per scan, after the loop, gated by a local `bool any_stunned`.
- *"Is there a window where a freshly placed EMP fires on frame 0 with cooldown=0?"* — Yes — by design after the placement special-case (subtask 5 zeroes EMP cooldown at place time). Documented in scenario 6.
- *"Does the wave script byte format break with `A=2`?"* — No: `enemy_id_t` is `u8`; values 0–255 fit. Verified by spawn dispatch reading `s_enemy_stats[type]` with the new row.
- *"Is the difficulty clamp `enemy_type >= 2 → 0` safe when extended to `>= 3`?"* — Yes: 2 is a valid index after the table extension. Pre-existing clamp logic protected against out-of-bounds; the new clamp simply admits one more legal value.
- *"Could pause cause stun_timer to drift?"* — No: `enemies_update()` is gated on `!modal_open`, so neither movement nor decrement runs during pause; total stun duration is preserved.
- *"Does adding 6 sprite slots (3 stun + 3 armored) push past the 256-tile sprite VRAM bank?"* — Current 37 + 6 = 43, far under 256.
- *"Does the cycle change `^=1 → modulo` change behavior for any existing test?"* — `test_math.c` doesn't currently assert on the cycle. New test `test_tower_cycle_wrap` covers it. The two-press round trip behavior on `TOWER_TYPE_COUNT==2` was XOR identity; modulo gives the same result for COUNT=2, so no behavioral regression if anyone reverts.

### Adversarial review (reviewer agent) — round 2
Two high-/medium-severity findings, both resolved:

1. **EMP idle blink would have used the firewall's alt tile.**
   `towers_render()` Phase 3 hardcoded
   `(type==TOWER_AV) ? TILE_TOWER_B : TILE_TOWER_2_B`, so adding a third
   tower would have made EMP blink with FW graphics. **Resolved**:
   subtask 5 now adds a `bg_tile_alt` field to `tower_stats_t` and
   replaces the ternary with a single per-row lookup. Generalizes for
   any future fourth tower (same rationale as D-IT3-18-14).

2. **Self-contradiction on freshly-placed EMP.**
   `towers_try_place()` sets `cooldown = stats.cooldown` (i.e. 120)
   unconditionally, so without intervention an EMP would *not* fire on
   frame 0 — contradicting the original Scenario 6. **Resolved**:
   subtask 5 now special-cases EMP at placement to zero the cooldown
   (matches D-IT3-18-7's intent that EMP feels responsive). Scenario 6
   reworded to assert the immediate-stun behavior explicitly.

Plus a verification note from the reviewer: Scenario 10 needs the
tester to ensure exactly one AV is in range so the shot count is
unambiguous — applied as an inline clarification.

### Cross-model validation (reviewer agent, gpt-5.4) — round 3
Three findings, all resolved:

1. **`enemy_t` encapsulation**. The original spec had `towers.c` write
   `enemy.stun_timer` directly, but `enemy_t` is private to
   `enemies.c`. **Resolved**: introduced public APIs
   `enemies_try_stun()` and `enemies_is_stunned()`. `enemy_t` stays
   private; `towers.c` calls the API. Subtasks 4, 5, and 9 updated;
   architecture data-flow diagram updated.

2. **Cycle host-test against existing pure-test pattern**. Asserting
   the cycle math from `test_math.c` would have required pulling
   `gtypes.h` and a private `cycle_tower_type()` into the test, which
   conflicts with the pure-helper testing convention. **Resolved**:
   added a new pure header `src/tower_select.h` with a single inline
   `tower_select_next(cur, count)`. `game.c` calls it; `test_math.c`
   asserts directly against it. No game source compiled into
   `test_math`. Subtasks 6 and 9 updated.

3. **Acceptance scenarios assumed unstated tower layouts**. Several
   scenarios required reaching W3+ but EMP-only runs die in W1.
   **Resolved**: Setup section now defines a "survival baseline"
   (1× AV-L0 at frame 0) and an "EMP-test loadout" (baseline + 2nd
   AV) that scenarios 3–8 and 11–12 reference; continuation chains
   are explicit; scenario 13's "unchanged" is now a concrete table of
   spawn counts and delays rather than a comparison to a prior build.
   Restart instruction added.

Plus a new **Edge cases** section codifies stun behavior under sell,
upgrade-mid-stun, overlapping EMPs, and 14-enemy AoE — flagged by the
reviewer's question 5 as worth nailing down.
