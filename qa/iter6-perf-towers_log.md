# QA Log: Iter-6 Perf Towers (Idle Tower Rescan Cooldown Floor)

## Date: 2025-04-29

## Summary
Verified the iter-6 performance fix: idle tower rescan cooldown floor (`TOWER_IDLE_RESCAN = 8`). Both TKIND_DAMAGE and TKIND_STUN no-target paths now set `cooldown = TOWER_IDLE_RESCAN - 1` (= 7), giving an actual rescan latency of 8 frames.

## Dev Workflow

| Command | Result | Notes |
|---------|--------|-------|
| `just build` | ✅ | ROM builds to 65536 bytes. Only pre-existing SDCC optimizer warnings (menu.c, towers.c) |
| `just test` | ✅ | All 14 test binaries pass |
| `just check` | ✅ | ROM check passes (65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr checksum OK) |
| `just clean` | ✅ | Removes build directory cleanly |
| `just run` | ✅ | mGBA launches, ROM loads, no crash |

## Static Code Review

### tuning.h (line 70)
- `#define TOWER_IDLE_RESCAN 8` — correctly defined with comment

### src/towers.c — TKIND_STUN no-target path (lines 344-348)
- `} else {` branch correctly sets `s_towers[i].cooldown = TOWER_IDLE_RESCAN - 1;`
- Comment references iter-6, explains -1 for actual latency

### src/towers.c — TKIND_DAMAGE no-target path (lines 352-356)
- `if (t == 0xFF) {` branch correctly sets `s_towers[i].cooldown = TOWER_IDLE_RESCAN - 1;`
- Comment references iter-6, explains -1 for actual latency

### tests/test_towers.c — New tests (lines 631-715)
- `test_damage_idle_rescan`: Drains AV cooldown, verifies no fire during rescan window, fires on expected frame
- `test_emp_idle_rescan`: Drains EMP F3 cooldown, verifies no stun during rescan window, stuns on expected frame
- `test_multi_tower_idle_rescan`: 4 AV towers synchronized, verifies burst timing
- All three registered in main() (lines 735-737)

### Existing test T3 compatibility verified
- `test_emp_cooldown_not_reset_on_empty_scan` passes because its first `towers_update()` only decrements F3 fresh-place cooldown (1→0) without scanning. The "no target found" path is never exercised in that test.

## Acceptance Scenarios

| # | Scenario | Result | Notes |
|---|----------|--------|-------|
| 9 | All host tests pass | ✅ | 14 binaries, 0 failures |
| — | ROM builds at 65536 bytes | ✅ | Confirmed via `just check` and `ls -la` |
| — | TKIND_DAMAGE no-target sets cooldown = 7 | ✅ | Static review confirmed (line 355) |
| — | TKIND_STUN no-target sets cooldown = 7 | ✅ | Static review confirmed (line 347) |
| — | mGBA launches without crash | ✅ | Ran for 5s with no crash or error output |

## Scenarios Not Directly Testable (Require Manual Gameplay)

| # | Scenario | Status | Notes |
|---|----------|--------|-------|
| 1 | 16 towers idle, music tempo | MANUAL | Requires interactive play + audio verification |
| 2 | Fast mode + 16 towers | MANUAL | Requires SELECT toggle |
| 3 | Tower fires within range latency | MANUAL | ≤8 frame latency imperceptible |
| 4 | EMP stun within range latency | MANUAL | ≤8 frame latency imperceptible |
| 5 | Heavy load: W10 + 16 towers + fast mode | MANUAL | Full gameplay session needed |
| 6 | Existing fire rate unchanged | MANUAL | Observation of shot cadence |
| 7 | Overlapping EMP no perma-freeze | MANUAL | But covered by host test `test_overlapping_emp_no_perma_freeze` |
| 8 | Between-wave performance | MANUAL | Music tempo during inter-wave |

## Notes
- The `projectiles_fire` failure path (pool full) intentionally retains retry-every-frame behavior — transient condition with valid target, not subject to idle rescan
- The Qt backingstore warning (`Back buffer dpr of 2 doesn't match...`) is cosmetic, not a game issue
- No new warnings introduced by the iter-6 code changes
- Build reproduces identically from clean state
