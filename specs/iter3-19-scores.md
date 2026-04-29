# Iter-3 #19 — Score & High Score (SRAM)

## Problem
The roadmap's final feature: persist a per-map best score via MBC1+RAM
battery save. Players currently have no run-to-run progression hook. We add
(a) a per-run score accumulator, (b) battery-backed high-score storage in
SRAM, (c) title-screen and gameover-screen score readouts. This is the
**last** iter-3 feature and is architecturally heavier than the others
because it triggers the cartridge type conversion from `0x00` (ROM only) to
`0x03` (MBC1+RAM+BATTERY).

## Architecture

### Cartridge conversion (single coherent flip)
Header bytes change as follows:

| Byte | Was | Now | Meaning |
|------|-----|-----|---------|
| 0x147 cart type | 0x00 (ROM only) | **0x03** (MBC1+RAM+BATT) | enables MBC1 + battery-backed SRAM |
| 0x148 ROM size  | 0x00 (32 KB / 2 banks) | **0x01** (64 KB / 4 banks) | clean MBC1 minimum, large headroom |
| 0x149 RAM size  | 0x00 (none)     | **0x01** (2 KB) | enough for 24 bytes of save data |

`justfile` build invocation gains explicit flags split per the canonical
GBDK example (`vendor/gbdk/examples/cross-platform/banks/Makefile`):
```
-Wl-yt0x03 -Wm-yo4 -Wm-ya1
```
- `-Wl-yt0x03` → linker → cart type byte 0x147 = MBC1+RAM+BATT
- `-Wm-yo4`   → makebin → 4 ROM banks (64 KB), byte 0x148 = 0x01
- `-Wm-ya1`   → makebin → 1 RAM bank (2 KB), byte 0x149 = 0x01

The build sanity-check script (`just check`) is extended to assert the
exact three header bytes above and ROM size = 65536; a silent flag-reject
regression therefore fails CI.

GBDK's crt0 + linker auto-pack code into bank 0 (16 KB) and bank 1 (16 KB
swappable, default-active). All current code (~28 KB) fits cleanly in
banks 0+1. Banks 2-3 stay empty. **No `__banked` annotations required.**

### Score subsystem (runtime)
New module `src/score.{h,c}` owns a single `u16 s_score` accumulator. It
does **not** cache the high-score table; it reads through `save_get_hi`
on demand (record check is once per game, on `enter_gameover` — no need
to cache).

```c
// score.h
void  score_reset(void);                       // call from enter_playing
void  score_add_kill(u8 enemy_type);           // BUG=10, ROBOT=25, ARMORED=50, ×diff_mult
void  score_add_wave_clear(u8 wave_num);       // 100 × wave_num × diff_mult
void  score_add_win_bonus(void);               // 5000 × diff_mult
u16   score_get(void);                         // read site: gameover, save_check
bool  score_commit_if_record(void);            // calls save_get_hi + (on improvement) save_set_hi
```

The cached high-score table lives **only** in `save.c` as
`static u16 s_hi[9]`. `score.h` does not expose `score_get_hi`; UI code
(title.c, gameover.c) calls `save_get_hi` directly. Single-owner cache,
no staleness risk.

All multiplier and base-points math lives in pure-helper header
**`src/score_calc.h`** (`<stdint.h>` only) so host tests can verify it.

### Difficulty multiplier
Add to `src/difficulty_calc.h`:
```c
// returns score multiplier as numerator over /8 denominator
// EASY=8 (×1.0), NORMAL=12 (×1.5), HARD=16 (×2.0)
static inline uint8_t difficulty_score_mult_num(uint8_t diff);
// applied as: scaled = (base * num) >> 3
```

### Save subsystem (SRAM I/O)
New module `src/save.{h,c}`:
```c
void save_init(void);                          // boot-time: enable RAM, validate magic, load cache OR zero+stamp
u16  save_get_hi(u8 map_id, u8 diff);          // pure cache read
void save_set_hi(u8 map_id, u8 diff, u16 v);   // updates cache + writes SRAM (enable→write→disable)
```

**SRAM layout** (24 bytes at 0xA000, little-endian):
```
0xA000-0xA003  magic   = 'G','B','T','D'   (0x47 0x42 0x54 0x44)
0xA004         version = 0x01
0xA005         pad     = 0x00
0xA006-0xA017  hi[9]   = 9 × u16 in slot order:
                          slot = map_id*3 + diff
                          map ∈ {0,1,2}, diff ∈ {0=EASY,1=NORMAL,2=HARD}
```

**Access protocol** (per write/read transaction, brief):
1. `ENABLE_RAM;` (writes 0x0A to 0x0000)
2. `SWITCH_RAM(0);` (we use bank 0 only)
3. read or write through `_SRAM[i]`
4. `DISABLE_RAM;` (writes 0x00 to 0x0000)

Always `DISABLE_RAM` before returning, even on read paths, to minimize the
power-loss corruption window. The serialization helpers (offset compute,
magic stamp, cache hydrate) live in pure-helper header **`src/save_calc.h`**
for host testing.

**Validation**: `save_init` reads magic+version. On mismatch (fresh cart or
future format change), it zeros all 9 hi entries in cache, then writes a
fresh header + zeroed table back to SRAM. No CRC; risk accepted (24 bytes,
write window <100 µs).

### Score formula (final)
- Per kill: BUG=10, ROBOT=25, ARMORED=50
- Per wave clear (waves 1..10): `100 × wave_num` (so wave 10 = 1000)
- Win bonus (all 10 waves cleared, HP > 0): `5000`
- Difficulty multiplier applied to **every** add: `(base × num) >> 3`,
  `num ∈ {8,12,16}` for {EASY, NORMAL, HARD}
- Saturation: if accumulator would exceed `0xFFFF`, clamp to `0xFFFF`
  (worst-case real run ≈ 22000, headroom is fine; clamp is defensive)

Score is **separate** from energy. Energy spending does not affect score.

### Gameflow integration
- `main.c`: call `save_init()` once after audio/gfx init.
- `game.c::enter_playing()`: call `score_reset()` at end.
- `game.c::enter_gameover(win)`: call `score_add_win_bonus()` if win, then
  `bool record = score_commit_if_record();`, pass record into
  `gameover_enter(win, score_get(), record)`.

### Score hooks (where calls fire)
- `enemies.c::enemies_apply_damage` returning true (kill): caller in
  `projectiles.c::step_proj` already captures bounty pre-call (per the
  bounty-capture convention); add `score_add_kill(captured_type)` at the
  same site.
- `waves.c`: the existing state machine increments `s_cur` and transitions
  out of `WS_SPAWNING` the moment the **last spawn event of wave N
  succeeds** (waves.c:149-158). We treat this edge as "wave N cleared"
  for scoring purposes — i.e., **score awards on last-spawn-completes,
  not on last-enemy-drains**. Rationale: (a) the simpler edge already
  exists in the state machine; tracking per-wave drain would require
  tagging in-flight enemies with a wave id; (b) waves do not overlap
  (an `inter_delay` separates them) so the player has done the work to
  reach this edge; (c) the win-bonus (subtask 8) is gated on actual win
  (`HP > 0` AND all waves cleared AND `enemies_count() == 0`), so any
  premature wave-clear award is still meaningful only if the player
  ultimately survives. Add `u8 waves_just_cleared_wave(void)` returning
  `0` or `1..10` as a one-shot edge: `static u8 s_just_cleared` is set
  in `waves_tick` at the same point `s_cur` increments, cleared on read.
  `game_update` polls and forwards to `score_add_wave_clear(n)`.

### UI integration (per `specs/iter3-19-scores-design.md`)
- **Title**: row 15 cols 5..13, label `HI: 00000` (5-digit zero-pad). New
  5th dirty flag `s_hi_dirty` inserted in the title priority chain
  (`diff > map > focus > hi > prompt`). Set on title enter and on every
  map or difficulty cycle.
- **Gameover**: inside `DISPLAY_OFF` bracket, after the WIN/LOSE tilemap
  blit, paint `SCORE: NNNNN` at row 14 cols 4..15. If `record == true`,
  also paint `NEW HIGH SCORE!` at row 13 cols 2..16. Both static; only the
  existing PRESS START blink animates.
- **HUD**: untouched. No live score display.

### Module impact map
- NEW `src/save.h`, `src/save.c`, `src/save_calc.h`
- NEW `src/score.h`, `src/score.c`, `src/score_calc.h`
- `src/main.c` — call `save_init()`
- `src/game.c` — `enter_playing` resets score; `enter_gameover` commits +
  passes args; `game_update` polls `waves_just_cleared_wave()`
- `src/waves.c`, `src/waves.h` — add `u8 waves_just_cleared_wave(void)`
  one-shot edge accessor
- `src/projectiles.c` — single `score_add_kill` call alongside existing
  bounty-capture site
- `src/difficulty_calc.h` — add `difficulty_score_mult_num`
- `src/title.c`, `src/title.h` — add `s_hi_dirty` and `draw_hi_now()` row
- `src/gameover.c`, `src/gameover.h` — `gameover_enter(bool win, u16 score, bool record)`
- `src/economy.{c,h}` — **untouched**
- `src/hud.{c,h}` — **untouched**
- `tools/gen_assets.py` — **untouched** (all glyphs already present)
- `justfile` — add MBC flags to `build`; extend `check` to assert new
  header bytes and 64 KB ROM size
- NEW `tests/test_save.c`, `tests/test_score.c`

## Subtasks

1. ✅ **Cart-type conversion** — Update `justfile build` to pass
   `-Wl-yt0x03 -Wm-yo4 -Wm-ya1` to `lcc` (split prefixes per canonical
   GBDK example). Update `justfile check` to assert: file size = 65536,
   byte 0x147 = 0x03, byte 0x148 = 0x01, byte 0x149 = 0x01, header
   checksum at 0x14D matches Pan Docs algorithm. Done when `just build
   && just check` pass and `mgba` boots the title screen with no
   regression.

2. ✅ **`src/save_calc.h`** — Header-only `<stdint.h>`-only module
   with: `save_slot_offset(map_id, diff)`, `save_pack_u16(out, v)`,
   `save_unpack_u16(in)`, `save_magic_ok(buf)`, `save_stamp_header(buf, ver)`.
   Done when `tests/test_save.c` covers round-trip, magic-OK, magic-bad,
   slot-bounds.

3. ✅ **save module** — `src/save.{h,c}` implementing `save_init`,
   `save_get_hi`, `save_set_hi` over `_SRAM` with ENABLE_RAM/DISABLE_RAM
   bracketing. WRAM cache: `static u16 s_hi[9]`. Done when boot on a fresh
   mGBA `.sav` produces all-zero hi cache and stamps valid magic+version
   in SRAM (verified by hex-dumping the `.sav` file post-boot).

4. ✅ **score_calc.h pure helpers** — `<stdint.h>`-only header with
   `score_kill_base(enemy_type)`, `score_wave_base(wave_num)`,
   `SCORE_WIN_BONUS = 5000`, `score_apply_mult(base, mult_num)`,
   `score_add_clamped(cur, delta)`. Done when `tests/test_score.c` covers
   each enemy type, each wave 1..10, each difficulty multiplier, and
   saturation at 0xFFFF.

5. ✅ **score module** — `src/score.{h,c}` implementing the public API
   over `score_calc.h`, reading current difficulty via
   `game_difficulty()`. `score_commit_if_record()` reads
   `save_get_hi(map_id, diff)`, compares against `s_score`, and on
   improvement calls `save_set_hi(map_id, diff, s_score)` and returns
   true. No local high-score cache (cache lives in `save.c`). Done when
   host tests link the module with a stub `save.h` and verify
   commit-on-record / no-commit-on-tie.

6. ✅ **Difficulty multiplier** — Extend `src/difficulty_calc.h` with
   `difficulty_score_mult_num` returning `{8, 12, 16}`. Done when host
   `test_difficulty.c` adds three asserts.

7. ✅ **Scoring hooks** — Wire `score_add_kill` at the existing
   bounty-capture site in `projectiles.c`. Add `waves_just_cleared_wave`
   one-shot edge in `waves.c`; call `score_add_wave_clear(n)` from
   `game_update` when nonzero. Done when a debug build prints
   incrementing scores in the gameover screen.

8. ✅ **Game flow integration** — `enter_playing` → `score_reset`;
   `enter_gameover(win)` → `if (win) score_add_win_bonus();
   bool rec = score_commit_if_record(); gameover_enter(win, score_get(), rec);`.
   `main.c` calls `save_init()` after `gfx_init`. Done when a full run
   end-to-end shows correct final score on gameover.

9. ✅ **Title HI line** — Add `s_hi_dirty` (5th flag) and `draw_hi_now()`
   rendering `HI: NNNNN` at row 15 cols 5..13. Inserted in priority chain
   after focus, before prompt. Set on `title_enter` and after any
   difficulty/map cycle. Done when cycling map or difficulty on title
   updates the HI value within one frame.

10. ✅ **Gameover score & banner** — Update `gameover_enter` signature to
    `(bool win, u16 score, bool record)`. Inside `DISPLAY_OFF` bracket,
    paint `SCORE: NNNNN` at row 14 and (if record) `NEW HIGH SCORE!` at
    row 13. Done when WIN/LOSE/record/no-record cases all render
    correctly per design ASCII mocks.

11. ✅ **Host tests** — `tests/test_save.c` (slot offsets, pack/unpack,
    magic validation, header stamp). `tests/test_score.c` (each
    enemy/wave/diff combo, saturation, commit-if-record logic with stub
    save). Add both to `justfile test` target. Done when `just test`
    reports 12 binaries passing.

12. ✅ **Manual scenarios** — Add to `tests/manual.md`: (a) fresh-cart
    cold-boot test, (b) power-cycle persistence test, (c) NEW HIGH SCORE
    banner test (lose-with-record + win-with-record), (d) per-(map,diff)
    isolation test (set HARD-Map-3 high score, verify EASY-Map-1 unchanged).
    Done when manual.md merged.

## Acceptance Scenarios

### Setup
```sh
just build        # produces build/gbtd.gb at 65536 bytes
just check        # asserts header bytes 0x147=0x03, 0x148=0x01, 0x149=0x01
just test         # 12 host test binaries pass
rm -f build/gbtd.sav   # fresh cart for cold-boot scenarios
mgba-qt build/gbtd.gb  # or any DMG-compatible emulator with .sav support
```

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Cart conversion | `just build && just check` | All assertions pass; ROM = 64 KB; cart byte = 0x03 |
| 2 | Fresh-cart cold boot | Delete .sav; boot ROM; observe title | `HI: 00000` shown at row 15; .sav file created with magic `GBTD` + version 1 + 18 zero bytes |
| 3 | Score accumulates | Play Map 1 / NORMAL; let wave 1 spawn fully and clear; let game-over occur (lose by HP). Note: wave 1 contains BUGs only per `waves.c`. | At gameover, SCORE ≥ wave-1 clear bonus = `(100 × 1 × 12) >> 3 = 150` plus per-BUG kills `(10 × 12) >> 3 = 15` each |
| 4 | New high score on win | Win Map 1 NORMAL with full HP | Gameover shows `SCORE: NNNNN` and `NEW HIGH SCORE!`; title now shows updated HI |
| 5 | New high score on lose | Lose with score > prior best | Same banner appears even on LOSE screen |
| 6 | No new high score | Replay; achieve score ≤ prior best | Gameover shows SCORE but no banner |
| 7 | Power-cycle persistence | Get HI on Map 2 HARD; close emulator; reopen | Title with Map 2 HARD selected shows the saved HI |
| 8 | Per-(map,diff) isolation | Set HI on Map 3 HARD; cycle to Map 1 EASY | Map 1 EASY HI is independent (still 00000 if untouched) |
| 9 | Title HI updates with cycle | On title, press cycle to change difficulty or map | HI: line updates within one frame to the new slot's value |
| 10 | Saturation safety | Force-feed score 0xFFF0 + add 100 (debug build) | Score clamps to 0xFFFF; no wraparound |
| 11 | Difficulty multiplier (BUG kill) | Kill one BUG on EASY, NORMAL, HARD | Score deltas exactly `10`, `15`, `20` (`(10×{8,12,16})>>3`); ratio 8:12:16 holds because 10×N is a multiple of 8 only for N=8,16 — but the formula yields exactly these integer values; verify by API not ratio |
| 12 | Energy ≠ score | Spend all energy mid-run | Score unaffected by energy spend/award |

### UI states verified
- Title: HI line on first boot (`00000`), HI line after a record run, HI line cycling between 9 slots
- Gameover: WIN+record, WIN+no-record, LOSE+record, LOSE+no-record (4 variants)
- HUD: unchanged across all scenarios

## Constraints
- ROM ≤ 64 KB (post-conversion budget; current ~28 KB + ~1.5 KB new = ~29.5 KB)
- WRAM growth ≤ 24 B (`s_score` 2 + `s_hi[9]` 18 + bookkeeping ≤ 4)
- SRAM = 24 B used / 2048 B available
- No new external deps (no Python libs, no brew packages)
- All host-testable math in `<stdint.h>`-only headers (`score_calc.h`,
  `save_calc.h`); no `gtypes.h` from these
- Three-read-site rule respected for high-score reads: title HI line,
  gameover record check, save module commit (≤ 3)
- Save writes ONLY at `enter_gameover` (single trigger) — never mid-game
- DISABLE_RAM after every transaction (corruption resilience)
- Pure-helper rule: score and save serialization are header-only
- All prior conventions preserved (modal precedence, OAM ownership,
  bounty-capture, dirty-flag chains)

## Decisions

| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| Cart type byte 0x147 | 0x03 (MBC1+RAM+BATT), 0x09 (ROM+RAM+BATT) | **0x03** | Standard MBC1 path, well-supported by all emulators and flash carts |
| ROM size byte 0x148 | 0x00 (32 KB, MBC1-quirky), 0x01 (64 KB) | **0x01 (64 KB)** | Clean MBC1 minimum; avoids GBDK linker quirks; gives huge headroom |
| RAM size byte 0x149 | 0x01 (2 KB), 0x02 (8 KB) | **0x01 (2 KB)** | Smallest legal MBC1 RAM; 24 B used; no benefit to larger |
| lcc/makebin flags | `-Wm-y*` (all), `-Wl-y*` (all), split | **`-Wl-yt0x03 -Wm-yo4 -Wm-ya1` (split)** | Mirrors canonical GBDK example `examples/cross-platform/banks/Makefile`; `-yt` routes via linker, `-yo`/`-ya` route via makebin |
| Score type | u16, u32 | **u16** | Worst-case ~22000 < 65535; saves WRAM and tile width |
| Score formula | flat-per-kill, exponential, weighted | **kill {10/25/50} + 100×wave + 5000 win, ×{1,1.5,2}** | Action-rewarding, predictable, fits u16 |
| Score vs energy | shared, separate | **separate** | Hoarding energy shouldn't inflate score; separation aligns with "reward action" |
| Save scope | per-map (3), per-(map,diff) (9) | **per-(map, difficulty) = 9 slots** | Meaningful HARD bragging rights; only 18 B more |
| Magic bytes | `GBTD` ASCII, `0xDEADBEEF` | **`GBTD` (0x47 0x42 0x54 0x44)** | Self-documenting in hex dumps |
| Version byte | none, 1 byte | **1 byte = 0x01** | Future-proofs format; cheap |
| Save trigger | per-frame, on score change, on gameover | **on gameover only** | Minimizes power-loss corruption window |
| HUD live score | yes, no | **no** | HUD row 0 is full; arcade convention puts score on title/gameover |
| First-run display | `HI: -----`, `HI: 00000` | **`HI: 00000`** | Zero is a real u16; beating it triggers banner naturally |
| Title HI label | `HI:`, `BEST:`, `HISCORE:` | **`HI:`** | Shortest, arcade-standard; minimal tile budget |
| Banner animation | static, blink | **static** | Tile budget; one-blinker rule already used by PRESS START |
| Bank tagging | pre-tag with `__banked`, defer | **defer** | All current code fits banks 0+1; let GBDK auto-pack |
| Saturation | wrap, clamp at 0xFFFF | **clamp** | Defensive; real runs nowhere near limit anyway |
| Corruption resilience | CRC, checksum, magic-only | **magic + version only** | 24 B is too small to justify CRC; brief write window mitigates risk |

## Review

### Adversarial review (self, prior to cross-model)
1. **MBC1 + 32 KB ROM is legal but quirky** — chose 64 KB to sidestep entirely.
2. **Energy/score conflation** — explicitly separated; convention noted.
3. **Bounty-capture race** — score_add_kill placed at the existing
   capture site, NOT after `enemies_apply_damage` returns; convention
   already documented; reused.
4. **Wave-clear edge detection** — added explicit one-shot
   `waves_just_cleared_wave()` to avoid double-counting if `game_update`
   runs multiple frames in a clear state.
5. **SRAM access from interrupt context** — score module never called
   from ISR; save writes only on `enter_gameover` (main loop). Safe.
6. **Magic check on garbage SRAM** — version byte adds future migration
   path; on mismatch, full re-stamp.
7. **Title 5th dirty flag exceeds budget** — verified: priority chain
   still services ONE per frame, max BG writes per frame unchanged
   (HI line is 9 tiles ≤ 12-write cap).
8. **Gameover signature break** — touches one call site only; updated
   atomically with subtask 10.
9. **Per-(map,diff) slot ordering** — fixed formula `map*3 + diff`
   covered by `save_calc.h` and host-tested.
10. **`-Wl-` vs `-Wm-` flag form** — standardized on `-Wl-`; `just check`
    asserts the resulting header bytes regardless of flag form, so a
    silent flag-rejection regression would fail CI.

### Cross-model validation pass
A second adversarial review pass with `gpt-5.4` was completed. Findings
resolved below.

### Resolutions from first review pass
1. **`-Wl-yo`/`-Wl-ya` ignored by lcc** — fixed: split prefixes per the
   canonical GBDK example. `-Wl-yt0x03 -Wm-yo4 -Wm-ya1` is now the
   locked flag set. `just check` asserts the resulting header bytes so
   any silent flag-rejection regression fails CI.
2. **Wave-clear edge ambiguity** — fixed: explicitly chose
   "last-spawn-of-wave-N completes" as the scoring edge. Documented
   rationale in §"Score hooks". The simpler existing edge in waves.c
   suffices; per-wave drain tracking is not required.

### Resolutions from cross-model validation pass (gpt-5.4)
1. **Subtask 1 still had old flag form** — fixed: subtask 1 now uses
   `-Wl-yt0x03 -Wm-yo4 -Wm-ya1` matching the architecture and decisions.
2. **Cache ownership contradiction** — fixed: `save.c` is the single
   owner of `s_hi[9]`; `score.c` reads via `save_get_hi` on demand and
   keeps no local cache. Removed `score_get_hi` from `score.h`.
   WRAM budget now: `s_score` (2) + `save.c::s_hi[9]` (18) + bookkeeping
   (≤ 4) = 24 B as stated.
3. **Acceptance scenarios 3 and 11** — fixed: scenario 3 rewritten to
   match wave 1's BUG-only spawn list; scenario 11 rewritten to use BUG
   kill (deterministic per-difficulty values 10/15/20) instead of an
   inexact ratio claim.
