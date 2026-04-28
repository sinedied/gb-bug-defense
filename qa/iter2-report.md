# QA Report — GBTD Iteration 2

## Verdict: SHIP (1 cosmetic regression carried over from MVP)

All dev workflows pass; ROM builds to valid 32 768 B DMG image at the
spec budget cap; all 12 host-side tests (incl. new F1 audio reset test)
pass; all 4 reviewer fixes (F1–F4) verified present in source. Boot
smoke test launches cleanly in mGBA. The remaining iter-2 acceptance
criteria require manual on-device playthrough (gated by the
known macOS mGBA headless-input/audio limitation, same as MVP QA).

---

## Layer 1 — Developer workflow

| # | Check | Result | Evidence |
|---|---|---|---|
| 1 | `just --list` recipes & descriptions | ⚠ PARTIAL | All 10 recipes listed. **D1 (cosmetic) regression: `build` description still reads `"1 bank = 16 KB" in makebin — see memory/decisions.md.)`** — the comment block above `build:` opens with a paren that shifts `just`'s "first comment line" heuristic to the wrong line. Same defect as MVP-QA D1; **not fixed in iter-2**. |
| 2 | `just setup` | ✅ PASS | `Toolchain OK: …/vendor/gbdk/bin/lcc`; mGBA + Rosetta probes succeed. |
| 3 | `just build` | ✅ PASS (with warnings) | `Built build/gbtd.gb (32768 bytes)` = exactly 32 KB ROM cap. 3 SDCC peephole warnings ("warning 110: conditional flow changed by optimizer …") in `menu.c:140`, `menu.c:151`, `towers.c:145` — pre-existing benign optimizer informational notes, not new in iter-2 (already noted in MVP QA). |
| 4 | `just test` | ✅ PASS | `test_math: all assertions passed`. 12 test functions confirmed in `tests/test_math.c` (lines 367–378): short_distance_unaffected, overflow_corner_cases, signed_cast_paths, in_vs_out_of_range_decision, sell_refund, passive_income_tick, wave_event_index_advance, tower_stats_lookup, enemy_bounty_lookup, sell_then_place_same_tile, dist_squared_extended_range, **audio_state_reset_clears_priority** (F1, line 340). |
| 5 | `just check` | ✅ PASS | `ROM check OK (32768 bytes, cart=0x00, ≤ 32 KB)` + tests pass. |
| 6 | `just clean` | ✅ PASS | `rm -rf build/`; `build/` removed. |
| 7 | `just run` | ✅ PASS | mGBA launched; ROM accepted; killed cleanly after 5 s. Only stderr output is a benign Qt/macOS HiDPI warning (`qt.qpa.backingstore: Back buffer dpr of 2 doesn't match …`). No emulator error/crash. |

---

## Layer 2 — Game functionality

### Boot smoke test
✅ PASS — `mgba -3 build/gbtd.gb` for 5 s exits cleanly via signal,
no error/warning beyond the cosmetic Qt HiDPI message.

### Iter-2 §11 acceptance criteria classification

| # | Scenario | Class | Notes |
|---|---|---|---|
| 1 | Build & host tests | **AUTOMATED-PASS** | `just check` exit 0; ROM 32 768 B; 12 tests pass. |
| 2 | Boot flow (title→playing, HUD `HP:5 E:030 W:01/10 A`) | MANUAL-REQUIRED | Need eyes on title + HUD render; mGBA on macOS has no headless screenshot path. |
| 3 | Place AV tower + SFX_TOWER_PLACE | MANUAL-REQUIRED | Spec already marks SFX as MANUAL. |
| 4 | Cycle tower type (B toggles A↔F) | MANUAL-REQUIRED | Visual HUD col 19 toggle. |
| 5 | Place FW tower (cost=15, brick tile) | MANUAL-REQUIRED | Visual + cost verification. |
| 6 | Insufficient energy → no placement | MANUAL-REQUIRED | Behavioural check. |
| 7 | Robot spawns in W3 (visually distinct, faster) | MANUAL-REQUIRED | Visual + temporal. |
| 8 | Bounty differentiates (bug +3 / robot +5) | MANUAL-REQUIRED | HUD frame-counter check. |
| 9 | Open upgrade menu (entities freeze) | MANUAL-REQUIRED | Visual + behavioural. |
| 10 | Upgrade tower (cost, faster fire, MAX line) | MANUAL-REQUIRED | Visual + behavioural. |
| 11 | Sell tower (refund, BG revert, OAM clean) | MANUAL-REQUIRED | OAM hygiene visual. |
| 12 | Cancel menu via B | MANUAL-REQUIRED | Behavioural. |
| 13 | Passive income (+6 over 1080 frames) | MANUAL-REQUIRED on device; **partial AUTOMATED** via `test_passive_income_tick` (period boundary correctness). |
| 14 | Wave progression to 10 | MANUAL-REQUIRED on device; **partial AUTOMATED** via `test_wave_event_index_advance`. |
| 15 | Win + SFX_WIN | MANUAL-REQUIRED | Spec marks SFX MANUAL. |
| 16 | Lose + SFX_LOSE | MANUAL-REQUIRED | Spec marks SFX MANUAL. |
| 17 | Replay reset (energy/HP/wave/menu/selected type) | MANUAL-REQUIRED | Behavioural; `enter_playing()` calls `audio_reset()` confirmed (game.c:55). |
| 18 | OAM hygiene across menu→lose→title | MANUAL-REQUIRED | Visual. |
| 19 | ROM size ≤ 32 768 B | **AUTOMATED-PASS** | Exactly 32 768 B. |
| 20 | E2E win run (all SFX, both tower types, upgrade+sell) | MANUAL-REQUIRED | Full playthrough. |
| 21 | E2E lose run (LOSE SFX) | MANUAL-REQUIRED | Full playthrough. |

**Summary:** 2 AUTOMATED-PASS · 0 AUTOMATED-FAIL · 19 MANUAL-REQUIRED
(2 of which have partial automated coverage via host tests).
Manual checks unaddressed by automation are documented in
`tests/manual.md` ("Iteration 2 — manual checks" section) and remain
the operator's responsibility — same constraint as MVP iteration.

---

## Layer 3 — Reviewer fix verifications

| Fix | Status | Evidence |
|---|---|---|
| **F1** audio reset on session entry + ticked in title | ✅ PASS | `src/audio.c:131` `void audio_reset(void)`; called from `src/game.c:55` inside `enter_playing()`; `audio_tick()` invoked from `GS_TITLE` dispatch (`src/game.c:131`) and `GS_WIN`/`GS_LOSE` (`src/game.c:140`). New host test `test_audio_state_reset_clears_priority` at `tests/test_math.c:340`. |
| **F2** `silence_channel` writes `NRx2 = 0x00` | ✅ PASS | `src/audio.c:119–121`: `NR12_REG = 0x00; NR22_REG = 0x00; NR42_REG = 0x00;`. Comment at `src/audio.c:113–117` accurately describes "DAC off" semantics and explicitly calls out the prior `0x08` mistake. |
| **F3** menu_open forces cursor to steady tile | ✅ PASS | `src/menu.c:53` `cursor_blink_pause(true)`; `src/menu.c:59` `set_sprite_tile(OAM_CURSOR, SPR_CURSOR_A);` immediately after, with explanatory comment at lines 54–58. |
| **F4** `tuning.h` extracted; tests use it; `-Isrc` in test recipe | ✅ PASS | `src/tuning.h` exists; `tests/test_math.c:26` `#include "tuning.h"`; no local redefinitions of `TOWER_AV_COST`, `TOWER_FW_COST`, `MAX_TOWERS`, `PASSIVE_INCOME_PERIOD`, `TOWER_RANGE_SQ` in `src/*.c` (single source of truth confirmed); `justfile:79` test recipe contains `-Isrc tests/test_math.c`. |

---

## ROM metadata

| Field | Offset | Value | Note |
|---|---|---|---|
| Size | — | 32 768 B | At 32 KB cap (spec §11 budget). |
| Nintendo logo | 0x104–0x133 | matches canonical (`ce ed 66 66 cc 0d 00 0b 03 73 00 83 …`) | Required for boot ROM check. |
| CGB flag | 0x143 | `0x00` | DMG-only ✓ |
| Cart type | 0x147 | `0x00` | ROM only, no MBC ✓ |
| ROM size | 0x148 | `0x00` | 32 KB / 2 banks ✓ |
| Old licensee | 0x14B | `0x33` | "use new licensee" ✓ |
| Mask ROM ver | 0x14C | `0x01` | iter-2 bumped from `0x00`? (informational) |
| Header checksum | 0x14D | `0x53` | Standard GBDK auto-fill. |
| Global checksum | 0x14E–F | `0x989E` | Standard GBDK auto-fill (not validated by boot ROM but cosmetically correct). |

---

## Defects

### D1 — `just --list` shows wrong description for `build` recipe (regression carried over from MVP QA)
- **Severity:** low (cosmetic — devs see misleading help text).
- **Repro:** `just --list` → `build` line reads `"1 bank = 16 KB" in makebin — see memory/decisions.md.)`.
- **Cause:** `just` extracts the first non-blank `#` comment line directly above the recipe. The 4-line comment block above `build:` opens with `# Compile and link the ROM. …` but `just`'s heuristic picks up the closing line of the parenthetical instead. Was reported as MVP-QA D1; not addressed in iter-2.
- **Fix hint:** put the human-friendly one-liner immediately above `build:` (e.g. `# Compile and link the ROM (32 KB DMG image).`) and move the makebin commentary above it or into a normal comment block separated by a blank line.

### Build warnings (informational, not defects)
3 SDCC peephole warnings (`warning 110: conditional flow changed by
optimizer: so said EVELYN the modified DOG`) at `src/menu.c:140`,
`src/menu.c:151`, `src/towers.c:145`. These are SDCC announcing that
its peephole optimizer rewrote a control-flow construct safely; no
behavioural impact. Pre-existing (MVP).

---

## Counts
- Layer 1: **6 PASS / 1 PARTIAL (D1 cosmetic) / 0 FAIL** of 7 checks.
- Layer 2: **2 AUTOMATED-PASS / 0 AUTOMATED-FAIL / 19 MANUAL-REQUIRED** of 21 acceptance scenarios; boot smoke ✅.
- Layer 3: **4 / 4 fix verifications PASS**.
- Defects: **1 low (D1, regression).**

## Final verdict: **SHIP**
The iter-2 implementation is functionally and toolchain-wise ready to
hand to a human playtester for the manual SFX/visual/playthrough
checks that the macOS+mGBA workflow cannot automate. D1 is cosmetic
and a known carry-over; recommend fixing in a follow-up housekeeping
patch but not a ship-blocker.
