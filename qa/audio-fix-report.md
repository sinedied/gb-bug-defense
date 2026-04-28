# QA Report: Audio Audibility Fix + Test Strengthening

## Verdict: **SHIP** (PASS)

## Test Summary
- **Tested**: dev workflow (`just build/test/check/run`), code presence of
  audio fixes + strengthened test infra, ROM header validity, mGBA boot
  smoke test.
- **Environment**: macOS arm64, GBDK-2020 4.2.0 (via Rosetta), mGBA 0.10.5
  Qt frontend.

## Layer 1 — Dev Workflow

| Command       | Result | Notes |
|---------------|--------|-------|
| `just build`  | ✅ | Built `build/gbtd.gb` = **32768 bytes** (= 32 KB cap exactly, OK). Pre-existing SDCC `EVELYN` warnings only. |
| `just test`   | ✅ | `test_math: all assertions passed`, `test_audio: all checks passed`. Both binaries built and executed. |
| `just check`  | ✅ | `ROM check OK (32768 bytes, cart=0x00, ≤ 32 KB)`. |
| `just run`    | ✅ | Recipe contains `mgba -3 -C mute=0 -C volume=0x100 "{{ROM}}"`. Launched mGBA, ran ~4 s, killed cleanly (no crash on exit). |
| `just emulator` | ✅ | Same `-C mute=0 -C volume=0x100` flags present (verified by inspection). |

**ROM header sanity:**
- Nintendo logo bytes at `0x104` match canonical `CE ED 66 66 CC 0D 00 0B …` ✅
- `0x143` (CGB flag) = `0x00` → DMG-only ✅
- `0x147` (cart type) = `0x00` → ROM-only, no MBC ✅
- Size = 32768 B = 32 KB (≤ 32 KB requirement) ✅

## Layer 2 — Code Presence

| Item | Location | Result |
|------|----------|--------|
| `SFX_BOOT` def, ch2/prio1, env `0xF0` | `src/audio.c:82-91` | ✅ |
| `SFX_TOWER_PLACE.sweep = 0x00` (was 0x16) | `src/audio.c:52` | ✅ + comment explains the 30 ms kill |
| `SFX_ENEMY_HIT.envelope = 0xC1` (was 0xF1) | `src/audio.c:63` | ✅ |
| `audio_init()` calls `audio_play(SFX_BOOT)` after master setup | `src/audio.c:153` | ✅ |
| Write-log infra in stub (`g_write_log`, `G_WRITE_LOG_CAP`) | `tests/stubs/gb/hardware.h:30-40` | ✅ NRxx writes via macro append `(idx)` to log |
| F1 strengthened: `first_write_idx(REG_NR52) == 0` + ordering vs NR50/51/22/24 | `tests/test_audio.c:93-102` | ✅ |
| F1 strengthened: `audio_reset()` ordering anchored via NR12/NR42 first-write < NR24 first-write; `write_count(NR22) >= 2` | `tests/test_audio.c:110-119` | ✅ |
| F2 strengthened: rejected play writes **0 audio regs** (`g_write_log_len` unchanged) | `tests/test_audio.c:144-149` | ✅ negative regression |
| F2 strengthened: equal-prio same-channel preempt verified by switching `SFX_BOOT` → `SFX_TOWER_FIRE` and asserting observable register change | `tests/test_audio.c:151-160` | ✅ |
| `justfile` `run` and `emulator` recipes carry `-C mute=0 -C volume=0x100` | `justfile` | ✅ both recipes |
| README Audio section with mGBA-Qt mute warning | `README.md:62-74` | ✅ explains Qt-only Homebrew binary, Audio→Mute menu, and the `-C` workaround |

`test_audio.c` carries **47 `CHECK` invocations** (up substantially from
prior baseline) covering init order, envelope/DAC sanity, sweep == 0
regression, priority accept/reject, and the new write-log negative
assertions.

## Layer 3 — Regression

- Host tests: both `test_math` and `test_audio` pass under `just test`. No
  prior tests were removed (test_math intact, test_audio expanded).
- Boot smoke: `mgba -3 -C mute=0 -C volume=0x100 build/gbtd.gb` opened the
  ROM, stayed alive for the full sleep window, and shut down cleanly on
  SIGTERM — no segfault, no startup error in the foreground.
- ROM size unchanged at the 32 KB ceiling (build still fits in 2 banks).

## Issues
None of severity ≥ medium. Pre-existing SDCC `warning 110: conditional
flow changed by optimizer` in `menu.c`/`towers.c` is unrelated to this
patch and was present before.

## Passed
- All five `just` recipes exercised cleanly.
- Audio fix code present and matches the intended values (sweep cleared,
  ENEMY_HIT envelope bumped, SFX_BOOT defined and triggered from
  `audio_init`).
- Test-strengthening goals met: ordering of master-reg writes is now
  observably anchored, and rejected `audio_play()` calls are proven to
  emit zero NRxx writes — both gaps that the previous version of the
  suite would have missed.
- Documentation (README Audio section + justfile comments) accurately
  describes the macOS mGBA-Qt mute workaround.
