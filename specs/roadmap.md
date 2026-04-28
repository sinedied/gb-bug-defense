# Roadmap: GBTD — Game Boy Tower Defense

## Goal
A monochrome Game Boy (DMG, 160×144, 4-shade palette) tower defense game where the player defends a computer from waves of robot agents and "bugs" sent by a runaway AI. The output is a real `.gb` ROM playable in an emulator. Theme: retro cyber-defense with dark humor about runaway AI.

## Toolchain

### SDK: GBDK-2020 (C)
- **Why**: Mature, actively maintained, C-based, excellent docs, ships with `lcc`, `png2asset`, `bankpack`, `makebin` and other asset/build tooling. Gives us full control over tiles/sprites/banks — important for a TD where we manage many on-screen entities and a custom HUD.
- **Rejected**: ZGB (adds an opinionated engine, less docs on macOS, harder to debug at the metal level for a TD); GB Studio (no-code, event-based — cannot express tower placement, pathing, projectiles, or wave logic cleanly); RGBDS pure assembly (too slow for an MVP cycle).
- **Install (macOS)**: download GBDK-2020 release tarball into `vendor/gbdk`; pinned version recorded in the justfile.

### Emulator: mGBA (SDL binary via Homebrew)
- **Why**: Best macOS CLI ergonomics. `brew install mgba` provides the `mgba` SDL binary that accepts a ROM path and opens a window — exactly what `just run` needs. Accurate DMG emulation.
- **Install**: `brew install mgba` (formula — installs the `mgba` and `mgba-qt` binaries on `PATH`. The `.app` bundle is a separate `brew install --cask mgba` and is **not** required.)
- **Run**: `mgba -3 build/gbtd.gb` (3× scale window).
- **Out of scope**: a fully headless test harness (`mgba-test` / Lua scripting) — the MVP relies on a human pressing keys in the SDL window. Automated/headless QA can be added later.
- **Rejected**: SameBoy (great accuracy, but CLI binary install on macOS is less consistent — requires manual build or cask); BGB (Windows-only, needs Wine).

### Task runner: just (justfile)
Expected targets:
| Target | Purpose |
|--------|---------|
| `just setup` | Install/verify GBDK-2020 toolchain into `vendor/` |
| `just build` | Compile sources → `build/gbtd.gb` |
| `just run` | `build` then launch mGBA on the ROM (one-command playtest) |
| `just clean` | Remove `build/` |
| `just check` | Verify the ROM was produced and is a valid DMG header |
| `just emulator` | Launch mGBA on an existing ROM without rebuilding |

## Hardware constraints to respect
The planner and coder MUST treat these as hard limits:
- Resolution: **160×144**, **4 shades** (monochrome BGP). No color.
- Background: 32×32 tile map, 20×18 visible tiles (8×8 each).
- Sprites: **40 OAM entries total**, **max 10 sprites per scanline** — keep enemies + projectiles + UI cursor under this budget per row.
- Tile data: 384 tiles shared between BG and sprites in DMG (256 unique per layer with overlap). Plan a tight tileset.
- ROM: start with 32 KB (no MBC) for MVP; switch to MBC1 when content grows.
- CPU: ~4 MHz, single-frame budget tight — avoid per-frame allocations and heavy math; prefer fixed-point.
- Input: D-pad + A/B/Start/Select only. Tower placement uses a cursor.

## Iteration 1 — MVP (one playable slice)

| # | Feature | Description | UI | Dependencies |
|---|---------|-------------|----|--------------|
| 1 | Project skeleton & toolchain | GBDK-2020 wired in, `justfile` with `build`/`run`/`clean`/`setup`, `build/gbtd.gb` produced and boots in mGBA showing a blank screen with the project name. | yes | — |
| 2 | Title screen | Static title screen with game name + "PRESS START". Start transitions to the game screen. | yes | 1 |
| 3 | Map & fixed enemy path | One hand-authored map (BG tilemap) with a single fixed path from a spawn edge to the "computer" tile at the end. Path stored as a sequence of waypoints. | yes | 1 |
| 4 | Placement cursor & one tower type | Player moves a cursor with the D-pad, presses A to place a single tower type on valid (non-path) tiles. B cancels. Limited starting "energy" currency; placing costs energy. HUD shows energy + computer HP. | yes | 3 |
| 5 | One enemy type ("bug") | Bug sprite walks the path at fixed speed, has HP, despawns at end and damages computer HP. | yes | 3 |
| 6 | Tower targeting & projectiles | Tower auto-acquires nearest in-range bug each tick, fires a projectile sprite that deals damage on hit. Respect the 10-sprites-per-line limit. | yes | 4, 5 |
| 7 | Per-kill energy bounty | Killing a bug awards a small amount of energy, enabling mid-wave placements. Creates a real decision loop in the MVP. (Passive income deferred to iteration 2.) | yes | 6 |
| 8 | Wave system (1–3 waves) | Hard-coded sequence of waves with N bugs each and inter-wave delay. HUD shows current wave. | yes | 5, 6 |
| 9 | Win / lose & game over screen | Win: all waves cleared with HP > 0. Lose: HP reaches 0. Game over screen shows result + "PRESS START" to return to title. | yes | 2, 6, 8 |
| 10 | One-command playtest | `just run` builds and launches mGBA on the ROM, verified on a clean macOS checkout. | no | 1–9 |

**MVP definition of done**: from a fresh clone, `brew install just mgba` + `just setup` + `just run` opens mGBA with a playable title→game→game-over loop using one map, one tower, one enemy, three waves.

## Iteration 2 — Core depth

| # | Feature | Description | UI | Dependencies |
|---|---------|-------------|----|--------------|
| 10 | Second enemy: "robot agent" | Faster, higher HP. Introduces threat variety. | yes | MVP |
| 11 | Second tower type | E.g. slow/AoE tower (firewall) vs. single-target (antivirus). Tower-select UI in placement mode. | yes | MVP |
| 12 | Tower upgrade / sell | A on existing tower opens upgrade/sell menu; refunds partial cost. | yes | 11 |
| 13 | Passive energy income over time | Slow trickle of energy between/within waves on top of the MVP's per-kill bounty. Tightens the economy loop. | yes | MVP |
| 14 | Expanded wave script (8–12 waves) | Mixed enemy composition, ramping difficulty, boss-ish final wave. | no | 10 |
| 15 | SFX (channels 1, 2, 4) | Place, fire, hit, enemy death, win/lose stingers. | no | MVP |

## Iteration 3 — Content & polish

| # | Feature | Description | UI | Dependencies |
|---|---------|-------------|----|--------------|
| 16 | Music (channel 3 + others) | Title theme + in-game loop + game-over jingle. Use a tracker export (hUGETracker) compatible with GBDK. | no | 15 |
| 17 | Second & third map | New tilemaps with different path layouts; map-select screen. | yes | 14 |
| 18 | Third enemy + third tower | Armored bug, EMP tower (stuns), etc. | yes | 11, 14 |
| 19 | Score & high score (SRAM) | Persist best score per map via MBC1+RAM battery save. | yes | 17 |
| 20 | Difficulty modes | Easy / Normal / Hard scaling HP + spawn rate. | yes | 14 |
| 21 | Animated tiles & sprite polish | Idle animations for towers, hit flashes for enemies, computer "corruption" visual as HP drops. | yes | MVP |
| 22 | Pause menu | Start button pauses, shows wave info, allows quit-to-title. | yes | MVP |

## Deferred (out of scope)
- **Color (CGB) support** — vision is DMG monochrome.
- **Multiplayer / link cable** — large scope, low value for a TD.
- **Procedural maps** — authored maps are tighter for an 8-bit TD.
- **In-game level editor** — tooling cost dwarfs game value.
- **Cutscene / narrative system** — dark-humor flavor delivered via short HUD strings and game-over quips instead.
- **Custom MBC5 / large ROM** — MVP stays in 32 KB; MBC1 only when iteration 3 content forces it.

## Decisions
| Decision | Choice | Rationale |
|----------|--------|-----------|
| SDK | GBDK-2020 | Mature C toolchain, full hardware control, best docs/community for hobby DMG ROMs. |
| Emulator | mGBA (CLI via Homebrew) | One-command install on macOS, accurate DMG, headless-friendly for QA. |
| Task runner | just | Project convention; simple, cross-shell. |
| MVP cartridge | 32 KB no-MBC | Fits MVP content; defers banking complexity. |
| Audio (MVP) | None | Keeps MVP minimal; SFX added in iteration 2, music in iteration 3. |
| Save data | None in MVP | SRAM/MBC1 introduced with high-scores in iteration 3. |
