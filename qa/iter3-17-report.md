# QA Report — Iter-3 #17 Maps & Map-Select

## Verdict: **SHIP**

## Layer 1 — Dev workflow

| Check | Result | Notes |
|---|---|---|
| `just build` | ✅ | ROM = 32768 bytes (≤ 32 KB cap, on-the-line). 2 SDCC `evelyn` warnings in menu.c:151 / towers.c:180 — pre-existing, not from #17. |
| `just test` | ✅ | All 10 host binaries pass: test_math, test_audio, test_music, test_pause, test_game_modal, test_anim, test_difficulty, test_enemies, test_towers, test_maps. |
| `just check` | ✅ | "ROM check OK (32768 bytes, cart=0x00, ≤ 32 KB)". |
| `just run` | ✅ | mGBA launched, process stayed alive ≥4 s, killed cleanly. One benign Qt backingstore dpr warning (cosmetic, pre-existing on Apple Silicon). |
| ROM headers | ✅ | Nintendo logo at 0x104 matches reference; cart type @0x147 = 0x00 (no MBC); CGB flag @0x143 = 0x00 (DMG-only); ROM size byte @0x148 = 0x00 (32 KB / 2 banks). |

## Layer 2 — Code presence

| Item | Result |
|---|---|
| `src/map.h`: `MAP_COUNT=3`, `MAX_WAYPOINTS=10`, `map_def_t`, `map_load(u8)`, `map_active()`, `map_waypoint_count()` declared | ✅ |
| `src/map.c`: `s_maps[MAP_COUNT]` registry with 3 entries (waypoint counts 8/10/8, computer TL=(18,4) for all 3) | ✅ |
| `src/map.c`: `map_load()` clamps id, resets corruption state | ✅ (id ≥ MAP_COUNT → 0; calls `map_set_computer_state(COMPUTER_HP_MAX)`-equivalent reset) |
| `src/map.c` does NOT include `game.h` (D8) | ✅ Only `#include "map.h"` family; comment at line 8 explicitly enforces D8 |
| `WAYPOINT_COUNT` macro removed from `src/map.h` | ✅ Only mention is the deletion comment |
| `src/enemies.c::step_enemy` uses `map_waypoint_count()` | ✅ Line 154: `const u8 wp_n = map_waypoint_count();` |
| `src/map_select.h` exists with `<stdint.h>` only | ✅ No GBDK pull-in |
| `src/map_select.h` defines `MAP_SELECT_COUNT 3u` | ✅ |
| `src/title.c` has `#if MAP_SELECT_COUNT != MAP_COUNT / #error` (F1) | ✅ Lines 10–12 |
| `src/title.c` second selector row 12, focus chevron col 3, UP/DOWN edge-toggle, mutually-exclusive if/else-if branches | ✅ MAP_ROW=12, FOCUS_COL=3, single `if/else if/else if` chain ensures one action per frame |
| `src/title.c` 4-flag priority chain: diff > map > focus > prompt | ✅ Documented in header comment lines 29–30 |
| `src/game.c::s_active_map` file-scope, `game_active_map()`/`game_set_active_map()` | ✅ Lines 34, 45–48; setter clamps `m < MAP_COUNT` |
| `src/game.c::enter_playing` calls `map_load(game_active_map())` inside DISPLAY_OFF/ON | ✅ Line 76, between DISPLAY_OFF and DISPLAY_ON |
| `tools/gen_assets.py` emits 3 bundles `gameplay{1,2,3}_{tilemap,classmap,waypoints}` | ✅ Lines 1193–1207 |
| `tools/gen_assets.py` plain `const`, no legacy `gameplay_tilemap` | ✅ Old name appears only in deletion-explanation comments (lines 972, 1150) |
| `tests/test_maps.c`: cycle wrap, segment walker, per-map invariants, `CHECK_EQ(MAP_SELECT_COUNT, MAP_COUNT)` (F1) | ✅ T1 cycle, T2 includes `CHECK_EQ(MAP_SELECT_COUNT, MAP_COUNT)` at line 85, T3 invariants, T4 segment walk |
| `justfile` `test_maps` recipe links `res/assets.c` | ✅ |

## Layer 3 — Regression

| Check | Result |
|---|---|
| Previously-passing host tests still pass | ✅ All 9 prior binaries green |
| Boot smoke in mGBA — no crash | ✅ Process stable ≥4 s |
| ROM size still within budget | ✅ 32768 / 32768 (right at the line — flag for future iterations) |
| Pause / upgrade-sell tests | ✅ test_pause green |
| Animations | ✅ test_anim green |
| Music | ✅ test_music green |
| Difficulty | ✅ test_difficulty green |
| 3rd enemy / 3rd tower (#18) | ✅ test_enemies, test_towers green; map.c registry shows Map 1 still has 8 waypoints (geometry unchanged) |

## Layer 4 — Manual checks

`tests/manual.md` Iter-3 #17 section (Scenarios 17–20+) present and covers:
- Cold-boot two-selector layout (DIFFICULTY row 10, MAP row 12, focus chevron col 3)
- UP/DOWN toggles focus; LEFT/RIGHT routes to focused selector
- Map cycle wrap (MAP 1 ↔ MAP 3)
- Per-map distinct path verification + computer at TL=(18,4) end position invariant
- Map persists across game-over → title

Manual scenarios were not driven interactively (smoke-only mGBA boot), but the spec-required scenarios are documented for human/manual playtest.

## Issues

None.

## Notes / Watch items (non-blocking)

- ROM size is exactly 32768 bytes — at the cap. Any future addition will overflow without compression or refactor. Worth tracking in `memory/decisions.md`.
- Pre-existing SDCC warning 110 ("EVELYN the modified DOG") in menu.c:151 and towers.c:180 — not introduced by #17.
- Qt backingstore dpr warning from mGBA on macOS — cosmetic, pre-existing.
