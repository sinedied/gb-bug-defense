# QA Log: Feature #26 — Upgraded Tower Visual

## Session 1

**Date**: 2025-01-27
**Tester**: QA Agent

### Commands Tested

| Command | Result | Notes |
|---------|--------|-------|
| `just build` | ✅ PASS | Produces `build/bugdefender.gb` (65536 bytes). Warnings are SDCC optimizer noise, not errors. |
| `just test` | ✅ PASS | All 13 host-side tests pass. |
| `just check` | ✅ PASS | ROM validation passes (65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr=0x33). |

### Additional Verification

| Check | Result | Notes |
|-------|--------|-------|
| `grep -c 'bg_tile_l1' src/towers.h` | ✅ Output: 1 | Field exists in tower_stats_t |
| `grep 'TILE_TOWER_L1' res/assets.h` | ✅ 6 constants found | TILE_TOWER_L1, _L1_B, _2_L1, _2_L1_B, _3_L1, _3_L1_B |
| `grep 'MAP_TILE_COUNT' res/assets.h` | ✅ Value is 29 | Grew from 23 → 29 as specified |

### Source Verification

- `towers.h`: `bg_tile_l1` and `bg_tile_alt_l1` fields present in `tower_stats_t`
- `towers.c`: All 3 tower types have `.bg_tile_l1` and `.bg_tile_alt_l1` in s_tower_stats[]
- `towers.c`: `towers_upgrade()` sets `dirty = 1` and `idle_phase = 0` (lines 168-169)
- `towers.c`: Phase 2 uses level-branched tile selection (line 216)
- `towers.c`: Phase 3 uses level-branched base/alt selection (lines 237-238)

### Acceptance Scenarios (code-level only; visual scenarios require emulator)

| # | Scenario | Verified | Notes |
|---|----------|----------|-------|
| 1 | Upgrade shows new tile | ✅ (code) | dirty=1 on upgrade triggers Phase 2 repaint with L1 tile |
| 2 | All 3 types have distinct L1 tiles | ✅ (code) | 6 unique tile constants at offsets 23-28 |
| 3 | L1 idle animation works | ✅ (code) | Phase 3 branches on level for base/alt |
| 4 | L0 towers unaffected | ✅ (code) | Ternary `level ? L1 : L0` preserves L0 path |
| 5 | Sell L1 tower restores ground | ✅ (code) | Phase 1 sell-clear is level-independent |
| 6 | Mixed L0/L1 idle coexist | ✅ (code) | Per-tower level check in Phase 3 |
| 7 | Upgrade during alt-phase repaint | ✅ (code) | idle_phase=0 reset prevents stale mismatch |

### Issues Found

None.
