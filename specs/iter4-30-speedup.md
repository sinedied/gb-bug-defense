# Speed-Up Toggle

## Problem
Late-game waves (7–10) have long spawn sequences with 50–90 frame intervals. Experienced players waiting for enemies to trickle out perceive dead time. A 2× speed toggle gives players control over pacing without altering game balance (same number of enemies, same HP, same economy).

## Architecture

### Data
- `static bool s_fast_mode;` — file-scope in `game.c`, alongside `s_state`, `s_anim_frame`, etc.
- Reset to `false` in `enter_playing()`, placed immediately before `hud_init()` (after `pause_init()`).

### Entity-Tick Extraction
Extract the entity-tick logic from the normal gameplay else-branch into a new `static void entity_tick(void)` in game.c:

```c
static void entity_tick(void) {
    towers_update();
    enemies_update();
    projectiles_update();
    if (s_gate_active) {
        s_gate_blink++;
        if (s_gate_blink >= 60) s_gate_blink = 0;
    } else {
        waves_update();
    }
    /* Consume wave-clear edge after each tick so a second entity_tick
     * call cannot overwrite the one-shot flag (future-proofing against
     * shorter inter-wave delays). */
    {
        u8 cleared = waves_just_cleared_wave();
        if (cleared) score_add_wave_clear(cleared);
    }
}
```

### Modified Normal Gameplay Branch
Inside the `else` block of `playing_update()` (no modal open):

```c
cursor_update();
/* Speed toggle: SELECT edge. Implicit modal gate — this code is
 * unreachable when pause or menu is open. */
if (input_is_pressed(J_SELECT)) {
    s_fast_mode = !s_fast_mode;
    hud_set_fast_mode(s_fast_mode);
}
if (!s_gate_active && input_is_pressed(J_B)) {
    cycle_tower_type();
}
if (input_is_pressed(J_A)) {
    /* ... existing A-press logic (menu open / tower place) unchanged ... */
    /* early-return paths unchanged */
}
entity_tick();
if (s_fast_mode) entity_tick();
```

### Items That Remain Single-Call Per Frame
- `economy_tick()` — outside the else-branch, runs once per frame
- `audio_tick()` — outside the else-branch, runs once per frame
- `s_anim_frame++` — in `game_update()` before state switch (already correct)
- Input dispatch / `cursor_update()` — inside else-branch but before entity ticks
- Gameover checks — after economy/audio ticks
- START → pause check — end of frame

### HUD Changes

**New API in `hud.h`:**
```c
void hud_set_fast_mode(bool on);
```

**Implementation in `hud.c`:**
```c
static bool s_hud_fast;   /* false after hud_init() */

void hud_set_fast_mode(bool on) {
    if (s_hud_fast != on) {
        s_hud_fast = on;
        s_dirty_t = 1;
    }
}
```

**Modified `s_dirty_t` handler in `hud_update()`:**
```c
if (s_dirty_t) {
    if (s_hud_fast) {
        put_char(18, 0, '>');
        put_char(19, 0, '>');
    } else {
        put_char(18, 0, ' ');
        u8 type = game_get_selected_tower_type();
        const tower_stats_t *st = towers_stats(type);
        put_char(19, 0, (char)st->hud_letter);
    }
    s_dirty_t = 0;
}
```

**`hud_init()` changes:**
- Add `s_hud_fast = false;` alongside existing dirty-flag resets.
- Remove the existing standalone `put_char(18, 0, ' ');` line from the static-labels block — col 18 is now fully managed by the `s_dirty_t` handler (which fires on the same `hud_init()` call since `s_dirty_t = 1` is set and `hud_update()` is called at the end of `hud_init()`).

### BG-Write Budget Analysis

**Gate-active phase** (before first tower placed):
- Gate blink transition: 12 tiles (once per ~30 frames)
- Fast-mode SELECT toggle: 2 tiles (cols 18–19, player-initiated)
- No other sources possible: B-press cycling gated by `!s_gate_active`; no enemies → no corruption/HP/energy; economy_tick gated by `!s_gate_active`
- Worst case (blink transition + toggle same frame): **14 tiles** ✓

**Gate-lift frame** (first tower placed):
- Energy spend: 3 tiles (hud_mark_e_dirty)
- Gate restore: 12 tiles (gate_render → gate_restore_tiles)
- Tower tile: 1 tile (towers_render)
- Pre-feature total: **16 tiles** (already at cap)
- Edge case: SELECT + A on exact same frame adds fast-indicator: +2 → **18 tiles**
- This is a single-frame, once-per-session event requiring simultaneous SELECT+A. On hardware, the 2 excess writes would land early in the next scanline causing a brief 1-frame tile flicker. Accepted as cosmetic-only; no mitigation needed.

**Post-gate normal gameplay** (steady state):
- HUD worst case (all dirty simultaneously): HP(1) + E(3) + W(4) + T(2) = **10** tiles
- Corruption (enemy reaches computer): **4** tiles
- Tower idle-LED blink: **1** tile
- Worst case (all simultaneous): **15 tiles** ✓

Note: The `s_dirty_t` handler now always writes 2 tiles (cols 18 and 19) vs 1 pre-feature (col 19 only). This +1 applies to every frame where `s_dirty_t` fires — both SELECT-toggle frames and B-press tower-cycle frames.

### Modal Gating
- SELECT toggle only fires inside the else-branch of `playing_update()` — unreachable when pause or menu is open. No additional `game_is_modal_open()` check needed.
- Edge case: SELECT + A on same frame → toggle fires first (processed earlier in code), then tower place/menu open proceeds normally. Correct: the double-tick is naturally bypassed if A causes an early-return (menu-open path, which calls economy_tick + audio_tick + return before reaching entity_tick).

### Reset Behavior
- `enter_playing()`: `s_fast_mode = false` placed before `hud_init()`.
- `hud_init()`: sets `s_hud_fast = false`, sets `s_dirty_t = 1`, calls `hud_update()` → writes ' ' at col 18 and tower letter at col 19.
- Net: new game always starts with fast mode off, HUD shows tower letter.

## Subtasks

1. ✅ **Add `s_fast_mode` declaration and reset** — Declare `static bool s_fast_mode;` in game.c after `s_gate_dirty`. In `enter_playing()`, add `s_fast_mode = false;` on the line immediately before `hud_init();` (after `pause_init();`). Done when variable compiles and is `false` at game start.

2. ✅ **Extract `entity_tick()` helper** — Create `static void entity_tick(void)` in game.c containing: `towers_update()`, `enemies_update()`, `projectiles_update()`, the `s_gate_active` blink-or-`waves_update()` conditional, and the `waves_just_cleared_wave()` → `score_add_wave_clear()` drain. Replace the original inline code in the else-branch with a single `entity_tick()` call. Done when `just build` succeeds and manual smoke test confirms: place tower, enemies spawn, enemies take damage and die, wave counter advances in HUD.

3. ✅ **Add double-tick under `s_fast_mode`** — After the single `entity_tick()` call, add `if (s_fast_mode) entity_tick();`. For verification, temporarily set `s_fast_mode = true` in `enter_playing()` (after the `= false` line). Done when enemies visibly move at 2× speed with the hardcode. Revert hardcode after verification.

4. ✅ **Implement HUD `hud_set_fast_mode()` API** — In `hud.h`: add `void hud_set_fast_mode(bool on);`. In `hud.c`: add `static bool s_hud_fast;`, set to `false` in `hud_init()`. Implement `hud_set_fast_mode()` (no-op if value unchanged; sets flag; sets `s_dirty_t = 1`). Modify `s_dirty_t` handler to branch on `s_hud_fast`: write `'>','>'` at cols 18–19 when on; write `' '` + `st->hud_letter` when off. Remove the standalone `put_char(18, 0, ' ')` from `hud_init()`'s static-label block. Done when calling `hud_set_fast_mode(true)` shows ">>" at top-right and `(false)` restores " " + tower letter. Depends on: subtask 1 (hud_init reset).

5. ✅ **Add SELECT toggle input handling** — In the normal gameplay branch of `playing_update()`, after `cursor_update()` and before the `!s_gate_active && J_B` check, add: `if (input_is_pressed(J_SELECT)) { s_fast_mode = !s_fast_mode; hud_set_fast_mode(s_fast_mode); }`. Remove the temporary hardcode from subtask 3. Done when pressing SELECT in-game toggles between normal and 2× speed with HUD indicator updating. Depends on: subtasks 3, 4.

6. ✅ **Document BG-write budget** — Update the HUD layout comment at the top of `hud.c` to reflect that the `s_dirty_t` handler now writes 2 tiles (cols 18–19). Add a brief comment in `game.c` above `entity_tick()` noting the gate-lift + SELECT edge case (18 writes, accepted). Done when comments are present and worst-case counts match spec analysis.

7. ⬜ **Manual QA pass** — Execute acceptance scenarios 1–9 in emulator. Done when all 9 pass. Scenario 10 (hardware stress) is a separate pre-ship gate.

## Acceptance Scenarios

### Setup
`just build && just run`. Start a new game on any map/difficulty. Place at least one tower to lift the gate (unless testing gate interaction).

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Toggle on | Place tower; press SELECT | HUD shows ">>" at cols 18–19 row 0; enemies and projectiles visibly move ~2× faster |
| 2 | Toggle off | Press SELECT again | HUD shows " A" (space + tower letter); speed returns to normal |
| 3 | Disabled during pause | Open pause (START); press SELECT; unpause (START) | Fast mode state unchanged from before pause |
| 4 | Disabled during menu | Tap placed tower (A on tower tile); press SELECT; close menu (B) | Fast mode state unchanged from before menu opened |
| 5 | Reset on new game | Toggle fast mode on; let game end (win or lose); START → title; START → new game | Fast mode off; HUD shows tower letter at col 19, blank at col 18 |
| 6 | Tower-type cycle | Ensure fast mode off; press B to cycle tower type | Col 18 remains blank; col 19 shows new tower letter (e.g. A→F→E→A) |
| 7 | Gate interaction | Start new game; do NOT place tower; press SELECT | Fast mode on; gate blink visibly accelerates; HUD shows ">>" |
| 8 | Economy unchanged | Toggle fast mode on; watch energy counter during inter-wave delay | Energy ticks at same rate as normal mode |
| 9 | Audio unchanged | Toggle fast mode on; listen to music and SFX | Music tempo unchanged; tower-fire SFX trigger normally |
| 10 | Wave-10 stress (pre-ship hardware gate) | Play to wave 10 on Veteran with fast mode on and ≥12 towers | No severe frame drops causing audio stutter or enemy teleportation. **Must be verified on real DMG hardware before shipping.** |

## Constraints
- `s_fast_mode` MUST be file-scope `static` in game.c — not global, not exposed via getter.
- Entity ticks that double: `towers_update`, `enemies_update`, `projectiles_update`, `waves_update` (or gate blink when active).
- Single-call per frame: `economy_tick()`, `audio_tick()`, `s_anim_frame++`, input polling, `cursor_update`, gameover checks, START→pause.
- BG-write budget ≤16 tiles/frame maintained in steady state (gate-active: 14 max; post-gate: 15 max). Gate-lift + SELECT edge case (18 writes) accepted as cosmetic-only.
- **Hardware profiling required before shipping**: wave-10 with 2× entity tick, 16 towers × 14 enemies range checks + 8 projectile steps. Fallback options if frame overrun: (a) cap fast mode at waves 1–8, (b) skip second tick when cycle budget exceeded.

## Decisions
| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| State location | (A) file-scope game.c, (B) local static in playing_update, (C) global with getter | A | Same pattern as `s_state`, `s_anim_frame`; accessible from `enter_playing()` for reset; not exposed beyond file |
| HUD communication | (A) `hud_set_fast_mode(bool)` setter, (B) `game_is_fast_mode()` getter read by HUD, (C) piggyback `hud_mark_t_dirty()` only | A | Avoids exposing game.c internal state; HUD remains self-contained; dirty flag set internally on state change only |
| Score drain placement | (A) after both ticks combined, (B) inside entity_tick after each | B | Future-proof against shorter inter-wave delays; trivial cost (one extra cleared-on-read returning 0 on second tick) |
| SELECT code position | (A) before B/A input, (B) after all input | A | Orthogonal to tower actions; processed first avoids interaction with same-frame A-press early return |
| Col 18 ownership | (A) static label in hud_init + dirty_t writes col 19 only, (B) dirty_t handler owns both cols 18–19 | B | Single code path; eliminates redundant write; col 18 content depends on fast-mode state |
| Test approach | (A) new pure-helper + host test, (B) manual QA only | B | Toggle logic is trivial bool flip inside existing modal-gated branch; no complex predicate warrants a dedicated host test |
| Gate-lift + SELECT budget overflow | (A) defer HUD write, (B) block SELECT on gate-lift frame, (C) accept cosmetic risk | C | 18 writes is +2 over cap; single-frame once-per-session; on DMG hardware manifests as brief tile flicker; complexity of mitigation outweighs cosmetic cost |

## Review
**Adversarial review findings (primary):**
1. *BG-write budget omitted gate_render's 12-tile writes* — Resolved: spec now documents gate-active phase (worst 14), gate-lift frame (16 pre-feature, 18 with simultaneous SELECT+A accepted as cosmetic edge case), and post-gate steady state (15). The pre-existing gate-lift budget of 16 was already at cap before this feature.
2. *`s_fast_mode = false` placement contradicts between Architecture and Subtask 1* — Resolved: both now consistently say "before `hud_init()`, after `pause_init()`".
3. *s_dirty_t handler writes 2 tiles on every B-press frame (not just toggle frames)* — Resolved: budget analysis updated to reflect +1 tile on all `s_dirty_t` events; HUD worst case is now 10 (was 9).

**Cross-model validation findings:**
1. *Gate-lift + SELECT exceeds 16-tile budget* — Resolved: documented as accepted cosmetic edge case (18 writes, once-per-session, single frame). Pre-feature gate-lift was already at 16.
2. *Scenario 10 not runnable in dev environment* — Resolved: split into pre-ship hardware gate; subtask 7 scoped to scenarios 1–9.
3. *Subtask 2 "score increments" not observable in-game* — Resolved: done criterion changed to observable HUD behavior (enemies die, wave counter advances).
