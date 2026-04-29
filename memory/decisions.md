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

### Iter-2: Firewall tower = single-target, slow, high-damage, longer range
- **Date**: 2025-01-30
- **Context**: Iter-2 spec (`specs/iter2.md`) needed a meaningful counterpart to the MVP "antivirus" tower without expanding `projectiles.c` for AoE.
- **Decision**: Firewall is single-target, cooldown 120 f (L0) / 90 f (L1), damage 3 (L0) / 4 (L1), range 40 px (5 tiles), cost 15. Antivirus stays single-target, cooldown 60 f / 40 f, damage 1 / 2, range 24 px, cost 10. One upgrade level only.
- **Rationale**: AoE requires a multi-target damage loop and an area-flash sprite (both expand `projectiles.c` non-trivially). High-damage single-shot at long range fills the strategic role of "anti-robot sniper" and keeps `projectiles.c` change to a one-parameter API extension (`damage` arg).
- **Alternatives**: AoE firewall (rejected — scope), chain lightning (out of scope), slow-effect debuff (requires per-enemy slow-state).

### Iter-2: B button cycles tower type; A on existing tower opens upgrade/sell menu
- **Date**: 2025-01-30
- **Context**: Two tower types need a selection mechanism; existing towers need an upgrade/sell entry point.
- **Decision**: B (no-op in MVP, "reserved for future menu" per `mvp-design.md §5`) edge-toggles `s_selected_type` between AV and FW; HUD col 19 reflects choice. A on a tile occupied by a tower opens the upgrade/sell menu (`towers_index_at(tx,ty) >= 0`); A on empty buildable tile places the currently-selected tower.
- **Rationale**: Zero-friction cycle for 2 types; reuses an MVP-reserved button; A's overload is unambiguous because tower presence is mutually exclusive with placement validity.
- **Alternatives**: SELECT (less discoverable), START opens dedicated panel (overkill for 2 types).

### Iter-2: Upgrade/sell menu = sprite overlay with entity freeze
- **Date**: 2025-01-30
- **Context**: Need an in-game menu without DISPLAY_OFF blink and without exceeding per-scanline sprite limits.
- **Decision**: 14 sprites in OAM 1..14 (formerly tower-reserved per F3). On `menu_open`: enemies + projectiles + waves are gated and their OAM hidden (`enemies_hide_all`, `projectiles_hide_all`). Modal — auto-resumes on `menu_close`. `playing_update` early-returns the frame menu opens.
- **Rationale**: Avoids BG redraw blink; uses already-reserved OAM range; freezing eliminates per-scanline math worry. Distinct from the iter-3 "pause menu" (no Start binding).
- **Alternatives**: BG-with-tile-restore (write-budget tight + race risk); DISPLAY_OFF (visible blink); HUD-mode reuse (loses HP/E/W readout).

### Iter-2: Wave script = const event lists per wave; 10 waves, 50-frame spawn floor; MAX_ENEMIES=14
- **Date**: 2025-01-30
- **Context**: Mixed bug+robot composition needs interleaving without runtime parser; pool size constrains spawn cadence.
- **Decision**: Each wave is `const spawn_event_t events[]` of `(type, delay)` pairs; 10 waves; `MAX_ENEMIES` bumped 12 → 14; `OAM_PROJ_BASE` shifted 29 → 31; spawn-interval floor of 50 frames in W8/W9/W10 to fit pool given 480-frame bug path traversal.
- **Rationale**: Const data, no parser, ~324 B ROM. Pool/spawn math: 480/50 ≈ 10 alive worst case; 14 leaves margin. Boss-feel finale via total count (28), not cadence.
- **Alternatives**: External data file (no payoff at iter-2 scale); 35-f cadence (overflows even bumped pool).

### Iter-2: SFX engine = hand-coded `audio.c`, channels 1/2/4 only, per-channel priority preempt
- **Date**: 2025-01-30
- **Context**: 6 SFX needed; ch3 (wave) reserved for iter-3 music.
- **Decision**: Hand-rolled `audio.c` with `const sfx_def_t S_SFX[]` containing `nrx1` (duty/length), `envelope` (NRx2), `sweep` (NR10 ch1), `pitches[]`, priority. Stop sequence writes `NRx2 = 0x08` (DAC off) — NOT `NRx4 = 0`. Start sequence: NRx1 → NRx2 → NRx3 → NRx4|0x80 (trigger). New `priority >= cur` preempts on the same channel.
- **Rationale**: hUGE/GBT-Player pull a tracker runtime + bank infra better suited for iter-3 music. ~720 B for 6 SFX is acceptable.
- **Alternatives**: hUGEDriver (deferred to iter-3 with music); GBT-Player (same).

### Iter-2: Stay on 32 KB no-MBC
- **Date**: 2025-01-30
- **Context**: Iter-2 adds ~2.4 KB; budget §11 in `specs/iter2.md` shows worst-case ~21 KB.
- **Decision**: No MBC1 in iter-2. Defer to iter-3 when music + extra maps land.
- **Rationale**: 11 KB headroom. Roadmap explicitly defers banking until forced.
- **Alternatives**: Pre-emptive MBC1 (unnecessary complexity).

### Iter-2 review F1: `audio_reset()` API + reset on session entry
- **Date**: 2025-01-31
 ch1 priority sticks at 3 forever, blocking every subsequent ch1 SFX (`SFX_TOWER_PLACE`, future win/lose) for the rest of the session.
- **Decision**: Add `audio_reset()` to `audio.c` (zeroes per-channel state struct + calls `silence_channel()` for ch1/2/4; does NOT touch NR50/51/52 master regs so it's safe to call repeatedly). `enter_playing()` calls it after every other module init. Title state also calls `audio_tick()` so the jingle drains naturally if the player sits at the title.
- **Rationale**: Surgical, reusable for future state transitions. Matches the existing per-module `_init()` reset pattern.
- **Alternatives**: Reuse `audio_init()` (writes NR52  overkill, possible double-init issues); only drain via title's `audio_tick()` (doesn't help when player skips the jingle entirely by mashing START).master 

### Iter-2 review F2: `silence_channel()` writes NRx2 = 0x00 (DAC truly off)
- **Date**: 2025-01-31
 DAC stays enabled, NR52 channel-on flag never clears, DC offset on real hardware.
- **Decision**: Write `NRx2 = 0x00` in `silence_channel()`. Re-arm path (`start_note()`) already writes a non-zero envelope and triggers via NRx4 bit 7, so the next `audio_play()` is unaffected.
- **Rationale**: Matches pandocs DAC behaviour; eliminates DC offset; makes NR52 flag accurate for any future audio debugging.

### Iter-2 review F3: Force cursor to steady on-phase tile when menu opens
- **Date**: 2025-01-31
 invisible cursor for the entire menu session.
- **Decision**: After `cursor_blink_pause(true)`, `menu_open()` directly writes `set_sprite_tile(OAM_CURSOR, SPR_CURSOR_A)` (the steady valid-on-phase tile). No new API  the `cursor_blink_pause` flag is preserved for when `cursor_update` re-runs after `menu_close()`.surface 
- **Rationale**: Smaller surface than a dedicated `cursor_force_steady()` API for a one-line fix.

### Iter-2 review F4: Extract pure-data tuning constants into `src/tuning.h`
- **Date**: 2025-01-31
- **Context**: `tests/test_math.c` redefined `PASSIVE_INCOME_PERIOD`, tower stats, enemy bounties, `MAX_TOWERS` etc.  tautological mirrors. If gameplay values drifted, tests still passed.locally 
- **Decision**: New header `src/tuning.h` contains ONLY pure-data constants (no `<gb/gb.h>`, no GBDK types). `gtypes.h` `#include "tuning.h"`; gameplay code keeps using the constants by the same names. `tests/test_math.c` `#include "tuning.h"` directly (justfile passes `-Isrc`). New `TOWER_*_COOLDOWN(_L1)` and `TOWER_*_DAMAGE(_L1)` macros replace the magic numbers in `towers.c`'s stats table so the test now anchors them. Bounties are pulled via `BUG_BOUNTY` / `ROBOT_BOUNTY`.
- **Rationale**: Eliminates constant drift at the type-checker level; minimal blast radius (one new file, gtypes.h slimmed, towers.c stats table de-magic-numbered).
- **Alternatives**: Compile gameplay `.c` files directly into the host test binary (would pull in `<gb/gb.h>`  too invasive); keep mirrors with a comment ( reviewers asked for real coverage).rejected shims 

### Iter-2 audio diag: boot chime + SFX audibility tuning + Qt-mGBA mute workaround
- **Date**: 2026-04-28
- **Context**: User reported NO audio in mGBA on macOS. Investigation showed three compounding causes: (1) macOS Homebrew ships only Qt mGBA (no SDL `mgba` binary; the lowercase symlink resolves to the Qt `.app` via case-insensitive APFS), and Qt mGBA can boot with `Audio > Mute` enabled or with Qt Multimedia volume effectively at  silent emulator with no in-ROM way to know; (2) `SFX_TOWER_PLACE` programmed NR10 sweep `0x16` (period 1, shift 6, down) which dropped the 11-bit frequency by ~26 every 7.8 ms and disabled the channel by hardware within ~30  the place sound was a near-inaudible pop, indistinguishable from "no audio"; (3) `SFX_ENEMY_HIT` envelope `0x71` (vol 7) was quiet enough to be missed when other channels were active.ms zero 
- **Decision**:
  1. Added `SFX_BOOT` (single ~200 ms ch2 tone, vol 15, no envelope decay) fired from `audio_init()`. Plays once at ROM start so a silent emulator is immediately diagnosable as host-side rather than ROM-side. Cost: one S_SFX entry (~24 B) + one pitch constant.
 16 frames). The volume envelope still decays the note out cleanly.
  3. Bumped `SFX_ENEMY_HIT` envelope from `0x71` (vol 7) to `0xC1` (vol 12).
  4. `justfile` `run` and `emulator` recipes now pass `-C mute=0 -C volume=0x100` to mGBA so Qt's mute toggle and any persisted-low volume can't silence the run regardless of prior user state.
  5. README has a new "Audio" section explaining the Qt-vs-SDL situation on macOS Homebrew and the manual menu toggles to check if launching mGBA outside `just run`.
  6. New host-side tests in `tests/test_audio.c` (compiles `src/audio.c` against stub headers in `tests/stubs/gb/`) lock in: NR52-master-on ordering, boot-chime trigger, priority preempt rules (lower-priority cannot interrupt active higher-priority SFX), per-frame countdown silencing single-note SFX with `NRx2 = 0x00`, multi-note advancement, and `audio_reset()` preserving NR50/51/52.
- **Rationale**: Boot chime is the simplest possible "audio works" probe and adds a well-defined ground truth for any future audio regressions. The NR10 sweep removal is a real correctness fix, not just a tuning  the previous setting made the SFX inaudible by design. Qt-mGBA mute override is belt-and-suspenders so users don't have to know about the menu toggles to hear anything.preference 
- **Alternatives**: Switch the justfile to download the SDL frontend separately ( adds another binary to the toolchain story for one host); keep the sweep and instead extend duration further ( sweep period 1 disables the channel before the longer duration helps); add an in-ROM "audio test" menu ( disproportionate UI cost vs a one-shot boot chime).rejected rejected rejected 

### Audio test stub: write-order log for ordering + write-count assertions
- **Date**: 2026-04-29
- **Context**: Two MEDIUM review findings on `tests/test_audio.c`: (F1) the "NR52 written first" test only checked the final byte value, so a regression that wrote NR50 before NR52 (the DMG quirk that drops NR10..NR51 writes while NR52 bit 7=0) would still pass; (F2) the equal-priority preempt test played `SFX_TOWER_FIRE` twice and asserted NR24 trigger, but both calls wrote identical  so a regression that silently rejected the second `audio_play()` would also pass.bytes 
- **Decision**: Extend `tests/stubs/gb/hardware.h` with an append-only write log: each `NRxx_REG` macro becomes `_NR_LOG(idx)` which records the register index in `g_write_log[]` (cap 256, bounds-checked) and yields the lvalue slot via a comma-expression (`*((..., &g_audio_regs[idx]))`). Test helpers `first_write_idx(reg)` and `write_count(reg)` query the log. F1 now asserts `first_write_idx(NR52) == 0` and `< first_write_idx({NR50, NR51, NR22, NR24})`, plus that `audio_reset()`'s NR12/NR42 silence writes precede the SFX_BOOT trigger (catches regressions that delete `audio_reset()` from `audio_init()`). F2 now plays `SFX_BOOT` then `SFX_TOWER_FIRE` (two distinct ch2/prio-1 SFX with different envelopes/pitches) and asserts post-state reflects the SECOND SFX (NR22==0xA1, NR23==0x83). Added a complementary same-prio-different-channel test (SFX_TOWER_FIRE on ch2 + SFX_ENEMY_HIT on ch4, both prio 1) confirming both stay active.
- **Rationale**: Approach A (append-only log of writes) is strictly more powerful than per-register first-write counters and only ~10 lines of stub code. The lvalue-via-comma-expression macro keeps `src/audio.c` completely untouched (no behaviour change, ROM size unchanged at 32768 B). Negative regression checks confirmed: introducing `NR50_REG = 0x77;` before `NR52_REG = 0x80;` makes the F1 ordering assertions fail; flipping the preempt comparator from `<` to `<=` (rejecting equal-priority same-channel) makes the F2 NR22/NR23 assertions fail.
- **Alternatives**: Per-register first-write sequence numbers (Approach B; sufficient for ordering but couldn't easily express "NR22  2 times"); count-only tracking (no ordering info); modifying `audio.c` to call a logging hook (invasive,  tests must not perturb production code).rejected written 

### Iter-3 #22: Pause = flag inside PLAYING + new `pause.{h,c}` module
- **Date**: 2026-05-01
- **Context**: Iteration-3 roadmap item #22 — START button pauses the game with RESUME / QUIT options.
- **Decision**: New module `src/pause.{h,c}` mirroring the iter-2 modal sprite-overlay pattern. Pause is a `bool s_open` flag inside `pause.c`; the top-level state machine stays at `{TITLE, PLAYING, WIN, LOSE}`. `playing_update` early-returns into `pause_update()` when paused, identical to the menu-open arm. New helper `game_is_modal_open()` is the single source of truth for "is gameplay frozen". `pause_open()` is a defensive no-op when `menu_is_open()`.
- **Rationale**: Render path is identical to PLAYING (same BG, only an overlay differs); a dedicated state would add transition bookkeeping with no benefit. Separate module from `menu.c` because anchor (fixed center vs tower-relative), layout (3×8 vs 2×7), action set, and exit transition (calls `enter_title`) all differ materially.
- **Alternatives**: New `GS_PAUSED` state (rejected — bookkeeping); extend `menu.c` with a mode flag (rejected — pollutes upgrade/sell logic).

### Iter-3 #22: OAM range 1..16 shared by pause + upgrade/sell menu (mutually exclusive)
- **Date**: 2026-05-01
- **Context**: Pause overlay needs 16 sprite cells; iter-2 menu uses 14. OAM is capped at 40 with hard pools.
- **Decision**: Bump `OAM_MENU_COUNT` 14 → 16. Promote previously reserved slots 15..16 into the menu range. Both modules own the same physical OAM range 1..16 — collision is impossible because `game.c` enforces single-modal-at-a-time. `menu.c::hide_menu_oam` now hides slots 1..16 on `menu_close()` even though `menu_render` only paints 1..14. Do not repurpose slots 15..16 for anything other than menu/pause overlay.
- **Rationale**: Densest OAM packing. Slots 15..16 were already reserved-unused (legacy from iter-2 F3 tower-OAM reservation).
- **Alternatives**: Carve a separate range for pause (no free slots without shrinking enemies/projectiles pool — both at hard min).

### Iter-3 #22: Same-frame gameover supersedes pause-open
- **Date**: 2026-05-01
- **Context**: If START is pressed on the same frame an enemy reaches the computer (HP→0) or the last wave is cleared, two transitions race.
- **Decision**: `playing_update` checks `economy_get_hp() == 0` and `waves_all_cleared()` *before* the START → `pause_open()` handler. Both gameover branches end with explicit `return;` (the WIN branch in iter-2 code was missing this — added as part of iter-3 #22). Gameover transitions execute first; the pause request is silently dropped that frame.
- **Rationale**: Gameover is terminal; pausing into a lost game would require special UX and feels broken to the player.
- **Alternatives**: Pause first, then enter gameover on resume (rejected — bizarre UX).

### Iter-3 #22: `audio_reset()` lives in the QUIT path, NOT in `enter_title()`
- **Date**: 2026-05-01
- **Context**: Quit-to-title from the pause menu must silence any in-flight SFX from the abandoned run. Naively adding `audio_reset()` to `enter_title()` would also fire it on every boot (since `game_init() → enter_title()` runs at ROM start), racing with `audio_init()` and risking the NR52-first ordering invariant locked-in by `tests/test_audio.c`.
- **Decision**: `playing_update`'s pause-quit branch calls `audio_reset()` immediately before `enter_title()`. `enter_title()` itself stays audio-free.
- **Rationale**: Surgical — applies the reset exactly where in-flight SFX exists. Preserves boot-time audio ordering. `audio_reset()` is idempotent so the localised call is safe.
- **Alternatives**: Add to `enter_title()` (rejected — fragile, conflicts with test_audio ordering); add a new `audio_drain()` API (overkill — `audio_reset` already does this).

### Iter-3 #22: No pause-open/close SFX in iter-3
- **Date**: 2026-05-01
- **Context**: Pause UX could include a small chime; user prompt left it optional.
- **Decision**: Pause is silent on open and close. `audio_tick()` continues running so any in-flight SFX drains naturally (per iter-2 F1).
- **Rationale**: Avoids touching `audio.c` and adding a new ordering test in `test_audio.c`. Pause is a short-duration UX; silent transition matches the iter-2 modal style.
- **Alternatives**: Add `SFX_PAUSE_OPEN`/`SFX_PAUSE_CLOSE` (deferred — minimal-audio bias).

### Iter-4 #23: Difficulty tier rename (Easy→Casual, Hard→Veteran) and selector widen
- **Date**: 2025-07-21
- **Context**: Feature #23 renames difficulty tiers and rebalances HP/spawn/energy. "VETERAN" is 7 chars but the title-screen selector only rendered 6-char labels.
- **Decision**: Widen `draw_selector` from 6-char labels (10 tiles) to 7-char labels (11 tiles). All label strings (difficulty and map) become 7 chars. `DIFF_W` and `MAP_W` change from 10 to 11. Enum symbols renamed: `DIFF_EASY`→`DIFF_CASUAL`, `DIFF_HARD`→`DIFF_VETERAN`; integer values 0/1/2 preserved.
- **Rationale**: 11 writes/frame for selector render is still under the 12-write worst case (prompt blink) and well within the 16-write VBlank cap. Abbreviating "VETERAN" to 6 chars would require misspelling or unclear abbreviation. Screen fits (col 5 + 11 = col 15, within 20-col width).
- **Alternatives**: Abbreviate to 6 chars (rejected — ugly/confusing); add `#define` aliases instead of rename (rejected — old names shouldn't persist).

### Modal-precedence helper extracted to `src/game_modal.h`
- **Date**: 2026-05-02
- **Context**: F1  `playing_update()` ran `menu_update()` then fell through to the END-of-frame START handler. Same-frame "A/B closes upgrade menu + START pressed" passed `!menu_is_open()` (just closed) and unintentionally opened pause. The bug went undetected because all pause tests linked only `src/pause.c` in isolation; modal dispatch in `playing_update()` had no host-level coverage.regression 
- **Decision**: (1) Latch `bool menu_was_open = menu_is_open();` at the top of `playing_update()` BEFORE any modal-mutating call. (2) Extract the START-gating predicate into a header-only pure function `playing_modal_should_open_pause(menu_was_open, start_pressed, menu_now_open, pause_now_open)` in `src/game_modal.h`. (3) Add `tests/test_game_modal.c` covering F1 + 4 supporting cases; wired into `just test`.
- **Rationale**: A header-only `static inline` helper keeps the predicate testable on the host with zero GBDK linkage (no game.c/towers/enemies pull-in). Latching `menu_was_open` is cheaper and clearer than an early `return;` after `menu_update()` and preserves the existing "audio_tick + economy_tick run while modal" behavior unchanged for both pause and upgrade-menu branches.
- **Alternatives**: (a) Early `return;` after the `menu_is_open()` branch (Option B in the F1  works but duplicates the `economy_tick()/audio_tick()` pair already present in the menu_open() in-frame path and the pause branch, increasing the surface area of the "drain modal SFX/income" invariant. (b) Compile `src/game.c` against a wide host-stub layer for an end-to-end test of `playing_ rejected as too invasive (game.c transitively pulls towers/enemies/projectiles/waves/economy/hud, all GBDK-dependent).update()` finding) 

### Iter-3 #21: Global anim-frame counter; idle scanner is round-robin, ≤1 BG write/frame, modal-gated
- **Date**: 2026-05-08
- **Context**: Iter-3 #21 needs tower idle animation while preserving the iter-2 ≤16 BG writes/frame budget; multiple animation systems need a synchronisation source.
- **Decision**: Single `static u8 s_anim_frame` in `game.c`, incremented at top of `game_update()`, exposed via `game_anim_frame()`. Tower idle uses `towers_idle_phase_for(frame, idx) = ((frame + (bitrev4(idx & 0xF) << 2)) >> 5) & 1` (defined in new pure header `src/towers_anim.h`) — 32-frame ON / 32-frame OFF; bitrev4 stagger spreads adjacent placement slots across the 64-frame period (slot 0 vs 1 = exactly anti-phase, satisfying QA scenario 10). `towers_render()` Phase 3 round-robin scans for one stale tower and writes ≤1 BG tile/frame; gated on `!game_is_modal_open()`. Reset `s_anim_frame=0` in `enter_playing()`.
- **Rationale**: Bounds worst-case writes (sell + place + idle are mutually exclusive within a single `towers_render` call due to `return;`-after-each-phase, so towers contribute ≤1 BG write/frame total → new worst-case 10+4+1=15); deterministic phase per session; modal gating consistent with iter-3 #22 "no BG writes during modal".
- **Alternatives**: Per-entity counters (more state, no benefit for 3 anims); paint all stale towers per frame (busts budget at 16-tower pool); LCD STAT IRQ scheduling (overkill); linear `idx<<2` stagger (clusters slots 0..3 in same half-period — failed QA scenario 10).

### Iter-3 #21: Computer state = 5 (HP-keyed, including pristine + 4 corrupted)
- **Date**: 2026-05-08
- **Context**: Roadmap #21 calls for progressive computer corruption visual as HP drops 5→0.
- **Decision**: Replace `map_set_computer_damaged(bool)` with `map_set_computer_state(u8 hp)`. Pure mapping `map_hp_to_corruption_state(hp)` lives in new pure header `src/map_anim.h` (host-testable, `<stdint.h>` only). HP=5→state 0 (pristine), HP=4→1, HP=3→2, HP=2→3, HP=1 and HP=0 both → state 4 (heavy static, reuses existing `_D` tile set). `map_load()` must reset `s_state=0` and `s_state_dirty=false` to avoid stale repaint on quit-to-title → new-game.
- **Rationale**: HP=0 reuse keeps new tile budget at 7 (under per-iter ROM growth target). HP=0 state-4 is documented as "guaranteed visible only on single-bug arrival" — simultaneous multi-damage skips state 4 because `enter_gameover` does `DISPLAY_OFF` + full-screen redraw before `playing_render` runs. Acceptable: gameover supersedes play-field anyway.
- **Alternatives**: Logic-phase `map_render()` workaround for simultaneous multi-damage (rejected — violates frame-loop split); dedicated state-4 tile set distinct from `_D` (rejected — wastes 4 tiles for no visual benefit).

### Iter-3 #21: Hit flash = 3-frame sprite override on non-killing hit only
- **Date**: 2026-05-08
- **Context**: Need visceral feedback when a projectile hits an enemy.
- **Decision**: Per-enemy `u8 flash_timer` (1 B × 14 enemies = 14 B WRAM). Set to `FLASH_FRAMES=3` (in `tuning.h`) by `enemies_set_flash()`, called from `projectiles.c::step_proj` when `enemies_apply_damage` returns false. Killing hits do NOT flash (sprite hides immediately). Decrement happens inside `step_enemy` via pure helper `enemies_flash_step(u8*)` in new header `src/enemies_anim.h` (host-testable). Because `step_enemy` is gated by `enemies_update` which `playing_update` skips during modal, `flash_timer` automatically freezes during pause/menu — do NOT add a separate tick path.
- **Rationale**: Matches "feedback on partial damage" intent; killing-hit sprite-hide is louder than any flash; modal freeze is automatic via existing gating.
- **Alternatives**: Flash on killing hit too (rejected — competes with sprite hide); shared global flash timer (rejected — multiple simultaneous hits clobber each other).

### Iter-3 #21 F1: `enemies_set_flash()` writes flash tile immediately (same-frame audio/visual sync)
- **Date**: 2026-05-12
- **Context**: Review F1 — `playing_update()` runs `enemies_update()` BEFORE `projectiles_update()`. The original `enemies_set_flash()` only armed `flash_timer`; the flash tile was selected later inside `step_enemy()` on the NEXT frame. Result: SFX_ENEMY_HIT plays one frame before the white flash sprite is visible. The previous source comment in `projectiles.c` claiming same-frame sync was incorrect.
- **Decision**: `enemies_set_flash(idx)` now also calls `set_sprite_tile(OAM_ENEMIES_BASE + idx, SPR_BUG_FLASH | SPR_ROBOT_FLASH)` immediately (selected from `e->type`). The next `step_enemy()` either keeps the flash tile (if `enemies_flash_step()` still returns true) or restores the walk anim. No new state, no new BG writes, +1 sprite-tile write per non-killing hit (sprite OAM, not BG — does not touch the ≤16 BG-writes/frame budget).
- **Rationale**: Cheaper than reordering `playing_update()` (would risk other invariants). Cheaper than per-frame ordering tests (issue is genuinely visual + timing, not host-testable — added manual check #17 in `tests/manual.md`). Cost: 1 immediate `set_sprite_tile` call inside the projectile-hit branch.
- **Alternatives**: (a) Swap `enemies_update`/`projectiles_update` order in `playing_update` — rejected, ripples through other invariants (movement-vs-collision ordering). (b) Defer SFX to next frame — rejected, distorts audio cadence. (c) Add a dedicated "pending flash" pass between updates — rejected, more state for no benefit.

### Iter-3 #21 F2: WRAM growth budget correction (32 B, not 31 B)
- **Date**: 2026-05-12
- **Context**: Review F2 — `specs/iter3-21-animations.md` line ~289 documents WRAM growth as "14 + 16 + 1 = 31 B" (per-enemy `flash_timer` + per-tower `last_state` + global `s_anim_frame`). It omits `s_idle_scan_idx` (1 B static in `towers.c`, the round-robin idle-scanner cursor introduced in the same iteration).
- **Decision**: Actual WRAM growth for iter-3 #21 is **32 B**, not 31 B. Breakdown: 14 (enemy flash_timer) + 16 (tower last_state) + 1 (game.c s_anim_frame) + 1 (towers.c s_idle_scan_idx) = 32 B. Code is correct as implemented; only the spec's documented projection was off by 1 B. Per project rule "Do NOT modify `specs/`", correction is recorded here for traceability.
- **Rationale**: 1 B over a 31 B projection is well inside any practical WRAM budget for the game; no design change warranted. Recorded so future audits don't chase the missing byte.
- **Alternatives**: Edit the spec — rejected (project rule).

### Iter-3 #20: Difficulty modes — 2-D HP table, formula spawn scaler, BG-only title selector
- **Date**: 2026-05-15
- **Context**: Roadmap #20 — Easy/Normal/Hard scaling HP + spawn rate.
- **Decision**: Difficulty is a `u8 s_difficulty` static at file scope in
  `src/game.c`, initialised to `DIFF_NORMAL`, never reset by `enter_title()` or
  `enter_playing()` (so quit-to-title preserves selection within a power-on
  session; SRAM is feature #19). Enum `{DIFF_EASY=0, DIFF_NORMAL=1, DIFF_HARD=2}`
  in `src/difficulty_calc.h` (host-testable header — `game.h` re-exports via
  `#include`); getter `game_difficulty()` / setter `game_set_difficulty()`.
  Pure header `src/difficulty_calc.h` (joins `tuning.h`, `game_modal.h`,
  `*_anim.h` as `<stdint.h>`-only host-testable helpers) holds: a 2-D 6-byte
  `DIFF_ENEMY_HP[3][2]` table (EASY {2,4} / NORMAL {BUG_HP, ROBOT_HP} / HARD
  {5,9}) — chosen over a single multiplier because user's HARD targets
  (bug=5, robot=9) imply non-uniform ratios; a `(base*num)>>3` spawn-interval
  scaler with global 30-frame floor (HARD num=6 ×0.75, EASY num=12 ×1.5);
  and a 3-entry starting-energy lookup (EASY=45, NORMAL=30, HARD=24). Title
  screen renders selector `< LABEL >` at row 10 cols 5..14 in BG tiles (no
  OAM); LEFT/RIGHT edge-only (`input_is_pressed`) cycle with wrap; A and B
  inert; START commits and calls `enter_playing()` unchanged. Tower stats
  and wave composition are difficulty-invariant; the iter-2 D12 50-frame
  spawn floor is now a NORMAL-balance number, superseded by the engine-wide
  30-frame floor.
- **Rationale**: 2-D HP table = exact designer intent for 6 B; formula spawn
  scaler = O(1) over many base delays; BG selector = zero OAM/render cost
  reusing the existing dirty-flag pattern from PRESS-START blink. File-scope
  static persistence avoids touching `enter_*` and is the simplest possible
  "remember last selection within session". Edge-only cycle prevents
  over-cycling on a held D-pad in a 3-state selector.
- **Alternatives**: Per-difficulty single HP multiplier (rejected — can't
  match user's targets); per-(diff, base) spawn lookup table (rejected —
  combinatorial); HUD letter indicator (rejected — HUD row 0 fully occupied
  by iter-2 convention; reflow disproportionate); difficulty stored in
  `title.c` (rejected — it's cross-module state, `game.c` is the right
  owner); resetting `s_difficulty` in `enter_title` (rejected — would lose
  the selection on quit-to-title); `input_is_repeat` for cycling (rejected
  per D9 — would over-cycle on held D-pad).

### Title VBlank: selector-first, blink-deferred
- **Date**: 2026-05-16
- **Context**: F1 (MEDIUM) review finding on iter-3 #20. `title_render`
  serviced both `s_diff_dirty` (10 writes) and `s_dirty` (12 writes) in
  the same VBlank when LEFT/RIGHT landed on the same frame as the 30-frame
 tile corruption
  reachable ~once per 0.5 s of active cycling on real DMG.
- **Decision**: Service at most one dirty region per `title_render` call.
  Selector wins (user input must be visually responsive within 1 frame);
  blink is deferred 1 frame and re-fires on the next 30-frame edge anyway.
  Implemented as an early `return;` after `draw_diff_now()` in
  `src/title.c::title_render`.
- **Rationale**: Cheapest fix that keeps the established 16-write/frame
  budget intact. No new state, no new flags, no logic change to update.
  Worst-case title contribution drops from 22 to 12 writes.
- **Alternatives**: (a) split prompt redraw across two frames (6+ adds6) 
  state and complicates the blink; (b) move title to `DISPLAY_OFF`
   flicker on every cycle, regression vs current UX;brackets 
  (c) deprioritize  input feels laggy, regression vs F1's ownselector 
  responsiveness goal.

### Iter-3 #16 D-MUS-1..5 (2026-05-22)

> **D-MUS-1: Custom mini music engine, no hUGETracker.** Music authored
> as `mus_row_t[]` C arrays in `src/music.c`. Driver in
> `src/music.{h,c}` (~600 B). Avoids GUI tooling dependency and
> 2-3 KB hUGEDriver runtime. Future swap to hUGEDriver remains
> possible; API surface (`music_play/stop/tick/duck`) would not change.

> **D-MUS-2: Music channel allocation = CH3 (melody) + CH4 (percussion).**
> CH1/CH2 remain SFX-only. CH3 has no SFX so no arbitration. CH4 SFX
> (`SFX_ENEMY_HIT`/`DEATH`) preempt the music's CH4 row; music re-arms
> at the next row boundary via `music_notify_ch4_busy/free`. Within
> `audio_tick`: SFX advance -> ch4 edge-detect notify -> `music_tick`.

> **D-MUS-3: SFX_WIN and SFX_LOSE removed.** Replaced by `MUS_WIN` /
> `MUS_LOSE` stinger songs in `src/music.c`. Removes ch1-prio-3
> occupancy that previously blocked TOWER_PLACE on the gameover screen.
> The dead multi-note `sfx_def_t` fields (`note_count`,
> `frames_per_note`, multi-element `pitches`) and the `audio_tick`
> advance branch were also deleted; every remaining SFX is single-note.
> `tests/test_audio.c::test_priority_preempt_rules` rewritten to use
> CH4 (`SFX_ENEMY_DEATH` prio 2 preempts; `SFX_ENEMY_HIT` prio 1
> rejected) - preserves the F1 lower-prio-rejected invariant on a
> still-existing channel. Multi-note state-machine coverage migrated
> to `tests/test_music.c`.

> **D-MUS-4: Pause menu ducks NR50 to 0x33 (~43%).** Single-register
> write on `pause_open`/`pause_close` via `music_duck(1)`/
> `music_duck(0)`. Quit-to-title path explicitly restores `NR50=0x77`
> after `audio_reset()` (which deliberately does not touch NR50/51/52).
> The upgrade/sell menu does NOT duck (transient, gameplay isn't
> paused).

> **D-MUS-5: `audio_reset()` is the master reset.** Internally calls
> `music_reset()` (silences CH3+CH4 + clears music state). Continues
> to NOT touch NR50/51/52 (preserves F1 semantics). All existing
> callers unchanged.

> **D-MUS supplementary: synchronous-arm `music_play()`.** `music_play(id)`
> writes row-0 NR3x/NR4x BEFORE returning, mirroring
> `audio_play -> start_note`. Required because `enter_gameover()` blocks
> the main loop for several frames during DISPLAY_OFF + 360-tile
> redraw; without sync arm the WIN/LOSE stinger first note is silent.
> `music_play(current_song)` is idempotent (returns early on same
> song).

> **D-MUS supplementary: include direction.** `audio.c` MAY include
> `music.h` (to call `music_init/reset/tick/notify_ch4_*`). `music.c`
> MUST NOT include `audio.h`. Avoids circular includes and keeps
> audio.c the leaf module. The "is music CH4 row armed" check lives
> entirely in `music.c`; audio.c calls `music_notify_ch4_busy()`
> unconditionally (no-op when music idle).

> **D-MUS supplementary: `music_init()` is init-once at boot.** Wave
> RAM (0xFF30-0xFF3F) is loaded exactly once (DAC off via NR30=0x00,
> 16 bytes via `_AUD3WAVERAM`, DAC stays off until first row trigger).
> `audio_init()` calls `music_init()` AFTER NR52 master-on AND AFTER
> the `audio_play(SFX_BOOT)` line. Ordering verified by
> `test_audio::test_init_writes_nr52_first_and_fires_boot_chime`.

> **D-MUS supplementary: MUS_NONE removed.** `enum { MUS_TITLE = 0,
> MUS_PLAYING, MUS_WIN, MUS_LOSE, MUS_COUNT }`. Use `music_stop()`
> for idle. Avoids empty `s_songs[]` slot, undefined
> `music_play(MUS_NONE)`, and idempotency-check edge cases.

> **D-MUS supplementary:  `music_play()` preserves SFX-owned CH4.**F1 
> When switching songs, `music_play(other)` only silences CH4 if
> `ch4_blocked == 0`. If an SFX (e.g. `SFX_ENEMY_DEATH`) currently owns
> CH4, NR42 is left alone so the SFX completes naturally; the engine
> then re-arms music CH4 at the next row boundary after
> `music_notify_ch4_free()` lifts the block. Without this guard the
> death SFX is truncated mid-play AND the row-0 arm is skipped (because
> `arm_current_row()` also respects the  worst-of-both-worldsblock) 
> silence. Regression covered by manual smoke (`tests/manual.md` F1
> section); host tests can't easily simulate the SFX-overlap path.

> **D-MUS supplementary:  CH4 SFX-end uses a two-stage latch.**F2 
> `music_notify_ch4_free()` no longer clears `ch4_blocked` directly. It
> sets `ch4_just_freed`; `music_tick()` clears `ch4_blocked` at the END
> of the current tick (after the row-arm decision) and fires a deferred
> arm via `ch4_arm_pending` at the TOP of the FOLLOWING tick. Required
> because `audio_tick` calls `music_notify_ch4_free()` and then
> `music_tick()` in the same frame; if `frames_left` also reaches 0 on
> that tick, clearing the block synchronously would let
> `arm_current_row()` re-arm CH4 zero-gap on the very frame the SFX
> silenced (audible click on DMG, contradicts the
1 tick of percussion silence" guarantee). Regression test:> "
> `test_music.c::test_ch4_arbitration_boundary_on_unblock` (boundary
 deferred arm fires).

### Iter-3 #18: Tower kind discriminator (damage vs stun)
- **Date**: 2025-01-31
- **Context**: EMP tower stuns instead of damaging; needed a clean way to keep damage and stun parameters in tower_stats_t without overloading fields.
- **Decision**: Add `u8 kind` (TKIND_DAMAGE/TKIND_STUN) and dedicated `stun_frames`/`stun_frames_l1` fields to tower_stats_t. Damage tower fields are unread for stun towers and vice versa. tower_t per-tower WRAM unchanged. towers.c never touches enemy_t directly; it calls public enemies_try_stun()/enemies_is_stunned() APIs.
- **Rationale**: Explicit > clever. Future towers (debuff, slow, AoE-damage) slot in by adding a kind value and the relevant stat fields.
- **Alternatives**: Reuse damage_l0/l1 as stun_frames (rejected: bug-bait at every read site). Direct enemy_t.stun_timer write from towers.c (rejected: breaks encapsulation, fails host-test isolation).

### Iter-3 #18: EMP cooldown only resets on a successful pulse
- **Date**: 2025-01-31
- **Context**: An EMP that scans an empty range every frame would otherwise have a 120-frame dead window after each fruitless attempt.
- **Decision**: When an EMP scan finds zero stunnable targets, cooldown stays at 0 (the scan re-runs each frame). Cooldown resets to 120 only when at least one enemy was actually stunned that frame. Freshly-placed EMP also starts with cooldown=0.
- **Rationale**: Avoids unreadable dead windows. Keeps EMP feeling responsive. The 14-iteration scan per frame is negligible cost on DMG.
- **Alternatives**: Always reset cooldown after scan (rejected: dead-window UX). Start cooldown at 120 on placement (rejected: 2-second silent delay after placing).

### Iter-3 #18: Sprite  flash > stun > walkpriority 
- **Date**: 2025-01-31
- **Context**: An enemy can be both flash (3 frames, hit feedback) and stun (60+ frames, frozen) simultaneously.
- **Decision**: When both flash_timer and stun_timer are >0 in the same frame, the FLASH tile renders. stun_timer continues to count down underneath. Flash is 3 frames; stun is 60+; total stun duration is preserved.
- **Rationale**: Flash is the damage-feedback channel and must not be silenced by stun. 3 lost stun-frames are imperceptible against 60+.
- **Alternatives**: Stun > flash (rejected: damage feedback would be invisible while stunned). Pause stun_timer during flash (rejected: complicates accounting for no benefit).

### Iter-3 #18: Stun cannot stack or extend
- **Date**: 2025-01-31
- **Context**: Multiple EMPs whose ranges overlap a single enemy.
- **Decision**: enemies_try_stun() returns false if stun_timer > 0; the second EMP does not extend the stun. Upgrade L1 (90-frame stun) does not retroactively extend an active L0 stun (60).
- **Rationale**: "Extend on re-stun" would let 2 EMPs perpetually freeze any enemy and trivialize tanks. Stacking should be a geographic strategy (different path segments), not a force-multiplier on the same tile.
- **Alternatives**: Refresh stun on every successful try (rejected: trivializes tanks). Sum stun durations (rejected: same problem).

### Iter-3 #18: Tower kind discriminator (damage vs stun)
- **Date**: 2025-01-31
- **Context**: EMP tower stuns instead of damaging; needed a clean way to keep damage and stun parameters in tower_stats_t without overloading fields.
- **Decision**: Add `u8 kind` (TKIND_DAMAGE/TKIND_STUN) and dedicated `stun_frames`/`stun_frames_l1` fields to tower_stats_t. Damage tower fields are unread for stun towers and vice versa. tower_t per-tower WRAM unchanged. towers.c never touches enemy_t directly; it calls public enemies_try_stun()/enemies_is_stunned() APIs.
- **Rationale**: Explicit > clever. Future towers (debuff, slow, AoE-damage) slot in by adding a kind value and the relevant stat fields.
- **Alternatives**: Reuse damage_l0/l1 as stun_frames (rejected: bug-bait at every read site). Direct enemy_t.stun_timer write from towers.c (rejected: breaks encapsulation, fails host-test isolation).

### Iter-3 #18: EMP cooldown only resets on a successful pulse
- **Date**: 2025-01-31
- **Context**: An EMP that scans an empty range every frame would otherwise have a 120-frame dead window after each fruitless attempt.
- **Decision**: When an EMP scan finds zero stunnable targets, cooldown stays at 0 (the scan re-runs each frame). Cooldown resets to 120 only when at least one enemy was actually stunned that frame. Freshly-placed EMP also starts with cooldown=0.
- **Rationale**: Avoids unreadable dead windows. Keeps EMP feeling responsive. The 14-iteration scan per frame is negligible cost on DMG.
- **Alternatives**: Always reset cooldown after scan (rejected: dead-window UX). Start cooldown at 120 on placement (rejected: 2-second silent delay after placing).

### Iter-3 #18: Sprite  flash > stun > walkpriority 
- **Date**: 2025-01-31
- **Context**: An enemy can be both flash (3 frames, hit feedback) and stun (60+ frames, frozen) simultaneously.
- **Decision**: When both flash_timer and stun_timer are >0 in the same frame, the FLASH tile renders. stun_timer continues to count down underneath. Flash is 3 frames; stun is 60+; total stun duration is preserved.
- **Rationale**: Flash is the damage-feedback channel and must not be silenced by stun. 3 lost stun-frames are imperceptible against 60+.
- **Alternatives**: Stun > flash (rejected: damage feedback would be invisible while stunned). Pause stun_timer during flash (rejected: complicates accounting for no benefit).

### Iter-3 #18: Stun cannot stack or extend
- **Date**: 2025-01-31
- **Context**: Multiple EMPs whose ranges overlap a single enemy.
- **Decision**: enemies_try_stun() returns false if stun_timer > 0; the second EMP does not extend the stun. Upgrade L1 (90-frame stun) does not retroactively extend an active L0 stun (60).
- **Rationale**: "Extend on re-stun" would let 2 EMPs perpetually freeze any enemy and trivialize tanks. Stacking should be a geographic strategy (different path segments), not a force-multiplier on the same tile.
- **Alternatives**: Refresh stun on every successful try (rejected: trivializes tanks). Sum stun durations (rejected: same problem).

### Iter-3 #18 F1: 3-outcome EMP cooldown rule prevents overlapping-EMP perma-freeze
- **Date**: 2025-02-01
 keep cooldown=0) let an idle 2nd EMP poll every frame waiting for the rival's stun to expire. Because towers_update runs before enemies_update each frame (game.c:159-160), the 2nd EMP always grabbed the unstun frame and re-stunned, repeating indefinitely. This contradicts spec L269-276 ("stacking EMPs is  perma-freeze was rejected").wasteful 
 keep cooldown=0 to retry next frame. The new (b) branch is the chain-lock breaker: a redundant EMP burns its 120-frame cycle on a target it can't actually pulse, phase-locking with the rival rather than perma-freezing.
 keep polling).
- **Alternatives**: Reorder enemies_update before towers_update (rejected: cross-cutting frame-order change with broader implications). Per-enemy "stun cooldown owner" tracking (rejected: extra state, more complex).

### Iter-3 #18 F2: TKIND_STUN upgrade preserves cooldown
- **Date**: 2025-02-01
- **Context**: towers_upgrade() unconditionally reset cooldown to cooldown_l1. For damage towers this is "immediately observable" L1 cadence. For EMP, cooldown_l0 == cooldown_l1 == 120, so resetting an idle EMP at cooldown=0 forced a 120-frame dead  a regression of the cooldown-on-success invariant (D-IT3-18-7).window 
- **Decision**: Special-case TKIND_DAMAGE in towers_upgrade(): only damage towers reset cooldown on upgrade. EMP (TKIND_STUN) preserves its current cooldown across upgrade so an idle EMP fires on its next eligible frame with the new L1 stun duration.
- **Rationale**: Keeps the upgrade UX immediate for the user (next pulse uses 90-frame stun) without violating cooldown-on-success.
- **Alternatives**: Skip cooldown reset entirely for all kinds (rejected: damage tower L1 cadence change becomes observable only after the next natural cooldown). Set EMP cooldown to 0 explicitly on upgrade (rejected: equivalent to current state in practice but adds same-frame SFX collision risk for the  see F3).pulse 

### Iter-3 #18 F3: Freshly-placed EMP cooldown=1 to avoid same-frame SFX collision
- **Date**: 2025-02-01
- **Context**: towers_try_place(EMP) plays SFX_TOWER_PLACE on CH1 (priority 2). Same frame, towers_update would fire the EMP at cooldown=0, calling SFX_EMP_FIRE (also CH1, priority 2). audio_play's preempt rule is `def->priority < s_ch[idx]. equal-priority preempt is  so the fire SFX overwrote the place SFX immediately and the player lost placement audio confirmation.allowed prio` 
- **Decision**: Initialize freshly-placed EMP cooldown to 1 (not 0). The first pulse fires on frame N+1 instead of frame N, ~16 ms  imperceptible. SFX_TOWER_PLACE finishes its envelope cleanly before any SFX_EMP_FIRE is queued.later 
- **Rationale**: Surgical 1-frame defer preserves the "stuns near-immediately on placement" UX promise while honoring the audio mixing contract. No changes to the SFX preempt rule (which is correct for the general case).
- **Alternatives**: Suppress SFX_EMP_FIRE when SFX_TOWER_PLACE was triggered the same frame (rejected: bespoke audio-state lookup per call site). Lower SFX_EMP_FIRE priority (rejected: cross-cutting tweak that affects other interactions). Move towers_update before towers_try_place in playing_update (rejected: changes input latency semantics).

### Iter-3 # Map registry & two-row title selector17 
- **Date**: 2026-04-28
- **Context**: Roadmap # three maps with different path layouts and a map-select on title. Spec at `specs/iter3-17-maps.md`.17 
- **Decision**: 
  - Maps are described by `map_def_t { tilemap, classmap, waypoints, waypoint_count, computer_tx, computer_ty }`. `src/map.c` owns one `static const map_def_t s_maps[MAP_COUNT]`; `map_load(u8 id)` selects active. `map.c` does NOT include `game.h`; the active id flows in as the `map_load` argument (D8).
  - Active map persists at file scope in `src/game.c` as `static u8 s_active_map = 0`, symmetric with `s_difficulty`. Neither is reset in `enter_title()`/`enter_playing()`. Power-on default = MAP_1 via `.data` zero-init. SRAM persistence is feature #19.
  - `map_render` reads `computer_tx/ty` from the active def; the literal `(18, sr)` block in `map.c::map_render` is removed.
  - All three maps anchor the 22 computer cluster at TL=(18,4) (designer  keeps corruption LUT and pause anchor invariant). The fields exist so future maps can move it.choice 
  - `WAYPOINT_COUNT` macro is deleted from `src/map.h`; `enemies.c::step_enemy` uses `map_waypoint_count()` (cached) instead. This is the only non-`map.c` change required for the registry refactor.
  - Title screen carries TWO stacked selectors (difficulty row 10, map row 12) plus a one-tile focus chevron `>` at column 3 of the focused row. UP/DOWN edge-only toggles `s_title_focus`; LEFT/RIGHT cycles the focused selector. Both `enter_title()` resets `s_title_focus = 0`.
  - Title VBlank priority chain (extends iter-3 #20 selector-first / blink-deferred): `s_diff_dirty` > `s_map_dirty` > `s_focus_dirty` > `s_dirty`. Service ONE per frame; each branch returns. Worst-case 12 writes/16 cap.frame, 
  - Pure-helper family extended: `src/map_select.h` (`<stdint.h>`-only, `MAP_COUNT`-wrap cycle). Tested in `tests/test_maps.c`.
  - Wave script and difficulty constants are shared across maps. Path geometry is the only per-map dimension. Three-read-site rule for difficulty is unaffected.
- **Rationale**: Mirrors iter-3 #20 difficulty pattern at every architectural level (file-scope persistence, pure-helper cycle, edge-only input, selector-first VBlank scheduling). Preserves all existing budgets (ROM, BG-write/frame, OAM, modal precedence). Map-agnostic waves/towers keeps scope tight.
- **Alternatives**: 
  - Combined diff+map selector with one widget toggling axis ( more render code, no UX win).rejected 
  - Per-map wave script ( explodes balancing surface).rejected 
  - Bit-packed buildable bitmap to save 680 bytes ROM ( refactors hot `map_class_at` for a saving the 32 KB cap doesn't need).rejected 
  - `map_load(void)` reading `game_active_map()` internally ( would force `map.c` to depend on `game.h`, breaking host-test linkability).rejected 

- Iter-3 #17 F1: Compile-time + host-test guard for MAP_SELECT_COUNT / MAP_COUNT sync. `#error` in `src/title.c` (after includes pull in both `map.h` via `game.h` and `map_select.h`) catches drift at ROM build; `CHECK_EQ(MAP_SELECT_COUNT, MAP_COUNT)` in `tests/test_maps.c` catches drift on host-test build. Negative regression verified: setting `MAP_SELECT_COUNT 4u` fails build with the `#error` and `just test` reports the assertion failure.

### Iter-3 #19: MBC1+RAM cartridge conversion
- **Date**: 2026-04-29
- **Context**: Roadmap feature #19 requires per-map high-score persistence. Original MVP cart was no-MBC ROM-only (cart byte 0x147=0x00, ROM-size 0x148=0x00 → 32 KB, RAM-size 0x149=0x00). High-score storage requires battery-backed SRAM, which requires MBC1+RAM+BATTERY (0x147=0x03).
- **Decision**: Flip cartridge to MBC1+RAM+BATTERY: 0x147=0x03, 0x148=0x01 (64 KB / 4 banks), 0x149=0x01 (2 KB / 1 bank). Build flags split per canonical GBDK example: `-Wl-yt0x03 -Wm-yo4 -Wm-ya1`. `just check` asserts the three header bytes and 65536 ROM size.
- **Rationale**: 64 KB is the clean MBC1 minimum (avoids GBDK quirks with 32 KB MBC1 carts) and gives huge headroom over current ~28 KB usage. 2 KB SRAM is the smallest legal MBC1 RAM and stores all 24 needed bytes with room for future migration. Split flag form (`-Wl-yt`, `-Wm-yo`, `-Wm-ya`) mirrors `vendor/gbdk/examples/cross-platform/banks/Makefile` — the all-`-Wl-` form is silently ignored for `-yo`/`-ya`.
- **Alternatives**: 32 KB MBC1 (legal but linker-quirky); 8 KB SRAM (overkill); all-`-Wm-` or all-`-Wl-` forms (latter silently rejected by lcc).

### Iter-3 #19: SRAM save format
- **Date**: 2026-04-29
- **Context**: High-score persistence layout in SRAM at 0xA000.
- **Decision**: 24-byte format: `'G','B','T','D'` magic (4) + version=0x01 (1) + pad (1) + 9 × u16 little-endian high-scores at offsets 6..23. Slot index = `map_id*3 + diff` for `map ∈ {0,1,2}`, `diff ∈ {EASY=0,NORMAL=1,HARD=2}`. On magic/version mismatch, save module zeroes all 9 slots and re-stamps a fresh header. Single-owner cache `static u16 s_hi[9]` lives in `save.c`; no other module caches.
- **Rationale**: ASCII magic is self-documenting in hex dumps; version byte future-proofs migration; per-(map,diff) slots support meaningful HARD bragging rights at trivial 18-byte cost. Single-owner cache eliminates staleness risk.
- **Alternatives**: Per-map only (3 slots, less granular); CRC checksum (over-engineered for 24 bytes); dual-cached in score.c too (staleness risk).

### Iter-3 #19: Score formula and accumulation
- **Date**: 2026-04-29
- **Context**: Per-run score, separate from energy, for high-score system.
- **Decision**: u16 score with saturation clamp at 0xFFFF. Per-kill base: BUG=10, ROBOT=25, ARMORED=50. Per-wave-clear bonus: `100 × wave_num`. Win bonus (all 10 waves cleared, HP > 0): 5000. Difficulty multiplier applied to every add: `(base × num) >> 3` with `num ∈ {EASY:8, NORMAL:12, HARD:16}` (effective ×1.0 / ×1.5 / ×2.0). Wave-clear edge fires on last-spawn-of-wave-N completes (the existing `s_cur++` edge in waves.c), exposed as `waves_just_cleared_wave()` one-shot. Save trigger: only at `enter_gameover` to minimize power-loss corruption window.
- **Rationale**: u16 covers worst-case (~22000 HARD run) with headroom; clamp is defensive. Score separation from energy prevents exploit of energy hoarding. Integer multiplier with /8 base avoids floats. Last-spawn edge reuses existing state machine; per-wave drain tracking would need new enemy tagging.
- **Alternatives**: u32 (wastes WRAM/tiles); shared with energy (exploitable); per-frame save (corruption risk); per-wave drain edge (needs enemy tagging).

### Iter-3 #19 follow-up: makebin `-ya N` ↔ byte 0x149 mapping
- **Date**: 2026-04-29
- **Context**: Implementation revealed that the planner's research mapping `-Wm-ya1` → header byte 0x149=0x01 (2 KB) is incorrect. makebin's `-ya N` argument counts in 8 KB banks (one logical RAM bank = 8 KB on real MBC1), so `-ya1` → byte 0x149=0x02 (8 KB). To get the spec-required 0x149=0x01 (2 KB) we needed an extra `-Wm-yp0x149=0x01` byte-override after the bank-count flag.
- **Decision**: Final flag set is `-Wl-yt0x03 -Wm-yo4 -Wm-ya1 -Wm-yp0x149=0x01`. The `-ya1` allocation request stays for makebin's internal sizing; the `-yp` override patches the header byte to the correct 2 KB value. `just check` asserts the resulting byte regardless of flag form, so a future makebin behaviour change still surfaces as a CI failure.
- **Rationale**: Byte 0x149 is the only on-cart RAM-size advertisement; emulators allocate SRAM based on it. 2 KB (0x01) is what the spec asked for and is sufficient (24 B used). makebin emits a `caution: -yp0x0149=0x01 is outdated` notice but the override is functionally correct and the resulting header checksum recomputes properly (verified at 0x14D = 0x4E).
- **Alternatives**: Accept 0x149=0x02 (8 KB — functionally identical for our 24-byte payload, but contradicts the spec); modify GBDK's makebin (not portable across user installs).

### Iter-3 #19 F1: Fresh-cart SRAM reset is failure-atomic
- **Date**: 2026-04-29
- **Context**: Adversarial review  the original reset path in `save_init` stamped magic+version BEFORE zeroing the 9 score slots. If power was lost mid-reset, the next boot saw a valid header + stale/garbage score bytes, causing bogus high scores to surface ( on a future version-bump  old-format bytes reinterpreted as valid).reset or F1 
 reset runs again cleanly.
- **Rationale**: One-line reorder, no structural change, no ROM cost. Eliminates an entire failure class on real hardware. Single ENABLE_RAM/DISABLE_RAM window unchanged.
- **Alternatives**: CRC over the slot table (over-engineered); shadow-copy two-phase commit (~50 extra bytes for 24 bytes of  not worth it).data 

### Iter-4: Game rename to Bug Defender
- **Date**: 2026-06-01
- **Context**: User feedback: "GBTD isn't the game name: just a suggestion, maybe Bug Defender?"
- **Decision**: Rename the game to "Bug Defender" in all user-visible locations. ROM header internal title = "BUGDEFENDER" (11 chars). Title screen text/logo, README, AGENTS.md, justfile output target (`build/bugdefender.gb`). Internal C namespace prefixes (`GBTD_` header guards) remain unchanged to avoid a codebase-wide refactor.
- **Rationale**: "Bug Defender" is thematic (player literally defends against bugs), memorable, and fits the 11-char ROM header. Internal code prefixes are developer-facing and don't affect the player.
- **Alternatives**: Keep "GBTD" (rejected — user explicitly requested a real name); "Cyber Defense" (too generic); full rename of all code identifiers (rejected — disproportionate refactor risk for zero user value).

### Iter-4: Difficulty tier rename and rebalance
- **Date**: 2026-06-01
- **Context**: User feedback: (1) "In hard mode you can't even kill the monsters from the first wave" — HARD HP {5,9,16} with AV L0 damage=1 and cd=60 requires 300f per bug kill, but path traversal is 480f, making it mathematically near-impossible with starting energy of 24. (2) "Normal mode is too easy: should be the easy mode already" — NORMAL provides no real challenge.
- **Decision**: Rename Easy→Casual, Hard→Veteran. Rebalance: Casual HP {1,3,6} (was {2,4,8}), Normal unchanged {3,6,12}, Veteran HP {4,7,14} (was {5,9,16}). Veteran starting energy 24→28. Spawn scaler unchanged. Math check: AV L0 vs Veteran bug HP=4 → 4 shots × 60f = 240f/kill, path=480f → 1 tower kills ~2 bugs. Two AV towers (20 energy from 28) kill ~4 of 5 wave-1 bugs. Viable with tight play.
- **Rationale**: Veteran HP reduction is the minimum fix that makes wave 1 survivable. "Casual" is more welcoming than "Easy"; "Veteran" signals challenge without implying unfairness. "Normal" unchanged as the anchor tier — if users later say Normal is too easy, wave composition can be tuned separately.
- **Alternatives**: Only reduce Veteran HP without renaming (rejected — user also said Normal felt like Easy, suggesting the naming itself sets wrong expectations); add a 4th difficulty (rejected — 3 tiers × 3 maps = 9 SRAM slots already; 4th tier = 12 slots, requires save format migration).

### Iter-4: Tower upgrade depth = L0→L1→L2 (max 3 levels)
- **Date**: 2026-06-01
- **Context**: User feedback: "It would be nice to have more tower upgrades." Current max = L1.
- **Decision**: Extend to L0→L1→L2 for all 3 tower types. L2 stats: AV (dmg 3, cd 30, cost 25), FW (dmg 6, cd 70, cost 30), EMP (stun 120f, cd 100, cost 20). `tower_stats_t` extended with `bg_tile_l2`, `bg_tile_alt_l2`, `upgrade_cost_l2`, and per-type L2 stat fields. `towers_can_upgrade` accepts level 0 or 1 (max=2). `menu.c` level guard changed from `!= 0` to `>= 2`. `towers_get_spent()` accumulates all costs for sell refund (max FW = 15+20+30 = 65, fits u8). L3+ deferred.
- **Rationale**: One additional upgrade level is the minimum meaningful response to the feedback. It adds one new decision point per tower per game. L3+ has diminishing returns and would strain the BG tile budget (6 tiles per level × 3 types).
- **Alternatives**: L0→L1→L2→L3 (rejected — diminishing returns, +18 tiles); type-branching upgrades (rejected — massive UI and balancing scope); L2 for some towers only (rejected — inconsistent UX).

### Iter-4: Range preview uses OAM 1..8 (menu/pause range)
- **Date**: 2026-06-01
- **Context**: Adversarial review found that projectile OAM (31..38) is actively written by `projectiles_update()` during normal gameplay. Assigning range preview dots to those slots would corrupt both systems.
- **Decision**: Range preview sprites use OAM 1..8 (a subset of the menu/pause shared range 1..16). This range is idle during normal gameplay (cleared by `menu_init()`/`pause_init()` on session start). `menu_open()` and `pause_open()` call `range_preview_hide()` alongside existing entity-hide calls.
- **Rationale**: Zero OAM conflict. Menu/pause slots are definitionally idle when the cursor is on a tower in normal play (modals are mutually exclusive with gameplay input).
- **Alternatives**: Projectile OAM 31..38 (rejected — actively written during gameplay); dedicated new OAM range (rejected — no free slots without shrinking enemy/projectile pools).

### Iter-4: Speed-up toggle = entity-only double-tick
- **Date**: 2026-06-01
- **Context**: Adversarial review found that running full `game_update()` twice per frame would double-fire input dispatch, cursor movement, modal checks, and `s_anim_frame`.
- **Decision**: SELECT toggles `s_fast_mode` in `playing_update()`. When active, only the entity-tick inner block (towers_update, enemies_update, projectiles_update, waves_update) runs a second time. economy_tick, audio_tick, input dispatch, cursor update, modal checks, s_anim_frame++, and gameover checks remain single-call. Wave-10 profiling on real DMG hardware required before shipping.
- **Rationale**: Entity-only double-tick is the smallest correct unit of acceleration. Keeps input, audio, and animation at normal cadence while doubling game simulation speed.
- **Alternatives**: Full game_update twice (rejected — doubles input/modal dispatch); frame-skip (rejected — visually jerky on DMG); timer manipulation (rejected — no user-space timer on DMG).

### Iter-4: First-tower prompt at mid-screen BG rows 7–8
- **Date**: 2026-06-01
- **Context**: Adversarial review found that the HUD row is fully occupied (20 cols) with no room for a "PLACE A TOWER" string.
- **Decision**: Display the prompt as a centered 2-line BG text block at play-field rows 7–8 (screen rows 8–9, below HUD). Text: "PLACE A" / "TOWER!" (12 tiles total). Blinks at 1 Hz. On gate lift (first tower placed), restore the 12 BG tiles from the map tilemap data. Consistent with pause-overlay mid-screen precedent.
- **Rationale**: Mid-screen placement is unmissable. 12 BG-tile restore on gate lift is a one-time write well within budget (happens on the same frame as tower placement, which is already 1 BG write — total 13, under 16 cap).
- **Alternatives**: Overwrite HUD wave field (rejected — stomps useful info); bottom-of-screen row (rejected — less visible, some maps have path tiles there).

### Iter-4 #29 rename clarifications
- **Date**: 2026-04-29
- **Context**: Implementing the Bug Defender rename (iter-4 #29).
- **Decision**: (1) AGENTS.md is unchanged — it contains no game-name text.
  (2) SRAM magic bytes 'G','B','T','D' (save_calc.h) are preserved for
  flash-cart save compatibility. (3) `GBTD_` C header-guard prefix is
  retained (internal, never user-visible). (4) Historical QA/spec docs
  are not updated — they are audit records of past state.
- **Rationale**: Save-compat is non-negotiable for real hardware users.
  Namespace prefix rename would touch 27+ files for zero user value.
- **Alternatives**: None — these are constraint clarifications, not design choices.

### Iter-4 #24: Gate state lives in game.c, not a new module
- **Date**: 2026-06-08
- **Context**: Feature #24 first-tower gate needs state tracking, BG text overlay, and integration with the playing_update flow.
- **Decision**: Gate state (`s_gate_active`, `s_gate_blink`, `s_gate_vis`, `s_gate_dirty`) lives in `game.c`. Blink-phase computation is extracted to a pure helper `src/gate_calc.h` for host testing.
- **Rationale**: The gate is a transient start condition, not a subsystem. `game.c` already owns `playing_update()` flow control and the `waves_update()` call site. A new `.c` module adds include complexity with no benefit. Pure helper keeps blink logic testable on the host.
- **Alternatives**: New `gate.c` module (rejected — over-engineering for ~30 lines of state); inside `waves.c` (rejected — waves.c has no BG-write capability); all inline in game.c without pure helper (rejected — untestable blink logic).

### Iter-4 #24: Skip waves_update(), no changes to waves.c
- **Date**: 2026-06-08
- **Context**: Gate must freeze wave progression until first tower placement. `waves_init()` sets `s_timer = FIRST_GRACE`.
- **Decision**: `playing_update()` simply skips calling `waves_update()` while `s_gate_active == 1`. When the gate lifts, calls resume and the timer counts down from FIRST_GRACE naturally.
- **Rationale**: Zero changes to waves.c. The timer freeze is an emergent property of not calling the update function. Simplest possible approach — no new API, no new state in waves.c.
- **Alternatives**: `waves_pause()`/`waves_resume()` API (rejected — new public API for a single call site); reset timer via `waves_arm_grace()` (rejected — redundant; timer is already at FIRST_GRACE from waves_init).

### Iter-4 #24: B-button gated during gate to avoid BG-write budget overflow
- **Date**: 2026-06-08
- **Context**: If A+B are pressed simultaneously on the gate-lift frame, `cycle_tower_type()` marks HUD T dirty (1 BG write) and `towers_try_place()` marks HUD E dirty (3 writes). Combined with gate restore (12) and tower render (1) = 17 writes, exceeding the 16-write budget.
- **Decision**: Gate the `J_B` handler in `playing_update()` on `!s_gate_active`. Tower type cycling is blocked until the gate lifts.
- **Rationale**: During the gate, no tower exists and no tower has been placed, so cycling the type indicator has minimal value. The guard is a single `if` condition that prevents a real 17-write budget overflow on DMG hardware.
- **Alternatives**: Accept the 17-write possibility (rejected — convention violation on real hardware causes tile corruption); defer gate restore to next frame (rejected — one-frame stale text after placement is a UX regression).

### Iter-4 #25: Menu BG rectangle = 6×2 tiles, deferred VBlank phases, render reorder
- **Date**: 2026-06-08
- **Context**: Feature #25 adds an opaque shade-0 BG rectangle behind the upgrade/sell menu sprite overlay text area to ensure readability over dark map tiles.
- **Decision**: (1) Paint 6 cols × 2 rows = 12 BG tiles with `(u8)' '` (shade-0 blank) behind sprite columns 1–6, skipping column 0 (the `>` cursor). (2) Save/clear and restore are deferred to `menu_render()` (VBlank-safe) via pending flags set in `menu_open()`/`menu_close()`. Save uses `get_bkg_tile_xy()` to capture actual VRAM state (including tower tiles, corruption tiles). (3) `menu_render()` moves before `towers_render()` in `playing_render()` so Phase 3 idle-blink correction is the last writer for adjacent tower tiles in the restore region. (4) Anchor computation extracted to a static helper shared by `menu_open()` and `menu_render()`. WRAM cost: 16 bytes. No API changes.
- **Rationale**: 12 tiles (not 14) keeps the sell-close worst case at exactly 16 BG writes (3 HUD + 12 restore + 1 sell-clear = 16 = cap). The `>` cursor is a bold sprite glyph readable over any background. `get_bkg_tile_xy()` (VRAM read) is necessary over `map_tile_at()` (ROM read) to preserve adjacent tower BG tiles and corruption tiles in the save buffer. Deferred phases avoid VRAM access during active LCD scan (menu_open/close run in the update path).
- **Alternatives**: 7×2 = 14 tiles (rejected — busts 16-write budget on sell-close: 14+3+1=18); `map_tile_at()` for save (rejected — loses tower tiles and corruption on restore); immediate VRAM read in `menu_open()` (rejected — VRAM inaccessible during LCD modes 2/3); BG-only menu without sprites (rejected — requires DISPLAY_OFF blink or complex tile-save for larger region).

### Iter-4 #26: L1 tower visual = per-type L1 BG tiles + dirty-flag repaint on upgrade
- **Date**: 2026-06-09
- **Context**: Feature #26 — upgraded (L1) towers display the same BG tile as L0, giving no visual feedback that an upgrade occurred.
- **Decision**: Extend `tower_stats_t` with `bg_tile_l1` and `bg_tile_alt_l1` fields (placed after `bg_tile_alt`). Add 6 new BG tile designs (3 types × main + idle-alt) to `gen_assets.py`. `towers_upgrade()` sets `dirty = 1` and `idle_phase = 0` after setting `level = 1`. `towers_render()` Phase 2 (placement/dirty) and Phase 3 (idle LED) branch on `s_towers[i].level` to select L0 vs L1 tile fields. MAP_TILE_COUNT grows 23 → 29. L1 tile designs preserve each tower type's identity (dish, wall, rings) with filled-corner + denser-center upgrade markers. The upgrade-close frame budget is now always exactly 16 writes (HUD energy 3 + menu BG restore 12 + tower Phase 2 dirty 1) — zero margin; future features must audit this path.
- **Rationale**: `dirty` flag reuses the existing Phase 2 placement mechanism with zero new infrastructure. `idle_phase = 0` reset prevents up to 32 frames of stale display (Phase 3 would misinterpret the current tile). Struct extension is ROM-only (`static const`), no WRAM growth. 6 tiles is well within the ~105 remaining tile budget.
- **Alternatives**: Sprite overlay for L1 indicator (rejected — OAM budget too tight); palette swap (rejected — DMG has one BGP for all BG tiles); single "upgraded" tile shared by all types (rejected — loses type identity at a glance).

### Iter-4 #27: L2 tower upgrades — level dispatch, struct extension, menu/upgrade logic
- **Date**: 2026-06-10
- **Context**: Feature #27 — towers support only L0→L1. Players have no further upgrade path. L2 adds a second upgrade level for all 3 tower types.
- **Decision**: (1) `tower_stats_t` gains 6 fields (`upgrade_cost_l2`, `cooldown_l2`, `damage_l2`, `bg_tile_l2`, `bg_tile_alt_l2`, `stun_frames_l2`) interleaved next to their L1 counterparts (struct grows 15→21 bytes, ROM-only). (2) All binary level ternaries (`level ? L1 : L0`) become 3-way: `lvl >= 2 ? L2 : lvl ? L1 : L0`. (3) `towers_can_upgrade()` gate: `level >= 2` (was `!= 0`). `towers_upgrade()` uses `level++` (was `= 1`), picks cost via `level == 0 ? upgrade_cost : upgrade_cost_l2`. (4) EMP (TKIND_STUN) preserves cooldown across L1→L2 (extending F2/D-IT3-18-7). (5) Menu: guard `>= 2`, cost source ternary, display branch `level < 2`. (6) 6 new BG tiles, MAP_TILE_COUNT 29→35. L2 tile designs: same identity + outer-ring darkened + center filled. BG write budget unchanged (16 writes on upgrade-close frame).
- **Rationale**: Inline nested ternary for level dispatch is clear at 6 sites and SDCC-friendly (vs. helper macro or array indexing). Field interleaving groups related data. `level++` is simpler than target-level assignment and pairs with `>= 2` guard. F2 cooldown preservation applies at all level hops — resetting would force idle EMP into 100f dead window. Max total spent (FW 65) fits u8.
- **Alternatives**: Level-indexed arrays in struct (rejected — over-engineered for 3 levels, harder to read); helper macro for dispatch (rejected — only 6 sites, inline is clear enough); reset EMP cooldown on L2 upgrade (rejected — violates D-IT3-18-7).

### Iter-4 #28: Title screen art — asset-only change, block-letter logo + scene motif
- **Date**: 2026-06-11
- **Context**: Feature #28 — the text-only title screen needs a visual upgrade with BG tile art. Budget ≤ 40 new tiles; selector/blink/HI logic unchanged.
- **Decision**: (1) Add 17 new BG tiles (MAP_TILE_COUNT 35→52, indices 163–179): 11 block-letter tiles for "BUG" (2-tile-high, 3-pixel stroke; U shares its top tile via horizontal symmetry, saving 1 slot), 2 dark-grey circuit-trace tiles (HLINE + NODE), 2 front-facing bug-icon tiles, 2 satellite-tower-icon tiles. (2) Reuse existing COMP_TL/TR/BL/BR (130–133) for the computer monitor in the scene — no new tiles needed. (3) "DEFENDER" at row 3 uses existing font tiles (zero cost). (4) `title_map()` in gen_assets.py is rewritten; no C source changes. `title_enter()` continues to load the 20×18 tilemap via `set_bkg_tiles` during DISPLAY_OFF. (5) All 17 new tile defines added to assets.h; `title_map()` uses Python `*_IDX` constants to prevent silent index mismatches.
- **Rationale**: Asset-only change = zero runtime risk. 17 tiles << 40 budget; 180/256 BG VRAM used. Reusing COMP tiles saves 4 slots. U symmetry exploit saves 1 slot. Dark grey for traces follows the palette convention ("reserve black for primary elements").
- **Alternatives**: Custom block letters for both "BUG" and "DEFENDER" (rejected — 24+ letter tiles, too many); all font text with decorative frame only (rejected — minimal visual impact); dual-bug scene with mirrored variants (rejected — no BG flip on DMG, doubles icon tile count).
