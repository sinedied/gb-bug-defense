# QA Log: iter5-perf-optimization

## Session 1 — 2025-01-XX

### Environment
- macOS (Darwin, arm64)
- GBDK-2020 4.2.0 (vendor/gbdk/)
- mGBA 0.10.5_2 (Homebrew Qt frontend)
- just (Homebrew)

### Dev Workflow Commands Tested
| Command | Result | Notes |
|---------|--------|-------|
| `just check` | ✅ | ROM 65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr checksum valid, 14 tests pass |
| `just build` | ✅ | Produces build/bugdefender.gb (65536 bytes). SDCC warning 110 on menu.c/towers.c (pre-existing optimizer notes, harmless) |
| `just test` | ✅ | All 14 test binaries pass |
| `just clean` | ✅ | Removes build/ |
| `just assets` | ✅ | Regenerates res/assets.{c,h} |
| `just run` | ✅ | mGBA window opens, ROM boots (verified PID stayed alive) |

### Code Inspection Checklist

#### 1. Menu dirty flag
- [x] `menu_open()` sets `s_menu_dirty = 1` (line 87)
- [x] `menu_update()` D-pad only sets dirty on actual change: `s_sel != 0`/`s_sel != 1` guard (lines 109-110)
- [x] `menu_render()` early returns if `!s_menu_dirty` (line 193), after BG save/restore which remain unconditional
- [x] OAM paint block ends with `s_menu_dirty = 0` (line 232)
- [x] `menu_init()` resets `s_menu_dirty = 0` (line 69)
- [x] Re-opening on different tower: `menu_open()` always sets dirty=1 regardless of s_tower_idx change

#### 2. Manhattan early-reject
- [x] `acquire_target` signature: `(u8 cx_px, u8 cy_px, u16 range_sq, u8 range_px)` — 4th param added (line 271)
- [x] Call site (line 348): `acquire_target(cx, cy, range_sq, st->range_px)` where `range_sq = (u16)st->range_px * (u16)st->range_px` (line 305)
- [x] Manhattan check in acquire_target (line 282): `if (adx > (u16)range_px || ady > (u16)range_px) continue;`
- [x] Manhattan check in TKIND_STUN loop (line 329): `if (adx > (u16)st->range_px || ady > (u16)st->range_px) continue;`
- [x] Both use the SAME range_px that produced range_sq. Correctness: if adx > range_px, then adx² > range_sq, so d2 > range_sq. Never rejects valid targets.

#### 3. Range preview early return
- [x] Early return (line 45): `if (tx == s_last_tx && ty == s_last_ty && s_visible) return;`
- [x] Only fires when ALL THREE conditions hold: cursor unchanged AND dots visible
- [x] Cursor moving triggers hide: early return skipped → towers_index_at called → idx<0 → hide path runs (lines 49-54)
- [x] Dwell accumulation (s_visible=0): early return skipped because `!s_visible` → normal path runs
- [x] First arrival on tower tile: s_last_tx/ty differ → early return skipped → dwell starts

#### 4. Cursor cache
- [x] Init sentinel: `s_cache_tx = 0xFF; s_cache_ty = 0xFF;` in cursor_init() (lines 27-28). PF_COLS=20, PF_ROWS=17, so 0xFF is unreachable.
- [x] cursor_update() cache hit: `s_tx == s_cache_tx && s_ty == s_cache_ty` → use cached (line 60)
- [x] cursor_update() cache miss: recomputes, stores (lines 62-66)
- [x] Invalidation: `cursor_invalidate_cache()` sets `s_cache_tx = 0xFF` (line 33)
- [x] Called from `towers_try_place()` (line 161) and `towers_sell()` (line 212)
- [x] Map switch: `cursor_init()` called from both `enter_title()` and `enter_playing()` → cache reset
- [x] Declared in cursor.h (line 13)
- [x] test_towers.c has stub (line 137): `void cursor_invalidate_cache(void) {}`

#### 5. Projectile position cache
- [x] `ex_px = enemies_x_px(p->target)` and `ey_px = enemies_y_px(p->target)` read once (lines 61-62)
- [x] Used for direction (lines 63-66): `tx = (fix8)((i16)ex_px << 8)`, `ty = (fix8)((i16)ey_px << 8)`
- [x] Used for hit detection (lines 71-72): `dxp = (i16)ex_px - px`, `dyp = (i16)ey_px - py`
- [x] No intervening mutation of enemy state between the two uses
- [x] Same target index throughout — no index change between reads

### Performance Impact Assessment
- **Menu**: 14 OAM cells × 2 calls each = 28 GBDK function calls eliminated per frame when menu is open and no input
- **Manhattan reject**: With ranges 16/24/40 px on a 160×136 playfield, most of 224 potential pairs (16×14) rejected by cheap u8 comparison before expensive u16×u16 multiply (~100-200 cycles each)
- **Range preview**: Eliminates 16-iteration `towers_index_at()` loop every frame when cursor is stationary on tower with dots shown
- **Cursor cache**: Eliminates `towers_at()` (16-iteration loop) + `map_class_at()` every frame when cursor stationary
- **Projectile cache**: Saves 2 function calls per alive projectile per frame (was 4, now 2)

### Issues Found
- None blocking.

### Documentation Note
- README `just check` description says "≤ 32 KB, cart byte 0" but actual check validates 65536 bytes + cart=0x03 (MBC1+RAM+BATT). This is pre-existing (since iter-3 MBC1 migration) and not introduced by this PR.

### Verdict
**PASS** — All optimizations are behavior-preserving. Build, tests, and ROM launch all succeed. No regressions detected.
