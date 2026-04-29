# Roadmap: Bug Defender

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
- **Run**: `mgba -3 build/bugdefender.gb` (3× scale window).
- **Out of scope**: a fully headless test harness (`mgba-test` / Lua scripting) — the MVP relies on a human pressing keys in the SDL window. Automated/headless QA can be added later.
- **Rejected**: SameBoy (great accuracy, but CLI binary install on macOS is less consistent — requires manual build or cask); BGB (Windows-only, needs Wine).

### Task runner: just (justfile)
Expected targets:
| Target | Purpose |
|--------|---------|
| `just setup` | Install/verify GBDK-2020 toolchain into `vendor/` |
| `just build` | Compile sources → `build/bugdefender.gb` |
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
| 1 | Project skeleton & toolchain | GBDK-2020 wired in, `justfile` with `build`/`run`/`clean`/`setup`, `build/bugdefender.gb` produced and boots in mGBA showing a blank screen with the project name. | yes | — |
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

## Iteration 4 — User-feedback fixes + deeper upgrades

Based on 8 user-feedback items (all must-haves) plus 2 selected PM suggestions. Ordered: correctness/UX blockers first, then new content, then rename + art last.

### Iteration 4a — Correctness & UX fixes (do first)

| # | Feature | Description | UI | Dependencies |
|---|---------|-------------|----|--------------|
| 23 | Difficulty rebalance | Rename tiers: Easy → Casual, Normal (unchanged), Hard → Veteran. Rebalance the `DIFF_ENEMY_HP` table, spawn scaler, and starting energy so that (a) Casual is a relaxed tutorial-like experience, (b) Normal provides a real challenge, (c) Veteran is punishing but beatable from wave 1. New HP tables: Casual {1,3,6}, Normal {3,6,12} (unchanged), Veteran {4,7,14}. Veteran starting energy raised 24→28. Spawn scaler: Casual num=14 (×1.75), Normal 8 (×1.0 unchanged), Veteran num=6 (×0.8, was ×0.75 — identical numerator, change is HP-only). Update all label strings in `title.c` (`s_diff_labels`), `difficulty_calc.h` enum comments, and any docs referencing "Easy"/"Hard". Wave composition and tower stats unchanged. | yes | — |
| 24 | First-tower gate | Wave 1 timer does not start until the player places at least one tower. During the gate, a centered 2-line BG text block at play-field rows 7–8 blinks "PLACE A" / "TOWER!" (12 tiles, consistent with pause-overlay BG precedent). The existing `FIRST_GRACE` (60 frames) countdown begins only after the gate lifts. On gate lift, restore the 12 BG tiles from the map tilemap. Gate applies to every new game; not triggered on pause-resume. | yes | — |
| 25 | Upgrade/sell menu background | Before painting the menu sprite overlay, write an opaque rectangle of shade-0 BG tiles behind the text area (≤12 tiles). On `menu_close()`, restore the original BG tiles underneath (read-before-write on `menu_open()`, stored in a small static buffer). Ensures menu text is readable over any map tile. Menu sprite layout and OAM allocation unchanged. Respects the ≤16 BG writes/frame budget: open/close each happen on a single frame where no other dirty source fires (menu is modal — HUD/idle/corruption writes are gated). | yes | — |
| 26 | Upgraded tower visual | L1 towers display a distinct BG tile per type. Extend `tower_stats_t` with `bg_tile_l1` and `bg_tile_alt_l1` fields. Add 6 new tile designs to gen_assets.py: AV_L1, FW_L1, EMP_L1 (main + idle-alt each). `towers_upgrade()` sets `dirty = 1` on the tower slot so `towers_render()` writes the new tile on next VBlank. Update `towers_render()` placement and idle-animation phases to branch on `s_towers[i].level` when selecting which tile to write (`bg_tile` vs `bg_tile_l1`). | yes | — |

### Iteration 4b — New content & polish

| # | Feature | Description | UI | Dependencies |
|---|---------|-------------|----|--------------|
| 27 | Deeper tower upgrades (L2) | Add a second upgrade level: L0 → L1 → L2 for all three tower types. Extend `tower_stats_t` with `bg_tile_l2`, `bg_tile_alt_l2`, `upgrade_cost_l2`, and per-type L2 stat fields. L2 stats: AV (dmg 3, cd 30, upg cost 25), FW (dmg 6, cd 70, upg cost 30), EMP (stun 120f, cd 100, upg cost 20). Add 6 new tiles (L2 main + L2 idle-alt × 3 types) to gen_assets.py. Update `towers_can_upgrade()` to accept level 0 or 1 (max level = 2). Update `menu.c`: change the `level != 0` guard to `level >= 2`; change the `menu_render()` cost/dash branch from `level == 0` to `level < 2` so "UPG" and cost display correctly for L1 towers. `towers_get_spent()` accumulates all costs for sell refund (max = FW 15+20+30 = 65, fits u8). Score-calc unchanged. | yes | 26 |
| 28 | Title screen art | Replace the text-only title screen with a BG-art tilemap: game logo at top, stylized computer/bug motif in the center area. Selectors (difficulty row 10, map row 12), focus chevron, HI line (row 15), and "PRESS START" blink (row 13) remain functional at their current rows with unchanged logic. Budget ≤ 40 new BG tiles for the art (well within the remaining ~105 available slots). Asset generated via gen_assets.py. `title_enter()` continues to write a 20×18 tilemap via `DISPLAY_OFF` bracket. | yes | 29 |
| 29 | Rename to Bug Defender | Rename the game in all user-visible locations: ROM header internal title (11 chars: "BUGDEFENDER"), title screen tilemap text/logo tiles, README.md, AGENTS.md, `justfile` build target (`build/bugdefender.gb`), `just run`/`just check` references, and `specs/roadmap.md` header line. Single atomic commit. Internal code identifiers (`GBTD_` header guards, old ROM filename references inside `lcc` flags, etc.) are updated to match the new output filename but namespace prefixes in C macros remain `GBTD_` to avoid a massive refactor. | yes | — |
| 30 | Speed-up toggle | SELECT toggles 2× game speed via a `bool s_fast_mode` flag inside `playing_update()`. When active, the entity-tick inner block (`towers_update`, `enemies_update`, `projectiles_update`, `waves_update`) runs a second time within the same frame. `economy_tick()` and `audio_tick()` remain single-call (music stays normal tempo). Input dispatch, cursor update, modal checks, `s_anim_frame++`, and gameover checks run only once (after both ticks). HUD shows ">>" at col 18–19 row 0 when fast mode is on (replaces tower-type letter, which reappears when fast mode is off). Disabled during modals. Resets to off on new game. Note: wave-10 profiling on real DMG hardware required before shipping — worst case is 2× (16 towers × 14 enemies range checks + 8 projectile steps). | yes | — |
| 31 | Tower range preview | When cursor rests on an existing tower tile for ≥15 frames, draw the tower's range radius as a dotted circle using sprites in OAM 1..8 (a subset of the menu/pause range, idle during normal gameplay). Reuse the existing `SPR_PROJ` dot tile to avoid new sprite tiles. Preview clears immediately when cursor moves off the tower tile. `menu_open()` and `pause_open()` call `range_preview_hide()` alongside existing entity-hide calls. Disabled while any modal is open. | yes | — |

### Deferred (backlog)

| # | Feature | Description | UI | Dependencies | Rationale |
|---|---------|-------------|----|--------------|-----------|
| 32 | Wave preview in pause | Pause screen shows next wave composition (e.g., "W4: B×5 R×2 A×1"). Reads from the const wave script data. | yes | — | Nice-to-have; pause already shows current wave number. Lower priority than gameplay and UX fixes. |
| 33 | Game-over stats line | Game-over screen shows "Killed: N · Built: N · Wave: N". Requires new per-session counters in economy or score module. | yes | — | Pure polish; no gameplay impact. Deferred until core iteration 4 ships. |
| 34 | Per-difficulty HI on title | Title screen shows top HI score for all three difficulty tiers simultaneously, not just the selected one. | yes | — | Title already shows HI for current selection (iter-3 #19). Adding all three strains the ≤16 BG-writes/frame budget; marginal UX gain. |

### PM suggestions disposition
Five improvements were suggested by the PM alongside the 8 user-feedback items:
1. **Speed-up toggle (SELECT = 2×)** → **Included as #30.** High quality-of-life for repeat players; fits naturally alongside gameplay fixes.
2. **Tower range preview on cursor** → **Included as #31.** Directly supports the deeper upgrade system (#27) by helping players evaluate tower placement and coverage.
3. **Wave preview in pause screen** → **Deferred as #32.** Lower impact; pause already shows current wave number. Can revisit in iteration 5.
4. **Game-over stats line** → **Deferred as #33.** Pure polish with no gameplay impact. Low cost but also low priority vs the 8 must-have items.
5. **Per-difficulty HI display on title** → **Deferred as #34.** Current single-HI display (iter-3 #19) is adequate. Adding all 3 tiers simultaneously is tight on the title screen BG-write budget.

### Iteration 4 decisions
| Decision | Choice | Rationale |
|----------|--------|-----------|
| Difficulty tier names | Casual / Normal / Veteran | "Easy" felt dismissive; "Casual" is welcoming. "Hard" was numerically unbeatable; "Veteran" signals challenge without implying unfairness. "Normal" unchanged as the anchor tier. |
| Veteran HP reduction | 5→4 / 9→7 / 16→14 | AV L0 (dmg 1, cd 60) must kill wave-1 bugs (HP 4) before they traverse the path (480f). 4 shots × 60f = 240f/kill; two towers = ~4 kills from 5 bugs. Viable with tight play. Old values (5/9/16) required 300f/kill — mathematically unbeatable with starting energy. |
| Veteran starting energy | 24 → 28 | 28 affords 2 AV towers (20) with 8 remainder — no third tower, but enough to survive wave 1. Creates genuine opening tension without being impossible. |
| L2 upgrade scope | L0 → L1 → L2, all 3 tower types | User asked for "more upgrades." L2 is the minimum meaningful addition (one new decision point per tower). L3+ deferred — diminishing returns, tile budget pressure, and 3 upgrade levels already triples the depth. |
| Range preview OAM | OAM 1..8 (menu/pause shared range) | Projectile OAM (31..38) is actively written by `projectiles_update()` during gameplay — reusing those slots corrupts both systems. Menu/pause OAM (1..16) is idle during normal play and already cleared on session start. |
| Speed-up: entity-only double-tick | Only towers/enemies/projectiles/waves run twice | Running full `game_update()` twice would double-fire input dispatch, cursor movement, modal checks, and `s_anim_frame`. Entity-only double-tick is the smallest correct unit of acceleration. |
| First-tower prompt location | Play-field rows 7–8, centered BG text | HUD row is fully occupied (20 cols). Mid-screen BG text matches pause-overlay precedent and is unambiguous. Restored from map tilemap on gate lift. |
| Title art depends on rename | #28 depends on #29 | Art tilemap must embed the new game name. Building art tiles before the rename is settled would require rework. |
