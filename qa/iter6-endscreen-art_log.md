# QA Log: iter6-endscreen-art

## Session 1

**Date**: 2025-01-20  
**Spec**: `specs/iter6-endscreen-art.md`  
**Build command**: `just check`

### Build & Tests
- `just check` passes: ROM = 65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr=0x33
- All 14 host tests pass (test_math, test_audio, test_music, test_pause, test_game_modal, test_anim, test_difficulty, test_enemies, test_towers, test_maps, test_save, test_score, test_gate, test_range_calc)
- `just assets` regenerates cleanly: font=2048 B, map_tiles=944 B, sprites=688 B

### Static Verification (tilemap data)
All verifications are against the generated `res/assets.c` and `res/assets.h`.

#### assets.h
- [x] 6 new `#define TILE_ENDSCR_*` at indices MAP_TILE_BASE+53..58
- [x] `MAP_TILE_COUNT` = 59

#### map_tile_data
- [x] 944 bytes (59 tiles × 16 bytes each)
- [x] Tile 53 (STATIC): pixel art matches spec exactly (dark noise pattern ~80% dark)
- [x] Tile 54 (BROKEN_HL): pixel art matches spec exactly (fragmented trace rows 3-4)
- [x] Tile 55 (SKULL_T): pixel art matches spec exactly (dome + eye sockets + teeth)
- [x] Tile 56 (SKULL_B): pixel art matches spec exactly (jaw + neck)
- [x] Tile 57 (SHIELD_T): pixel art matches spec exactly (flat top + solid mass + taper)
- [x] Tile 58 (SHIELD_B): pixel art matches spec exactly (point continuation)

#### lose_tilemap (360 tiles = 20×18)
- [x] Row 0: all STATIC (181) — full-width dark noise band
- [x] Row 1: "     GAME OVER      " — 'G' at col 5 (centered per floor((20-9)/2)=5)
- [x] Row 2: all STATIC (181) — full-width dark noise band
- [x] Row 3: all spaces (blank)
- [x] Row 4: [S,S, NODE, BROKEN_HL×14, NODE, S,S] — broken circuit divider
- [x] Row 5: all spaces (blank)
- [x] Row 6: [S×4, BUG_T, BROKEN_HL×3, S, COMP_TL_D, COMP_TR_D, S, BROKEN_HL×3, BUG_T, S×4]
- [x] Row 7: [S×4, BUG_B, S×3, S, COMP_BL_D, COMP_BR_D, S, S×3, BUG_B, S×4]
- [x] Row 8: all spaces (blank)
- [x] Row 9: [S,S, NODE, HLINE×14, NODE, S,S] — clean intact circuit divider
- [x] Row 10: [S×6, BUG_T, S×2, SKULL_T, S×2, BUG_T, S×7] — death motif
- [x] Row 11: [S×6, BUG_B, S×2, SKULL_B, S×2, BUG_B, S×7]
- [x] Row 12: all spaces (blank — breathing room)
- [x] Rows 13-15: all spaces (RESERVED for code-drawn overlays)
- [x] Row 16: all spaces (blank)
- [x] Row 17: all STATIC (181) — bottom dark noise band

#### win_tilemap (360 tiles = 20×18)
- [x] Row 0: all spaces (bright, open — contrast with lose)
- [x] Row 1: "      VICTORY!      " — 'V' at col 6 (centered per floor((20-8)/2)=6)
- [x] Row 2: all spaces (blank)
- [x] Row 3: [S,S, NODE, HLINE×14, NODE, S,S] — clean circuit divider
- [x] Row 4: all spaces (blank)
- [x] Row 5: [S×4, TW_T, HLINE×3, S, COMP_TL, COMP_TR, S, HLINE×3, TW_T, S×4]
- [x] Row 6: [S×4, TW_B, S×3, S, COMP_BL, COMP_BR, S, S×3, TW_B, S×4]
- [x] Row 7: all spaces (blank)
- [x] Row 8: [S,S, NODE, HLINE×14, NODE, S,S] — clean circuit divider
- [x] Row 9: all spaces (blank)
- [x] Row 10: [S×6, TW_T, S×2, SHIELD_T, S×2, TW_T, S×7] — triumph motif
- [x] Row 11: [S×6, TW_B, S×2, SHIELD_B, S×2, TW_B, S×7]
- [x] Row 12: all spaces (blank — breathing room)
- [x] Rows 13-15: all spaces (RESERVED for code-drawn overlays)
- [x] Rows 16-17: all spaces (bright, open)

#### Visual Contrast
- [x] Lose: 120 non-space tiles, 60 STATIC (dark noise), 240 blank
- [x] Win: 60 non-space tiles, 0 STATIC, 300 blank
- [x] Win screen measurably brighter/lighter (300 vs 240 blank tiles, 0 vs 60 dark tiles)

#### Score/Prompt Overlay (gameover.c)
- [x] Banner ("NEW HIGH SCORE!"): row 13, cols 2-16 — no overlap with art (last art at row 11)
- [x] Score ("SCORE: NNNNN"): row 14, cols 4-15 — no overlap
- [x] Prompt ("PRESS START"): row 15, cols 4-15 — no overlap
- [x] Blink: 30-frame toggle (1 Hz at 60fps) — unchanged from prior implementation
- [x] START returns to title: both GS_WIN and GS_LOSE → enter_title() on J_START press

#### gfx_init Tile Loading
- [x] `set_bkg_data(MAP_TILE_BASE, MAP_TILE_COUNT, map_tile_data)` loads 59 tiles at VRAM 128+

### Visual Testing (emulator)
- Unable to test interactively in mGBA (GUI-only, no headless screenshot capability available in this environment)
- Acceptance scenarios 1-16 (visual/interactive) could not be verified via emulator
- However, static data analysis confirms all tilemaps are correctly constructed per spec

### Issues Found
None.

### Verdict
**PASS** — All build, data, and structural acceptance criteria verified. Visual verification deferred due to environment constraints (no headless emulator available).
