# QA Log: Feature #27 — Deeper Tower Upgrades (L2)

## Session 1 — 2025-01-27

### Environment
- macOS (Darwin, Apple Silicon with Rosetta)
- GBDK 4.2.0 (vendor/gbdk)
- `just` at /opt/homebrew/bin/just

### Commands Tested

| Command | Result | Notes |
|---------|--------|-------|
| `just build` | ✅ PASS | Produces build/bugdefender.gb (65536 bytes). Compiler warnings 110 in menu.c and towers.c (optimizer flow — expected for nested ternaries). |
| `just test` | ✅ PASS | All 13 test suites pass (test_math, test_audio, test_music, test_pause, test_game_modal, test_anim, test_difficulty, test_enemies, test_towers, test_maps, test_save, test_score, test_gate). |
| `just check` | ✅ PASS | ROM validation: 65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr=0x33. |

### Additional Verifications

| Check | Expected | Actual | Result |
|-------|----------|--------|--------|
| MAP_TILE_COUNT in res/assets.h | 35 | 35 | ✅ |
| L2 test functions in test_towers.c | > 0 | 14 references (7 functions + 7 calls) | ✅ |

### L2 Test Functions Verified
1. `test_l2_can_upgrade_levels` — L0→L1→L2 gating
2. `test_l2_spent_and_refund` — cost/refund economics
3. `test_l2_av_damage` — AV tower L2 damage values
4. `test_l2_emp_stun_duration` — EMP L2 stun frames
5. `test_l2_emp_upgrade_preserves_cooldown` — F2/D-IT3-18-7 preserved across L1→L2
6. `test_l2_tower_stats` — struct field correctness
7. `test_l2_render_tiles` — BG tile selection per level

### Issues Found
None.
