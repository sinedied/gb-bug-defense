# Iter-3 #17 — Map Layouts (Map 1, Map 2, Map 3)

Scope: path-topology design for the three selectable maps. Tileset, palette, BG
tile graphics (indices 128–150), wave script, difficulty constants, and HUD are
**unchanged**. The only thing that varies per map is path geometry and computer
cluster position.

All coordinates below are **play-field-local** (origin = top-left of the play
field, i.e. screen row 1). The play field is **20 cols × 17 rows** (PF_COLS=20,
PF_ROWS=17). Screen row 0 is HUD.

Legend for ASCII grids:
- `G` = ground (TC_GROUND, buildable, drawn as TILE_GROUND)
- `P` = path  (TC_PATH, drawn as TILE_PATH)
- `C` = computer cluster cell (TC_COMPUTER, 2×2 cluster of TILE_COMP_*)

For every map, the spawn waypoint `[0]` is `{-1, R}` (off-grid, col -1) and
the path's first on-grid tile is `{0, R}`. The terminal waypoint is the
TC_COMPUTER cell that the path enters (matches MVP convention: enemies "stop"
on that cell to damage the cluster).

---

## Map 1 — "MAP 1" (reference, **frozen** — reproduced from `src/map.c`)

Computer TL: `computer_tx = 18`, `computer_ty = 4`. Cluster cells (18,4),
(19,4), (18,5), (19,5).

```
   col:  0         1
         0123456789012345678901
   r 0   GGGGGGGGGGGGGGGGGGGG
   r 1   GGGGGGGGGGGGGGGGGGGG
   r 2   PPPPPPPPPGGGGGGGGGGG
   r 3   GGGGGGGGPGGGGGGGGGGG
   r 4   GGGGGGGGPGGGGGGGGGCC
   r 5   GGGGGGGGPGGGGGGPPPPC   ← path enters cluster at (18,5)
   r 6   GGGGGGGGPGGGGGGPGGGG
   r 7   GGGGGGGGPGGGGGGPGGGG
   r 8   GGGGGGGGPGGGGGGPGGGG
   r 9   GGGGGGGGPPPPPPPPGGGG
   r 10  GGGGGGGGGGGGGGGGGGGG
   r 11..16  all GGGGGGGGGGGGGGGGGGGG
```

Waypoints (C array literal, 8 entries):

```c
{ -1, 2 }, { 0, 2 }, { 8, 2 }, { 8, 9 },
{ 15, 9 }, { 15, 5 }, { 17, 5 }, { 18, 5 },
```

Tile counts:
- TC_PATH tiles: **29**
- TC_COMPUTER tiles: 4 (the 2×2 cluster; the terminal waypoint (18,5) is one
  of these)
- TC_GROUND (buildable) tiles: 20·17 − 29 − 4 = **307**

Segment trace check (every consecutive waypoint pair is purely H or V):
1. (0,2)→(8,2)  H, 9 cells, all P
2. (8,2)→(8,9)  V, 7 new cells, all P
3. (8,9)→(15,9) H, 7 new cells, all P
4. (15,9)→(15,5) V, 4 new cells, all P
5. (15,5)→(17,5) H, 2 new cells, all P
6. (17,5)→(18,5) H, 1 new cell, that cell is C (TC_COMPUTER) ✓ terminal

---

## Map 2 — "MAP 2" / theme: **Maze** (long, twisty)

Design intent: more turns than Map 1, S/zigzag shape so towers placed on any
single side cover only a slice of the route. Plenty of buildable ground above
the path and below row 8.

Computer TL: `computer_tx = 18`, `computer_ty = 4` (same slot as Map 1 — keeps
`map_render()` simple and lets the cluster animation hit the same screen rows
across all maps).

```
   col:  0         1
         0123456789012345678901
   r 0   GGGGGGGGGGGGGGGGGGGG
   r 1   PPPPPGGGGGGGGGGGGGGG
   r 2   GGGGPGGGGGGGGGGGGGGG
   r 3   GGGGPGGGGGGGGGGGGGGG
   r 4   GGPPPGGGGGGGGGGGGGGG
   r 5   GGPGGGGGGGGPPPPPPPCC
   r 6   GGPGGGGGGGGPGGGGGGCC
   r 7   GGPGGGGGGGGPGGGGGGGG
   r 8   GGPPPPPPPPPPGGGGGGGG
   r 9   GGGGGGGGGGGGGGGGGGGG
   r 10..16  all GGGGGGGGGGGGGGGGGGGG
```

Wait — clarification on row 5/6 around the cluster: the path lands on row 5
heading right and enters the cluster cell (18,5). The cluster occupies (18,4),
(19,4), (18,5), (19,5). So row 4 cols 18–19 are C, row 5 cols 18–19 are C, and
row 5 col 17 is the last pure-P tile. Row 6 cols 18–19 are G again. Corrected
local view of the right edge:

```
   r 4   ...GGGGGGGGGGGGGCC   (cols 18,19 = C)
   r 5   ...PPPPPPPCC         (cols 11..17 = P, 18..19 = C)
   r 6   ...PGGGGGGGG         (col 11 = P, 18..19 = G)
```

Waypoints (C array literal, 10 entries — at the ≤10 budget):

```c
{ -1, 1 }, { 0, 1 }, { 4, 1 }, { 4, 4 },
{ 2, 4 }, { 2, 8 }, { 11, 8 }, { 11, 5 },
{ 17, 5 }, { 18, 5 },
```

Tile counts:
- TC_PATH tiles: **32**
  - row 1 cols 0..4 → 5
  - col 4 rows 2,3,4 → 3
  - row 4 cols 2,3   → 2
  - col 2 rows 5..8  → 4
  - row 8 cols 3..11 → 9
  - col 11 rows 5,6,7 → 3
  - row 5 cols 12..17 → 6
- TC_COMPUTER tiles: 4 (terminal waypoint (18,5) is one of them)
- TC_GROUND (buildable) tiles: 340 − 32 − 4 = **304**

Segment trace check (all H or V; all interior cells are P, terminal cell is C):
1. (0,1)→(4,1)   H, 5 cells P
2. (4,1)→(4,4)   V, 3 new cells P
3. (4,4)→(2,4)   H, 2 new cells P
4. (2,4)→(2,8)   V, 4 new cells P
5. (2,8)→(11,8)  H, 9 new cells P
6. (11,8)→(11,5) V, 3 new cells P
7. (11,5)→(17,5) H, 6 new cells P
8. (17,5)→(18,5) H, 1 new cell C ✓ terminal

No two non-adjacent segments share a cell (verified by inspection: rows
{1,4,5,8} and cols {2,4,11,17} — each shared cell is a waypoint turn).

---

## Map 3 — "MAP 3" / theme: **Sprint** (short, near-straight, one shallow S)

Design intent: short total path → enemies cross the field fast under the same
wave/HP/speed constants → harder to kill them in time. Geometry is a near-
horizontal trunk with one shallow 2-row dip then back up, which gives towers
fewer tiles of "flanking adjacency" than Map 1 or Map 2. Most ground is far
from the path; effective tower coverage hinges on placing in the narrow band
between rows 4 and 8.

Computer TL: `computer_tx = 18`, `computer_ty = 4` (same slot as Maps 1 & 2).

```
   col:  0         1
         0123456789012345678901
   r 0   GGGGGGGGGGGGGGGGGGGG
   r 1   GGGGGGGGGGGGGGGGGGGG
   r 2   GGGGGGGGGGGGGGGGGGGG
   r 3   GGGGGGGGGGGGGGGGGGGG
   r 4   GGGGGGGGGGGGGGGGGGCC
   r 5   PPPPPPPPGGGGGGPPPPCC
   r 6   GGGGGGGPGGGGGGPGGGGG
   r 7   GGGGGGGPPPPPPPPGGGGG
   r 8   GGGGGGGGGGGGGGGGGGGG
   r 9..16  all GGGGGGGGGGGGGGGGGGGG
```

Waypoints (C array literal, 8 entries):

```c
{ -1, 5 }, { 0, 5 }, { 7, 5 }, { 7, 7 },
{ 14, 7 }, { 14, 5 }, { 17, 5 }, { 18, 5 },
```

Tile counts:
- TC_PATH tiles: **22**
  - row 5 cols 0..7   → 8
  - col 7 rows 6,7    → 2
  - row 7 cols 8..14  → 7
  - col 14 rows 6,5   → 2
  - row 5 cols 15..17 → 3
- TC_COMPUTER tiles: 4 (terminal waypoint (18,5) is one of them)
- TC_GROUND (buildable) tiles: 340 − 22 − 4 = **314**

Segment trace check:
1. (0,5)→(7,5)   H, 8 cells P
2. (7,5)→(7,7)   V, 2 new cells P
3. (7,7)→(14,7)  H, 7 new cells P
4. (14,7)→(14,5) V, 2 new cells P
5. (14,5)→(17,5) H, 3 new cells P
6. (17,5)→(18,5) H, 1 new cell C ✓ terminal

---

## Selector labels

10-tile selector widget (`< NAME >`) — same width as the difficulty selector.
Inner label is up to 6 characters.

| Slot | Label | Rationale |
|------|-------|-----------|
| 0 | `MAP 1` | Numeric, unambiguous, sortable. The MVP map is the canonical "first" map. |
| 1 | `MAP 2` | Numeric — keeps the selector visually uniform; player learns that higher number == different layout, not necessarily harder. |
| 2 | `MAP 3` | Same. |

Rationale for **not** using thematic names ("MAZE" / "SPRINT"):
- Thematic names imply difficulty ordering (MAZE sounds harder, SPRINT sounds
  easier — but the *player* finds Sprint harder due to short path).
  Numeric labels avoid pre-loading the player with wrong expectations.
- Uniform `MAP N` width keeps the selector visually balanced; "SPRINT" is 6
  chars and would visually outweigh "MAP 1".
- Internal/spec docs and comments still refer to the themes ("Maze", "Sprint")
  as design intent — only the on-screen label is `MAP N`.

---

## Validation invariants (assert in `tests/test_maps.c`)

For each of the 3 maps, the host test must assert:

1. **Waypoint count ≤ 10.** (Map 1 = 8, Map 2 = 10, Map 3 = 8.)
2. **Spawn waypoint is off-grid:** `wp[0].tx == -1`, and `wp[0].ty == wp[1].ty`,
   and `wp[1].tx == 0` (the path enters the play field at column 0 on the
   spawn row).
3. **All segments orthogonal:** for every consecutive pair `(a, b)`,
   `a.tx == b.tx XOR a.ty == b.ty` (exactly one axis differs).
4. **Segment cells classify correctly:** every cell strictly between `a` and `b`
   on the straight line, plus `a` itself for non-spawn segments, has
   `map_class_at(...) == TC_PATH`. The final waypoint `wp[N-1]` has
   `map_class_at(...) == TC_COMPUTER`.
5. **Path reaches the cluster:** `wp[N-1]` is one of the 4 cells
   `{(cx,cy), (cx+1,cy), (cx,cy+1), (cx+1,cy+1)}` for that map's
   `(computer_tx, computer_ty)`.
6. **Computer fits the play field & avoids HUD:** `0 ≤ computer_tx ≤ 18`,
   `0 ≤ computer_ty ≤ 15` (HUD is screen row 0; play-field-local row 0 is the
   topmost legal row, i.e. `computer_ty >= 0` is enough — there is no HUD
   conflict in PF-local coordinates).
7. **All path tiles are inside the play field:** for every on-grid waypoint,
   `0 ≤ tx ≤ 19` and `0 ≤ ty ≤ 16`.
8. **Cluster cells are TC_COMPUTER**, surrounding cells default to TC_GROUND
   unless explicitly path.

Per-map expected `(path_tile_count, ground_tile_count)`:

| Map | Computer TL | Waypoints | TC_PATH | TC_GROUND |
|-----|-------------|-----------|---------|-----------|
| 1   | (18, 4)     | 8         | 29      | 307       |
| 2   | (18, 4)     | 10        | 32      | 304       |
| 3   | (18, 4)     | 8         | 22      | 314       |

(All three maps keep the computer cluster in the same slot, so
`map_render()` 's hardcoded `(18, sr)..(19, sr+1)` writes remain correct
even before the planned refactor to read `computer_tx/ty` from the active
`map_def`. The refactor is still in scope of #17 but it is not blocked by
geometry.)
