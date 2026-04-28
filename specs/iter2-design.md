# GBTD Iteration 2 — Visual / UX Design Spec

Target: Game Boy DMG. 160×144, 4-shade BGP, 8×8 tiles. Builds on `specs/mvp-design.md`.
All ASCII art uses the `gen_assets.py` `sprite_from_art()` / `tile_bg_tile()` legend:

| Char | BGP idx | Shade        |
|------|---------|--------------|
| `.`  | 0       | White        |
| `%`  | 1       | Light grey   |
| `+`  | 2       | Dark grey    |
| `#`  | 3       | Black        |

OBP0 mirrors BGP. All new art respects the palette assignments in `memory/conventions.md`
(black reserved for "things the player must locate quickly": entities, text, cursor).

VRAM budget summary (this iteration adds):
- Sprite tiles: 2 (robot) + 18 (menu glyphs) = **20 new** → totals 8 + 20 = 28/256 used.
- BG tiles:    1 (firewall) → totals 11 + 1 = 12 used.

---

## 1. Robot agent enemy (sprite, 2 walk frames)

**Design intent.** The MVP enemy `SPR_BUG_A/B` is a *horizontal centipede* silhouette
that fills all 8 columns and has antennae on both ends. The Robot must read as a
*vertical, humanoid, mechanical* figure at the same 8×8 footprint so that even at
DMG contrast loss the player can tell the two enemy types apart by silhouette alone.

**Silhouette differentiators vs. SPR_BUG_*:**
- Vertical primary axis (cols 1..6 used; cols 0,7 mostly empty) vs. bug's horizontal.
- A single-pixel antenna spike at the top (col 4) — unique to the robot.
- Distinct head/torso/legs three-band structure, where the bug is one continuous mass.
- Black-dominant outline with dark-grey fill (vs. bug's all-black body with no fill).

**Shading.** Outline = `#` (shade 3, black). Body fill = `+` (shade 2, dark grey). Eye
slit = `.` (shade 0, white) inside the head. Background = white. ≥ 60% of non-white
pixels are black so the silhouette survives DMG contrast loss.

### SPR_ROBOT_A (right-stride: legs apart)

```
....#...   row0  antenna spike (col 4)
..####..   row1  head top      (cols 2..5)
..#..#..   row2  head sides + white eye-band (cols 3,4 white)
..####..   row3  head bottom
.######.   row4  shoulders     (cols 1..6, broader than head)
.#++++#.   row5  torso: black outline + dark-grey fill
.#++++#.   row6  torso (cont.)
.##..##.   row7  legs apart    (cols 1..2 left foot, 5..6 right foot)
```

### SPR_ROBOT_B (mid-stride: feet together)

Identical to frame A in rows 0..6; only row 7 changes. This isolates the animation
delta to the footwear so the player perceives a clean walk cycle, not flicker.

```
....#...   row0  antenna spike
..####..   row1  head top
..#..#..   row2  head + eye-band
..####..   row3  head bottom
.######.   row4  shoulders
.#++++#.   row5  torso
.#++++#.   row6  torso
...##...   row7  feet together (cols 3..4) — mid-stride
```

**Animation cadence.** Reuse the existing bug walk-cycle period (alternate A/B every
~16 frames at the enemy's tick rate). No palette flicker. No hit-flash this iteration.

**Naming.** Append to `sprite_tiles[]` in `gen_assets.py`:

```python
('SPR_ROBOT_A', SPR_ROBOT_A),
('SPR_ROBOT_B', SPR_ROBOT_B),
```

---

## 2. Firewall tower BG tile (`TILE_TOWER_2`)

**Design intent.** Existing `TILE_TOWER` is a radially-symmetric octagonal turret
(antivirus). The Firewall must read as a **rectangular brick barrier** at a glance
— orthogonal grain, not radial — so the two tower types are unambiguous on a static
screenshot.

**Tile-alignment guarantee.** The four corners (rows 0,7 × cols 0,7) are white,
matching `TILE_GROUND`'s base shade. This produces a rounded rectangular footprint
that reads as a discrete object on white ground without the brick grain bleeding
visually into adjacent ground tiles. No pixel touches the tile edge except along
the top/bottom mortar lines, where the visual continuity is intentional (bricks
look like a wall, not a floating crate).

**Shading.**
- `#` outline = mortar lines + brick edges (shade 3).
- `+` brick face fill (shade 2).
- `%` single light-grey "ember" pixel hints at the *fire*-wall theme without
  introducing flame iconography that wouldn't survive 8×8.
- `.` corner whitespace for tile alignment.

### TILE_TOWER_2 (Firewall)

```
.######.   row0  top edge of wall  (white corners cols 0,7)
#+##+##+   row1  brick row A       (3 bricks, mortar in black)
########   row2  mortar line
+##%##++   row3  brick row B (offset) — light-grey ember pixel at col 3
########   row4  mortar line
#+##+##+   row5  brick row A
########   row6  mortar line
.######.   row7  bottom edge       (white corners)
```

The half-pixel offset between row-A and row-B bricks (cols 1 vs col 0) is the
classic running-bond brick pattern. Even at 1× zoom it reads as masonry.

**Naming.** Append to `map_tiles[]` in `gen_assets.py`:

```python
('TILE_TOWER_2', TOWER2_BG),
```

Placed via `set_bkg_tile_xy(col, row + HUD_H, TILE_TOWER_2_IDX)` exactly like
`TILE_TOWER`.

---

## 3. Menu sprite glyphs (font copy → sprite VRAM)

**Decision.** Do **not** redraw the glyphs. The MVP `FONT` dict in `gen_assets.py`
already contains pixel-perfect 5×7-in-8×8 bitmaps for every required character.
We copy those bitmaps verbatim into the *sprite* VRAM bank so the upgrade/sell
overlay can be drawn with OAM (overlapping the BG without disturbing it).

**Glyphs required (18 total).**

```
U  P  G  S  E  L     0  1  2  3  4  5  6  7  8  9     :  >
```

**Naming convention.** `SPR_GLYPH_<TOKEN>` where `<TOKEN>` is:
- The literal char for letters: `U, P, G, S, E, L`.
- The literal digit for numerals: `0..9`.
- A spelled-out token for punctuation: `COLON` (`:`) and `GT` (`>`).

Resulting symbols, in the order they should be appended to `sprite_tiles[]`:

```
SPR_GLYPH_U      SPR_GLYPH_P      SPR_GLYPH_G      SPR_GLYPH_S
SPR_GLYPH_E      SPR_GLYPH_L      SPR_GLYPH_COLON  SPR_GLYPH_GT
SPR_GLYPH_0      SPR_GLYPH_1      SPR_GLYPH_2      SPR_GLYPH_3
SPR_GLYPH_4      SPR_GLYPH_5      SPR_GLYPH_6      SPR_GLYPH_7
SPR_GLYPH_8      SPR_GLYPH_9
```

**Generation idiom (in `gen_assets.py`).** A 5×7 FONT entry is centered into 8×8
the same way the BG font helper does it (1-col left pad, 1-row top + bottom pad,
right pad to 8). Black pixels become `B`, blanks become `W`. Pseudocode:

```python
def glyph_to_sprite(ch):
    rows = [[W]*8 for _ in range(8)]               # white field
    for r, line in enumerate(FONT[ch]):            # 7 rows of 5 chars
        for c, px in enumerate(line):
            if px == '#':
                rows[r + 1][c + 1] = B             # 1-col / 1-row inset
    return encode(rows)
```

This guarantees the sprite glyphs are pixel-identical to the BG glyphs on the
HUD, so a digit in the HUD wave counter and a digit in the upgrade-cost menu
match exactly — no "two fonts" feel.

**OBP0.** Glyphs are black-on-white; OBP0 mirrors BGP, so they render as text-
on-bg. The white background pixels of each glyph **are opaque** (palette index 0,
which on sprites = transparent on DMG). The transparent white means glyphs
overlap the underlying BG cleanly without a boxy outline. The trailing column
and right-side padding stay transparent → tight letter spacing with no gaps.

---

## 4. Tower-select HUD indicator

**Placement.** Column 19 (rightmost cell) of HUD row 0. The MVP HUD as documented
in `memory/conventions.md` is `HP:05  E:030  W:1/3` (17 chars + 3 trailing blanks).
Iteration 2 widens the wave format to `W:NN/NN` and consumes one of those trailing
blank cells for the tower-select indicator.

**Glyphs.** A single BG-font character at `(col=19, row=0)`:
- `A` → Antivirus tower selected (the existing `TILE_TOWER`).
- `F` → Firewall tower selected (the new `TILE_TOWER_2`).

Updated 20-char HUD row (exact tile-by-tile mockup):

```
col:  0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
       H P : 5 . E : 0 3 0  .  W  :  0  1  /  1  0  .   A
```

Rendered as literal text with `.` = blank tile (shade 0), one char per tile:

```
HP:5 E:030 W:01/10 A
```

(20 chars exactly, no padding.)

**Redraw policy.** Indicator updates only on cursor toggle (LEFT/RIGHT shoulder
or a dedicated SELECT-press — exact binding owned by the input spec). Single
`set_bkg_tile_xy(19, 0, 'A' | 'F')` call, VBlank-safe.

**States.**
| State          | Glyph at col 19 | Notes                                       |
|----------------|-----------------|---------------------------------------------|
| Antivirus sel. | `A` (shade 3)   | Default on game start.                      |
| Firewall sel.  | `F` (shade 3)   | Player has switched.                        |
| Locked / N/A   | `.` (blank)     | Reserved; not used in iter 2 (always one tower selectable). |

No animation, no blink. The cursor sprite already provides the active feedback
loop — the HUD letter is purely a label.

---

## 5. Upgrade / sell menu overlay

**Anatomy.** A 14-OAM-slot widget arranged as **2 rows × 7 columns**, each cell
a single 8×8 glyph sprite drawn from the new `SPR_GLYPH_*` set. Total widget
footprint: 56 × 16 px.

### 5.1 Content layout (cell-by-cell)

```
                col offset:  0    1    2    3    4    5    6
row offset 0 (top):          >    U    P    G    :    Nh   Nl
row offset 1 (bottom):       .    S    E    L    :    Nh   Nl
```

- `>` is the **selection cursor** glyph; it occupies col 0 of the *active* row
  and col 0 of the inactive row is left **blank** (sprite hidden / OAM y=0).
- `Nh` / `Nl` are the high and low decimal digits of the cost or refund.
  Two-digit, zero-padded, range 00..99 (matches iter 2 economy spec).
- All other cells are static glyphs.

Concrete examples:

```
>UPG:15        Cursor on UPG; upgrade costs 15 energy.
 SEL:08

 UPG:15        Cursor on SEL; selling refunds 8 energy.
>SEL:08
```

### 5.2 OAM slot allocation

The 14 widget sprites take a **contiguous, dedicated** OAM range. Per the
existing OAM map in `memory/conventions.md` (slots 37..39 reserved), we extend:

| Slot range   | Owner                        |
|--------------|------------------------------|
| 0            | cursor (existing)            |
| 1..16        | towers reserved (existing)   |
| 17..28       | enemies (existing)           |
| 29..36       | projectiles (existing)       |
| **37..39**   | **menu cells (3 of 14)** — repurposed from "reserved" |
| **OAM cap raised? NO.** Iter 2 stays at 40 OAM. The menu only opens while the cursor is parked over a placed tower, and during that window we **freeze enemy spawns** for 1 frame and reuse the upper enemy/projectile slots that are inactive. |

**Decision (revised).** Rather than co-opting active gameplay slots, the menu is
exclusive with gameplay action: when the menu is **open**, the run-state is
`PAUSED_MENU`, enemies and projectiles do not advance, and OAM slots 17..36
(20 slots) are guaranteed free. The menu lives in slots **17..30** (14 slots).
Slot 0 (cursor) stays visible behind the menu. Slots 1..16 (towers, BG-resident)
are unchanged. This avoids any per-scanline budget regression.

When the menu **closes**, slots 17..30 are hidden (`y = 0`) and gameplay resumes
on the next frame.

### 5.3 Anchor offset rules

Let the selected tower's screen-pixel top-left be `(Tx, Ty)` where:

```
Tx = col * 8
Ty = (pf_row + HUD_H) * 8     # HUD_H = 1, so Ty = (pf_row + 1) * 8
```

The widget is 56×16 px. Its top-left anchor `(Mx, My)` is computed as:

**Vertical placement:**
```
if pf_row <= 1:          // tower is in the top two play-field rows
    My = Ty + 16          // float BELOW: 8 px below tower bottom edge
else:
    My = Ty - 16          // float ABOVE: bottom of widget = top of tower
```

The 16-px gap (= 2 tile rows including a 1-row spacing) keeps the widget
visually detached from the tower so the cursor brackets remain readable.

**Horizontal placement (centered on tower, then clamped):**
```
Mx_raw = Tx + 4 - 28      // center 56-px widget on 8-px tower → -24
                          // (the +4 - 28 == -24 form is kept to make the
                          //  "center on tower midline" intent explicit)
Mx = clamp(Mx_raw, 0, 160 - 56)   // i.e. 0 .. 104
```

**Per-cell sprite positions** (given anchor `(Mx, My)`):
```
cell (col_off, row_off) →  ( Mx + col_off*8, My + row_off*8 )
```

OAM `y` field on DMG is `screen_y + 16`; `x` field is `screen_x + 8`. The
existing tower OAM helpers already apply this offset — reuse the same helper
for menu cells.

### 5.4 Interaction states

| Element          | State            | Visual                                      |
|------------------|------------------|---------------------------------------------|
| Cursor `>`       | Row 0 active     | `>` at (Mx, My); col-0 row-1 hidden         |
| Cursor `>`       | Row 1 active     | `>` at (Mx, My+8); col-0 row-0 hidden       |
| Whole widget     | Opening          | All 14 sprites placed in the same frame; no fade (DMG can't dither alpha). |
| Whole widget     | Closed           | All 14 OAM `y` set to 0 in one pass.        |
| `UPG` row        | Max level        | Row replaced by literal `UPG:--` (two `-` glyphs from FONT, **add `SPR_GLYPH_DASH` if not already present** — note: `-` is already in FONT, will require 1 extra sprite tile, raising new-sprite total to 21/24, still under budget). |
| `SEL` row        | Always available | Refund value = 50% of total invested, floor.|
| Insufficient E   | UPG cost shown   | The cost digits remain shade 3 (do not grey out — DMG contrast loss kills shade 1 on white). Instead the **A button is consumed silently** and the HUD `E:` field blinks once (1 frame inverted via temporary BGP swap). |

### 5.5 User flow

1. Player parks cursor on a placed tower → presses **A**.
2. Game state → `PAUSED_MENU`. Enemies/projectiles freeze. Menu OAM 17..30
   populated; default cursor row = 0 (`>UPG`).
3. **UP / DOWN** moves the `>` between rows (auto-repeat per
   `memory/conventions.md`: 12-frame initial, 6-frame repeat).
4. **A** confirms:
   - On `UPG` row: deduct cost, increment tower level, refresh widget digits
     (or collapse to `UPG:--` if max).
   - On `SEL` row: refund energy, remove tower BG tile, close menu.
5. **B** cancels: close menu, resume gameplay.
6. **START** also closes the menu (forgiving exit).

### 5.6 Accessibility

- **Keyboard / D-pad only** — no diagonal or chord input required.
- The selection cursor uses the same `>` glyph players already see in the title
  screen / pause prompts → reinforces "this is selectable" affordance.
- All menu glyphs are shade 3 on shade 0 (transparent sprite white): maximum
  DMG contrast, identical to HUD text.
- The menu never overlaps the HUD row (vertical placement rule guarantees
  `My ≥ 8` when floating below the top two rows; when floating above from
  pf_row ≥ 2, `My = Ty - 16 ≥ 8`).
- No flashing > 2 Hz. The only blink is the single-frame HUD `E:` flash on
  insufficient funds (≤ 1 Hz event-driven).
- The widget is opaque enough (every cell is a glyph with mostly-white sprite
  background, so the underlying BG shows through between letters) that the
  tower and adjacent path remain partially visible — players can still see
  their spatial context while the menu is open.

---

## 6. Asset-pipeline checklist

In `tools/gen_assets.py`:

1. Add `SPR_ROBOT_A`, `SPR_ROBOT_B` via `sprite_from_art(...)` using the art
   in §1; append both to `sprite_tiles[]`.
2. Add `TOWER2_BG` via the existing `tile_bg_tile`-style helper (or inline
   `encode([...])`) using the art in §2; append `('TILE_TOWER_2', TOWER2_BG)`
   to `map_tiles[]`.
3. Add a `glyph_to_sprite(ch)` helper (§3); generate the 18 `SPR_GLYPH_*`
   entries in the order listed and append to `sprite_tiles[]`.
4. Re-run `just gen-assets` (or equivalent); confirm `res/assets.h` exposes:
   - `SPR_ROBOT_A`, `SPR_ROBOT_B` indices.
   - `TILE_TOWER_2` index.
   - All 18 `SPR_GLYPH_*` indices.
5. Verify `sprite_tiles` length ≤ 256 (will be 28) and `map_tiles` length ≤
   128 in the BG sub-region (will be 12).

---

## 7. Convention deltas (to be appended to `memory/conventions.md`)

- **Tower-select HUD slot.** HUD col 19 row 0 reserved for the selected-tower
  letter (`A`/`F`/blank). Wave field widens to `W:NN/NN`.
- **Menu glyph sprites.** Reuse BG `FONT` bitmaps for sprite-bank glyphs via
  `glyph_to_sprite()`; never hand-redraw a digit/letter that already exists in
  `FONT`.
- **PAUSED_MENU run-state.** Opening any tower context menu freezes enemy and
  projectile updates and frees OAM slots 17..30 for the menu widget. Closing
  the menu hides those slots in one OAM pass.
- **Menu anchor rule.** Context menus float 16 px above the selected tile
  unless `pf_row ≤ 1`, in which case they float 16 px below. Horizontal anchor
  is centered on the tile and clamped to `[0, 160 - widget_w]`.
