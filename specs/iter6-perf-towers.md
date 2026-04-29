# Iter-6: Deeper Performance Fixes (Tower Idle Rescan)

## Problem
The game drops frames at 6+ towers, worsening with tower count (not enemy count). SELECT fast-forward makes slowdowns WORSE (doubles the entity-tick inner block). Music slows — objective frame-overrun indicator. Iter-5 fixed menu repaint, multiply-elision, and cursor cache but did NOT address the dominant cost: every tower whose cooldown is 0 with no valid target runs a full 14-slot enemy scan EVERY frame.

## Investigation Findings

### Hypothesis A: per-tower idle animation writes BG every frame — DENIED
`towers_render()` Phase 3 (src/towers.c:248-266) already has a dirty check:
```c
u8 want = towers_idle_phase_for(cur_frame, i);
if (want == s_towers[i].idle_phase) continue;
```
Writes at most 1 tile per frame (returns after first write). No fix needed.

### Hypothesis B: EMP tower rescans every frame at cooldown=0 — CONFIRMED
src/towers.c:345: `/* else: empty range — keep cooldown=0, re-scan next frame. */`
When no targets in range, EMP towers keep cooldown=0 and run the full 14-enemy scan loop every frame. With 3 EMPs idle: 3 × 14 = 42 iterations/frame.

### Hypothesis C: TKIND_DAMAGE towers also rescan when no target — CONFIRMED
src/towers.c:348-349:
```c
u8 t = acquire_target(cx, cy, range_sq, st->range_px);
if (t == 0xFF) continue;  // cooldown stays at 0!
```
When `acquire_target` returns 0xFF (no target), the tower just `continue`s without setting cooldown. Next frame: cooldown still 0, re-enters targeting. With 16 DAMAGE towers idle: 16 × 14 = 224 iterations/frame.

### Hypothesis D: VBlank BG write throughput — NOT THE BOTTLENECK
`playing_render()` in steady state produces 0-1 BG writes/frame. HUD uses dirty flags. No issue.

### Hypothesis E: HUD redraw scope — NOT THE BOTTLENECK
`hud_update()` uses dirty flags. Passive income fires once per 180 frames. Trivial.

### Hypothesis F: animation frame counter forces redundant writes — DENIED
`towers_idle_phase_for()` transitions once per 32 frames. The `want == idle_phase` guard prevents writes. No issue.

### Hypothesis G: fast-mode doubles the hot path — CONFIRMED
src/game.c:288-289:
```c
entity_tick();
if (s_fast_mode) entity_tick();
```
`entity_tick()` calls `towers_update()`. In fast mode with 16 idle towers: 2 × 16 × 14 = 448 iterations/frame of enemy scanning.

### Cost Estimate (pre-fix)
On SDCC-compiled GB Z80 (~70K cycles/frame budget):
- `enemies_alive(j)` cross-unit function call: ~80 cycles (push, CALL, prologue, array-index, compare, epilogue, RET)
- Dead enemy path (12 dead slots per tower): 12 × 80 = 960 cycles
- Alive enemy full distance check (2 alive): 2 × 200 = 400 cycles
- Per tower scan total: ~1,360 cycles
- **16 towers × 2 ticks (fast mode): ~43,520 cycles/frame (62% of budget!)**

This leaves only ~27K cycles for enemies_update (~7K), projectiles_update (~3K), audio/music (~3K), input/rendering (~10K) = ~23K minimum. Total: ~66,520 — dangerously close to the 70K ceiling. Any variability (alive enemies, projectile hits, HUD redraws) pushes over.

## Architecture

### Single fix: idle rescan cooldown floor

Add `#define TOWER_IDLE_RESCAN 8` to `tuning.h`. When a tower's targeting pass finds no valid target, set `cooldown = TOWER_IDLE_RESCAN - 1` instead of leaving it at 0. The `-1` ensures actual rescan latency is exactly `TOWER_IDLE_RESCAN` frames (the decrement-then-skip pattern consumes one frame per count).

### Burst pattern (not uniform stagger)

All towers placed simultaneously (common case: player builds between waves) will synchronize their rescan timers. Every `TOWER_IDLE_RESCAN` = 8 frames (or ~4 frames in fast mode, since cooldown decrements twice per frame), all idle towers scan in a single burst, then go idle again.

**Worst-case burst frame (fast mode):**
- First entity_tick: all 16 towers have cooldown=0 → all scan → all set cooldown=7
- Second entity_tick: all 16 towers have cooldown=7 → decrement to 6 → skip (no second scan)
- Burst cost: 16 × 1,360 = **~21,760 cycles** (31% of budget)
- Non-burst frames: **0 cycles** for tower scanning

**Average amortized cost:**
- Fast mode: one burst every ~4 frames → 21,760 / 4 ≈ 5,440 cycles/frame average
- Normal mode: one burst every 8 frames → 21,760 / 8 ≈ 2,720 cycles/frame average

**Worst-case total frame budget (burst + everything else, fast mode):**
| Component | Cycles |
|-----------|--------|
| towers_update (burst) | 21,760 |
| enemies_update ×2 (14 alive) | 7,000 |
| projectiles_update ×2 (8 alive) | 3,200 |
| Audio/music/input | 5,000 |
| Rendering (OAM + 1 BG write) | 3,000 |
| **Total** | **~40,000** |
| **Headroom** | **~30,000 (43%)** |

Compare pre-fix worst case: ~66,500 cycles/frame EVERY frame (5% headroom).

### Behavioral impact analysis
- **Max first-shot latency**: 8 frames (133ms at 60fps). At BUG_SPEED 0.5 px/frame, enemy moves 4 pixels — less than 1 tile, well within any tower's range diameter (16–40 px). Imperceptible in a tower-defense context.
- **Steady-state fire rate unchanged**: once a tower fires, cooldown is the normal 30–120 frames. The idle rescan floor only applies to the "no target found" path.
- **Between waves**: towers set rescan timers on first empty scan; burst scans every 8 frames find no targets instantly (all slots dead = fast rejection). Total cost negligible.
- **Fast-mode acceleration preserved**: the rescan floor doesn't dampen fast-mode; towers still acquire targets and fire at 2× cadence once enemies are in range.

### Why this suffices (no additional fix needed)
- Hypotheses A/D/E/F: already optimized or trivial cost
- Hypothesis G (fast-mode): even the worst-case burst in fast mode (31% budget) leaves 43% headroom — more than enough for the doubled entity processing
- No BG writes, no new modules, no API changes, no cross-module coupling

## Subtasks

1. ✅ **Add `TOWER_IDLE_RESCAN` constant to tuning.h** — Add `#define TOWER_IDLE_RESCAN 8` after line 66 (`#define PROJ_DAMAGE 1`), with a comment: `/* Iter-6: frames between idle targeting rescans (no target found) */`. Done when: constant defined, `just build` succeeds.

2. ✅ **Apply idle rescan to TKIND_DAMAGE path** — In `src/towers.c` `towers_update()`, change the no-target path (line 348-349) from:
   ```c
   u8 t = acquire_target(cx, cy, range_sq, st->range_px);
   if (t == 0xFF) continue;
   ```
   to:
   ```c
   u8 t = acquire_target(cx, cy, range_sq, st->range_px);
   if (t == 0xFF) {
       s_towers[i].cooldown = TOWER_IDLE_RESCAN - 1;
       continue;
   }
   ```
   Done when: a DAMAGE tower with no target in range has cooldown set to 7 (TOWER_IDLE_RESCAN - 1) so actual rescan latency = 8 frames. Depends on: subtask 1.

3. ✅ **Apply idle rescan to TKIND_STUN empty-range path** — In `src/towers.c` `towers_update()`, replace the implicit else at line 345 (`/* else: empty range — keep cooldown=0, re-scan next frame. */`) with an explicit else block:
   ```c
   } else {
       /* Iter-6: empty range — idle rescan delay instead of
        * scanning every frame. -1 so actual latency = TOWER_IDLE_RESCAN frames. */
       s_towers[i].cooldown = TOWER_IDLE_RESCAN - 1;
   }
   ```
   Done when: a STUN tower with no target in range has cooldown set to 7 (TOWER_IDLE_RESCAN - 1) so actual rescan latency = 8 frames. Depends on: subtask 1.

4. ✅ **Add host tests for idle rescan behavior** — In `tests/test_towers.c`, add three new test functions before `int main(void)` and register them in `main()`:

   **`test_damage_idle_rescan`**:
   ```c
   static void test_damage_idle_rescan(void) {
       reset_all();
       CHECK(towers_try_place(5, 5, TOWER_AV));
       /* AV initial cooldown = 60. Drain to 0. */
       for (int f = 0; f < 60; f++) towers_update();
       /* Now cooldown=0. Call with no enemies → should set rescan. */
       s_proj_fire_count = 0;
       towers_update();  /* scan, no target → cooldown = TOWER_IDLE_RESCAN */
       CHECK_EQ(s_proj_fire_count, 0);
       /* Run TOWER_IDLE_RESCAN - 1 more updates (7). Cooldown goes 8→1. */
       for (int f = 0; f < TOWER_IDLE_RESCAN - 1; f++) towers_update();
       CHECK_EQ(s_proj_fire_count, 0);  /* still in rescan delay */
       /* Next update: cooldown 1→0. Falls through to scan but no enemy. */
       towers_update();
       CHECK_EQ(s_proj_fire_count, 0);  /* scanned but no enemy */
       /* Now place enemy. Next scan will find it (after another rescan). */
       place_enemy(0, 44, 52);  /* in range of AV at (44, 52) */
       /* Tower just set cooldown=8 again (empty scan). Need 8+1 frames. */
       for (int f = 0; f < TOWER_IDLE_RESCAN; f++) towers_update();
       /* Cooldown just hit 0. Next update → fires. */
       towers_update();
       CHECK(s_proj_fire_count > 0);
   }
   ```

   **`test_emp_idle_rescan`**:
   ```c
   static void test_emp_idle_rescan(void) {
       reset_all();
       CHECK(towers_try_place(5, 5, TOWER_EMP));
       /* F3: fresh-place cooldown=1. Drain it. */
       towers_update();  /* cooldown 1→0 */
       /* Scan with no enemies → set cooldown=TOWER_IDLE_RESCAN. */
       s_stun_call_count = 0;
       towers_update();
       CHECK_EQ(s_stun_call_count, 0);  /* no enemies = no stun calls */
       /* Wait TOWER_IDLE_RESCAN frames for cooldown to drain. */
       for (int f = 0; f < TOWER_IDLE_RESCAN; f++) towers_update();
       /* Cooldown=0 now. Place enemy and fire. */
       place_enemy(0, 44, 52);
       s_stun_call_count = 0;
       towers_update();
       int found = 0;
       for (int k = 0; k < s_stun_call_count; k++) {
           if (s_stun_calls[k].result) found = 1;
       }
       CHECK(found);
   }
   ```

   **`test_multi_tower_idle_rescan`** (synchronized burst validation):
   ```c
   static void test_multi_tower_idle_rescan(void) {
       reset_all();
       /* Place 4 AV towers. */
       CHECK(towers_try_place(2, 2, TOWER_AV));
       CHECK(towers_try_place(3, 2, TOWER_AV));
       CHECK(towers_try_place(4, 2, TOWER_AV));
       CHECK(towers_try_place(5, 2, TOWER_AV));
       /* Drain all cooldowns to 0 (AV cooldown = 60). */
       for (int f = 0; f < 60; f++) towers_update();
       /* All 4 towers at cooldown=0. Next update: all scan empty range,
        * all set cooldown=TOWER_IDLE_RESCAN. */
       s_proj_fire_count = 0;
       towers_update();
       CHECK_EQ(s_proj_fire_count, 0);
       /* Place an enemy near tower at (5,2) — center (44, 28).
        * AV range = 24px. Tower at (5,2) center = 5*8+4=44, (2+1)*8+4=28.
        * Enemy at (44, 28) is distance 0 — in range of tower at (5,2). */
       place_enemy(0, 44, 28);
       /* Run towers_update for TOWER_IDLE_RESCAN - 1 frames.
        * All towers still cooling (8→1). No fire. */
       for (int f = 0; f < TOWER_IDLE_RESCAN - 1; f++) towers_update();
       CHECK_EQ(s_proj_fire_count, 0);
       /* Next update: cooldown 1→0 for all. Falls through to scan.
        * Tower at (5,2) finds enemy in range → fires. Others don't. */
       towers_update();
       CHECK(s_proj_fire_count > 0);
   }
   ```

   Add all three test names to main() call list.
   Done when: `just test` passes with all new + existing tests. Depends on: subtasks 2, 3.

5. ✅ **Verify all host tests pass** — Run `just test`. All 14 test binaries + new tests pass. Done when: zero failures reported. Depends on: subtask 4.

6. ✅ **Build validation** — Run `just check`. ROM builds, header validates. Done when: output confirms sizes and checksums correct. Depends on: subtask 5.

## Acceptance Scenarios

### Setup
1. `just build` succeeds without warnings
2. `just test` — all host-side test binaries pass (including new idle rescan tests)
3. `just run` — launch in mGBA

### Measurement method
Music tempo is the objective proxy for frame rate on DMG. The music engine advances one row-duration countdown per frame. If frames overrun, `wait_vbl_done()` returns late and music plays at half speed — immediately, unambiguously audible. Either the melody plays at correct tempo or it doesn't.

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | 16 towers idle, music tempo | NORMAL difficulty; place 16 AV towers far from path (no enemies in range); wait for W5+ enemies to spawn | Music at consistent correct tempo throughout |
| 2 | Fast mode + 16 towers | Place 16 towers off-path; toggle SELECT; let enemies spawn | Music at correct tempo; entities move visibly faster |
| 3 | Tower fires within range latency | Place AV tower 1 tile from path edge; send bugs | Tower fires within ≤8 frames of bug entering range (~133ms, imperceptible) |
| 4 | EMP stun within range latency | Place EMP adjacent to path; wait for bug to enter range | EMP fires pulse within ≤8 frames of bug entering range |
| 5 | Heavy load: W10 + 16 towers + fast mode | Play to wave 10 on NORMAL with 16 mixed towers (AV/FW/EMP); fast mode on | Music tempo correct; no visible frame drops; enemies don't teleport |
| 6 | Existing fire rate unchanged | Place AV L2 tower in range of path; observe shot cadence | 30 frames between consecutive shots (same as pre-fix) |
| 7 | Overlapping EMP no perma-freeze | Place 2 overlapping EMPs near path corner; observe bug movement | Enemies have movement windows between stun cycles (F1 behavior preserved) |
| 8 | Between-wave performance | 16 towers placed; observe during inter-wave delay (180 frames) | Music at full correct tempo during all inter-wave waiting |
| 9 | All host tests pass | Run `just test` | All test binaries report OK (0 failures) |

## Constraints
- Frame budget: ~70K cycles/frame on DMG (4.19 MHz / 59.73 Hz)
- BG-write budget: ≤16 writes/frame — **unchanged** (this fix adds zero BG writes)
- No gameplay changes: fire rates, damage values, stun durations all unchanged
- Max engagement latency: ≤8 frames (133ms) — imperceptible for tower defense
- All 14 existing host tests must pass unchanged (none exercise the "cooldown=0 + no target" path for multiple frames)
- New WRAM: 0 bytes (reuses existing `cooldown` field in tower_t struct)
- New ROM: 1 constant definition + ~6 lines of code change

## Decisions
| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| Rescan interval value | 4, 6, 8, 16 frames | 8 | 133ms max latency (sub-perception for TD). Reduces average scan cost 8–10×. Value of 4 gives less savings; 16 introduces noticeable 266ms first-shot delay. |
| Stagger mechanism | (A) None (accept synchronized bursts); (B) Per-tower offset `+ (i & 3)` | A | Worst-case burst is 31% of frame budget with 43% headroom remaining. Stagger adds code complexity and slightly varies per-tower rescan period for no user-visible benefit. Simplest correct solution. |
| Fix scope | (A) Rescan floor only; (B) + O(1) enemy count cache; (C) + spatial pre-filter | A | Rescan floor alone reduces worst-case tower scan cost from 62% to 31% of budget (burst) and near-zero average. Additional optimizations have diminishing returns and higher risk. |
| Where to set rescan | (A) Inline in towers_update; (B) New helper function | A | Two-line change per path. No API changes, no new functions, no cross-module coupling. |
| Test approach | (A) Modify existing T3; (B) Add new tests alongside | B | T3 ("not_reset_on_empty_scan") doesn't exercise the changed path due to test structure (enemy placed before scan fires). Adding dedicated rescan tests is cleaner and preserves T3 as an F3-defer regression guard. |
| Fast-mode compensation | (A) Halve TOWER_IDLE_RESCAN in fast mode; (B) Keep same value | B | In fast mode, effective wall-time rescan interval halves naturally (8 ticks / 2 ticks-per-frame ≈ 4 frames). Still within budget without special-casing. |
| projectiles_fire failure | (A) Apply rescan floor on pool-full; (B) Leave as retry-next-frame | B | Pool-full means tower HAS a valid target; pool clears in 1–3 frames. Quick retry is correct for transient conditions. |

## Test Strategy
- **3 new tests in test_towers.c**: `test_damage_idle_rescan`, `test_emp_idle_rescan`, `test_multi_tower_idle_rescan` verify single-tower latency and multi-tower synchronized behavior.
- **No new stubs needed**: existing test infrastructure (enemy stubs, projectile stubs, audio stubs) suffices.
- **No changes to existing tests**: all 17 existing tests in test_towers.c pass unchanged. None exercise the "cooldown=0 with no target for multiple consecutive frames" path because their test scenarios place enemies before the first scan or test behavior after successful firing.
- **All other 13 test binaries unaffected**: change is entirely within towers.c (only linked by test_towers).
- **`TOWER_IDLE_RESCAN` is defined in tuning.h**: tests see the same value via `#include "gtypes.h"` → `#include "tuning.h"`, preventing constant drift.

## Risk Assessment
| Fix | Risk | Notes |
|-----|------|-------|
| TOWER_IDLE_RESCAN for TKIND_DAMAGE | LOW | Only "no target found" path changed. Fire rate once targeting succeeds is unchanged. |
| TOWER_IDLE_RESCAN for TKIND_STUN | LOW | Only "empty range" path changed. EMP with targets in range fires exactly as before. F1 overlapping-EMP anti-chain-lock unaffected (that sets full cooldown on found_target). |
| Synchronized burst | LOW | All towers may scan on same frame after idle period. Worst case: 31% of frame budget for one tick, with 43% headroom. Does not cause frame drops. |
| Subtle behavior change (first-shot delay) | NEGLIGIBLE | Max 133ms delay before first engagement. At EMP range=16px and bug speed=0.5px/f, enemy takes 32+ frames to cross range diameter. 8-frame delay = 4px into zone vs 0px — no gameplay difference. |

## Review
**Primary adversarial review**: PASS. Confirmed code paths match investigation. F1 overlapping-EMP rule unaffected. Fast-mode reasoning verified. One awareness note: `projectiles_fire` failure path retains its retry-every-frame behavior — intentionally unchanged since it's a transient condition with a valid target.

**Cross-model validation findings**:
1. **(HIGH) "2 scans/frame max" claim is incorrect** — Fixed: architecture now accurately describes synchronized burst pattern (all towers may scan on same frame). Worst case quantified at 31% budget with 43% headroom. No stagger mechanism needed.
2. **(MED) No multi-tower host test** — Fixed: added `test_multi_tower_idle_rescan` that places 4 towers, verifies synchronized rescan timing, and confirms correct firing once enemy appears post-delay.

## Out of Scope
- Game-over/win screen graphics (separate spec)
- Any new gameplay mechanics
- Tower stats / firing rates / damage changes
- Further optimizations (spatial pre-filter, O(1) enemy count, inlining enemies_alive)
- enemies_update or projectiles_update optimization (not the bottleneck)
- BG-write budget changes (already at spec)
