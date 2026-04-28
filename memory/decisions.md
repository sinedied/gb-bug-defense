# Decisions

<!-- Append new decisions at the end using this format:

### <Decision Title>
- **Date**: YYYY-MM-DD
- **Context**: What prompted this decision
- **Decision**: What was decided
- **Rationale**: Why this choice
- **Alternatives**: What else was considered
-->

### Game Boy SDK: GBDK-2020
- **Date**: 2025-01-27
- **Context**: Bootstrapping a Game Boy DMG tower-defense game that must build to a real `.gb` ROM and be implementable by a small planner→coder→QA team on macOS.
- **Decision**: Use GBDK-2020 (C, with `lcc`, `png2asset`, `bankpack`, `makebin`) as the SDK for the project.
- **Rationale**: Mature, actively maintained, C-based with good macOS support, full control over tiles/sprites/banks (necessary for tower-defense entity counts and a custom HUD), strong docs and community, and integrates cleanly with hUGETracker for later audio work.
- **Alternatives**: ZGB (opinionated engine layer, weaker macOS docs, harder low-level debugging); GB Studio (no-code/event-based — cannot express tower placement, pathing, projectiles, or wave logic cleanly); RGBDS pure assembly (too slow for an MVP cycle).

### Emulator: mGBA (SDL CLI via Homebrew)
- **Date**: 2025-01-27
- **Context**: Need a single `just run` command on macOS that builds the ROM and launches it in an emulator for playtesting.
- **Decision**: Use mGBA installed via `brew install mgba`, invoked as `mgba -3 build/gbtd.gb`.
- **Rationale**: Homebrew formula installs the `mgba` SDL binary directly on `PATH`, no extra setup needed. Accurate DMG emulation. Simple invocation suits a one-command workflow.
- **Alternatives**: SameBoy (excellent accuracy but CLI install on macOS requires manual build or cask, less consistent); BGB (Windows-only, would need Wine on macOS).

### Task runner: just (justfile)
- **Date**: 2025-01-27
- **Context**: Need a single, language-agnostic command surface for build/run/clean across the team.
- **Decision**: Use `just` with targets `setup`, `build`, `run`, `clean`, `check`, `emulator`.
- **Rationale**: Lightweight, cross-shell, no Make footguns, well supported on macOS via Homebrew.
- **Alternatives**: Make (acceptable but more error-prone for newcomers); npm scripts (wrong ecosystem); shell scripts (no discoverability).

### MVP cartridge: 32 KB no-MBC
- **Date**: 2025-01-27
- **Context**: Choosing initial ROM banking for the MVP slice (1 map, 1 tower, 1 enemy, 3 waves, no audio).
- **Decision**: Ship the MVP as a 32 KB no-MBC ROM. Switch to MBC1 only when iteration 3 content (extra maps, music, SRAM high-scores) requires it.
- **Rationale**: Defers banking complexity until it's actually needed; MVP content easily fits in 32 KB.
- **Alternatives**: Start on MBC1 immediately (unnecessary complexity for the MVP); MBC5 (overkill for the entire planned scope).

### GBDK install path: pinned tarball into `vendor/gbdk/`
- **Date**: 2025-01-27
- **Context**: `just setup` must work on a fresh macOS clone with no Homebrew tap available for GBDK-2020.
- **Decision**: Download the pinned `gbdk-macos.tar.gz` release asset (currently 4.2.0) into `vendor/gbdk/` (gitignored). Apple Silicon runs the x86_64 binaries via Rosetta 2; `just setup` aborts with an install hint if Rosetta is missing.
- **Rationale**: No official Homebrew tap; pinning gives byte-reproducible builds across machines/CI; `vendor/` is self-contained and removable via `just clean-all`.
- **Alternatives**: Homebrew tap (none official, fragile); building GBDK from source (slow, more deps); manual user install (poor UX for fresh clones).

### MVP gameplay tuning numbers
- **Date**: 2025-01-27
- **Context**: Need fixed numeric values for the MVP economy/wave loop so the spec is implementable without further design negotiation.
- **Decision**: `START_HP=5`, `START_ENERGY=30`, `TOWER_COST=10`, `KILL_BOUNTY=3`, `MAX_ENERGY=255`. Bug HP=3, speed=0.5 px/frame. Tower range=24 px (3 tiles), fire rate=60 frames, damage=1, targeting="first" (highest waypoint index). Projectile speed=2 px/frame, damage=1, fire-and-forget on target death. Waves: W1=5 bugs/90 f, W2=8/75 f, W3=12/60 f; inter-wave delay=180 f; first-spawn grace=60 f. Pools: enemies=12, projectiles=8, towers=16.
- **Rationale**: 30 starting energy = 3 towers; per-kill bounty of 3 funds a 4th tower mid-W1; total bounty 75 + 30 start = 105 affords ~10 towers (well below the 16-tower pool). Pool sizes derived from worst-case W3 stall math (path traversal 480 frames / 60 f spawn = 8 alive + safety margin = 12).
- **Alternatives**: A real economy-balance pass post-MVP could adjust these; documented here so the next planner has a starting point.

### Justfile uses shebang recipes
- **Date**: 2025-01-27
- **Context**: Multi-line `if`/loops in `just` recipes don't behave as one shell unless the recipe is a shebang script.
- **Decision**: Any recipe with control flow uses `#!/usr/bin/env bash` + `set -euo pipefail`. Single-line recipes stay plain.
- **Rationale**: `set shell` only changes the shell binary, not the per-line execution model. Shebang recipes guarantee atomic-failure semantics and let `$VAR` work without `$$` escaping.
- **Alternatives**: Collapse every recipe into a one-liner (unreadable); switch to Make (rejected in roadmap).


### Skip png2asset; hand-coded asset arrays via Python generator
- **Date**: 2025-01-28
 `build/gen/*.c`). Authoring the five required PNGs (font + map tiles + sprites + three full-screen text maps) is high-friction overhead for an MVP that fits in <8 KB of asset data.
- **Decision**: Generate the asset C arrays directly with `tools/gen_assets.py` (Python + a hand-coded 57 ASCII font). Output is committed to `res/assets.{c,h}` so production builds need only `lcc`. `just assets` re-runs the generator. No `png2asset` invocation.
- **Rationale**: Per the implementation brief, the user explicitly authorised "carefully crafted .c arrays" or "Python +  whichever was faster. Plain Python (no PIL) and direct C-array emission is the fastest path and removes a brittle dependency. ROM size and tile counts remain well within 17 budgets (font 128 BG tiles + map 10 + sprites 8 = 146/384).the PIL" 
- **Alternatives**: png2asset on hand-painted PNGs (more authoring time, no ROM benefit); GBDK built-in `font.h` (works for HUD but not for full-screen tilemaps which still need external authoring).

### `lcc` flags: drop spec-mandated `-Wm-yC -Wm-yt0x00 -Wm-yo1`
- **Date**: 2025-01-28
- **Context**: 4 prescribes `-Wm-yC -Wm-yt0x00 -Wm-yo1` claiming they force a 32 KB DMG-only ROM with header checksum.spec 
- **Decision**: Build with no `-Wm-y*` flags. GBDK's `makebin` defaults already produce a 32 KB no-MBC DMG-only ROM with valid Nintendo logo + header checksum.
- **Rationale**: `makebin --help` confirms the spec misread the flags: `-yC` = "GameBoy Color **only**" (CGB-only flag at 0x143, breaks DMG boot), `-yo1` = "1 ROM bank" = 16 KB (default 2 banks = 32 KB), `-yt0x00` = MBC type 0 (already the default). Verified post-build: header byte 0x143 = 0x00 (DMG), 0x147 = 0x00 (no MBC), file size = 32768 B, header checksum 0x14D auto-computed.
- **Alternatives**: Use `-Wm-yt0x00` for explicitness only (no functional difference); kept config minimal instead.

### Towers render as BG tiles (not sprites)
- **Date**: 2025-01-29
- **Context**: Adversarial review  with up to 16 towers, 12 enemies, and 8 projectiles all sharing OAM, the player can place enough towers on a single screen row to exceed the DMG hardware limit of 10 sprites per scanline, causing flicker / dropped sprites on real hardware.F3 
- **Decision**: Render towers on the BG layer via a dedicated `TILE_TOWER` BG tile (added to `map_tile_data[]`). `towers_try_place()` flags a per-tower `dirty` bit; `towers_render()` writes the tile during the VBlank window. OAM slots `OAM_TOWERS_BASE..OAM_TOWERS_BASE+15` are now reserved-unused.
- **Rationale**: Towers are stationary and tile- a textbook fit for the BG layer. Frees 16 OAM slots and eliminates the per-scanline budget problem entirely.aligned 
- **Alternatives**: OAM defrag/Z-sort with a soft cap on towers per row (added complexity for no upside); leave as sprites and document the limit (real DMG users would see flicker).

 update
- **Date**: 2025-01-29
- **Context**: Adversarial review  the previous main loop ran `game_update()` (which performed `set_bkg_*` calls in HUD/blink/computer-damaged paths) before `wait_vbl_done()`, meaning BG writes hit VRAM during LCD mode 3 and were silently dropped on real DMG.F1 
 game_update()`. Each module is split into `_update()` (logic + sprite shadow-OAM, safe outside VBlank) and `_render()` (small VBlank-safe BG writes). Full-screen BG redraws (`title_enter`, `gameover_enter`, `enter_playing`+`map_load`) bracket their writes with `DISPLAY_ `DISPLAY_ON`.OFF` 
- **Rationale**: Cleaner than a single-phase design. Worst-case render BG-write count is ~14 tiles (HUD digits + computer 4-tile swap + 1 newly-placed tower) which fits comfortably in the ~1080-cycle DMG VBlank window. The display-off blank frame on state transitions is acceptable.
- **Alternatives**: Keep single-phase but move `wait_vbl_done()` to the top (would still require the `_update`/`_render` split for any BG write that depends on logic-derived state); use the LCD STAT interrupt to schedule writes (overkill for MVP).
