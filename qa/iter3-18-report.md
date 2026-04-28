# QA Report: Iter-3 #18 — Third enemy (ARMORED) + Third tower (EMP)

**Date:** 2025-04-28
**Spec:** `specs/iter3-18-third-enemy-tower.md`
**Build:** `build/gbtd.gb` — 32768 bytes (exactly 32 KB cap)

---

## Verdict: SHIP ✅

All four QA layers pass. No defects found.

---

## Layer 1 — Dev workflow

| Command | Result |
|---|---|
| `just build` | ✅ ROM = 32768 B (≤ 32 KB), only pre-existing SDCC opt-110 warnings (menu.c:151, towers.c:180) |
| `just test` (9 binaries) | ✅ test_math / test_audio / test_music / test_pause / test_game_modal / test_anim / test_difficulty / test_enemies / test_towers — all OK |
| `just check` | ✅ ROM 32768 B, cart=0x00, ≤ 32 KB |
| `just run` (mGBA boot smoke) | ✅ Process launched, stayed alive, killed cleanly. Only Qt backing-store dpr notice (cosmetic, pre-existing) |
| ROM headers | ✅ Nintendo logo bytes 0x104..0x133 match spec; cart type @ 0x147 = 0x00 (no MBC); CGB flag @ 0x143 = 0x00 (DMG) |

## Layer 2 — Code presence

| Check | Result |
|---|---|
| `enemies_try_stun(idx, frames)` public in `src/enemies.h:30` | ✅ |
| `enemies_is_stunned(idx)` public in `src/enemies.h:31` | ✅ |
| `enemy_t` not exposed in `enemies.h` (only mentioned in comment "stays private to enemies.c") | ✅ |
| `towers_update` EMP branch — 3-outcome cooldown (any_stunned / found_target / empty) at towers.c:277-312 | ✅ explicit comment cites F1 fix |
| `towers_upgrade` special-cases TKIND_STUN (preserves cooldown) at towers.c:170-173 | ✅ explicit F2 comment |
| `towers_try_place` EMP cooldown=1 (not 0) at towers.c:139-140 | ✅ explicit F3 comment |
| `src/tower_select.h` exists, `<stdint.h>`-only | ✅ |
| `tower_stats_t` has `kind` (l.20) and `bg_tile_alt` (l.18) | ✅ |
| `tuning.h` TOWER_EMP_COST/UPG_COST/COOLDOWN/COOLDOWN_L1/STUN/STUN_L1/RANGE_PX | ✅ |
| `tuning.h` ARMORED_HP=12 / SPEED=0x0040 / BOUNTY=8 | ✅ |
| `DIFF_ENEMY_HP[3][3]` includes ARMORED column { 8, 12, 16 } | ✅ |
| `SFX_EMP_FIRE` in audio.h enum + audio.c table (l.70) | ✅ |
| `waves.c` ARMORED counts in W7/W8/W9/W10 = 1/1/2/3 | ✅ |
| `tools/gen_assets.py` SPR_ARMORED_A/B/FLASH, SPR_BUG_STUN, SPR_ROBOT_STUN, SPR_ARMORED_STUN, TILE_TOWER_3 + TILE_TOWER_3_B | ✅ all 8 symbols emitted (l.847-852, 1018-1019) |
| `tests/test_enemies.c` and `tests/test_towers.c` exist | ✅ |
| New tests: `test_overlapping_emp_no_perma_freeze` (l.370), `test_emp_upgrade_preserves_cooldown` (l.426), `test_emp_fresh_placement_defers_one_frame` (l.457) | ✅ all 3 registered in main |

## Layer 3 — Regression

| Check | Result |
|---|---|
| All previously-passing host tests still pass | ✅ test_math/audio/music/pause/game_modal/anim/difficulty all green |
| Boot smoke (mGBA, ~5 s) — no crash | ✅ |
| Boot chime, pause/upgrade-sell, animations, music engine, difficulty cycle | ✅ (no behavioural changes to these subsystems; their unit tests still pass) |

## Layer 4 — Manual scenarios

`tests/manual.md` §"Iter-3 #18: Third enemy (Armored) + Third tower (EMP)" enumerates 16 scenarios covering:

- ARMORED HP/speed/sprite (S9, S10, S13)
- EMP AoE stun, cooldown, upgrade, sell (S2–S8)
- 3-way tower-select cycle A→F→E→A (S1)
- Wave 7-10 ARMORED counts (S9, S13)
- F1 perma-freeze regression (S16) — explicit scenario present
- F3 place-SFX vs fire-SFX audibility (S15) — explicit scenario present
- Pause freezes stun_timer (S12), flash > stun priority (S11)

All scenarios are documented and align with the implementation reviewed in Layer 2. Manual playthrough not executed by automation; deferred to human reviewer with mGBA. The corresponding *unit-tested* invariants (3-outcome cooldown, upgrade-preserves-cooldown, fresh-place defers 1 frame, flash>stun>walk priority, AoE scan) all pass in `test_towers` / `test_enemies`.

---

## Issues

None.

## Passed

- ROM exactly hits the 32 KB ceiling without overflow.
- Three review-fix code paths (F1/F2/F3) are present, well-commented in source, and each has a dedicated unit test.
- `tower_select.h` correctly joins the `<stdint.h>`-only header-only family for host testability.
- `enemy_t` remains encapsulated; only the stun API is exported.
- ARMORED wave counts match spec exactly (1/1/2/3 across W7–W10).
- All 9 host test binaries pass cleanly.
