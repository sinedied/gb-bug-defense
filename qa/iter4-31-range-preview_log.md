# QA Log: Iter-4 #31 — Tower Range Preview

## Session 1

**Date**: 2025-01-27
**Verdict**: PASS

### Build Verification
- ROM build via `lcc` direct command: ✅ (exit code 0, warnings only from optimizer in menu.c/towers.c — pre-existing)
- ROM file size: 65536 bytes ✅
- ROM header at 0x147–0x149: `03 01 01` ✅ (MBC1+RAM+BATTERY, 64KB ROM, 2KB RAM)
- `just check` full pipeline (setup + build + test + header checksum): ✅

### Test Suite
All 14 host-side tests pass:
1. test_math ✅
2. test_audio ✅
3. test_music ✅
4. test_pause ✅
5. test_game_modal ✅
6. test_anim ✅
7. test_difficulty ✅
8. test_enemies ✅
9. test_towers ✅
10. test_maps ✅
11. test_save ✅
12. test_score ✅
13. test_gate ✅
14. test_range_calc ✅ (new — includes T7 regression for {0,0} valid pixel)

### Code Inspection — Sentinel Fix Consistency
- `src/range_calc.h` line 39–43: off-screen branch returns `{255, 255}` ✅
- `src/range_preview.c` line 77: sentinel check is `dot.x == 255 && dot.y == 255` ✅
- `tests/test_range_calc.c` T2/T3: clipping tests verify `{255, 255}` ✅
- `tests/test_range_calc.c` T7 (line 139–140): `range_calc_dot(28, 28, 40, 5)` returns `{0, 0}` (valid pixel, not hidden) ✅

### Integration Wiring
- `src/game.c`: `#include "range_preview.h"` (line 18), `range_preview_init()` in `enter_title()` (line 127) and `enter_playing()` (line 151), `range_preview_update(cursor_tx(), cursor_ty())` in non-modal branch after `cursor_update()` (line 259) ✅
- `src/menu.c`: `#include "range_preview.h"` (line 9), `range_preview_hide()` in `menu_open()` (line 92) ✅
- `src/pause.c`: `#include "range_preview.h"` (line 8), `range_preview_hide()` in `pause_open()` (line 88) ✅
- `src/tuning.h`: `RANGE_DWELL_FRAMES 15` defined (line 102) ✅
- `src/gtypes.h`: `OAM_RANGE_BASE 1`, `OAM_RANGE_COUNT 8` defined (lines 42–43) ✅
- `justfile`: test_range_calc compile+run in `test` recipe (lines 183–185) ✅

### Notes
- mGBA not installed; runtime/visual acceptance scenarios (1–12 from spec) could not be executed in-emulator. All verifiable aspects (build, tests, header, code consistency) pass.
- SDCC optimizer warnings (warning 110) are pre-existing and benign.
- The `-yp0x0149=0x01` caution from makebin is pre-existing (legacy flag form; byte is still correctly set as validated by header check).
