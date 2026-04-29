# Upgrade/Sell Menu Background

## Problem

The upgrade/sell menu is a sprite overlay rendered on top of the BG layer. When the menu opens over dark map tiles (path, computer, towers), the thin sprite glyphs (especially digits and colons) lack contrast and are hard to read. An opaque shade-0 BG rectangle behind the text area ensures readability over any map tile. OAM layout and sprite positions must remain unchanged.

## Architecture

### Approach: VBlank-safe read-before-write with deferred phases

All BG reads and writes happen inside `menu_render()` (called from `playing_render()` during VBlank), using two deferred phases controlled by flags set in `menu_open()` / `menu_close()`:

1. **Phase 1 — Save + Clear** (fires once on the first render after `menu_open()`): Read 12 BG tiles from VRAM via `get_bkg_tile_xy()` into a static buffer, then overwrite them with the blank tile `(u8)' '` (0x20, all shade-0).

2. **Phase 2 — Restore** (fires once on the first render after `menu_close()`): Write the 12 saved tiles back from the buffer via `set_bkg_tile_xy()`.

This deferred design avoids VRAM access outside VBlank (`menu_open()`/`menu_close()` run in the update path during active LCD scan) and keeps all BG writes in the established VBlank-safe render window.

### BG tile region

The sprite overlay is a 2-row × 7-column grid. Column 0 is the `>` selection cursor — a bold single glyph that remains readable over any background. Columns 1–6 contain the actual text and digits that need the background.

**BG rectangle: 6 columns × 2 rows = 12 tiles.**

Screen tile coordinates:
- `bg_col = (mx / 8) + 1` — one tile right of the sprite anchor (skipping cursor column)
- `bg_row = my / 8` — same tile row as the sprite anchor

Where `mx` and `my` are the pixel anchor computed identically to `menu_render()`. Both are always tile-aligned (proof: `tx_px` is `tower_tx * 8`, arithmetic preserves 8-alignment, clamp values 0 and 104 are multiples of 8; `my = ty_px ± 16` where `ty_px = (ty + HUD_ROWS) * 8`).

### Blank tile

`(u8)' '` (0x20) — the ASCII space character, fully shade-0 (white). Consistent with `gfx_clear_bg()` and HUD blank regions. No new tile needed.

### Render order change

`menu_render()` moves **before** `towers_render()` in `playing_render()`. This is a defensive ordering for adjacent-tower idle blink correctness: if a live tower adjacent to the menu target occupies a tile inside the 6×2 rect, its idle-LED blink phase may have drifted while the menu was open (the global `s_anim_frame` increments every frame, including during modal). On the close frame, `menu_render()` restores the saved (potentially stale blink-phase) tile, then `towers_render()` Phase 3 scans for stale towers and overwrites with the correct phase — last writer wins. Without the reorder, the restore would run after Phase 3 and overwrite its correction with a stale tile for up to 16 frames (one idle-scan round-robin cycle). Not a hard bug (the blink period is 32 frames) but a visible jitter that's trivially avoided by ordering.

Note: the sold tower's own BG tile is never inside the rect (the menu floats ±2 tile-rows from the anchor tower), so sell-clear vs restore is not a conflict.

New order:
```
hud_update → map_render → gate_render → menu_render → towers_render → pause_render
```

This follows the same principle as the existing `gate_render` before `towers_render` placement: "later writer wins on overlap."

### Anchor helper extraction

The anchor computation (pixel `mx`, `my` from tower index) is duplicated between `menu_open()` (for `bg_col`/`bg_row`) and `menu_render()` (for sprite placement). Extract into a `static void compute_anchor(u8 *out_mx, u8 *out_my)` helper called by both.

### Static state additions (menu.c)

| Variable | Type | Purpose |
|----------|------|---------|
| `s_bg_col` | `u8` | Screen tile column of BG rect left edge |
| `s_bg_row` | `u8` | Screen tile row of BG rect top edge |
| `s_menu_bg_buf` | `u8[12]` | Saved BG tiles (6 cols × 2 rows, row-major) |
| `s_bg_save_pending` | `u8` | 1 = save + clear on next render |
| `s_bg_restore_pending` | `u8` | 1 = restore on next render |

Total WRAM growth: **16 bytes**.

### BG write budget analysis

The menu is modal — while open, enemies/projectiles/waves are frozen, so `map_render()` (corruption), `gate_render()` (inactive when towers exist), and `towers_render()` Phase 3 (idle toggle, gated by `game_is_modal_open()`) produce zero BG writes.

| Frame | HUD | Map | Gate | Menu BG | Towers | Total | Cap |
|-------|-----|-----|------|---------|--------|-------|-----|
| Open (save+clear) | 0–3 (passive income) | 0 | 0 | **12** | 0 (gated) | 12–15 | ≤16 ✓ |
| Open (ongoing) | 0–3 | 0 | 0 | 0 | 0 (gated) | 0–3 | ≤16 ✓ |
| Close B (no sell) | 0–3 | 0 | 0 | **12** | 0–1 (idle) | 12–16 | ≤16 ✓ |
| Close A-UPG | 3 (energy) | 0 | 0 | **12** | 0–1 (idle) | 15–16 | ≤16 ✓ |
| Close A-SEL (sell) | 3 (energy) | 0 | 0 | **12** | 1 (sell-clear) | **16** | ≤16 ✓ |

Worst case is 16 on sell-close, exactly at cap. Phase 1 sell-clear returns before Phase 3 idle, so they never stack.

### API changes

None. All changes are internal to `menu.c` and the render order in `game.c::playing_render()`.

## Subtasks

1. ✅ **Extract anchor helper** — Move the sprite anchor computation (`mx`, `my` from tower index) out of `menu_render()` into a `static void compute_anchor(u8 *out_mx, u8 *out_my)` helper in `menu.c`. Both `menu_open()` and `menu_render()` call it. `menu_render()` behavior must be identical before and after this refactor. Done when `menu_render()` calls the helper and sprite positions are unchanged.

2. ✅ **Add BG state and wire open/close/init** — Add the five new static variables (`s_bg_col`, `s_bg_row`, `s_menu_bg_buf[12]`, `s_bg_save_pending`, `s_bg_restore_pending`) to `menu.c`. In `menu_open()`: after setting `s_tower_idx = idx` (the helper reads it), call `compute_anchor()` to get `mx`/`my`, compute `s_bg_col = (mx / 8) + 1` and `s_bg_row = my / 8`, set `s_bg_save_pending = 1`. In `menu_close()`: set `s_bg_restore_pending = 1`. In `menu_init()`: zero all five BG state variables (ensures no spurious restore on `enter_title()`/`enter_playing()` which fully redraw the BG). Done when `menu_open()` stores the BG rect position and `menu_close()` arms the restore flag. Depends on subtask 1.

3. ✅ **Implement BG save/clear and restore in menu_render** — Add two new phases at the **top** of `menu_render()`, before the existing `if (!s_open) return;` guard:
   - **Save + Clear phase**: If `s_bg_save_pending`, loop over the 6×2 region: read each tile with `get_bkg_tile_xy(s_bg_col + c, s_bg_row + r)` into `s_menu_bg_buf[r * 6 + c]`, then write `(u8)' '` with `set_bkg_tile_xy(s_bg_col + c, s_bg_row + r, (u8)' ')`. Clear `s_bg_save_pending`. Fall through to sprite rendering.
   - **Restore phase**: If `s_bg_restore_pending`, loop over the 6×2 region: write `s_menu_bg_buf[r * 6 + c]` back with `set_bkg_tile_xy()`. Clear `s_bg_restore_pending`. Return (no sprite rendering — menu is closed).

   Done when: opening the menu paints a white rectangle behind cols 1–6 of the sprite grid, and closing the menu restores the original BG tiles. Depends on subtask 2.

4. ✅ **Reorder menu_render before towers_render** — In `game.c::playing_render()`, move `menu_render()` to run immediately after `gate_render()` and before `towers_render()`. Update the inline comment from `/* sprite OAM only — no BG writes */` to `/* BG save/restore + sprite OAM */`. This ensures `towers_render()` Phase 3 idle-blink correction is the last writer for any adjacent tower tiles inside the 6×2 rect on the close frame (the sold tower's own tile is geometrically outside the rect — no sell-clear conflict). Done when `playing_render()` has the order: `hud_update → map_render → gate_render → menu_render → towers_render → pause_render`. Depends on subtask 3.

## Acceptance Scenarios

### Setup
1. `just build && just run` — launch the ROM in mGBA.
2. Place at least one tower on a dark tile (path-adjacent ground, or surrounded by path tiles for visual contrast).
3. Additional towers may be needed for multi-tower overlap scenarios.

### Scenarios

| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Menu readable over dark tiles | Place a tower adjacent to a path. Press A on it to open menu. | White rectangle visible behind "UPG:NN" and "SEL:NN" text. All digits and glyphs clearly readable. The `>` cursor column has no background change (remains over the map tile). |
| 2 | BG restores on B-close | Open menu on a tower over dark tiles. Press B to close. | Map tiles fully restored to their pre-menu state. No white rectangle remnant. Adjacent tower BG tiles intact. |
| 3 | BG restores on upgrade-close | Open menu on an upgradeable tower (level 0, enough energy). Press A on UPG. | Menu closes, energy decreases in HUD, and all BG tiles behind the menu are fully restored. |
| 4 | BG restores on sell-close | Open menu. Navigate to SEL, press A. | Tower is sold (removed from map, refund in HUD). The tower's BG tile is replaced with ground tile. All other BG tiles in the menu area are restored correctly. No ghost tower tile. |
| 5 | Adjacent tower in menu rect | Place two towers vertically: one at row R (for the menu target) and one at row R−1 or R+1 (within the 2-row-above/below menu rect). Open menu on the first tower. Close with B. | Both tower BG tiles are intact after menu close. The adjacent tower's tile was saved and correctly restored by the buffer. |
| 6 | Menu at left screen edge | Place a tower at column 0 or 1 (leftmost buildable). Open menu. Close with B. | BG rectangle is clamped to the screen. No visual glitches. Tiles restored correctly. |
| 7 | Menu at right screen edge | Place a tower at column 18 or 19 (rightmost). Open menu. Close with B. | BG rectangle clamped. No glitches. Tiles restored. |
| 8 | Menu flips above/below | Place a tower in play-field row 0 or 1 (menu goes below). Open and close. Then place a tower in row 3+ (menu goes above). Open and close. | Both orientations show the white background correctly and restore cleanly. |
| 9 | OAM layout unchanged | Open menu on any tower. Observe: `>` cursor appears in the leftmost column, "UPG:NN" on the top row and "SEL:NN" on the bottom row, with digits showing the correct upgrade cost and sell refund. Navigate between UPG/SEL. | Cursor `>` moves between rows. Text and digit positions are consistent with the 2×7 sprite grid layout. No sprites misaligned, missing, or flickering. |
| 10 | Corruption tiles preserved | Let enemies damage the computer (HP < 5) so corruption tiles are visible. Place a tower near the computer such that the menu rect overlaps the 2×2 corruption region. Open and close the menu. | Corruption tiles are preserved exactly as they were before the menu opened. No revert to pristine computer tiles. |
| 11 | Pause after menu close | Open menu, close with B, then immediately press START to pause. | Pause opens normally. No BG corruption from the menu restore. |

## Constraints

- **≤16 BG writes per frame**: All paths verified in budget table above. Sell-close is the tightest at exactly 16.
- **OAM unchanged**: No changes to OAM slot allocation, sprite tile assignments, or sprite positions.
- **No new files**: All changes in `menu.c` (logic) and `game.c` (render order).
- **VRAM timing**: `get_bkg_tile_xy()` reads VRAM, which is only accessible during VBlank (modes 0/1). The deferred-phase architecture ensures all reads happen inside `menu_render()` (called from `game_render()` immediately after `wait_vbl_done()`).
- **No scroll**: Game uses SCX=SCY=0, so screen tile coords = BG tile map coords.

## Decisions

| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| BG rect size | 7×2 (14 tiles, full sprite grid) vs 6×2 (12 tiles, skip cursor col) | 6×2 = 12 tiles | 14 tiles busts the 16-write budget on sell-close (14+3+1=18). The `>` cursor is a bold single glyph readable over any background. 12 fits the ≤12 constraint. |
| Blank tile | `TILE_GROUND` (shade-0 with subtle dots) vs `(u8)' '` (pure shade-0 white) | `(u8)' '` | Clean text background per the "0 = white: text background" convention. Consistent with `gfx_clear_bg()`. No subtle texture competing with text. |
| Read method | `get_bkg_tile_xy()` (VRAM read) vs `map_tile_at()` (ROM tilemap read) | `get_bkg_tile_xy()` | `map_tile_at()` returns original map tiles, missing tower BG tiles and corruption. VRAM read captures the actual current BG state. Adjacent towers would be lost on restore if using ROM read. |
| Sell-close tile conflict | Render reorder (menu before towers) vs buffer patching vs no reorder | Render reorder | Defensive: ensures `towers_render()` Phase 3 idle-blink correction is the last writer for adjacent tower tiles in the rect, avoiding a stale blink phase after restore. The sold tower's own tile is geometrically outside the rect (menu floats ±2 rows from anchor), so sell-clear is not a conflict. Follows existing "later writer wins" pattern (gate→towers). |
| Deferred vs immediate BG write | Save+clear in `menu_open()` vs defer to `menu_render()` | Deferred to `menu_render()` | `menu_open()` runs in the update path (active LCD scan); VRAM reads via `get_bkg_tile_xy()` outside VBlank return 0xFF on DMG. Deferring to render ensures VBlank-safe access. |

## Review

### Adversarial review (Decker, Claude Sonnet)

**F1 (MEDIUM): Sell-close race rationale was geometrically impossible.** The spec originally claimed the render reorder was needed because the sold tower's BG tile would be in the save buffer. The reviewer proved this is impossible: the menu floats ±2 tile-rows from the anchor tower, so the tower's BG tile at `tower_ty + HUD_ROWS` is always ≥1 row outside the 6×2 rect. **Resolved**: Updated the rationale to the correct reason (adjacent tower idle-blink phase correction on close frame) and removed the false sell-close claim.

**Note**: Subtask 2 clarified that `s_tower_idx = idx` must be assigned before `compute_anchor()` is called (the helper depends on it).

All other aspects verified correct: budget arithmetic, VBlank safety, tile-alignment proof, off-screen bounds, subtask completeness.

### Cross-model validation (Decker, GPT-5.4)

**F1 (MEDIUM): Scenario #5 horizontal towers don't overlap menu rect.** The 3-tower horizontal row setup has all towers on the same screen row, which is ≥1 row outside the menu rect (±2 rows from anchor). **Resolved**: Changed to a vertical setup where the adjacent tower is within the menu rect's 2-row span.

**F2 (MEDIUM): Scenario #9 had no testable oracle.** "Identical to pre-feature behavior" requires a baseline comparison artifact. **Resolved**: Rewritten with concrete observable expectations (cursor position, text layout, digit correctness, navigation behavior).

**F3 (MEDIUM): Computer-corruption overlap had no acceptance coverage.** The architecture justifies `get_bkg_tile_xy()` for this case but no scenario tested it. **Resolved**: Added Scenario #10 (corruption tiles preserved across menu open/close).

Edge cases verified correct by validator: upgrade doesn't change BG tile (tower tile is outside rect anyway); quit-to-title from pause after prior menu is already blocked by `menu_was_open` latch; `menu_init()` in `enter_playing()` correctly clears pending state; HUD HP cannot dirty on menu-open frame (enemies are gated).
