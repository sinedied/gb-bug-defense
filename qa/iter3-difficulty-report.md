# QA Report — Iter-3 #20 Difficulty modes

## Verdict: ISSUES FOUND (low/medium — docs only; ship-blocking? **NO** — recommend SHIP with doc fix follow-up)

## Layer 1 — Dev workflow
| Check | Result |
|-------|--------|
| `just build` | ✅ ROM 32 768 bytes (== 32 KB cap) |
| `just test` (6 binaries) | ✅ test_math, test_audio, test_pause, test_game_modal, test_anim, test_difficulty all pass |
| `just check` | ✅ ROM ≤ 32 KB, cart=0x00 |
| `just run` (mGBA launch) | ✅ Launched cleanly, no crash; killed after ~4 s |
| ROM headers | ✅ Nintendo logo bytes at 0x104 match (`ce ed 66 66 ...`); cart=0x00; CGB=0x00 (DMG); MBC none |

Note: 3 pre-existing SDCC "EVELYN the modified DOG" warnings in `menu.c:140/151` and `towers.c:152` — unchanged from prior iterations, not introduced by #20.

## Layer 2 — Code presence
| Check | Result |
|-------|--------|
| `src/difficulty_calc.h` exists, `<stdint.h>`-only, no GBDK | ✅ |
| `DIFF_*` enum (EASY/NORMAL/HARD/COUNT) | ✅ |
| `DIFF_ENEMY_HP[3][2]` 2-D table = `{{2,4},{BUG_HP,ROBOT_HP},{5,9}}` | ✅ |
| `difficulty_scale_hp/interval/starting_energy` + cycle wrap | ✅ |
| `src/game.c`: `static u8 s_difficulty = DIFF_NORMAL;` + getter/setter | ✅ (lines 28, 34–36) |
| `src/title.c`: selector at row 10 cols 5..14 | ✅ (`DIFF_ROW=10`, `DIFF_COL=5`, 10 tiles wide) |
| `title_render` services `s_diff_dirty` first then `return;` (F1 fix) | ✅ (lines 101–105) |
| `enemies.c` reads `game_difficulty()` for HP scaling | ✅ (line 73) |
| `waves.c` reads it for spawn interval scaling | ✅ (line 154) |
| `economy.c` reads it for starting energy | ✅ (line 13) |
| `tools/gen_assets.py` has `<` and `>` in FONT dict | ✅ (lines 129, 138) |
| `tests/test_difficulty.c` has T1–T9 (9 tests) | ✅ all 9 named, all pass |
| `justfile` `test` recipe builds + runs `test_difficulty` | ✅ |

## Layer 3 — Regression
| Check | Result |
|-------|--------|
| All previously-passing host tests still pass | ✅ |
| Boot smoke in mGBA | ✅ no crash |
| Boot chime / audio_init untouched | ✅ (no diff in audio paths; cannot audibly verify in unattended run — host audio test still passes) |
| Pause modal precedence unchanged | ✅ test_pause + test_game_modal pass |
| Animations untouched | ✅ test_anim passes |

## Layer 4 — Manual checks (doc audit; in-emulator gameplay = MANUAL-REQUIRED)
| Check | Result |
|-------|--------|
| `tests/manual.md` contains iter-3 #20 scenarios D1–D14 | ⚠️ **partial** — D2, D3, D4, D11, D12, D13, D14 are well-formed table rows; **D1 and D5–D10 are mangled** (missing the `\| Dx \| <name> \|` label/title columns; rows start with stray fragments like `title.`, ` press START.`, ` wait through W1 grace…`, ` back at title.`). |
| F1 regression scenario added | ⚠️ **present but scrambled** — heading reads `### Iter-3 # F1 (MEDIUM) regression: title VBlank BG-write budget20` (trailing `20` out of place), and body sentences have transposed words: `is notnone`, `remain  nopristine`, `of holding1`, `in a single  past the ~16-write budget.VBlank`. The 5 numbered steps are still readable and the test intent is clear. |

## Issues

### I1 — `tests/manual.md` iter-3 #20 table partially scrambled
- **Severity**: medium (docs only, no code impact, doesn't block ship)
- **Steps to reproduce**: `sed -n '320,333p' tests/manual.md` — lines 320, 324–329 begin with whitespace + a stray sentence fragment instead of the expected `| Dx | <scenario name> | <steps> |` table row prefix.
- **Expected**: 14 well-formed rows D1–D14 (matching the format already used by D2, D3, D4, D11–D14).
- **Actual**: 7 rows (D1, D5, D6, D7, D8, D9, D10) are missing their first two cells; only the "Expected" cell is intact. Markdown still renders but a tester following the doc cannot identify which scenario is which.

### I2 — F1 regression block has transposed/misplaced characters
- **Severity**: low (still readable, intent clear)
- **Location**: `tests/manual.md` lines 340–356
- **Examples**: heading ends `…budget20 ` (the `20` belongs in `#20`); `notnone`, `nopristine`, `holding1`, `single  past`, sentence-ending `VBlank` floats after the period.
- **Effect**: Looks like a copy/paste glitch — 5 readable numbered steps below survive, so the regression check is still executable.

## Passed
- All 6 host test binaries (math, audio, pause, game_modal, anim, difficulty/T1–T9)
- ROM builds at exactly 32 KB cap, valid DMG headers, mGBA launches cleanly
- All required code symbols present at correct locations; F1 fix (`return;` after selector) confirmed in `title.c`
- All 3 gameplay modules (`enemies.c`, `waves.c`, `economy.c`) correctly read `game_difficulty()`
- `title.c` consumes `game_difficulty()` only for UI, not gameplay (separation respected)

## Recommendation
**SHIP** — all code, build, and automated test gates pass. The two issues are pure documentation glitches in `tests/manual.md` that a follow-up doc PR can fix; they do not affect the ROM, the engine, or any acceptance criterion.

Manual gameplay validation (HP feel on EASY vs HARD, spawn-rate feel, starting-energy display, persistence across game-over) remains MANUAL-REQUIRED — mGBA on macOS has no headless input driver, so this QA pass cannot drive the title selector or play through W1.
