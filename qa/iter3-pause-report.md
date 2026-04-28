# QA Report — Iter3 #22 Pause Menu

**Verdict:** ✅ SHIP

## Layer 1 — Dev workflow
| Check | Result |
|-------|--------|
| `just build` | ✅ ROM = 32768 bytes (= 32 KB cap, exactly at limit, passes ≤32K) |
| `just test` | ✅ test_math, test_audio, test_pause, test_game_modal — all 4 pass |
| `just check` | ✅ ROM check OK (32768 bytes, cart=0x00, ≤ 32 KB) |
| `just run` | ✅ mGBA launched (PID 44293), ran 4 s, killed cleanly. Only stderr was a benign Qt DPR warning. |
| ROM headers | ✅ Nintendo logo bytes at 0x104 match canonical sequence; 0x143=0x00 (DMG), 0x147=0x00 (no MBC) |

Note: SDCC emits 3 pre-existing "EVELYN the modified DOG" optimizer notes (menu.c × 2, towers.c × 1) — same as prior iterations, not a regression.

## Layer 2 — Code presence
| Check | Result |
|-------|--------|
| `src/game_modal.h` exists (header-only predicate) | ✅ |
| `src/game.c` includes `game_modal.h` & calls `playing_modal_should_open_pause(...)` | ✅ (line 17 include, line 161 call) |
| `bool menu_was_open = menu_is_open();` latch at top of `playing_update` | ✅ (line 97) |
| `tests/test_game_modal.c` exists with ≥4–5 distinct tests | ✅ 5 tests (T1 F1-regression, T2 happy, T3 idempotency, T4 no-START sweep of 8 combos, T5 menu-still-open) |
| `justfile test` recipe compiles + runs `test_game_modal` | ✅ Stanza present and last in chain; output line confirmed |
| `src/pause.{h,c}` exist | ✅ |
| 6 new sprite glyphs A,R,M,Q,I,T in `tools/gen_assets.py` | ✅ All 6 bitmap rows + SPR_GLYPH_* defines + table entries present (defines #29–#34) |
| `OAM_MENU_COUNT = 16` in `src/gtypes.h` | ✅ (line 39) |
| `OAM_PAUSE_BASE`/`OAM_PAUSE_COUNT` aliases | ✅ (lines 40–41, alias OAM_MENU_BASE / OAM_MENU_COUNT) |
| `enter_gameover` WIN branch has explicit `return;` | ✅ (game.c line 153, with explanatory comment lines 144–147) |

## Layer 3 — Regression
| Check | Result |
|-------|--------|
| Existing host tests (math/audio/pause) still pass | ✅ |
| Boot smoke (mGBA) — no crash | ✅ Process stayed alive 4 s, no stderr errors |
| Boot chime invariant (D9: audio_init preserved) | ✅ test_audio asserts NRxx writes; passes (audible verification = MANUAL) |

## Layer 4 — Manual checks
`tests/manual.md` documents 11 pause-related scenarios under "Pause menu (iter-3 #22)":
1. Open / resume (happy)
2. Selection wrap (DOWN/UP wrap)
3. Quit-to-title (full reset)
4. Modal precedence over upgrade/sell menu (the F1 regression scenario)
5. Gameover wins over pause (LOSE)
6. Gameover wins over pause (WIN) — the missing-`return;` regression
7. START on TITLE / WIN / LOSE (pause local to GS_PLAYING)
8. Held START across resume (edge-detect sanity)
9. No SFX bleed while paused
10. Passive income during pause (economy_tick runs)
11. OAM slot positions while paused

**All 11 remain MANUAL-REQUIRED** — mGBA on macOS still has no headless input driver, identical constraint as previous iterations. The F1 fix's *logic* is covered automatically by `test_game_modal.c` T1; the *integration* (real GBDK build, real input/menu/pause modules wired together) needs eyes on the emulator.

## Issues
None.

## Summary
- 5/5 dev workflow checks ✅
- 10/10 code-presence checks ✅
- 3/3 regression checks ✅
- 11 manual scenarios documented (all flagged MANUAL-REQUIRED, as expected for this platform)
- 4 host test binaries, 4/4 pass

**Verdict: SHIP.**
