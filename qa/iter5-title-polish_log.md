# QA Log: Title Screen Polish (iter5-title-polish)

## Date: 2025-01-27

## Test Environment
- macOS (Darwin, Apple Silicon)
- GBDK 4.2.0 (via `just setup`)
- mGBA (Homebrew, Qt frontend)
- ROM: build/bugdefender.gb (65536 bytes)

## Dev Workflow

| Command | Result | Notes |
|---------|--------|-------|
| `just check` (build + test + ROM validation) | ✅ | ROM=65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr=0x33. All 14 host tests pass. |
| `just run` (mGBA launch) | ✅ | mGBA opens without crash, ROM runs. |

## Code Inspection Checklist

| Check | Result | Details |
|-------|--------|---------|
| `title_tilemap` row 16 all spaces | ✅ | All 20 bytes = 32. No `(C)` or `2025` ASCII anywhere. |
| `TILE_ARROW` defined as `(MAP_TILE_BASE + 52)` | ✅ | `res/assets.h` line 64 |
| `MAP_TILE_COUNT` = 53 | ✅ | `res/assets.h` line 65 |
| `map_tile_data` = 848 bytes (53×16) | ✅ | Verified via Python extraction |
| `draw_selector` writes only 7 cells | ✅ | Loop `for (i = 0; i < 7; i++)` writes `label[i]` |
| `draw_focus_now` writes `(u8)TILE_ARROW` | ✅ | Line 103 of title.c |
| Constants: DIFF_COL=7, MAP_COL=7, FOCUS_COL=5, DIFF_W=MAP_W=7 | ✅ | Lines 40-47 of title.c |
| `s_diff_labels` each exactly 7 chars | ✅ | "CASUAL ", "NORMAL ", "VETERAN" |
| `s_map_labels` each exactly 7 chars | ✅ | " MAP 1 ", " MAP 2 ", " MAP 3 " |
| Arrow tile pixel art matches spec | ✅ | 2bpp decode confirms filled right-triangle pattern |

## Acceptance Scenarios (by code inspection)

| # | Scenario | Result | Notes |
|---|----------|--------|-------|
| 1 | Copyright removed | ✅ | Row 16 is all spaces in title_tilemap |
| 2 | Arrow on focused selector | ✅ | draw_focus_now writes TILE_ARROW at col 5 of focused row (default=row 10) |
| 3 | No angle brackets | ✅ | draw_selector writes only 7-char label, no < or > |
| 4 | Focus toggle (DOWN) | ✅ | s_title_focus ^= 1; s_focus_dirty = 1 → draw_focus_now clears old, paints new |
| 5 | Focus toggle back (UP) | ✅ | Same mechanism |
| 6 | Difficulty cycle | ✅ | focus==0 routes LEFT/RIGHT to difficulty_cycle_*, sets s_diff_dirty |
| 7 | Map cycle | ✅ | focus==1 routes LEFT/RIGHT to map_cycle_*, sets s_map_dirty |
| 8 | PRESS START blink | ✅ | s_blink % 30 toggles visibility, draw_prompt_now at row 13 cols 4-15 (12 tiles) |
| 9 | HI line correct | ✅ | draw_hi_now writes "HI: NNNNN" at row 15 cols 5-13 |
| 10 | Game starts | ✅ (inferred) | title.c doesn't handle START (main state machine does); no code changed there |
| 11 | No stale tiles after cycling | ✅ | Cols 14-15 start as spaces from tilemap blit, never overwritten by new draw_selector |
| 12 | Title art unaffected | ✅ | Tilemap rows 0-9 contain expected tile values (block letters, DEFENDER, circuit line, icons) |

## Visual Smoke Test

- mGBA launched ROM without crash
- Process ran stably for 5+ seconds
- No interactive/screenshot verification possible (chrome-devtools not applicable to native mGBA window)

## Edge Cases Probed

- All 3 difficulty labels verified as exactly 7 chars (no buffer overrun risk)
- All 3 map labels verified as exactly 7 chars
- Col 6 (gap between arrow and label) never written post-tilemap blit — stays space
- Worst-case VBlank budget: 12 writes/frame (prompt blink) within 16-write cap
- TILE_ARROW evaluates to 180 (128+52) — within 0-255 BG tile range

## Issues Found
None.
