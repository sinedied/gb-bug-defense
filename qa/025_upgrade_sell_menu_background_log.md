# QA Log: Feature #25 — Upgrade/Sell Menu Background

## Session 1 — 2025-07-16

### Scope
Dev-workflow validation only (build, test, check). Visual/emulator acceptance
scenarios not yet tested (require mGBA runtime).

### Commands Tested

| # | Command | Result | Key Output |
|---|---------|--------|------------|
| 1 | `just build` | ✅ PASS | `Built build/bugdefender.gb (65536 bytes)` — compiled all sources, warnings are optimizer info only (SDCC warning 110) |
| 2 | `just test` | ✅ PASS | All 13 host-side tests passed: test_math, test_audio, test_music, test_pause, test_game_modal, test_anim, test_difficulty, test_enemies, test_towers, test_maps, test_save, test_score, test_gate |
| 3 | `just check` | ✅ PASS | `ROM check OK (65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr=0x33)` — size, cart type, ROM/RAM bank, header checksum all validated |

### Issues Found
None.

### Notes
- SDCC optimizer warnings (110) in `menu.c` and `towers.c` are expected and benign.
- `caution: -yp0x0149=0x01 is outdated` from makebin is informational, not an error.
- Visual acceptance scenarios (1–11 from spec) require mGBA emulator runtime and are not covered in this session.
