# Iter-3 #21 — Animated Tiles & Sprite Polish: Pixel-Art Spec

Legend: `.` = W (0, white), `,` = L (1, light grey), `o` = D (2, dark grey),
`#` = B (3, black). Each grid is 8x8.

All deltas below are layered ON TOP of the existing pristine art produced by
`comp_quadrant()` in `tools/gen_assets.py` (smile preserved through all
corruption states). The coder should add new BG-tile entries to `map_tiles`
named exactly as the headings.

---

## 1. Computer corruption — 4 progressive states

Pristine reference (state 0, already in repo as `TILE_COMP_*`):

```
TL              TR              BL              BR
########        ########        #,,,,,,,        ,,,,,,,#
#ooooooo        ooooooo#        #,,,,,,,        ,,,,,,,#
#o,,,,,,        ,,,,,,o#        #,,,,,,,        ,,,,,,,#
#o,,,,,,        ,,,,,,o#        #,,,,,#,        ,#,,,,,#
#o,,,##,        ,##,,,o#        #,,,,,,,        ,,,,,,,#
#o,,,,,,        ,,,,,,o#        #,,,,,,,        ,,,,,,,#
#o,,,,,,        ,,,,,,o#        #ooooooo        ooooooo#
#o,,,,,,        ,,,,,,o#        ########        ########
```

### State 1 — single stuck pixel
Adds: `TL(5,5) = B`. Other 3 quadrants identical to pristine.

#### TILE_COMP_TL_C1 — single stuck black pixel just below the smile
```
########
#ooooooo
#o,,,,,,
#o,,,,,,
#o,,,##,
#o,,,#,,
#o,,,,,,
#o,,,,,,
```
#### TILE_COMP_TR_C1 — identical to pristine TR
```
########
ooooooo#
,,,,,,o#
,,,,,,o#
,##,,,o#
,,,,,,o#
,,,,,,o#
,,,,,,o#
```
#### TILE_COMP_BL_C1 — identical to pristine BL
```
#,,,,,,,
#,,,,,,,
#,,,,,,,
#,,,,,#,
#,,,,,,,
#,,,,,,,
#ooooooo
########
```
#### TILE_COMP_BR_C1 — identical to pristine BR
```
,,,,,,,#
,,,,,,,#
,,,,,,,#
,#,,,,,#
,,,,,,,#
,,,,,,,#
ooooooo#
########
```

### State 2 — horizontal scanline tear (cumulative on top of state 1)
Adds a continuous black scanline across the upper screen on row 3 of TL+TR
(avoids the smile on row 4).

#### TILE_COMP_TL_C2 — stuck pixel + horizontal tear on row 3
```
########
#ooooooo
#o,,,,,,
#o######
#o,,,##,
#o,,,#,,
#o,,,,,,
#o,,,,,,
```
#### TILE_COMP_TR_C2 — horizontal tear continues across TR row 3
```
########
ooooooo#
,,,,,,o#
######o#
,##,,,o#
,,,,,,o#
,,,,,,o#
,,,,,,o#
```
#### TILE_COMP_BL_C2 — identical to pristine BL
```
#,,,,,,,
#,,,,,,,
#,,,,,,,
#,,,,,#,
#,,,,,,,
#,,,,,,,
#ooooooo
########
```
#### TILE_COMP_BR_C2 — identical to pristine BR
```
,,,,,,,#
,,,,,,,#
,,,,,,,#
,#,,,,,#
,,,,,,,#
,,,,,,,#
ooooooo#
########
```

### State 3 — diagonal crack + missing (white) pixels (cumulative)
Adds a zigzag crack arcing through all four quadrants and 2 white "dead
pixel" holes inside the screen interior.

#### TILE_COMP_TL_C3 — adds dead pixel (2,5)=W and crack pixel (6,4)=B
```
########
#ooooooo
#o,,,.,,
#o######
#o,,,##,
#o,,,#,,
#o,,#,,,
#o,,,,,,
```
#### TILE_COMP_TR_C3 — adds crack pixel (5,4)=B
```
########
ooooooo#
,,,,,,o#
######o#
,##,,,o#
,,,,#,o#
,,,,,,o#
,,,,,,o#
```
#### TILE_COMP_BL_C3 — adds two crack pixels at (1,4) and (2,5)
```
#,,,,,,,
#,,,,#,,
#,,,,,#,
#,,,,,#,
#,,,,,,,
#,,,,,,,
#ooooooo
########
```
#### TILE_COMP_BR_C3 — adds crack pixel (1,3)=B and dead pixel (5,2)=W
```
,,,,,,,#
,,,#,,,#
,,,,,,,#
,#,,,,,#
,,,,,,,#
,,.,,,,#
ooooooo#
########
```

### State 4 — heavy static
**Reuse existing `TILE_COMP_TL_D` / `TILE_COMP_TR_D` / `TILE_COMP_BL_D` /
`TILE_COMP_BR_D`.** No new tiles needed; map state 4 → existing `_D`
indices in the runtime selector.

---

## 2. Bug hit-flash sprite

Recipe: from `SPR_BUG_A`, swap B→L (`#`→`,`) and D→W (`+`→`.`). Keeps the
silhouette but renders it as a bright ghost-outline (no fully-black pixels)
for 3 frames after damage. White pixels stay transparent so the bug doesn't
become an opaque blob.

#### SPR_BUG_FLASH — bright/light silhouette of bug frame A
```
,.,..,.,
.,,,,,,.
.,,..,,.
,,,,,,,,
.,,,,,,.
.,,,,,,.
,.,..,.,
.......,
```

---

## 3. Robot hit-flash sprite

Same recipe applied to `SPR_ROBOT_A`: B→L, D→W.

#### SPR_ROBOT_FLASH — bright/light silhouette of robot frame A
```
....,...
..,,,,..
..,..,..
..,,,,..
.,,,,,,.
.,....,.
.,....,.
.,,..,,.
```

---

## 4. Tower idle frame B (2-frame LED blink)

### TILE_TOWER_B (AV tower, frame B)
Single-pixel diff from `TILE_TOWER` frame A: `(3,4)` toggled `W → L`. Reads
as a small light-grey LED blinking on inside the central window.
```
.,o##o,.
,o####o,
o##oo##o
#o#.,#o#
#o#..#o#
o##oo##o
,o####o,
.,o##o,.
```

### TILE_TOWER_2_B (FW tower, frame B)
Single-pixel diff from `TILE_TOWER_2` frame A: `(3,3)` toggled `L → W`. Reads
as the firewall's ember briefly flaring to its hottest (whitest) value.
```
.######.
#o##o##o
########
o##.##oo
########
#o##o##o
########
.######.
```

---

## Implementation hints (for the coder)

- All 12 new BG tiles (`TILE_COMP_*_C1..C3`, `TILE_TOWER_B`, `TILE_TOWER_2_B`)
  go into `map_tiles` in `tools/gen_assets.py`. State 4 reuses existing
  `_D` entries.
- New sprite tiles `SPR_BUG_FLASH` and `SPR_ROBOT_FLASH` go in the sprite
  block alongside `SPR_BUG_A` / `SPR_ROBOT_A`.
- Runtime corruption mapping suggested:
  `state ∈ {0,1,2,3,4} → {pristine, _C1, _C2, _C3, _D}`.
- Hit-flash duration: 3 frames (~50 ms), then revert to walk-cycle frame A/B.
- Tower idle B frame: alternate with frame A on a slow timer (~1 Hz, e.g.
  every 60 frames) regardless of fire activity.
