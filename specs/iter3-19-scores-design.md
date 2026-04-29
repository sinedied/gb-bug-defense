# Iter-3 #19 — Score & High Score (SRAM): UI/UX Design

Scope: visual placement and rendering rules for the score readout on the
title screen and the gameover screen. SRAM layout, save/load timing, and
score-event accounting are owned by the feature spec, not this design doc.

All coordinates are screen tile coords `(col, row)` on the 20×18 BG grid.
Text uses the existing 8×8 ASCII font (shade 3 on shade 0). No new glyphs.

---

## 1. Title screen — `HI:` line

### Decisions

| Question | Choice | Rationale |
|---|---|---|
| Label | **`HI:`** | 3 tiles. Universal arcade convention; instantly readable. `BEST:` (5) and `HISCORE:` (8) crowd the row and add no information. |
| Padding | **Leading zeros** (`HI: 00000`) | Constant 5-digit width → no flicker when score order-of-magnitude changes mid-session, no need to right-align logic. Arcade-standard. |
| First-run value | **`HI: 00000`** | Score is u16; stored zero is a legitimate value, not a sentinel. Beating zero with any score immediately triggers the `NEW HIGH SCORE!` banner — good UX. Also avoids a render-time special case (no "is-this-a-fresh-SRAM" branch). |
| Animation | **Static** | Only PRESS START blinks. Adding a second blinking element would create a competing focal point. |
| Per-(map, difficulty) | **Yes** | The displayed `HI:` value re-reads from SRAM whenever either selector cycles. Re-uses the existing `s_hi_dirty` flag set on map or difficulty change. |

### Layout

`HI: NNNNN` is **9 tiles** wide. Centered: `(20 − 9) / 2 = 5.5` → start at
**col 5**, occupies **cols 5..13** of **row 15**.

That row is empty in the existing title tilemap (logo 0..7, diff selector
10, map selector 12, PRESS START 13, attribution 16; rows 11, 14, 15, 17
are blank). Row 15 sits two rows below PRESS START — visually grouped
with the prompt as "the bottom-of-screen status block" — and well clear
of the attribution at row 16 (1-tile breathing space above attribution).

```
 col:  0         1
       0123456789012345678901
row 0  ....................
row 1  ....................
row 2  ....G.B.T.D.........
row 3  ...██████████████...
row 4  ...█.GBTD......█...
row 5  ...█..TOWER....█...
row 6  ...█.DEFENSE...█...
row 7  ...██████████████...
row 8  ....................
row 9  ....................
row10  ....<.NORMAL.>......     <- diff selector  (cols 5..14)
row11  ....................
row12  ....<.MAP.1..>......     <- map selector   (cols 5..14)
row13  ....PRESS.START.....     <- prompt         (cols 4..15, blinks)
row14  ....................
row15  .....HI:.00000......     <- NEW           (cols 5..13)
row16  ..(C).2025.GBTD.....     <- attribution
row17  ....................
```

(`.` = space tile. The focus chevron `>` lands at col 3 of row 10 or row
12 — unchanged.)

### Render integration — priority chain (extends iter-3 #17 and #20)

`title.c` gains a 5th dirty flag, **`s_hi_dirty`**. Service order in
`title_render` (one-per-frame, return after each):

```
s_diff_dirty  >  s_map_dirty  >  s_focus_dirty  >  s_hi_dirty  >  s_dirty
```

`s_hi_dirty` is below the selectors so a single LEFT/RIGHT press paints
the selector this frame and the new `HI:` value next frame — visually
indistinguishable, and keeps the per-frame BG-write cap at the heaviest
single branch (12 for the prompt). The `HI:` branch is **9 writes**.

`s_hi_dirty = 1` is set:
1. In `title_enter()` (initial paint).
2. After `game_set_difficulty(...)` in `title_update()`.
3. After `game_set_active_map(...)` in `title_update()`.

`draw_hi_now()` reads `score_get_hi(game_active_map(), game_difficulty())`
and emits the 9-tile string with `set_bkg_tile_xy` calls.

---

## 2. Gameover screen — `SCORE:` line + `NEW HIGH SCORE!` banner

### Decisions

| Question | Choice | Rationale |
|---|---|---|
| Score label | **`SCORE: NNNNN`** (12 tiles) | Spell it out — the gameover screen has the room and only one numeric value to display, so a 3-letter label like `SC:` would read as a typo. Same leading-zero padding as title. |
| Banner text | **`NEW HIGH SCORE!`** (15 tiles) | Standard arcade phrasing. Fits in a single row. Uses only existing font glyphs (letters, space, `!`). |
| Banner animation | **Static** | Tile-budget reasons (no new tiles for an animated banner) and to avoid competing with the PRESS START blink. The placement above the score line already provides the necessary visual prominence. |
| Banner placement | **Row 13** (above score) | Reading order: player's eye lands on banner first, then on the score that earned it. |
| Score placement | **Row 14** (just above prompt at row 15) | Groups score with the rest of the bottom status block. Doesn't disturb the WIN/LOSE central art (rows 2..12 in mvp-design §2.3/§2.4). |

Existing free rows on both win_tilemap and lose_tilemap (per
`specs/mvp-design.md` §2.3/§2.4): 13, 14, 16, 17. Rows 0, 1, 8, 9 are
also blank but visually disconnected from the prompt.

### Layout — all four variants

#### WIN, no new high score
```
 col:  0         1
       0123456789012345678901
row 0  ....................
row 1  ....................
row 2  ....SYSTEM.CLEAN....
row 3  ....................
row 4  ....................
row 5  .ALL.PROCESSES......
row 6  .TERMINATED..AND....
row 7  .SO.IS.YOUR.BATTERY.
row 8  ....................
row 9  ....................
row10  .....┌────────┐.....
row11  .....│..:)....│.....
row12  .....└────────┘.....
row13  ....................
row14  ....SCORE:.12345....     <- NEW (cols 4..15)
row15  ....PRESS.START.....
row16  ....................
row17  ....................
```

#### WIN, new high score
```
row 13  ..NEW.HIGH.SCORE!...     <- NEW banner (cols 2..16)
row 14  ....SCORE:.12345....     <- NEW score line
row 15  ....PRESS.START.....
```
Centered banner: `NEW HIGH SCORE!` = 15 chars, `(20−15)/2 = 2.5` → start
at col 2, occupies **cols 2..16** of row 13. (Equivalently: col 3
cols 3..17 — designer picks col 2 so the `!` lands one tile inside the
right edge for symmetry with the leading space.)

#### LOSE, no new high score
```
row 0  ....................
row 1  ....................
row 2  ....KERNEL.PANIC....
row 3  ....................
row 4  ....................
row 5  .THE.AI.WON.........
row 6  .PLEASE.REBOOT......
row 7  .YOUR.CIVILIZATION..
row 8  ....................
row 9  ....................
row10  ....┌──────────┐....
row11  ....│..X__X....│....
row12  ....└──────────┘....
row13  ....................
row14  ....SCORE:.12345....     <- NEW
row15  ....PRESS.START.....
row16  ....................
row17  ....................
```

#### LOSE, new high score
```
row 13  ..NEW.HIGH.SCORE!...     <- NEW banner (cols 2..16)
row 14  ....SCORE:.12345....     <- NEW score line
row 15  ....PRESS.START.....
```

A loss CAN beat the prior best (e.g. survived 8 of 10 waves on HARD).
The banner branch must therefore fire on either WIN or LOSE based purely
on `final_score > prior_hi`, not on win/lose state.

### Render integration

`gameover.c::gameover_enter(bool win, u16 final_score, bool new_hi)` is
extended:
- After the existing `set_bkg_tiles(0, 0, 20, 18, ...)` blanket write,
  inside the `DISPLAY_OFF` bracket, paint:
  - SCORE line at (4, 14): 12 `set_bkg_tile_xy` calls.
  - If `new_hi`: banner at (2, 13): 15 `set_bkg_tile_xy` calls.
- Both lines are **static** — no per-frame redraw. Only the existing
  `draw_prompt_now` blink path runs in `gameover_render`.

`new_hi` is computed by the caller (game.c gameover-transition site):
read prior hi for `(map, difficulty)`, compare, then `score_set_hi(...)`
and pass `new_hi = (final_score > prior_hi)` into `gameover_enter`.

---

## 3. Visual states summary

| Element | Animation | Redraw trigger |
|---|---|---|
| Title `HI:` line | Static | `s_hi_dirty` set on title enter / map change / difficulty change |
| Gameover `SCORE:` line | Static | Painted once in `gameover_enter` |
| Gameover `NEW HIGH SCORE!` banner | Static | Painted once in `gameover_enter` iff `new_hi` |
| Title PRESS START prompt | Blinks 1 Hz (unchanged) | `s_dirty` every 30 frames |
| Gameover PRESS START prompt | Blinks 1 Hz (unchanged) | `s_dirty` every 30 frames |

Rationale for static banner:
1. **Tile budget.** A blink-animated banner would either reuse the
   font (acceptable but creates two competing 1 Hz blinks) or need
   custom tiles (cost not justified). Static is cheaper and quieter.
2. **Hierarchy.** Only one element should blink at a time per screen;
   the prompt owns that role on both screens (per
   `memory/conventions.md` "no element blinks faster than 2 Hz" and
   the implicit "one blinker per screen" pattern).
3. **Placement already conveys importance.** Banner above the score
   line, larger horizontal footprint than the score, only present on
   new-HS runs.

---

## 4. Tile / glyph budget

**Zero new tiles.** All required glyphs already exist in the GBDK
default font:
- Digits `0`–`9` ✓
- Uppercase letters `A`–`Z` ✓ (uses `H`, `I`, `S`, `C`, `O`, `R`, `E`, `N`, `W`, `G`)
- `:` ✓ (already used by HUD `HP:` / `E:` / `W:`)
- `!` ✓ (in font, unused so far — first appearance)
- ` ` (space, tile 32) ✓

No `gen_assets.py` change required. No `assets.h` symbol additions.

Per-frame BG-write impact:
- Title: new heaviest single branch is the prompt (12), unchanged. The
  `HI:` branch is 9 writes — well below the 12-cap. Title stays at
  ≤ 12 writes/frame, inside the 16-cap.
- Gameover: `gameover_enter` writes happen inside `DISPLAY_OFF`, so the
  +27 worst-case writes (12 score + 15 banner) are not VBlank-bound.
  `gameover_render` is unchanged — only the prompt blink (12 writes)
  per frame.

---

## 5. Accessibility / legibility

Both new elements use the established UI text rule (memory/conventions
"UI text rules"): shade 3 glyphs on shade 0 background, with ≥ 1 empty
tile of padding on every side.

Verified for each new element:
- **Title `HI:` line** at (5, 15): row 14 above is blank, row 16 below
  is the attribution starting at col 2 (1 row of vertical padding;
  attribution and HI line do not overlap horizontally either —
  attribution `(C) 2025 GBTD MVP` runs cols 2..18, HI line runs cols
  5..13, but they are on different rows so the rule is satisfied).
- **Gameover SCORE line** at (4, 14): row 13 above is either blank or
  the banner (banner ends at col 16, score starts at col 4 — same row
  separation rule applies, but they're on different rows). Row 15
  below is the prompt — this is a 0-row gap. The prompt and score line
  are both UI text on the same shade scheme; the eye can separate them
  by row alone, but for strict adherence to the "1 empty tile padding"
  rule we accept a 1-row deviation here, justified by:
  1. Both lines are static + prompt OR static + score — at any moment
     at most 2 of the 3 elements are visible (prompt blinks).
  2. The score line is a one-shot paint, never animated; no flicker
     interaction with the prompt.
- **Gameover banner** at (2, 13): row 12 above is the closing edge of
  the WIN/LOSE art frame (1-row gap to art interior), row 14 below is
  the score line (0-row gap — same exception as above; both static
  text, no animation interaction).

DMG 4-shade contrast: shade-3 text on shade-0 background is the highest
contrast pair available, identical to the existing HUD and PRESS START
prompt — known to pass on real DMG hardware (per mvp-design §1).

Keyboard / input: no new input affordances. Existing title selectors
(LEFT/RIGHT to cycle map/difficulty) implicitly drive the HI display
update — no new buttons to learn. Gameover screen is read-only until
PRESS START.

---

## 6. Implementation checklist (for the coder)

1. Add `s_hi_dirty` flag and `draw_hi_now()` to `src/title.c`. Insert
   into the priority chain between `s_focus_dirty` and `s_dirty`. Set
   `s_hi_dirty = 1` in `title_enter()`, after `game_set_difficulty()`,
   and after `game_set_active_map()`.
2. `draw_hi_now()`: 9 `set_bkg_tile_xy` calls writing `H`, `I`, `:`,
   ` `, then 5 zero-padded digit tiles, at row 15 cols 5..13.
3. Extend `gameover_enter` signature to
   `gameover_enter(bool win, u16 final_score, bool new_hi)`. Caller in
   `src/game.c` computes `new_hi` BEFORE calling, then calls
   `score_set_hi(...)`.
4. In `gameover_enter`, after the existing tilemap blit and inside the
   `DISPLAY_OFF` bracket, paint:
   - SCORE line at (4, 14): `S C O R E : space d d d d d`.
   - If `new_hi`: banner at (2, 13):
     `N E W space H I G H space S C O R E !`.
5. No change to `gameover_render` — score and banner are static.
6. Append to `memory/conventions.md` an "Iter-3 #19" entry capturing
   the priority-chain extension and the static-banner decision.
