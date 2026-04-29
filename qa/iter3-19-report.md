# QA Report — Iter-3 #19 Score & High Score (SRAM)

**Date:** 2025-04-29
**Verdict:** ✅ **SHIP**

## Layer 1 — Dev workflow

| Check | Result |
|---|---|
| `just build` succeeds | ✅ |
| ROM size = 65536 bytes exactly | ✅ |
| `just test` — all 12 binaries pass | ✅ (test_math, test_audio, test_music, test_pause, test_game_modal, test_anim, test_difficulty, test_enemies, test_towers, test_maps, test_save, test_score) |
| `just check` passes (cart=0x03, rom=0x01, ram=0x01, hdr=0x4e) | ✅ |
| `just run` launches mGBA cleanly (killed after 6s) | ✅ |
| Nintendo logo at 0x104..0x133 intact | ✅ (matches Pan Docs reference: `ce ed 66 66 cc 0d …`) |
| Header checksum at 0x14D matches Pan Docs algo | ✅ (computed=expected=0x4e) |

Build emits a benign `caution: -yp0x0149=0x01 is outdated.` notice from makebin
(harmless — header bytes still verified by `just check`).

## Layer 2 — Code presence

| Check | Result |
|---|---|
| `src/save_calc.h` exists, `<stdint.h>` only | ✅ |
| `src/save.{h,c}` exist | ✅ |
| `static u16 s_hi[…]` lives ONLY in `src/save.c` | ✅ (line 9; no external refs — `s_hi_dirty` in title.c is a different symbol, a U8 paint flag) |
| `save_init` reset order: slots → version → magic LAST | ✅ (slots zeroed first, version written, magic 4 bytes written last; ordering invariant documented in comment block lines preceding the loop) |
| `src/score_calc.h` exists, `<stdint.h>` only | ✅ |
| `src/score.{h,c}` exist, no local hi-score cache | ✅ |
| `difficulty_score_mult_num()` returns {8,12,16} for {EASY, NORMAL, HARD} | ✅ |
| `main.c` calls `save_init()` AFTER `audio_init()` | ✅ (lines 13→14) |
| `game.c::enter_playing` → `score_reset()` | ✅ (line 96) |
| `game.c::enter_gameover` adds win bonus, checks/saves HI, passes `new_hi` | ✅ (lines 108–119) |
| `title.c` 5-flag priority chain | ✅ (`s_diff_dirty > s_map_dirty > s_focus_dirty > s_hi_dirty > s_dirty`) |
| Title HI line at row 15 cols 5..13 | ✅ (HI_ROW=15, HI_COL=5, HI_W=9) |
| `gameover_enter(bool win, u16 final_score, bool new_hi)` signature | ✅ |
| Gameover SCORE row 14, banner row 13, painted inside DISPLAY_OFF | ✅ |
| `projectiles.c` captures `etype` BEFORE damage call, then `score_add_kill(etype)` | ✅ (line 83 captures, line 87 awards) |
| `waves.c` exposes `waves_just_cleared_wave()` | ✅ |
| `enemies.h::enemies_type(idx)` accessor | ✅ |
| `tests/test_save.c::t_save_init_partial_reset_recovers` (F1 regression) | ✅ |
| `tests/test_score.c` covers kills, multipliers, saturation | ✅ (multiple cases, host run passes) |
| `justfile` builds + runs both new tests | ✅ |
| Build flags `-Wl-yt0x03 -Wm-yo4 -Wm-ya1 -Wm-yp0x149=0x01` | ✅ |
| `tests/manual.md` scenarios 28–32 present | ✅ |

## Layer 3 — Regression

| Check | Result |
|---|---|
| All 10 prior host test binaries still pass | ✅ |
| mGBA accepts MBC1+RAM cart | ✅ (no errors in stderr, only a Qt backing-store DPR notice) |
| mGBA writes `gbtd.sav` next to ROM | ✅ |

Did not visually re-validate every prior gameplay system (pause / sell /
animations / music / difficulty / maps / 3rd enemy & tower) — the hooks
that change with this iter are isolated to score/save and the gameover
signature (callers updated in game.c). All host tests covering those
subsystems pass.

## Layer 4 — SRAM persistence smoke

After fresh boot:

```
$ xxd -l 32 build/gbtd.sav
00000000: 4742 5444 0100 0000 0000 0000 0000 0000  GBTD............
00000010: 0000 0000 0000 0000 ffff ffff ffff ffff  ................
```

| Check | Result |
|---|---|
| `build/gbtd.sav` created by mGBA | ✅ |
| Starts with ASCII magic `GBTD` (47 42 54 44) | ✅ |
| Version byte 0x01 immediately after magic | ✅ |
| 9 score slots zeroed (18 bytes) + pad zero | ✅ |
| File size ≥ 24 bytes | ✅ (8192 — full mGBA SRAM page; payload region matches expected SRAM layout) |

This is the only host-observable proof the cart is read/written as
MBC1+RAM+BATT, and it confirms the F1-fixed write order produces a
valid stamp on a clean cart.

## Defects

None.

## Verdict

**SHIP.** All four layers pass. No issues found.
