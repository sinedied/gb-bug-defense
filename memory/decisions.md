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
