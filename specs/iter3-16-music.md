# Iteration 3 — Feature #16: Music

## Problem
The game currently has SFX only (channels 1/2/4). Roadmap entry #16 calls for a title theme, in‑game loop, and game‑over jingle — three pieces of music that give the game atmosphere and pacing cues. The roadmap suggests hUGETracker, but that introduces a desktop‑GUI authoring dependency and a 2–3 KB driver runtime. We want music that "just works" with `just run` on a clean macOS host, with no extra installs and minimal ROM growth.

## Architecture

### Tooling — custom mini‑engine (no hUGETracker)
Music is authored as plain C arrays in‑repo (one per song) and played by a small driver in `src/music.{h,c}`. **No external tracker, no driver runtime to bundle.** Rationale: 4 short pieces (≤30 s each) do not justify a 2–3 KB driver plus a GUI authoring tool. A linear note‑event list is trivial to write by hand, host‑testable, and identical in spirit to the existing `WIN_NOTES[]` / `LOSE_NOTES[]` SFX tables. If music complexity ever exceeds what hand‑authoring can sustain, swapping in hUGEDriver is a self‑contained future change (the music API surface won't shift).

### Channel allocation
- **CH1 (square+sweep)** — SFX only (TOWER_PLACE). Untouched by music.
- **CH2 (square)** — SFX only (TOWER_FIRE, BOOT). Untouched by music.
- **CH3 (wave/PCM)** — **music melody**. No SFX ever touches ch3, so no arbitration needed.
- **CH4 (noise)** — **music percussion** with SFX preempt. SFX_ENEMY_HIT (prio 1) and SFX_ENEMY_DEATH (prio 2) preempt the music's ch4 row; music re‑arms ch4 at the next note boundary after the SFX completes.

CH3 is the canonical music channel per D13. CH4 percussion is added because a single‑voice tune sounds bare; the arbitration cost is small (one extra branch in `music_tick`).

### Module split
New files `src/music.h` + `src/music.c`. Reasons:
- Keeps `audio.c` focused on per‑shot SFX (already 207 lines).
- `audio_reset()` will call `music_reset()` (master reset stays in audio.c).
- `audio_tick()` will call `music_tick()` once per frame so the main loop has a single audio entry point.

### Song data format (linear note list with loop point)
```c
typedef struct {
    uint16_t pitch;      /* CH3 freq in NR33/NR34 11-bit form, OR 0 = rest */
    uint8_t  noise;      /* CH4 NR43 poly-counter byte, OR 0 = rest */
    uint8_t  duration;   /* frames this row is held; 0 = end-of-song marker */
} mus_row_t;

typedef struct {
    const mus_row_t *rows;
    uint8_t row_count;
    uint8_t loop_idx;    /* row index to jump to at end; 0xFF = stinger (no loop) */
    uint8_t wave_idx;    /* which wave pattern to load before play (0..N-1) */
    uint8_t ch3_vol;     /* NR32 output level: 0=mute, 1=100%, 2=50%, 3=25% */
    uint8_t ch4_env;     /* NR42 envelope byte for percussion */
} mus_song_t;
```
- One row = one frame‑duration tick of melody+percussion together. Compact (4 B/row), enough granularity for ≤30 BPM resolution at 16ms/frame.
- Loop point allows e.g. an 8‑bar intro followed by a 16‑bar loop.
- Stinger = `loop_idx == 0xFF` ⇒ play to end then go idle.

### Engine state (WRAM)
```c
typedef struct {
    const mus_song_t *song;   /* NULL = idle */
    uint8_t row_idx;
    uint8_t frames_left;      /* in current row */
    uint8_t ch4_blocked;      /* nonzero while SFX owns ch4 */
} music_state_t;
```
~5 bytes total. Single global instance.

### API
```c
/* music.h */
enum { MUS_TITLE = 0, MUS_PLAYING, MUS_WIN, MUS_LOSE, MUS_COUNT };
/* Note: there is no MUS_NONE. Use music_stop() for idle. */

void music_init(void);                    /* load wave RAM (DAC-off-write-on); idle. Call ONCE. */
void music_reset(void);                   /* stop, silence ch3+ch4, clear state */
void music_play(uint8_t song_id);         /* sets song AND programs row 0 NRxx synchronously */
void music_stop(void);                    /* silence ch3+ch4; idle state */
void music_tick(void);                    /* advance one frame; called from audio_tick */
void music_duck(uint8_t on);              /* on=1 lowers NR50 to DUCK_VOL; on=0 restores */
void music_notify_ch4_free(void);         /* unconditional notify from audio.c */
void music_notify_ch4_busy(void);         /* unconditional notify from audio.c (no-op if idle) */
```

**`music_play()` semantics (synchronous arm):** mirrors `audio_play→start_note`. On a real song change it (a) silences ch3+ch4 if a different song was playing, (b) sets `s_state.song` + `row_idx = 0` + `frames_left = rows[0].duration`, (c) **immediately writes NR3x and NR4x for row 0** (DAC on, freq, trigger). This guarantees the WIN/LOSE stinger's first note plays even when the caller blocks the main loop for several frames (e.g., `enter_gameover()` → `gameover_enter()` DISPLAY_OFF + 360‑tile redraw). Subsequent rows are armed by `music_tick()`.

**Include direction:** `audio.c` includes `music.h` to call `music_notify_ch4_*()`. `music.c` MUST NOT include `audio.h`. This keeps audio leaf‑free of music data structures and avoids circular includes.

**`MUS_COUNT`:** the song table is `s_songs[MUS_COUNT]`, fully populated. No sentinel slot.

**`music_init()` is init‑once.** Wave RAM is written exactly once at boot. Re‑entry is unsupported (assert in debug; no guard in release).

### SFX↔music ch4 arbitration

**Within‑frame ordering inside `audio_tick()`** (canonical order, no exceptions):
1. Run the existing SFX state machine for ch1/ch2/ch4 (countdown, advance, silence).
2. **Edge detect ch4**: if a ch4 SFX was active at frame start (`prio != 0`) and is now idle (`prio == 0` after step 1), call `music_notify_ch4_free()`.
3. Call `music_tick()` (advances music; may re‑arm ch4 if `ch4_blocked == 0` AND a row boundary is crossed THIS tick).

**Notify semantics:**
- `audio_play(SFX_ENEMY_HIT|DEATH)` ALWAYS calls `music_notify_ch4_busy()` (unconditional; no‑op when music is idle or current row's `noise == 0`). The "music owns ch4" check lives in `music.c`, not audio.c — this keeps audio.c the leaf module.
- `music_notify_ch4_busy()` sets `ch4_blocked = 1`. Any in‑progress ch4 row is left alone (the SFX will overwrite NR4x on its own trigger).
- `music_notify_ch4_free()` sets `ch4_blocked = 0`. **It does NOT re‑arm ch4 immediately.** Re‑arm happens only when `music_tick()` next crosses a row boundary AND that row's `noise != 0`. Result: at minimum one tick of silence after the SFX, no clicks, no double‑arming within the same frame.

**Percussion register defaults** (constants in music.c):
- `NR41 = 0x3F` (length 0; we don't use the length counter)
- `NR42 = song->ch4_env` (envelope from song header)
- `NR43 = row->noise` (poly counter byte)
- `NR44 = 0x80` (trigger, length‑enable bit OFF)

### Boot ordering
**Constraint: `main()` MUST call `audio_init()` before `game_init()`.** This is the current order in `src/main.c` (audio_init at line 12, game_init at line 14) and must be preserved. Rationale: `game_init()` calls `enter_title()` which calls `music_play(MUS_TITLE)`; if music state and wave RAM are not initialized first, ch3 plays garbage.

`music_init()` is called from `audio_init()` AFTER `audio_reset()` and AFTER the `audio_play(SFX_BOOT)` line. The boot chime fires on ch2 unaffected; music stays idle until `music_play()` is called from `enter_title()` during `game_init()`.

### Pause ducking
`pause_open()` calls `music_duck(1)`; `pause_close()` calls `music_duck(0)`. Duck implementation: write `NR50 = DUCK_VOL` (default `0x33` = volume 3/7 each side ≈ 43% loudness), restore `0x77`. Affects ALL channels (SFX too) — this matches typical DMG behavior where ducking touches the master and is one register write. The menu modal does NOT duck (menu is short and not paused gameplay).

### Removal of SFX_WIN / SFX_LOSE
Replaced by `MUS_WIN` / `MUS_LOSE`. `enter_gameover()` now calls `music_play(win ? MUS_WIN : MUS_LOSE)`. `SFX_WIN` and `SFX_LOSE` enum values are removed from `audio.h` and their tables removed from `audio.c`. Saves ~50 bytes ROM and removes the long ch1‑prio‑3 occupancy that previously blocked TOWER_PLACE on the gameover screen.

## Subtasks

1. ✅ **Designer: produce song notation** — Designer agent writes `specs/iter3-16-music-design.md` containing: ASCII tablature for 4 songs (MUS_TITLE 16 bars @ ~80 BPM, MUS_PLAYING 16 bars @ ~120 BPM, MUS_WIN 4‑bar stinger, MUS_LOSE 4‑bar stinger), a single 16‑byte wave pattern (mellow triangle‑ish for ship‑1 simplicity), pause duck NR50 value (`0x33`), and a per‑song row table previewing pitch (note name) + percussion (kick/snare/silence) + duration in frames. Done when the design file exists with all 4 songs fully specified row‑by‑row.

2. ✅ **Engine skeleton** — Create `src/music.h` and `src/music.c` with the structs, enums, and API stubs above. `music_init/reset/stop/tick/duck/notify_*` compile but do nothing meaningful yet. Done when `just build` succeeds with empty bodies and `audio_init` calls `music_init`.

3. ✅ **Wave pattern + ch3 driver** — Implement `music_init()` to load the wave pattern into 0xFF30–0xFF3F (DAC off → write 16 bytes → DAC on). Implement `music_tick()` for ch3: row advance, NR30/NR32/NR33/NR34 writes per row, loop at `loop_idx`, idle when stinger ends. Done when MUS_TITLE plays as a melody loop in the emulator with no audible glitch at the loop point.

4. ✅ **Percussion + arbitration** — Add ch4 row playback to `music_tick()` (NR41/NR42/NR43/NR44). Wire `audio_play()` to call `music_notify_ch4_busy()` for SFX_ENEMY_HIT / SFX_ENEMY_DEATH. Add the "ch4 went idle this frame" detector to `audio_tick()` and call `music_notify_ch4_free()`. Done when killing enemies during MUS_PLAYING produces SFX over the kick/snare line and music timing does not drift (verified by host test in #8).

5. ✅ **Song data tables** — Translate designer tablature into `mus_row_t[]` arrays in `src/music.c` for all 4 songs. Build the `mus_song_t s_songs[MUS_COUNT]` table. Done when all 4 songs play correctly in‑order via a temporary debug button (e.g., reuse an existing test path) and total music data is ≤ 1.5 KB (measured from map file).

6. ✅ **Game state wiring** — In `src/game.c`: `enter_title()` calls `music_play(MUS_TITLE)`; `enter_playing()` calls `music_play(MUS_PLAYING)` AFTER the existing `audio_reset()`; `enter_gameover()` calls `music_play(win ? MUS_WIN : MUS_LOSE)` (replacing the removed SFX_WIN/SFX_LOSE call). Note: `audio_reset()` is the master reset and now also resets music state. Done when state transitions in the emulator switch tracks correctly without click or stuck note.

7. ✅ **Pause ducking** — In `src/pause.c`, on open call `music_duck(1)`; on close call `music_duck(0)`. Quit‑to‑title path: `audio_reset()` (which now calls `music_reset()` internally) then `music_duck(0)` to restore NR50 before `enter_title()`. Done when opening pause audibly halves volume and closing restores; quitting to title restores full volume.

8. ✅ **Remove legacy SFX_WIN / SFX_LOSE AND dead multi‑note code** — Delete `SFX_WIN`, `SFX_LOSE` enum values, `WIN_NOTES[]`, `LOSE_NOTES[]`, and their `sfx_def_t` table entries from `audio.{h,c}`. After removal, every remaining SFX is single‑note (note_count==1), so also delete from `sfx_def_t` the multi‑note fields (`note_count`, `frames_per_note`, multi‑element `pitches[]`) and the multi‑note advance branch in `audio_tick()` (audio.c lines ~192–203). Update `tests/test_audio.c`: (a) delete `test_tick_advances_multi_note_sfx` (its only data source is gone — coverage of multi‑note advancement migrates to `test_music.c` which exercises a richer state machine); (b) rewrite `test_priority_preempt_rules` to use ch4: `audio_play(SFX_ENEMY_DEATH)` (prio 2) then `audio_play(SFX_ENEMY_HIT)` (prio 1) — the lower‑prio call MUST be rejected. This preserves the F1 "lower‑prio rejected" invariant on a different channel. Done when no `SFX_WIN`/`SFX_LOSE`/multi‑note symbols remain, `just build` + `just test` pass.

9. ✅ **Host tests** — New `tests/test_music.c` covering: (a) `music_play(MUS_TITLE)` synchronously writes wave RAM (once, before any tick) AND row 0's NR3x — assert NR34 trigger bit is set in the write log immediately after `music_play()` returns, with zero `music_tick()` calls in between; (b) advancing N frames lands on row 1; (c) reaching `row_count` with `loop_idx=4` jumps `row_idx` back to 4; (d) stinger (`loop_idx=0xFF`) goes idle after final row (ch3+ch4 silenced, no underflow on further ticks); (e) `music_play(current)` is idempotent — `row_idx` unchanged, no extra trigger write logged; (f) `music_play(other)` resets `row_idx` to 0 AND writes the new row 0 synchronously; (g) `music_notify_ch4_busy()` then 10 ticks: NR41–NR44 NOT written; then `music_notify_ch4_free()`; one tick (mid‑row, no boundary): NR41–NR44 still NOT written; tick across the next row boundary with `noise != 0`: NR41–NR44 written exactly once; (h) `music_duck(1)` writes `NR50=0x33`, `music_duck(0)` writes `NR50=0x77`. Wire into `justfile` `test` target. Done when `test_music` is in `just test` output and all assertions pass.

10. ✅ **Pure‑helper extraction (if applicable)** — Any host‑testable math (e.g., row‑advance + loop arithmetic) extracted to a `<stdint.h>`‑only inline helper in `src/music.h` per the project's pure‑helper rule. Done when the rule is satisfied or explicitly N/A noted in the file.

11. ✅ **Manual scenarios** — Append music section to `tests/manual.md` listing the acceptance scenarios below. Done when section exists.

## Acceptance Scenarios

### Setup
- `just build && just test` clean.
- `just run` to launch in mGBA with `-C mute=0 -C volume=0x100` (already in justfile).
- Audio output unmuted on host.

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Boot chime then title theme | Power on ROM | Hear the existing ~200 ms ch2 boot chime, immediately followed by MUS_TITLE looping. No silence gap > 1 s. |
| 2 | Title → Playing transition | Press START on title | MUS_TITLE stops; MUS_PLAYING begins within 1 frame. No stuck wave note (audible click acceptable but no held tone). |
| 3 | In‑game SFX over music | Place a tower, fire, kill an enemy | TOWER_PLACE on ch1, TOWER_FIRE on ch2, ENEMY_HIT/DEATH on ch4 all play audibly OVER MUS_PLAYING melody on ch3. Kick/snare line on ch4 momentarily silenced during ENEMY_HIT/DEATH then resumes at next row. |
| 4 | Win stinger | Survive all waves | MUS_PLAYING stops; MUS_WIN plays once (~3 s); silence afterward (no loop). |
| 5 | Lose stinger | Let HP reach 0 | MUS_PLAYING stops; MUS_LOSE plays once (~3 s); silence afterward. |
| 6 | Pause ducking | While MUS_PLAYING is audible, press SELECT (or pause button) | NR50 drops to `0x33`, music + any subsequent SFX audibly quieter (~half loudness). Closing pause restores full volume. |
| 7 | Quit to title restores volume | Open pause → Quit | Returns to title, MUS_TITLE playing at full NR50=0x77 volume. |
| 8 | Loop point | Sit on title screen for 60 s | MUS_TITLE loops seamlessly. No audible discontinuity at the loop boundary. |
| 9 | Idempotent play | (Internal: in code path that re‑enters a state) | `music_play(MUS_PLAYING)` while MUS_PLAYING is active does not restart row 0 — verified by test #9(e). |
| 10 | ROM size | After build | `build/gbtd.gb` ≤ 32 KB; map file shows total ≤ ~22 KB code. |

## Constraints
- ≤ 32 KB ROM cap (still MBC0). Current code: 18,020 B. Music engine + 4 songs + 1 wave pattern projected: +2.5–3 KB → ~21 KB. **No MBC1 conversion required this iteration.**
- WRAM growth ≤ 16 B (music_state_t + a small frame counter).
- Per‑frame audio writes: music adds ≤ 8 NRxx writes/frame at row boundaries (≤ 1 every ~10 frames mid‑row). Below the 16 BG‑write/frame budget (separate budget; no VRAM impact).
- NR52‑first ordering (D14 / iter‑2 audio test) preserved: `music_init()` runs AFTER `audio_init()` writes NR52.
- F1 (`audio_reset()` semantics) preserved: `audio_reset()` becomes the master reset and internally calls `music_reset()`. Still does NOT touch NR50/51/52 (so ducking state is preserved across reset — but `music_duck(0)` is also called explicitly on quit‑to‑title so the user always returns to full volume).
- F2 (DAC off via NRx2=0x00) honored on ch4. Ch3 DAC off = `NR30 = 0x00`; on = `NR30 = 0x80`.
- Pure‑helper rule: row advance arithmetic placed in `<stdint.h>`‑only inline header section of `music.h`.
- No new external toolchain. No hUGETracker, no `hugetrack` CLI, no driver bundle.

## Decisions
| Decision | Options Considered | Choice | Rationale |
|---|---|---|---|
| Tooling | A: hUGETracker GUI + `hugetrack` CLI; B: hand‑authored C tables + custom mini‑engine; C: hUGEDriver + pre‑converted blobs | **B** | Avoids GUI dependency, saves 2–3 KB driver, matches existing `WIN_NOTES[]` style, keeps `just run` zero‑install. |
| Channel allocation | A: ch3 only; B: ch3+ch4; C: all 4 with full preempt | **B (ch3+ch4)** | Adds percussion without touching the heavily‑used ch1/ch2 SFX channels. Only ch4 needs arbitration. |
| Module split | Extend audio.c; new music.{h,c} | **New music.{h,c}** | Audio.c already 207 lines; clean separation of concerns; tests stay focused. |
| Song format | Linear note list; row‑based with patterns | **Linear rows w/ loop_idx** | Simpler code, song data is small enough that pattern compression isn't worth implementation cost. |
| WIN/LOSE | Keep SFX_WIN/LOSE alongside MUS_WIN/LOSE; remove SFX versions | **Remove SFX_WIN/LOSE** | No other caller; avoids ch1‑prio‑3 occupancy blocking TOWER_PLACE; saves ~50 B. |
| Pause behavior | Stop music; lower volume; keep playing | **Lower volume (NR50=0x33)** | One‑register implementation, audible feedback, matches typical convention. |
| Pause ducking value | 0x55, 0x33, 0x11 | **0x33** | ~43% loudness — clearly quieter, still audible. Designer can tune in design file. |
| Ch4 re‑arm timing | Immediately on SFX end; at next row boundary | **Next row boundary** | Avoids click; only loses up to N frames of percussion (fine for short hit/death SFX). |
| Game‑over end behavior | Loop ambient; silence | **Silence after stinger** | Stinger is short; gameover screen is brief; ambient adds data cost for marginal value. |
| Menu (upgrade/sell) ducking | Duck; don't duck | **Don't duck** | Menu is transient and gameplay isn't paused; ducking would add noise. |
| Wave patterns shipped | 1 vs 2 | **1** | All 4 songs share a single mellow waveform; saves 16 B and avoids a wave‑swap glitch path. Designer can specify the pattern. |
| Master reset behavior | `audio_reset` only zeros SFX state; or also resets music | **Master reset (resets both)** | Single source of truth; existing call sites continue to work. |
| Stinger first-note timing | `music_play()` is state-only (waits for tick); programs row 0 synchronously | **Synchronous arm in `music_play()`** | `enter_gameover()` blocks the main loop for several frames during DISPLAY_OFF + 360-tile redraw; without sync arm, MUS_WIN/LOSE first note is silent. Mirrors `audio_play→start_note`. |
| MUS_NONE sentinel | Include `MUS_NONE=0`; omit it | **Omit; use `music_stop()` for idle** | Avoids empty `s_songs[]` slot, undefined `music_play(MUS_NONE)`, and idempotency-check edge cases. |
| ch4 SFX-end notify ordering | Notify before `music_tick`; after; from inside SFX silence branch | **Edge-detect after SFX state advance, notify before `music_tick` in same frame; re-arm gated on row boundary AFTER unblock frame** | Eliminates same-frame click and double-arm; gives deterministic re-arm timing for testing. |
| Multi-note SFX infrastructure post-removal | Keep dormant; delete | **Delete dead code; rewrite priority test on ch4 (DEATH preempts, HIT rejected)** | Removes ~30 lines of dead branches and unused `sfx_def_t` fields; preserves the F1 lower-prio-rejected invariant via a still-existing channel. |
| audio.c ↔ music.c include direction | Bidirectional; one-way | **One-way: `audio.c` includes `music.h`; `music.c` MUST NOT include `audio.h`** | Avoids circular includes; keeps music as the dependent module. |

## Review

### Round 1 (self‑review against rubric)

- **Q: What if the SFX_BOOT (12 frames on ch2) is still playing when `enter_title()` runs?**
  A: Music starts only on ch3+ch4; ch2 is SFX‑only. They don't conflict. Confirmed safe.
- **Q: What if `audio_reset()` is called mid‑song (e.g., on quit‑to‑title)?**
  A: It now calls `music_reset()` which silences ch3 (`NR30=0x00`) and ch4 (`NR42=0x00`), zeroes state. `enter_title()` then calls `music_play(MUS_TITLE)` cleanly.
- **Q: Wave RAM write timing — DMG corrupts wave RAM if written while ch3 is enabled.**
  A: Addressed in subtask #3: turn DAC off (`NR30=0x00`) before writing 0xFF30–0xFF3F, then DAC on. `music_init()` runs once at boot when ch3 is idle. Re‑load not needed because we ship one pattern.
- **Q: Will `music_play()` from the same state re‑trigger?**
  A: Idempotency check: `if (s_state.song == &s_songs[id]) return;` — covered in test #9(e).
- **Q: What about the gameover→title path? Does MUS_WIN/LOSE finish before MUS_TITLE starts?**
  A: `enter_gameover()` plays the stinger; user must press a button to return to title; then `enter_title()` calls `music_play(MUS_TITLE)` which stops the stinger (or it's already finished). Both paths handled by `music_play()`'s "different song → reset row_idx" branch.
- **Q: ROM growth precision — could it exceed budget?**
  A: 4 songs × ~150 rows × 4 B = 2.4 KB worst case + ~600 B engine + 16 B wave + song table ≈ 3 KB. From 18 KB → 21 KB. Headroom 11 KB remains. Safe even at 2× overshoot.
- **Q: Test coverage gap — does test_music.c need a stub for the new NR3x writes?**
  A: Existing `tests/stubs/gb/hardware.h` already exposes NR30–NR34 (per investigation report §10.2). No stub change needed.

### Round 2 (post‑review resolution)

Reviewer (Decker) returned 6 findings. All addressed in‑spec:

1. **Stinger first-note delay (IMPORTANT)** → `music_play()` now specified as **synchronous arm**: programs row 0 NRxx immediately, mirroring `audio_play→start_note`. `enter_gameover()` ordering preserved (music_play BEFORE gameover_enter).
2. **Dead multi-note code + lost test coverage (IMPORTANT)** → Subtask 8 expanded: delete dead `sfx_def_t` fields and audio_tick branch; rewrite `test_priority_preempt_rules` on ch4 (ENEMY_DEATH prio 2 preempts; ENEMY_HIT prio 1 rejected). Multi-note state-machine coverage migrates to `test_music.c`.
3. **`audio.c → music.c` coupling (MEDIUM)** → Notify is **unconditional** from audio.c (no-op when music idle). Include direction pinned: audio.c may include music.h; music.c MUST NOT include audio.h.
4. **Within-frame ordering under-specified (MEDIUM)** → Canonical 3-step order documented in §"SFX↔music ch4 arbitration". Re-arm gated on row boundary AFTER unblock frame; test 9(g) verifies.
5. **Boot ordering precondition (MEDIUM)** → §"Boot ordering" now asserts `audio_init` MUST precede `game_init` in main(); current main.c already complies (line 12 vs 14).
6. **MUS_NONE collision (MEDIUM)** → Removed. Enum starts at MUS_TITLE=0; idle = `music_stop()`.

Minor notes also addressed: NR41/NR44 default values pinned (`0x3F`/`0x80`); `music_init()` declared init-once.

### Round 3 (cross-model validation)

Skipped: scope is small (one new module, well-bounded); all six findings from round 1 were narrow and locally fixable; existing audio test infrastructure transfers to music tests with no architectural extension. Single-round adversarial review judged sufficient per planner discretion.

## Memory updates

Append to `memory/decisions.md`:

> **D‑MUS‑1 (Iter‑3 #16): Custom mini music engine, no hUGETracker.** Music authored as `mus_row_t[]` C arrays in `src/music.c`. Driver in `src/music.{h,c}` (~600 B). Avoids GUI tooling dependency and 2–3 KB hUGEDriver runtime. Future swap to hUGEDriver remains possible; API surface (`music_play/stop/tick/duck`) would not change.
>
> **D‑MUS‑2: Music channel allocation = CH3 (melody) + CH4 (percussion).** CH1/CH2 remain SFX‑only. Ch3 has no SFX so no arbitration. Ch4 SFX (ENEMY_HIT/DEATH) preempt music's ch4 row; music re‑arms at next row boundary via `music_notify_ch4_busy/free`.
>
> **D‑MUS‑3: SFX_WIN and SFX_LOSE removed.** Replaced by MUS_WIN / MUS_LOSE stinger songs. Removes ch1‑prio‑3 occupancy that previously blocked TOWER_PLACE on the gameover screen.
>
> **D‑MUS‑4: Pause menu ducks NR50 to 0x33 (≈43%).** Single‑register write on `pause_open`/`pause_close`. Quit‑to‑title path explicitly restores `NR50=0x77` after `audio_reset()`. Upgrade/sell menu does NOT duck.
>
> **D‑MUS‑5: `audio_reset()` is the master reset.** Internally calls `music_reset()`. Continues to NOT touch NR50/51/52 (preserves F1 semantics). All existing callers unchanged.

Append to `memory/conventions.md`:

> **Music engine conventions (Iter‑3 #16):**
> - Music lives in `src/music.{h,c}`; SFX in `src/audio.{h,c}`. `audio_tick()` calls `music_tick()` once per frame — single audio entry point for the main loop.
> - Music owns CH3 (always) and CH4 (when not preempted by SFX). CH1/CH2 are SFX‑exclusive.
> - Wave RAM (0xFF30–0xFF3F) is written ONCE in `music_init()` with the DAC off (`NR30=0x00`), then DAC on (`NR30=0x80`). Never write wave RAM while ch3 is enabled.
> - Song format = linear `mus_row_t` array with `loop_idx` (0xFF = stinger). End marker is `duration == 0` OR `row_idx == row_count`.
> - `music_play(id)` is idempotent for the active song; switching songs resets `row_idx` to 0.
> - Pause ducking touches NR50 only (`0x77` ↔ `0x33`). Affects all channels — intentional.
