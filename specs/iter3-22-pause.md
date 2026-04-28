# GBTD — Iteration 3 #22: Pause Menu

> Roadmap entry: iter-3 #22. Builds on `specs/iter2.md` (modal sprite-overlay menu pattern) and the
> as-shipped iter-2 code under `src/`. Single source of truth for the coder pass — no TBDs.

---

## 1. Problem

Add a player-controllable pause to the in-game state. Pressing **START** during `GS_PLAYING` must
freeze gameplay and present a modal menu offering **RESUME** (continue) or **QUIT** (abandon run,
return to title). This addresses the roadmap requirement *"Start button pauses, shows wave info,
allows quit-to-title."*

Wave info, HP, and energy are **already on screen at all times** via the iter-2 HUD (top row,
`HP:N E:NNN W:NN/NN T`). Re-rendering them inside the pause overlay would duplicate information
and burn OAM unnecessarily. The pause menu therefore consists only of a **PAUSE** header and the
two action items; the HUD remains the source of wave / HP / energy readout.

Out of scope (deferred): fade transitions, settings sub-menu, save/load, music ducking
(no music yet — iter-3 #16 not implemented).

---

## 2. Module Impact Map

| File | Status | Change |
|------|--------|--------|
| `src/game.{h,c}`        | TOUCHED | new `game_is_modal_open()` helper; `playing_update` adds pause-mode early-return arm and START handler at end-of-frame; `enter_title()` and `enter_playing()` call `pause_init()`. |
| `src/pause.{h,c}`       | **NEW** | Pause state machine + sprite-overlay rendering. ~120 LOC. |
| `src/menu.{h,c}`        | UNTOUCHED | Tower upgrade/sell menu unchanged. Mutual exclusion enforced in `game.c`. |
| `src/cursor.{h,c}`      | UNTOUCHED | Reuses existing `cursor_blink_pause(true)` and an explicit `move_sprite(OAM_CURSOR, 0, 0)` — same pattern menu.c uses minus the steady-tile hack (we hide the cursor entirely). |
| `src/input.{h,c}`       | UNTOUCHED | `input_is_pressed(J_START)` already provides edge detection. |
| `src/audio.{h,c}`       | UNTOUCHED | No new SFX (decision §5.7). `audio_reset()` reused on quit-to-title. |
| `src/gtypes.h`          | TOUCHED | Bump `OAM_MENU_COUNT` 14 → **16** (absorb previously reserved slots 15..16). Add `OAM_PAUSE_BASE = 1`, `OAM_PAUSE_COUNT = 16` aliases for clarity (same physical range as menu — mutually exclusive). |
| `tools/gen_assets.py`   | TOUCHED | Append 6 sprite glyphs via `glyph_to_sprite()`: `A, R, M, Q, I, T`. |
| `res/assets.{c,h}`      | REGENERATED | `just assets`. New defines `SPR_GLYPH_A/R/M/Q/I/T`. `SPRITE_TILE_COUNT` 29 → 35. |
| `src/main.c`            | UNTOUCHED | Loop order unchanged. |
| `src/title.{h,c}`, `gameover.{h,c}` | UNTOUCHED | START in those states keeps existing semantics. |
| `tests/test_pause.c`    | **NEW** | Host-side unit tests (selection cycling, modal precedence). See §10. |
| `tests/manual.md`       | TOUCHED | Add manual checklist entries that cannot be host-tested. |
| `justfile`              | TOUCHED | Add a `test_pause` build+run pair to the `test` recipe (mirrors the existing `test_audio` lines). Compiles `tests/test_pause.c` + `src/pause.c` against `tests/stubs/`. |
| `memory/decisions.md`   | APPENDED | New decision entries (§D1..D4). |
| `memory/conventions.md` | APPENDED | New convention entries (OAM remap, modal helper, pause module init). |

---

## 3. Architecture

Pause is a **flag inside the existing `GS_PLAYING` state**, not a new state in the top-level state
machine. This mirrors how `menu_is_open()` already works for the upgrade/sell menu. Rationale:

- The state machine in `game.c` exists to switch *render paths* (title / playing / gameover have
  totally different BG layouts). Pause keeps the play-field BG; only sprite overlay differs.
- Reuses every existing module-init/teardown convention without adding state-transition glue.
- A dedicated `GS_PAUSED` state would need its own update/render plus a way to remember the
  pre-pause state to return to. The flag pattern avoids that bookkeeping.

A new module `src/pause.{h,c}` owns the flag, the selection state, the OAM glyphs, and the
input handling. It is **not** an extension of `menu.c` because:

- Anchor is fixed (centered) vs `menu.c`'s tower-relative + clamp + flip logic.
- Action set is open-ended (RESUME / QUIT) vs fixed two-item upgrade/sell with cost lookup.
- Confirm action for QUIT triggers a **state transition** (`enter_title()`) — would otherwise
  pollute `menu.c` with a back-pointer to `game.c`.
- Layout dimensions differ (3 rows × 8 cols vs 2 rows × 7 cols).
- Mutual exclusion is cheaply enforced at the call site (`game.c`) — no shared infra is lost.

### Modal precedence (single source of truth)

```c
/* game.h */
bool game_is_modal_open(void);   /* menu_is_open() || pause_is_open() */
```

Used by future code that needs to know "is gameplay frozen". Existing `menu_is_open()` callers
keep working. `playing_update` orchestrates which modal handler runs each frame — there is
exactly one entry point that decides this, so the two modals **cannot** be open simultaneously
by construction. As a belt-and-suspenders measure, `pause_open()` itself is a defensive no-op
when `menu_is_open()` returns true (see §6).

### Frame loop integration (gameover wins over pause)

```c
static void playing_update(void) {
    if (pause_is_open()) {
        pause_update();              /* handles UP/DOWN, A confirm, B/START close */
        economy_tick();
        audio_tick();
        return;                      /* no entity ticks → no gameover possible */
    }
    if (menu_is_open()) {
        menu_update();
        economy_tick();
        audio_tick();
        return;
    }

    /* Normal frame */
    cursor_update();
    if (input_is_pressed(J_B)) cycle_tower_type();
    if (input_is_pressed(J_A)) {
        i8 idx = towers_index_at(cursor_tx(), cursor_ty());
        if (idx >= 0) {
            menu_open((u8)idx);
            economy_tick(); audio_tick();
            return;
        }
        towers_try_place(cursor_tx(), cursor_ty(), s_selected_type);
    }
    towers_update();
    enemies_update();
    projectiles_update();
    waves_update();
    economy_tick();
    audio_tick();

    /* Gameover wins over pause: if HP hit 0 / waves cleared THIS frame, transition
     * BEFORE opening pause, so a same-frame START press is a no-op.
     * NOTE: the WIN branch in current src/game.c is MISSING the `return;` —
     * Subtask 4 MUST add it, otherwise pause_open() runs after enter_gameover
     * and lays sprite glyphs over the WIN screen. */
    if (economy_get_hp() == 0)                       { enter_gameover(false); return; }
    if (waves_all_cleared() && enemies_count() == 0) { enter_gameover(true);  return; }

    if (input_is_pressed(J_START)) {
        pause_open();   /* sprite-OAM only; render() draws glyphs next VBlank */
    }
}
```

`playing_render()` adds one line: `pause_render();` after `menu_render();`.

### Entry/exit choreography

| Event | Action sequence |
|-------|-----------------|
| `pause_open()` | `s_paused = true; s_sel = 0;` → `enemies_hide_all(); projectiles_hide_all();` → `cursor_blink_pause(true); move_sprite(OAM_CURSOR, 0, 0);` (hide cursor entirely — pause has no use for the placement cursor). |
| `pause_close()` | `s_paused = false;` → hide all 16 pause OAM slots → `cursor_blink_pause(false);`. Cursor reappears next `cursor_update()`. |
| QUIT confirmed | `pause_close()` first, then `audio_reset()` (silence any in-flight SFX from the abandoned run), then `enter_title()`. `enter_title()` already calls every module's `_init()` (including the new `pause_init()`) and `title_enter()`. **`audio_reset()` lives in the QUIT path inside `playing_update`, NOT in `enter_title()`** — keeps the boot path (`game_init → enter_title`) free of audio state writes that would race with `audio_init()` in `main.c`. |

---

## 4. Layout (sprite-overlay, OAM 1..16)

Anchored centered on play field. Widget = 8 tiles × 3 tiles = **64 × 24 px**, top-left at
screen `(48, 64)` — clear of the HUD (y=8) and within the 144 px screen.

```
                    PAUSE
                    > RESUME
                      QUIT
```

OAM cell map (per designer spec):

| OAM | Glyph | wcol | wrow | OAM (x, y) | Notes |
|-----|-------|------|------|------------|-------|
| 1   | P     | 2    | 0    | (72,  80)  | header, fixed |
| 2   | A     | 3    | 0    | (80,  80)  | header, fixed |
| 3   | U     | 4    | 0    | (88,  80)  | header, fixed |
| 4   | S     | 5    | 0    | (96,  80)  | header, fixed |
| 5   | E     | 6    | 0    | (104, 80)  | header, fixed |
| 6   | `>`   | 0    | 1\|2 | (56, 88) or (56, 96) | shared chevron, y-snap on selection change |
| 7   | R     | 2    | 1    | (72,  88)  | RESUME |
| 8   | E     | 3    | 1    | (80,  88)  | RESUME |
| 9   | S     | 4    | 1    | (88,  88)  | RESUME |
| 10  | U     | 5    | 1    | (96,  88)  | RESUME |
| 11  | M     | 6    | 1    | (104, 88)  | RESUME |
| 12  | E     | 7    | 1    | (112, 88)  | RESUME |
| 13  | Q     | 2    | 2    | (72,  96)  | QUIT |
| 14  | U     | 3    | 2    | (80,  96)  | QUIT |
| 15  | I     | 4    | 2    | (88,  96)  | QUIT |
| 16  | T     | 5    | 2    | (96,  96)  | QUIT |

Worst-case scanline sprite count = 7 (chevron + RESUME's 6 letters), under the 10-per-line limit.
Enemies / projectiles are hidden during pause so no other sprites overlap these scanlines.

### Why 16 OAM (was 14 for upgrade/sell menu)

`OAM_MENU_COUNT` was 14 because the iter-2 widget was 2 rows × 7 cols. Pause is 3 rows × 8 cols
of which only 16 cells are actually drawn. Slots 15..16 were marked "reserved unused" in the
iter-2 OAM allocation (legacy from the F3 tower-OAM reservation). We **promote them** to be
part of the menu/pause range, since both modals are mutually exclusive and the iter-2 menu
still uses only 14 of them (cells 15..16 stay hidden during upgrade/sell). New OAM map:

```
0       cursor
1..16   menu/pause (mutually exclusive — both modules own this range)
17..30  enemies (14)
31..38  projectiles (8)
39      reserved
```

Per-scanline budget remains comfortably under 10.

### Sprite tile budget

New tiles: `SPR_GLYPH_A/R/M/Q/I/T` = 6 tiles via `glyph_to_sprite()` (pixel-identical to HUD font).
`SPRITE_TILE_COUNT` 29 → **35 / 256**. Visual treatment per `specs/iter3-22-pause.md` design
section (inlined below in §5).

---

## 5. Design (visual spec, integrated from designer pass)

### 5.1 Anchor / clamping

Play field origin `(0, 8)`; size 160×136. Widget 64×24.
- `Mx = (160 - 64) / 2 = 48` (tile col 6) — no clamp needed.
- `My = 8 + (136 - 24) / 2 = 64` (tile row 8) — no clamp needed.

Anchor is **fixed**: there is no contextual placement (unlike iter-2 menu which tracked the
selected tower).

### 5.2 Chevron movement

Single chevron sprite (OAM 6) with two y values: `88` (RESUME) or `96` (QUIT). Snap on UP/DOWN
input via `input_is_repeat(J_UP|J_DOWN)`. Wrap around (UP at top → QUIT; DOWN at bottom → RESUME).
Instant snap, no tween, no blink.

### 5.3 Glyph art (6 new sprite tiles)

All six are emitted by `glyph_to_sprite('A'|'R'|'M'|'Q'|'I'|'T')` — exact mirrors of the
existing `FONT[]` entries in `tools/gen_assets.py`, padded by one column left and one row top
per the existing `glyph_to_sprite()` helper. **No hand-redrawn art.** This guarantees the pause
menu's letters are pixel-identical to the same letters appearing in the HUD (e.g. the `A` tower
indicator in HUD col 19 ≡ the `A` in `PAUSE`).

Append in this order to `sprite_tiles[]` in `gen_assets.py`:

```python
('SPR_GLYPH_A', glyph_to_sprite('A')),
('SPR_GLYPH_R', glyph_to_sprite('R')),
('SPR_GLYPH_M', glyph_to_sprite('M')),
('SPR_GLYPH_Q', glyph_to_sprite('Q')),
('SPR_GLYPH_I', glyph_to_sprite('I')),
('SPR_GLYPH_T', glyph_to_sprite('T')),
```

### 5.4 Cursor handling while paused

Hide the placement cursor (move OAM 0 to `(0, 0)`). Rationale: the cursor's only purpose is
tower placement, which is gated off during pause. Leaving it visible would distract from the
menu and add a per-scanline sprite the layout doesn't budget for. This is consistent with
"hide irrelevant sprites" from the iter-2 menu pattern. `cursor_blink_pause(true)` is also
set so on resume the cursor re-renders cleanly via `cursor_update()` next frame.

### 5.5 HUD interaction

The HUD (BG row 0) remains visible and continues to display HP / energy / wave / selected tower
during pause. The pause overlay starts at screen y=64, leaving 56 px of vertical clearance to
the HUD bottom (y=8). No HUD changes; no BG writes from pause module.

### 5.6 Transition style

Instant snap. No fade, no slide-in. Consistent with iter-2 modal.

### 5.7 Audio policy

- **No new SFX** for pause-open / pause-close. Defer per the user prompt's "keep minimal" guidance.
  Avoids touching `audio.c` and the corresponding `tests/test_audio.c` ordering tests.
- `audio_tick()` continues running every frame while paused (already wired in §3 frame loop),
  draining any in-flight short SFX naturally — same policy as the iter-2 menu (decision F1).
- Tower-fire / enemy-hit / enemy-death SFX cannot trigger because the originating modules
  (`towers_update`, `enemies_update`, `projectiles_update`) do not run while paused. **Verified**:
  these are the only `audio_play()` call sites for those SFX (`grep -n "audio_play" src/`).
- A win/lose stinger mid-play is impossible because pause cannot be opened while a gameover
  transition is in flight (gameover check runs *before* the START → pause check; see §3).

### 5.8 Accessibility

Single-channel chevron is acceptable here: the player has unlimited time to read (game is
frozen), the selected line is a binary (chevron present/absent), and there is no time-pressure
discrimination task. The `memory/conventions.md` "dual channel" rule applies to in-play
warnings (invalid placement), not to fully-modal selection.

---

## 6. `src/pause.h` (full proposed surface)

```c
#ifndef GBTD_PAUSE_H
#define GBTD_PAUSE_H
#include "gtypes.h"

void pause_init(void);   /* hides all pause OAM, resets state */
bool pause_is_open(void);
void pause_open(void);   /* defensive no-op if menu_is_open() — both modals
                          * share OAM 1..16 and must be mutually exclusive. */
void pause_close(void);

void pause_update(void); /* called from game.c when paused */
void pause_render(void); /* sprite OAM only — VBlank-safe */

/* On A pressed with QUIT selected, pause_update returns true to signal
 * the caller to enter_title(). Avoids pause.c needing a back-edge to game.c. */
bool pause_quit_requested(void);
#endif
```

`game.c::playing_update` consumes the quit signal:

```c
if (pause_is_open()) {
    pause_update();
    economy_tick();
    audio_tick();
    if (pause_quit_requested()) {
        pause_close();
        audio_reset();    /* silence in-flight SFX before state transition */
        enter_title();
        return;
    }
    return;
}
```

`pause_quit_requested()` returns true once and self-clears, like a one-shot flag.

---

## 7. Edge cases (all resolved)

| Case | Behavior | Justification |
|------|----------|---------------|
| START + same-frame HP→0 (enemy reaches computer) | LOSE wins; pause not opened. | Frame loop checks gameover *before* the START handler (§3). |
| START + same-frame last enemy killed on final wave | WIN wins; pause not opened. | Same as above. |
| START held across resume (long press) | No re-pause — `input_is_pressed` is edge-only. | Existing input layer already correctly distinguishes pressed vs held. |
| START while upgrade/sell menu open | START ignored. | `playing_update`'s `if (menu_is_open())` arm hits first; menu_update does not handle START. |
| A press on QUIT while energy is 0 / etc | QUIT always succeeds — no precondition. | Quit is unconditional abandonment. |
| Pause opened with enemies mid-flight | Their sprites are hidden via `enemies_hide_all()` on open; positions preserved in shadow state; resume rewrites OAM next entity tick. | Mirrors menu.c's hide pattern. |
| Pause + projectile mid-flight | Same as enemies — `projectiles_hide_all()` hides OAM; on resume, `projectiles_update()` re-emits next frame. | Same. |
| QUIT during wave 5 → return to title → START new game | Full reset: `enter_playing()` re-inits every module + calls `audio_reset()`. | Existing iter-2 pattern; verified in `enter_playing()` code path. |
| Pause attempt on TITLE state | START still runs `enter_playing()` (existing behavior in `game_update`'s GS_TITLE arm). | `playing_update` is only reached in GS_PLAYING; pause hook is local to it. |
| Pause attempt on WIN/LOSE | START still runs `enter_title()` (existing behavior in GS_WIN/GS_LOSE arm). | Same. |

---

## 8. Subtasks (ordered, single-coder pass)

1. ✅ **Sprite glyphs** — Add 6 entries (`A,R,M,Q,I,T`) to `tools/gen_assets.py`'s `sprite_tiles[]`. Run `just assets`. Done when `res/assets.h` exposes `SPR_GLYPH_A..T` and `SPRITE_TILE_COUNT == 35`.
2. ✅ **OAM constant remap** — Bump `OAM_MENU_COUNT` 14 → 16 in `gtypes.h`. Add `OAM_PAUSE_BASE/COUNT` aliases (same range). Update inline OAM-allocation comment. Done when ROM still builds and the iter-2 upgrade/sell menu still uses cells 0..13 (visually unchanged on a smoke test). Depends on: 1.
3. ✅ **Pause module** — Create `src/pause.{h,c}` per §6. Implement `pause_init/open/close/update/render/is_open/quit_requested`. Done when `pause_open()` shows 16 glyphs at the documented OAM positions, UP/DOWN moves the chevron, B closes, START closes, A on RESUME closes, A on QUIT sets quit_requested. Depends on: 1, 2.
4. ✅ **Game integration** — Wire pause into `src/game.c` per §3 frame loop:
   - Add `pause_init()` calls in `enter_title()` and `enter_playing()` alongside the existing module inits.
   - Add the pause arm to `playing_update` (handles pause_quit_requested → `pause_close(); audio_reset(); enter_title();`).
   - Add `pause_render()` to `playing_render` (after `menu_render()`).
   - Add `game_is_modal_open()` helper (in `game.c`, declared in `game.h`).
   - **Add the missing `return;` after `enter_gameover(true);` in the WIN branch of `playing_update`.** Without this, a same-frame START + last-enemy-killed press paints pause glyphs over the WIN screen (D7 violation).
   - **Do NOT add `audio_reset()` to `enter_title()`** — it goes only in the QUIT path inside `playing_update`, to avoid racing with `audio_init()` on the `game_init → enter_title` boot path.
   - Verify `just test` (including `tests/test_audio.c` ordering assertions) still passes after the changes.
   
   Done when manual scenarios #1–#13 in §9 pass and `just test` is green. Depends on: 3.
5. ✅ **Host tests** — Create `tests/test_pause.c` per §10 AND extend the `justfile` `test` recipe to compile and run it (add lines mirroring the existing `test_audio` pair: `cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc tests/test_pause.c src/pause.c -o "{{BUILD}}/test_pause"` + `"{{BUILD}}/test_pause"`). Done when `just test` runs the new binary and all T1..T11 cases pass. Depends on: 3.
6. ✅ **Manual checklist** — Append the entries listed in §10 (manual-required) to `tests/manual.md`. Done when the entries exist and reference this spec. Depends on: 4.
7. ✅ **Memory updates** — Append the four new decisions (§D1..D4) to `memory/decisions.md`. Append the three new conventions (§11) to `memory/conventions.md`. Done when both files contain the new entries.

---

## 9. Acceptance Scenarios

### Setup
- Fresh clone; `brew install just mgba`; `just setup`; `just build`; `just run`.
- mGBA opens with the title screen.

### Scenarios

| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Pause opens (happy) | 1. Title → START to enter PLAYING. 2. Wait for W1. 3. Press START. | Pause overlay appears within 1 frame: PAUSE header + > RESUME + QUIT. Enemies/projectiles freeze (no movement, no flicker). HUD remains visible. Placement cursor disappears. |
| 2 | Resume via START shortcut | (from #1 paused) Press START. | Overlay disappears. Enemies resume. Cursor reappears at last position. No re-pause. |
| 3 | Resume via B (cancel mirror) | (from #1) Press B. | Same as #2. |
| 4 | Resume via A on RESUME | (from #1) Press A (RESUME is selected by default). | Same as #2. |
| 5 | Selection navigation | (from #1) Press DOWN. | Chevron snaps to QUIT row (y=96). Press UP → snaps back to RESUME. Repeat DOWN/UP wraps. |
| 6 | Auto-repeat on hold | (from #1) Hold DOWN for 2 s. | Chevron oscillates at the standard 12/6-frame cadence between RESUME and QUIT (only 2 items, so it toggles each repeat tick). |
| 7 | Quit to title (full reset) | (from #1) Press DOWN, then A. | Title screen appears (no blink artifact beyond the existing DISPLAY_OFF redraw); then press START → new game starts cleanly with HP=5, E=30, W:01/10, no leftover enemies/towers from the abandoned run. |
| 8 | LOSE wins over pause | Place no towers. Wait until W1 first bug is one tile from the computer. On the frame the bug enters the computer tile, press START. | LOSE screen appears. Pause overlay never visible. (Tester may need several attempts to catch the exact frame; an approximation is acceptable.) |
| 9 | WIN wins over pause | Reach W10 with HP > 0. As the last enemy dies, mash START. | WIN screen appears. Pause overlay never visible. |
| 10 | Modal exclusivity (menu open) | Place a tower. Move cursor onto it. Press A → upgrade/sell menu opens. Press START. | Upgrade/sell menu remains open and unchanged. Pause does NOT open. |
| 11 | START on TITLE | At title, press START. | Game starts (existing behavior). |
| 12 | START on WIN/LOSE | After a win or lose, press START. | Returns to title (existing behavior). |
| 13 | START held across resume | (from #1) Press A (resume). Continue holding START for 1 s. | Pause does NOT immediately re-open. Releasing and re-pressing START does open pause. |
| 14 | No SFX bleed | (from #1) Listen during pause. | No tower-fire / enemy-hit / enemy-death SFX. Any in-flight stinger from before pause drains naturally over its remaining frames (mostly imperceptible at SFX lengths < 60 frames). |
| 15 | OAM cell positions match spec | (from #1) In mGBA Tools → OAM viewer, verify slots 1..16 are at the screen pixels listed in §4 table. | All 16 slots match. Enemies (17..30) and projectiles (31..38) are at (0,0). |
| 16 | Visual inspection | (from #1) | "PAUSE" reads cleanly at center, chevron is unambiguously next to RESUME, QUIT label is below. No glyph overlaps the HUD. |
| 17 | Passive income during pause | (from #1) Note current E value. Wait 4 s paused (≥ 2 × `PASSIVE_INCOME_PERIOD = 180 frames` ≈ 3 s at 60 Hz). Resume. | E increased by approximately the number of elapsed passive-income periods (1 per 180 frames). Confirms `economy_tick` keeps running during pause per §3 (same policy as iter-2 menu, decision D19). HP unchanged. No tower-fire, projectile, or enemy events occur. |

---

## 10. Tests

### 10.1 Host-side (`tests/test_pause.c`)

Compile `src/pause.c` against `tests/stubs/gb/` (same harness pattern as `test_audio.c`).
**`src/input.c` is NOT compiled into the test binary**: instead, the test stubs
`input_is_pressed(mask)` and `input_is_repeat(mask)` directly so each test case can script
button presses deterministically. Other stubs needed: `move_sprite`, `set_sprite_tile`, plus
no-op shims for `enemies_hide_all`, `projectiles_hide_all`, `cursor_blink_pause`, and a
test-time-injectable `menu_is_open()` (default false; test T2 sets it true to verify the
defensive guard). The test exercises pause module behavior, not collaborator side-effects.

| # | Test | Asserts |
|---|------|---------|
| T1 | `pause_init` hides all 16 OAM | After `pause_init()`, `move_sprite` was called with `(OAM_PAUSE_BASE+i, 0, 0)` for each `i ∈ 0..15`. |
| T2 | Open / close idempotency + defensive no-op | `pause_open(); pause_close(); pause_close();` does not crash and `pause_is_open()` returns false. Then with a stubbed `menu_is_open() = true` (test-time injectable), `pause_open()` does NOT set `s_open` (defensive guard from review R1). |
| T3 | Default selection is RESUME | After `pause_open()`, `pause_quit_requested()` returns false; A press does NOT set quit. |
| T4 | DOWN selects QUIT, A confirms QUIT | Stub `joypad` returns J_DOWN (one frame), then J_A; `pause_update()` twice; `pause_quit_requested()` true. Reading it again returns false (one-shot). |
| T5 | UP from RESUME wraps to QUIT | Stub UP edge after open; selection becomes QUIT (verify via chevron y after `pause_render()`). |
| T6 | DOWN from QUIT wraps to RESUME | Stub DOWN twice; selection back to RESUME. |
| T7 | B closes without quit | Open; stub B; `pause_update()`; `pause_is_open()` false; `pause_quit_requested()` false. |
| T8 | START closes without quit | Same as T7 with START. |
| T9 | A on RESUME closes without quit | `pause_update()` with A while sel=0; pause closed, no quit. |
| T10 | Render after open writes 16 OAM tiles with correct glyph IDs | Track `set_sprite_tile` calls during `pause_render()`; expect each of the 16 OAM slots in `OAM_PAUSE_BASE..OAM_PAUSE_BASE+15` to receive exactly the tile ID in the §4 table (e.g. slot 1 = `SPR_GLYPH_P`, slot 6 = `SPR_GLYPH_GT`, slot 16 = `SPR_GLYPH_T`). Catches glyph-swap regressions. |
| T11 | Render before open does nothing | `pause_render()` without `pause_open` → zero `set_sprite_tile` calls. |

Note: `playing_update` integration (modal precedence) is **not** host-tested — it pulls in
`enemies/projectiles/towers/waves/economy/audio` which depend on GBDK headers. The relevant
behaviors are covered by manual scenarios #8, #9, #10, #11, #12 in §9.

### 10.2 Manual-required (`tests/manual.md` additions)

- **Modal precedence in PLAYING** — scenario #10.
- **State-machine non-interference** — scenarios #11, #12 (pause must not affect TITLE/WIN/LOSE).
- **Same-frame ordering** — scenarios #8, #9 (gameover wins).
- **Held-button edge detection** — scenario #13.
- **No SFX bleed** — scenario #14.
- **Full reset on QUIT** — scenario #7.

---

## 11. Constraints & budget

| Resource | Before | Δ | After | Cap |
|----------|--------|---|-------|-----|
| Sprite tiles | 29 | +6 | 35 | 256 |
| BG tiles     | 12 | 0  | 12 | 256 |
| OAM (worst case, paused) | 1 cursor + 14 menu = 15 | becomes 16 pause + 0 cursor = 16 | 16 | 40 |
| OAM (worst case, playing) | 1 + 14 + 8 = 23 | unchanged | 23 | 40 |
| Per-scanline sprites (paused) | n/a | 7 | 7 | 10 |
| Per-frame BG writes | ≤ 16 | 0 (pause is sprite-only) | ≤ 16 | ~16 |
| ROM | ~21 KB iter-2 | +~400 B (pause.c + 6 tile data + glyphs) | ~21.4 KB | 32 KB |
| WRAM | iter-2 baseline | +~6 B (s_open, s_sel, s_quit_requested + padding) | — | 8 KB |

All hardware limits respected. No MBC1 needed.

---

## 12. Decisions

| # | Decision | Options | Choice | Rationale |
|---|----------|---------|--------|-----------|
| D1 | Pause = flag inside `GS_PLAYING`, not a new state | (a) new `GS_PAUSED` (b) flag inside PLAYING | **(b) flag** | Mirrors `menu_is_open()`. Avoids state-machine bookkeeping for the "return to PLAYING" case. Render path is identical to PLAYING. |
| D2 | New `pause.{h,c}` module (not extend `menu.c`) | (a) extend menu.c with mode flag (b) new module | **(b) new module** | Anchor, action set, and exit transition differ materially. Mutual exclusion is enforced trivially in `game.c`. Keeps `menu.c` focused on its tower-relative widget. |
| D3 | Reuse OAM range 1..16 (mutually exclusive with upgrade/sell menu) | (a) reuse (b) carve a separate range | **(a) reuse, bump count 14→16** | OAM is scarce (40 total). Both modals are mutually exclusive by construction so they cannot collide. Slots 15..16 were already reserved-unused; promoting them keeps the OAM map dense. |
| D4 | No info line in pause overlay; rely on persistent HUD for HP/E/W | (a) duplicate W:NN HP:N E:NNN inside overlay (b) HUD only | **(b) HUD only** | The 15-glyph info line would not fit in 16 OAM with header + items + cursor (would total 31 cells). HUD is always visible during pause. Avoids OAM and complexity. Roadmap requirement "shows wave info" satisfied by HUD remaining visible. |
| D5 | Cursor hidden during pause | (a) hide (b) leave at last position | **(a) hide** | The placement cursor has no function while paused; leaving it on adds a sprite to the scanlines without informational value. |
| D6 | No "pause open" SFX | (a) add small chime (b) silent | **(b) silent** | Defers `audio.c` change + new `tests/test_audio.c` ordering coverage. User prompt explicitly leaves it optional and recommends minimal. |
| D7 | Same-frame gameover supersedes pause | (a) pause first (b) gameover first | **(b) gameover first** | Gameover is terminal; pausing into it would require special "you-still-lost" UX. Place gameover checks before pause-open in frame loop. |
| D8 | START is an A-button-style edge press, not held | reuse `input_is_pressed(J_START)` | edge | Existing input layer already does this for J_A/J_B. Holding START across resume would auto-pause repeatedly — bad UX. |
| D9 | QUIT path goes through `enter_title()` (no new function); `audio_reset()` is called from the QUIT path in `playing_update`, NOT from `enter_title` | (a) put `audio_reset` in `enter_title` (b) put it in the QUIT path | **(b) QUIT path** | `enter_title` is also called at boot (`game_init`). Calling `audio_reset()` there would race with `audio_init()` (which writes NR52 first per the iter-2 audio ordering test) and could regress `tests/test_audio.c`. Localizing the reset to QUIT (where in-flight SFX actually exists) is correct and surgical. |
| D10 | `pause.c` signals quit via one-shot flag, not by calling `enter_title` directly | (a) flag + caller transitions (b) pause includes `game.h` and calls enter_title | **(a) flag** | Avoids circular include between `pause.h` and `game.c`'s static `enter_title`. Symmetrical with how iter-2 modules avoid back-edges. |

---

## 13. New memory entries to append

### `memory/decisions.md`

#### Iter-3 #22: Pause = flag inside PLAYING + new `pause.{h,c}` module
- **Date**: 2026-XX-XX (set by coder on commit)
- **Context**: Iteration-3 roadmap item #22 — START button pauses game with RESUME / QUIT options.
- **Decision**: New module `src/pause.{h,c}` mirroring the iter-2 modal sprite-overlay pattern. Pause is a `bool s_open` flag inside `pause.c`; the top-level state machine stays at `{TITLE, PLAYING, WIN, LOSE}`. `playing_update` early-returns into `pause_update()` when paused, identical to the menu-open arm. New helper `game_is_modal_open()` is the single source of truth for "is gameplay frozen".
- **Rationale**: Render path is identical to PLAYING (same BG, only an overlay differs); a dedicated state would add transition bookkeeping with no benefit. Separate module from `menu.c` because anchor, layout, action set, and exit transition all differ materially.
- **Alternatives**: New `GS_PAUSED` state (rejected — bookkeeping); extend `menu.c` with a mode flag (rejected — pollutes upgrade/sell logic).

#### Iter-3 #22: OAM range 1..16 shared by pause + upgrade/sell menu (mutually exclusive)
- **Date**: 2026-XX-XX
- **Context**: Pause overlay needs 16 sprite cells; iter-2 menu uses 14. OAM is capped at 40 with hard pools.
- **Decision**: Bump `OAM_MENU_COUNT` 14 → 16. Promote previously reserved slots 15..16 into the menu range. Both modules own the same physical OAM range 1..16 — collision is impossible because `game.c` enforces single-modal-at-a-time. **`menu.c::hide_menu_oam` now hides slots 1..16 (16 sprites) on `menu_close()` even though `menu_render` only paints 1..14 — slots 15..16 stay zeroed throughout the upgrade/sell session.** Do not repurpose slots 15..16 for anything other than menu/pause overlay.
- **Rationale**: Densest OAM packing. Slots 15..16 were already reserved-unused.
- **Alternatives**: Carve a separate range for pause (no free slots without shrinking enemies/projectiles pool — both at hard min).

#### Iter-3 #22: Same-frame gameover supersedes pause-open
- **Date**: 2026-XX-XX
- **Context**: If START is pressed on the same frame an enemy reaches the computer (HP→0) or the last wave is cleared, two transitions race.
- **Decision**: `playing_update` checks `economy_get_hp() == 0` and `waves_all_cleared()` *before* the START → `pause_open()` handler. Gameover transitions execute first; the pause request is silently dropped that frame.
- **Rationale**: Gameover is terminal; pausing into a lost game would require special UX and feels broken to the player.
- **Alternatives**: Pause first, then enter gameover on resume (rejected — bizarre UX).

#### Iter-3 #22: No pause-open/close SFX in iter-3
- **Date**: 2026-XX-XX
- **Context**: User prompt left a "small chime" optional; minimal-audio bias.
- **Decision**: Pause is silent on open and close. `audio_tick()` continues running so in-flight SFX drain naturally (per iter-2 F1).
- **Rationale**: Avoids touching `audio.c` and adding a new ordering test in `test_audio.c`. Pause is short-duration; the silent transition matches iter-2 menu UX.

### `memory/conventions.md`

#### Modal helper (iter-3)
`game_is_modal_open()` returns true iff a modal is open (`menu_is_open() || pause_is_open()`). Use this anywhere code needs to ask "is gameplay frozen". Do NOT add new modals without updating the helper.

#### OAM allocation (iter-3)
`0 = cursor, 1..16 = menu/pause (mutually exclusive), 17..30 = enemies, 31..38 = projectiles, 39 = reserved`. The pause module hides cursor (OAM 0) on open via `move_sprite(OAM_CURSOR, 0, 0)` (in addition to `cursor_blink_pause(true)`).

#### Module init in state transitions (iter-3 update)
`enter_title()` and `enter_playing()` now both call `pause_init()` alongside the iter-2 set (`cursor/towers/enemies/projectiles/menu`). `audio_reset()` is **NOT** added to `enter_title()` (would race with `audio_init()` on boot); it is called from the QUIT path inside `playing_update` instead.

---

## 14. Review

### Round 1 — self-review (planner)
Findings & resolutions:

- **Q: Could pause and menu both be open if a coder forgets the precedence in `playing_update`?**
  A: Mitigated by `game_is_modal_open()` helper convention + an early assert in `pause_open()`:
  ```c
  void pause_open(void) {
      /* Caller (game.c) is responsible for ensuring no other modal is open. */
      if (menu_is_open()) return;   /* defensive no-op */
      ...
  }
  ```
  Documented in §6 and D2.

- **Q: After QUIT → TITLE → new game, do leftover audio channels (e.g. an interrupted enemy-death SFX) leak?**
  A: The QUIT path in `playing_update` calls `audio_reset()` immediately before `enter_title()` (D9). `audio_reset()` is idempotent per iter-2 F1 — safe to call from any in-game state. **NOT** added to `enter_title()` itself, to preserve the boot-time audio ordering invariant locked in by `tests/test_audio.c`.

- **Q: Pause opened during the iter-2 `menu_open()` early-return frame — race?**
  A: Impossible: that frame's `playing_update` returns early before reaching the START handler. `menu_is_open()` is true at the start of the next frame, so the `if (menu_is_open())` arm runs first and START is consumed by `menu_update` (which ignores it).

- **Q: Holding START during the title→playing transition could auto-open pause on first PLAYING frame?**
  A: `enter_playing()` does not reset `input_poll`'s edge-state. If the player holds START across the transition, on the first PLAYING frame `s_held & ~s_prev` is 0 (START was held last frame too), so `input_is_pressed(J_START)` is false. ✓ No auto-pause.

- **Q: Auto-repeat on START?**
  A: `input.c` only auto-repeats D-pad bits; J_START has no auto-repeat. ✓

- **Q: Cursor reappears at "last position" after resume — what if the cursor was on a tower that was sold mid... wait, no towers can be sold during pause.**
  A: N/A — pause freezes everything. Cursor position is stable. ✓

### Round 2 — adversarial reviewer (Sonnet)

Delegated. Findings addressed:

- **R1 (MEDIUM)**: Current `src/game.c::playing_update` is missing a `return;` after `enter_gameover(true);` in the WIN branch. Adding the START → `pause_open()` handler at end-of-frame would, on the same frame the last enemy dies, run *after* the gameover transition and paint pause glyphs over the WIN screen — directly violating D7. **Resolution**: Subtask 4 now explicitly requires adding the missing `return;`, and the §3 frame-loop snippet has both gameover branches with explicit `return` plus a NOTE comment. Acceptance scenario #9 covers this.
- **R2 (MEDIUM)**: Putting `audio_reset()` in `enter_title()` is fragile because `enter_title()` is also called from `game_init()` at boot; `audio_reset()` writes NRx2=0 which races with `audio_init()`'s "NR52 first" ordering invariant locked-in by `tests/test_audio.c`. **Resolution**: `audio_reset()` moved out of `enter_title()` and into the QUIT path inside `playing_update` (in front of `enter_title()` in the snippet). D9 rewritten. Convention entry corrected. Subtask 4 calls out "do NOT add audio_reset to enter_title" + adds an acceptance bullet "verify just test stays green".
- **R3 (NOTE)**: `OAM_MENU_COUNT` 14→16 means `menu.c::hide_menu_oam` will now also zero slots 15..16 on `menu_close()`. Currently safe; recorded explicitly in the convention entry so a future coder doesn't repurpose 15..16.
- **R4 (NOTE)**: T10 strengthened to assert specific tile IDs per OAM slot (catches glyph-swap regressions).
- **R5 (NOTE)**: §10 now states that `src/input.c` is NOT compiled into the test binary; `input_is_pressed/is_repeat` are stubbed directly.
- **R6 (NOTE)**: T2 now also exercises the defensive `pause_open()` no-op when `menu_is_open()`.

### Round 3 — cross-model validation (gpt-5.4 reviewer)

Delegated. Findings:

- **V1**: When `pause_quit_requested()` triggers, `enter_title()` runs *after* `economy_tick()/audio_tick()` for that frame. Acceptable — those are no-ops during the transition (`enter_title` re-inits everything; `audio_reset()` runs immediately before).
- **V2**: T1 host test depends on `OAM_PAUSE_BASE` from `gtypes.h` (subtask 2). Subtask order is correct: 1 → 2 → 3 → (4, 5 in either order) → 6 → 7. Subtask 5 implicitly depends on 3 (which depends on 2).
- **V3**: Designer anchor math verified: cell P at `wcol=2` → screen x = `Mx + 2*8 = 48 + 16 = 64`, OAM x = `64 + 8 = 72`. ✓ Matches §4 table.
- **V4**: ROM growth estimate is rough (~400 B); acceptable within 11 KB headroom.

All review findings resolved. No open items.
