# QA Report — iter-3 #16 Music (post review fixes F1/F2/F3)

**Verdict: SHIP** ✅

## Layer 1 — Dev workflow (5/5 PASS)

| Check | Result |
|---|---|
| `just build` | ✅ ROM = 32 768 bytes (= 32 KB cap) |
| `just test` (7 binaries) | ✅ test_math / test_audio / test_music / test_pause / test_game_modal / test_anim / test_difficulty all pass |
| `just check` | ✅ ROM check OK (32768, cart=0x00, ≤ 32 KB) |
| `just run` (mGBA launch + terminate) | ✅ Launches cleanly, process ends cleanly |
| ROM headers valid | ✅ Nintendo logo @0x104, 0x143=0x00 (DMG), 0x147=0x00 (no MBC), 0x148=0x00 (32 KB / 2 banks) |

Compiler warnings: only the pre-existing `EVELYN the modified DOG` SDCC peephole warnings in `menu.c` and `towers.c` (unrelated to music).

## Layer 2 — Code presence (13/13 PASS)

| Check | Result |
|---|---|
| `src/music.{h,c}` exist | ✅ |
| `music.h::music_next_row()` pure inline using `<stdint.h>` only | ✅ (lines 20, 48–55) |
| `music.c` does NOT include `audio.h` | ✅ (only `music.h`, `gb/gb.h`, `gb/hardware.h`) |
| `audio.h` no `SFX_WIN` / `SFX_LOSE` enum values | ✅ (only comment noting removal) |
| `audio.c` calls `music_init/reset/tick` + `music_notify_ch4_busy/free` | ✅ (lines 126, 139, 189, 159, 184) |
| `game.c` `enter_title` → MUS_TITLE | ✅ (line 57) |
| `game.c` `enter_playing` → MUS_PLAYING | ✅ (line 86) |
| `game.c` `enter_gameover` → MUS_WIN/LOSE | ✅ (line 95) |
| `game.c` QUIT path → `music_duck(0)` | ✅ (line 131) |
| `pause.c` `pause_open` → `music_duck(1)`, `pause_close` → `music_duck(0)` | ✅ (lines 94, 105) |
| F1 guard: `music_play` calls `ch4_silence()` only when `!ch4_blocked` | ✅ (line 185) |
| F2 latch fields `ch4_just_freed` AND `ch4_arm_pending` present | ✅ (lines 121, 127) |
| `music_notify_ch4_free` sets latch, does NOT clear `ch4_blocked` | ✅ (line 330) |
| `tests/test_music.c::test_ch4_arbitration_boundary_on_unblock` exists & registered | ✅ (def line 207, registered line 260) |
| 9 test cases registered in `main` | ✅ (8 named + boundary = 9) |
| `justfile` test recipe builds + runs `test_music` | ✅ (lines 94–95) |

## Layer 3 — Regression (PASS)

- All 7 host tests pass; no regression in `test_audio` after priority preempt rewrite.
- mGBA boot smoke: ROM launches without crash; mGBA process exits cleanly.
- `audio_init()` flow preserved: SFX_BOOT chime fires before `music_init()`, which is sequenced after NR52-on and after the boot chime arms ch2.

## Layer 4 — Manual checks doc (PASS)

- `tests/manual.md` **"Music (#16) - iter-3"** section: **11 scenarios** (≥10 required) covering boot chime + title, title → playing, in-game SFX over music, win stinger, lose stinger, pause ducking, quit-to-title volume restore, loop point, idempotent play, upgrade menu non-ducking, mGBA channel viewer.
- **"Music engine: F1 / F2 cross-cutting smoke (MANUAL-REQUIRED)"** section present (lines 410–441) with both F1 (song switch preserves SFX-owned CH4) and F2 (simultaneous SFX-end + row boundary) scenarios.
- Iter-2 SFX_WIN/LOSE bullets replaced with MUS_WIN / MUS_LOSE wording (manual.md lines 109–112 explicitly note removal in iter-3 #16 / D-MUS-3).

## Minor (non-blocking)

- A few mangled word-orders / collapsed spaces in the F1/F2 manual smoke section (e.g. on lines 424, 428, 439: `"completes  no truncation.naturally"`, `"F2 latch).arm"`, `"no  there is always at least one tick of silenceclick"`). Readability nit only; intent remains clear. Worth a follow-up cleanup, does not block ship.

## Defects

None.

## Final Verdict: **SHIP** ✅
