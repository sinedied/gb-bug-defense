# GBTD MVP — Visual / UX Design Spec

Target: Game Boy DMG. 160×144 px. 20×18 tiles of 8×8. 4 shades, no color, no audio.
This spec covers the MVP scope from `specs/roadmap.md` (1 map, 1 tower, 1 enemy, 3 waves).
All positions are in tile coordinates `(col, row)` with origin at the top-left of the screen
unless explicitly noted as "play-field local" (origin at the top-left of the play field, i.e.
HUD-relative).

---

## 1. DMG Palette (BGP indices 0–3)

| BGP | Shade        | Used for                                              | Why it reads on a real DMG LCD |
|-----|--------------|-------------------------------------------------------|--------------------------------|
| 0   | White        | HUD bar background, text background, **buildable ground (subtle dotted texture)** | Lightest pixel — biggest area on screen, gives the game a calm "desktop" look. A 1-pixel checker dot on ground tiles makes ground vs HUD visually distinct without darkening it. |
| 1   | Light grey   | Tower silhouette mid-tone, computer chassis mid-tone, "ghost"/invalid cursor | Mid-tone reserved for shading 3D-feeling props. Avoided on the path so the path stays a clean dark band. |
| 2   | Dark grey    | **Path tiles** (solid fill with a dithered seam), tower base outline ring, computer screen frame | The path needs to read as a continuous "wire" across the board at a glance; dark grey vs white ground gives ~70% perceived contrast on the green LCD — the strongest path/ground separation we can get without going full black (which we reserve for entities). |
| 3   | Black        | All HUD text, enemy body, projectile, computer screen content, cursor brackets, tower turret cap | Black is reserved for **things the player must locate quickly**: text, enemies, the cursor. Using black exclusively for "alive / important" pixels makes them pop against grey path and white ground on the washed-out DMG screen. |

**Readability rule of thumb on real DMG hardware**: assume ~40% contrast loss vs an
emulator. By placing black-only entities on a non-black background (ground = white, path =
grey 2) the player can always pick out enemies and the cursor even with a dim battery.

OBP0 (sprite palette) mirrors BGP. OBP1 is reserved (unused in MVP — could later carry an
inverted "hit-flash" palette for enemies; out of scope here).

---

## 2. Screen Layouts

Grid legend used in mockups:
- `.` = empty / shade 0 (white)
- `:` = ground texture tile (shade 0 with shade-1 dot pattern)
- `#` = path tile (shade 2)
- `C` = computer goal tile (BG, multi-tile sprite of the goal)
- `S` = spawn edge marker
- letters/words = literal text rendered with the GBDK 8×8 font (one char per tile)

### 2.1 Title screen (20×18)

```
 col:  0         1
       0123456789012345678901
row 0  ....................   <- 1-tile top padding (shade 0)
row 1  ....................
row 2  ....G.B.T.D..........   <- centered logo block, custom 4-tile glyphs
row 3  ...██████████████....
row 4  ...█.GBTD......█....    (logo art occupies rows 3..7, cols 3..16)
row 5  ...█..TOWER....█....
row 6  ...█.DEFENSE...█....
row 7  ...██████████████....
row 8  ....................
row 9  ..A.RUNAWAY.AI.IS....   <- subtitle / tagline, rows 9..10
row10  ..SENDING.BUGS.......
row11  ....................
row12  ....................
row13  ....PRESS.START.....    <- prompt, row 13, cols 4..15 (12 chars)
row14  ....................   (prompt blinks at 1 Hz: visible 0.5s, hidden 0.5s)
row15  ....................
row16  ..(C).2025.GBTD.MVP.    <- attribution, row 16, cols 2..18
row17  ....................
```

Concrete element placements:
| Element        | Row(s) | Col(s) | Tiles                                    |
|----------------|--------|--------|------------------------------------------|
| Logo box frame | 3..7   | 3..16  | 1 box-corner tile + 1 box-edge-h + 1 box-edge-v (3 unique) |
| Logo text "GBTD TOWER DEFENSE" | 4..6 | 4..15 | font tiles |
| Tagline        | 9..10  | 2..17  | font tiles ("A RUNAWAY AI IS / SENDING BUGS")  |
| PRESS START    | 13     | 4..15  | font tiles, blinks at 1 Hz                 |
| Attribution    | 16     | 2..18  | font tiles ("(C) 2025 GBTD MVP")           |

Background fill is shade 0 everywhere outside the logo box.

### 2.2 Gameplay screen (20×18)

**HUD placement decision: TOP, 1 tile row (row 0).** Justification:
- The path's natural exit at the right edge is visually "below" the spawn at the left, so
  putting the HUD on top keeps the player's eye sweeping down/right into the action
  instead of bouncing past the HUD.
- A top HUD avoids overlap with the cursor when the player is placing towers near the
  bottom of the field (most TD habit places near choke points, often lower-right).
- 1 row is enough for the compact label format (see §6) and leaves a generous
  **17 rows** of play field.

```
 col:  0         1
       0123456789012345678901
row 0  HP:05  E:030  W:1/3.    <- HUD (shade 0 bg, shade 3 text)
row 1  :::::::::::::::::::.    <- play field starts; ground texture
row 2  ::::::::::::::::::::
row 3  S###############:::.    <- spawn at col 0, path heads right
row 4  ::::::::#:::::::::::
row 5  ::::::::#::::::::CC.    <- computer top-row tiles (cols 18-19, rows 5-6)
row 6  ::::::::#::::::CC.    .
row 7  ::::::::#::::::#::::    (computer is 2×2, see map §3)
row 8  ::::::::#::::::#::::
row 9  ::::::::#::::::#::::
row10  ::::::::########:::.
row11  ::::::::::::::::::::
...
row17  ::::::::::::::::::::
```

(Full play-field grid given in §3. The HUD only consumes row 0.)

HUD field layout — exact tile positions on row 0:
```
col:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
text: H  P  :  0  5     E  :  0  3  0     W  :  1  /  3
```
- `HP:` always rendered (cols 0..2). HP digits at cols 3..4 (zero-padded, max "99").
- `E:` always rendered (cols 6..7). Energy digits at cols 8..10 (zero-padded, max "999").
- `W:` always rendered (cols 12..13). Wave value at cols 14..16 in the form `1/3`.
- Cols 17..19: blank (shade 0). Reserved for a future "next-wave countdown" — left empty
  in MVP (no TBD; explicitly blank).

No icons in MVP. ASCII letters only — they are universally recognizable, ship in the
default GBDK font (zero new tile cost), and survive the DMG contrast loss better than
1-color icons.

### 2.3 Win screen (20×18)

```
row 0  ....................
row 1  ....................
row 2  ....SYSTEM.CLEAN....    <- title, row 2, cols 4..15
row 3  ....................
row 4  ....................
row 5  .ALL.PROCESSES......    <- one-liner (dark humor), rows 5..7
row 6  .TERMINATED..AND....
row 7  .SO.IS.YOUR.BATTERY.
row 8  ....................
row 9  ....................
row10  .....┌────────┐....
row11  .....│  :)    │....    <- ASCII smile inside a "window" frame
row12  .....└────────┘....    (drawn with corner+edge BG tiles)
row13  ....................
row14  ....................
row15  ....PRESS.START.....    <- row 15, blinks at 1 Hz
row16  ....................
row17  ....................
```

Background: shade 0. Frame uses the same 3 box tiles as the title-screen logo (reused —
no new tile cost).

### 2.4 Lose screen (20×18)

```
row 0  ....................
row 1  ....................
row 2  ....KERNEL.PANIC....    <- title, row 2
row 3  ....................
row 4  ....................
row 5  .THE.AI.WON........     <- one-liner (dark humor), rows 5..7
row 6  .PLEASE.REBOOT.....
row 7  .YOUR.CIVILIZATION.
row 8  ....................
row 9  ....................
row10  .....┌────────┐....
row11  .....│  X_X   │....    <- "dead" face inside the same window frame
row12  .....└────────┘....
row13  ....................
row14  ....................
row15  ....PRESS.START.....
row16  ....................
row17  ....................
```

Distinction from win screen at a glance: title text + face glyph differ; everything else
(layout, frame, prompt) is identical so we share tilemap structure.

---

## 3. Map Design — the one MVP map

Play field = 20 cols × 17 rows (rows 1..17 on screen; play-field-local rows 0..16).

Legend: `.` = ground (shade-0 ground texture tile), `#` = path (shade-2), `S` = spawn
edge marker (visual only — same tile as path-h, just at col 0), `C` = computer goal
(2×2 BG tile cluster at cols 18..19, play-field rows 4..5).

```
col:        0         1
            0123456789012345678901
pf row  0   ....................
pf row  1   ....................
pf row  2   S###############....   <- spawn corridor, row 2
pf row  3   ........#...........
pf row  4   ........#.........CC   <- computer top half (cols 18..19, pf row 4)
pf row  5   ........#.......#CC   <- path entry to computer at col 17, pf row 5
pf row  6   ........#.......#...
pf row  7   ........#.......#...
pf row  8   ........#.......#...
pf row  9   ........#########...
pf row 10   ....................
pf row 11   ....................
pf row 12   ....................
pf row 13   ....................
pf row 14   ....................
pf row 15   ....................
pf row 16   ....................
```

(On the actual screen these play-field rows are at screen rows 1..17; add +1 to convert.)

### Waypoints (play-field-local tile coords)

Bug spawns just off the left edge and walks tile-center to tile-center through:

```
WAYPOINTS = [
  (-1,  2),   // off-screen spawn
  ( 0,  2),   // enter screen
  ( 8,  2),   // turn 1: right -> down
  ( 8,  9),   // turn 2: down  -> right
  (15,  9),   // turn 3: right -> up
  (15,  5),   // turn 4: up    -> right
  (17,  5),   // last walkable path tile
  (18,  5),   // collision with computer (damage trigger)
]
```

That is **4 turns** (well above the "≥2" requirement) and forces the player to think
about three distinct chokes: the long top corridor (row 2), the long descender (col 8),
and the final ascender (col 15) right next to the computer.

### Tile counts

- Path tiles drawn on the map: 9 (top run) + 7 (down run) + 7 (mid run) + 4 (up run) +
  2 (final run) = **29 path tiles**
- Computer: 2×2 = **4 tiles** at (18,4)(19,4)(18,5)(19,5) play-field-local
- Total play-field tiles: 20 × 17 = 340
- **Buildable ground tiles: 340 − 29 − 4 = 307 tiles**

That's far more than the player can afford with the MVP energy economy, which is the
intent: choice of *where* to build matters more than running out of slots.

### Spawn marker

Visual only: the `S` at col 0 row 2 is just a regular path-horizontal tile. The bug
appears one tile off-screen (col −1) and walks on at speed, giving a natural "walking
in from the edge" feel without a dedicated tile.

---

## 4. Sprite & Tile Concepts

All sketches are 8×8 pixels using the four-shade legend:
`.` = shade 0 (white) · `░` = shade 1 (light grey) · `▒` = shade 2 (dark grey) · `█` = shade 3 (black).

### 4.1 Placement cursor (sprite, 1 OAM, 2 animation frames)

A 4-corner bracket reticle that does **not** obscure the tile underneath.

Frame A — "on" (shown 30 frames ≈ 0.5 s):
```
██....██
█.......
........
........
........
........
█.......
██....██
```

Frame B — "off" (shown 30 frames ≈ 0.5 s):
```
░░....░░
░.......
........
........
........
........
░.......
░░....░░
```

Blink rate = 1 Hz (Frame A ↔ Frame B). When the cursor sits on an **invalid** tile (path,
computer, or already-occupied), it switches to the "ghost" variant (frames A' and B'
identical to A/B but **all `█` replaced with `░`** — a noticeably weaker reticle) AND
the blink rate doubles to 2 Hz. That double-channel signal (color + speed) is robust
against DMG contrast loss. Both rates are well below the 4 Hz photosensitivity limit.

Tile cost: 2 unique sprite tiles (frames A and B). Ghost variant is the *same* tiles
re-rendered through OBP1 — no extra tile cost. (If we choose not to use OBP1 for MVP
simplicity, ghost = 2 more tiles = 4 total. Budget below assumes the worst case of 4.)

### 4.2 Tower — "Antivirus Turret" (sprite, 1 OAM, 1 frame in MVP)

Compact octagonal base with a turret cap pointing up. Static (no rotation in MVP — the
projectile carries the directional read).

```
.░▒██▒░.
░▒████▒░
▒██▒▒██▒
█▒█..█▒█
█▒█..█▒█
▒██▒▒██▒
░▒████▒░
.░▒██▒░.
```

Reads as a chunky shielded turret with a "lens" hole in the middle. Tile cost: **1
sprite tile**.

### 4.3 Bug enemy (sprite, 1 OAM, 2 walk frames)

Small 6-legged bug. Body is solid black so it pops against the dark-grey path.

Frame A — legs spread:
```
█.█..█.█
.██████.
.██▒▒██.
████████
.██████.
.██████.
█.█..█.█
.......█
```

Frame B — legs tucked (animate at 4 Hz, i.e. swap every 15 frames):
```
.█.██.█.
.██████.
.██▒▒██.
████████
.██████.
.██████.
.█.██.█.
█.......
```

Tile cost: **2 sprite tiles**. The two `▒` pixels in the middle are the bug's "eye
gleam" — a single mid-tone accent that helps it not look like a black blob on a grey
path.

### 4.4 Projectile (sprite, 1 OAM, 1 frame)

Small black diamond, 3×3 centered in an 8×8 sprite cell.

```
........
........
...█....
..███...
...█....
........
........
........
```

Tile cost: **1 sprite tile**. Direction is implicit — projectile's *motion vector*
carries the read; no rotated variants needed for MVP (single tower type, single enemy
type, point-blank-ish ranges).

### 4.5 Computer goal (BG tiles, 2×2 = 4 tiles, full-HP variant)

Drawn as a CRT monitor: chunky bezel + screen with a small icon.

Top-left tile (TL):                Top-right tile (TR):
```
████████                            ████████
█▒▒▒▒▒▒▒                            ▒▒▒▒▒▒▒█
█▒......                            ......▒█
█▒.░░░░░                            ░░░░░.▒█
█▒.░░░░░                            ░░░░░.▒█
█▒.░░██░                            ██░░░.▒█
█▒.░░██░                            ██░░░.▒█
█▒.░░░░░                            ░░░░░.▒█
```

Bottom-left (BL):                  Bottom-right (BR):
```
█▒.░░░░░                            ░░░░░.▒█
█▒......                            ......▒█
█▒▒▒▒▒▒▒                            ▒▒▒▒▒▒▒█
█▓██████                            ██████▓█
█▒▒▒▒▒▒▒                            ▒▒▒▒▒▒▒█    (▓ rendered as ▒ — only 4 shades)
█▒......                            ......▒█
█▒.STAND                            STAND.▒█
████████                            ████████
```

(That's a stylized monitor with screen + base. The little `██` in the middle of the
screen reads as a "smile" / cursor blip.)

Tile cost: **4 unique BG tiles** (TL, TR, BL, BR). Not animated.

**Damaged variant** (optional, included): same 4 tiles re-drawn with shade-2 "glitch"
dots scattered across the screen interior. Swapped in when computer HP ≤ 50%.
Additional tile cost: **4 BG tiles**. Total computer = 8 BG tiles.

---

## 5. Cursor Behavior — valid vs invalid placement

| Situation                            | Visible cue                          | Why this works on DMG |
|--------------------------------------|--------------------------------------|------------------------|
| On a buildable ground tile           | Bracket = solid black, blinks at 1 Hz | Strong, calm signal — "yes, here." |
| On a path tile or the computer tile  | Bracket = light grey, blinks at 2 Hz | Two simultaneous changes (lower contrast + faster pulse) so the player notices even on a washed-out screen. |
| On a tile already occupied by a tower| Same "ghost + 2 Hz" treatment as path | Reuses the same invalid state — no extra art. |
| Insufficient energy to place         | Bracket stays in valid style, **but** pressing A produces a 1-frame inversion of the HUD energy field (`E:030` flips to white-on-black for ~5 frames) | Routes the failure feedback to the *thing the player misjudged* (the budget) rather than the cursor. |

Movement: cursor snaps tile-by-tile on D-pad input with a 6-frame repeat delay after a
12-frame initial delay (standard GB feel). Holding the D-pad scrolls the cursor at
~10 tiles/sec.

Buttons:
- `A` — place tower (if valid + affordable). 4-frame "thump" anim: cursor brackets
  briefly contract one pixel inward.
- `B` — no-op in MVP (reserved for future "sell"). Documented so the planner doesn't
  invent a behaviour.
- `START` — pause is **out of scope** for MVP; planner should treat START as no-op
  during gameplay.

---

## 6. HUD content & redraw policy

HUD = row 0 only. Format (exactly, 17 visible chars + 3 trailing blanks):

```
HP:05  E:030  W:1/3
^^ ^^  ^ ^^^  ^ ^^
|| ||  | |||  | |+- wave_max  ('3' literal)
|| ||  | |||  + wave_cur     (1..3)
|| ||  + + +- energy 000..999 (zero-padded, 3 digits)
|| |+- HP units
|| +- HP tens
+- HP label
```

Character budget = 17 ≤ 20. ✓

### Redraw rules (CPU/VRAM-friendly)

Use direct `set_bkg_tile_xy` writes only on change. No per-frame full-row redraw.

| Field    | Tiles redrawn        | Trigger event                         |
|----------|----------------------|---------------------------------------|
| HP digits  | 2 tiles (cols 3..4)  | On computer-damage event only         |
| E digits   | 3 tiles (cols 8..10) | On energy spend (place tower) **or** on energy gain (kill bounty) |
| W cur      | 1 tile (col 14)      | On wave transition only                |
| Static labels | 0                | Drawn once on enter-gameplay; never redrawn |

Worst-case HUD redraws per frame: 2 + 3 + 1 = 6 tiles ≪ VBLANK budget.

---

## 7. Font

**Confirmed: GBDK-2020 default 8×8 ASCII font is used unmodified for all HUD and
screen text** (title, tagline, attribution, HP/E/W labels, win/lose copy, PRESS START).

Glyphs needed beyond standard printable ASCII: **none**. Specifically:
- `(`, `)`, `:`, `/`, digits `0-9`, uppercase `A-Z`, space — all present in the GBDK
  default font.
- The "window frame" on the win/lose screens is drawn with **3 dedicated map tiles**
  (corner, h-edge, v-edge — see §8), *not* with font box-drawing chars, so we don't
  need any non-ASCII glyph.
- Smile/dead-face on win/lose use literal `:`, `)`, `X`, `_` characters from the font.
  No custom face glyph.

This keeps the font import path identical to "include the GBDK header and `printf`-print
to the BG layer."

---

## 8. Tile / sprite VRAM budget

DMG VRAM: 384 tile slots shared between BG and sprites; BG layer can address up to 256
of them (with the standard `_VRAM` 0x8000 addressing mode used by GBDK default).

### BG tiles

| Group                      | Tiles | Notes |
|----------------------------|-------|-------|
| GBDK default font (printable ASCII 0x20..0x7E) | 96 | Imported once, shared by all screens. |
| Ground (buildable) tile    | 1     | One shared dotted-shade-0 tile. |
| Path straight horizontal   | 1     |       |
| Path straight vertical     | 1     |       |
| Path corner (NE, NW, SE, SW) | 4   | One per turn orientation. |
| Computer goal — full HP    | 4     | 2×2 cluster (TL/TR/BL/BR). |
| Computer goal — damaged    | 4     | 2×2 swapped in at HP ≤ 50%. |
| UI box-frame: corner       | 1     | Used on title, win, lose screens. |
| UI box-frame: edge horiz.  | 1     |       |
| UI box-frame: edge vert.   | 1     |       |
| Blank (shade 0 solid)      | 1     | Background fill / clear tile. |
| **BG total**               | **115** | Comfortably ≤ 256. |

### Sprite tiles

| Sprite               | Tiles | Notes |
|----------------------|-------|-------|
| Cursor frame A (valid) | 1   |       |
| Cursor frame B (valid) | 1   |       |
| Cursor frame A (ghost) | 1   | Used on invalid tiles. |
| Cursor frame B (ghost) | 1   |       |
| Tower (Antivirus Turret) | 1 | Single static frame. |
| Bug walk frame A     | 1     |       |
| Bug walk frame B     | 1     |       |
| Projectile           | 1     |       |
| **Sprite total**     | **8** | Comfortably ≤ 128 (per-layer headroom). |

**Combined VRAM use: 115 + 8 = 123 tile slots out of 384.** Fits with massive headroom
for iteration 2 additions (second tower, second enemy, music-driven UI flourishes).

---

## 9. OAM / per-scanline budget

DMG hard limits: **40 OAM entries total**, **10 sprites per scanline**.

### Worst-case OAM headcount during gameplay

| Object                 | Max instances | OAM each | OAM subtotal |
|------------------------|---------------|----------|--------------|
| Placement cursor       | 1             | 1        | 1            |
| Bugs on screen at once | **6**         | 1        | 6            |
| Projectiles in flight  | **4**         | 1        | 4            |
| Tower flash overlay    | 0 in MVP      | —        | 0            |
| **Total**              |               |          | **11 / 40**  |

Bug cap of 6 is a planner contract: the wave script must not spawn the next bug while
6 are already alive on the path. Three waves at this density (e.g. 3, 5, 7 bugs total
with min 1.5 s spacing) keep us safely under 6 concurrent.

Projectile cap of 4: with one tower-per-3-tile-range and a fire interval ≥ 30 frames,
even with several towers placed the engine can hard-cap projectiles in flight at 4 (drop
the oldest if exceeded — invisible to the player at this fire rate).

### Per-scanline analysis

The path layout (§3) places bugs on play-field rows 2, 3..9 (col 8 descender), 9, 5..9
(col 15 ascender), and row 5 (final approach). Crucially:

- Only **one** path tile exists per row in the descender (col 8) and ascender (col 15)
  segments, so at most 1 bug occupies any single 8-pixel scanline band along the
  vertical runs.
- The two horizontal runs (row 2, row 9) and the row-5 final approach can each host
  multiple bugs simultaneously. A single bug sprite is 8 px tall and aligned to the
  tile grid, so all bugs on the same horizontal run share the same 8 scanlines.
- Worst case: 4 bugs spaced along row 2 + cursor parked on row 2 + 1 projectile
  crossing row 2 = **6 sprites on those 8 scanlines** ≤ 10. ✓
- Cursor on a horizontal run with 4 bugs and 2 projectiles also crossing = 7 ≤ 10. ✓

The per-scanline 10-sprite limit is **not** a risk for this map. If the planner ever
relaxes the 6-bug cap, re-check this section.

---

## 10. Accessibility & readability

- **Text contrast**: every text glyph is BGP shade 3 (black) on shade 0 (white)
  background — the maximum contrast the DMG can produce. No grey-on-grey text anywhere.
- **Padding**: all text blocks (HUD line, title, tagline, win/lose copy, PRESS START)
  have at least **1 empty tile of shade 0** above and below, and at least **1 empty
  tile column** to the left and right of the first/last glyph. The HUD's lower edge
  (row 0 vs row 1 = play field) is the one exception, but row 1 is ground (shade 0
  with sparse dots), which still provides clear separation.
- **Flashing**: only two animated UI elements blink:
  - "PRESS START" prompt and HUD energy-fail flash: **1 Hz** (0.5 s on / 0.5 s off).
  - Cursor: **1 Hz** valid, **2 Hz** invalid.
  Both are well under the 3 Hz / 4 Hz photosensitivity thresholds. No element flashes
  faster than 2 Hz anywhere in the MVP.
- **Color independence**: there is no color in the design (DMG monochrome), so all
  state is conveyed by **shape and motion** (cursor brackets shrinking on placement,
  blink-rate doubling on invalid tiles, computer glitch dots on damage, distinct titles
  on win vs lose).
- **Keyboard / button discoverability**: every screen that requires input shows a
  literal "PRESS START" prompt. The gameplay screen's controls are intentionally
  minimal (D-pad + A) and match universal Game Boy convention, so no on-screen control
  hint is needed in MVP.

---

## Summary of bound decisions (no TBDs)

- HUD: top, 1 row, format `HP:05  E:030  W:1/3`, redraw on event only.
- Map: 20×17 play field, 4-turn path, 29 path tiles, 307 buildable tiles, 2×2 computer
  at play-field cols 18..19 rows 4..5.
- Palette: 0=white(ground/HUD bg), 1=light grey(tower mid-tone, ghost cursor),
  2=dark grey(path, computer chassis), 3=black(text, enemies, projectile, cursor).
- Sprites: 8×8, 8 unique tiles, ≤11 OAM in flight.
- BG: 115 unique tiles. Total VRAM 123/384.
- Font: GBDK default, ASCII only, no custom glyphs.
- Cursor invalid feedback: shade-1 brackets + 2 Hz blink (double channel).
- B and START are no-ops during MVP gameplay (documented to prevent scope creep).
