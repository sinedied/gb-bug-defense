# QA Log: 028 Title Screen Art

## Session 1

**Date**: 2025-01-30  
**Tester**: QA Agent  

### Commands Tested

| # | Command | Result | Notes |
|---|---------|--------|-------|
| 1 | `just build` | ✅ PASS | Produces `build/bugdefender.gb` (65536 bytes). Warnings are optimizer info only. |
| 2 | `just test` | ✅ PASS | All 13 host-side tests pass (math, audio, music, pause, game_modal, anim, difficulty, enemies, towers, maps, save, score, gate). |
| 3 | `just check` | ✅ PASS | Full build + tests + ROM validation: `ROM check OK (65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr=0x33)` |

### Additional Verification

| Check | Result | Output |
|-------|--------|--------|
| `MAP_TILE_COUNT` in `res/assets.h` | ✅ | `#define MAP_TILE_COUNT  52` (was 35, +17 new title art tiles) |
| Row constants in `src/title.c` | ✅ | `DIFF_ROW=10`, `MAP_ROW=12`, `PROMPT_ROW=13`, `HI_ROW=15` — unchanged |

### Issues Found

None.

### Notes

- Build produces `warning 110` in `menu.c` and `towers.c` — these are SDCC optimizer diagnostics, not errors.
- The `caution: -yp0x0149=0x01 is outdated` message is a benign GBDK linker note, not an error.
- ROM size is 65536 bytes (64 KB with MBC) which matches the cart type 0x03 (MBC1+RAM+Battery).
