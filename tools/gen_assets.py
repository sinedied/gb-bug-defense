#!/usr/bin/env python3
"""
Bug Defender asset generator.

Emits res/assets.c + res/assets.h containing all tile pixel data,
sprite tile data, and full-screen tilemaps for the title / win / lose
screens. Hand-coded in Python and committed to the repo as plain C
arrays — no png2asset dependency at build time.

DMG tile encoding:
    8 rows x 2 bytes; for each row, byte0 holds the LSB of each pixel's
    color index and byte1 holds the MSB (color 0..3, BGP=0xE4 ->
    lightest..darkest). Bit 7 of each byte = leftmost pixel.
"""
import os, sys

OUT_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__), "..", "res")
os.makedirs(OUT_DIR, exist_ok=True)

# Color shorthands.
W, L, D, B = 0, 1, 2, 3   # White, Light, Dark, Black

def encode(rows):
    """rows is a list of 8 lists of 8 ints in 0..3. Returns 16-byte tile."""
    assert len(rows) == 8, rows
    out = bytearray()
    for r in rows:
        assert len(r) == 8
        lo = hi = 0
        for i, c in enumerate(r):
            bit = 7 - i
            if c & 1: lo |= 1 << bit
            if c & 2: hi |= 1 << bit
        out += bytes([lo, hi])
    return bytes(out)

def blank():
    return [[W]*8 for _ in range(8)]

# ----------------------------------------------------------------------------
# Tiny 5x7 font in an 8x8 cell. Glyphs are rendered as black on white.
# Stored as ASCII art: '#' = black pixel, '.' = white. 5 cols x 7 rows; we add
# 1 col left padding and 1 row top + bottom padding to fit 8x8.
# ----------------------------------------------------------------------------

FONT = {
    ' ': [
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
    ],
    '!': [
        "..#..",
        "..#..",
        "..#..",
        "..#..",
        "..#..",
        ".....",
        "..#..",
    ],
    '(': [
        "...#.",
        "..#..",
        ".#...",
        ".#...",
        ".#...",
        "..#..",
        "...#.",
    ],
    ')': [
        ".#...",
        "..#..",
        "...#.",
        "...#.",
        "...#.",
        "..#..",
        ".#...",
    ],
    ',': [
        ".....",
        ".....",
        ".....",
        ".....",
        "..##.",
        "..#..",
        ".#...",
    ],
    '-': [
        ".....",
        ".....",
        ".....",
        ".###.",
        ".....",
        ".....",
        ".....",
    ],
    '.': [
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
        "..#..",
        "..#..",
    ],
    '/': [
        "....#",
        "...#.",
        "...#.",
        "..#..",
        ".#...",
        ".#...",
        "#....",
    ],
    ':': [
        ".....",
        "..#..",
        "..#..",
        ".....",
        "..#..",
        "..#..",
        ".....",
    ],
    '<': [
        "...#.",
        "..#..",
        ".#...",
        "#....",
        ".#...",
        "..#..",
        "...#.",
    ],
    '>': [
        ".#...",
        "..#..",
        "...#.",
        "....#",
        "...#.",
        "..#..",
        ".#...",
    ],
    '0': [".###.","#...#","#..##","#.#.#","##..#","#...#",".###."],
    '1': ["..#..",".##..","..#..","..#..","..#..","..#..",".###."],
    '2': [".###.","#...#","....#","...#.","..#..",".#...","#####"],
    '3': [".###.","#...#","....#","..##.","....#","#...#",".###."],
    '4': ["...#.","..##.",".#.#.","#..#.","#####","...#.","...#."],
    '5': ["#####","#....","####.","....#","....#","#...#",".###."],
    '6': [".###.","#...#","#....","####.","#...#","#...#",".###."],
    '7': ["#####","....#","...#.","..#..",".#...",".#...",".#..."],
    '8': [".###.","#...#","#...#",".###.","#...#","#...#",".###."],
    '9': [".###.","#...#","#...#",".####","....#","#...#",".###."],
    'A': [".###.","#...#","#...#","#####","#...#","#...#","#...#"],
    'B': ["####.","#...#","#...#","####.","#...#","#...#","####."],
    'C': [".###.","#...#","#....","#....","#....","#...#",".###."],
    'D': ["####.","#...#","#...#","#...#","#...#","#...#","####."],
    'E': ["#####","#....","#....","####.","#....","#....","#####"],
    'F': ["#####","#....","#....","####.","#....","#....","#...."],
    'G': [".###.","#...#","#....","#..##","#...#","#...#",".###."],
    'H': ["#...#","#...#","#...#","#####","#...#","#...#","#...#"],
    'I': [".###.","..#..","..#..","..#..","..#..","..#..",".###."],
    'J': ["..###","...#.","...#.","...#.","...#.","#..#.",".##.."],
    'K': ["#...#","#..#.","#.#..","##...","#.#..","#..#.","#...#"],
    'L': ["#....","#....","#....","#....","#....","#....","#####"],
    'M': ["#...#","##.##","#.#.#","#.#.#","#...#","#...#","#...#"],
    'N': ["#...#","##..#","##..#","#.#.#","#..##","#..##","#...#"],
    'O': [".###.","#...#","#...#","#...#","#...#","#...#",".###."],
    'P': ["####.","#...#","#...#","####.","#....","#....","#...."],
    'Q': [".###.","#...#","#...#","#...#","#.#.#","#..#.",".##.#"],
    'R': ["####.","#...#","#...#","####.","#.#..","#..#.","#...#"],
    'S': [".####","#....","#....",".###.","....#","....#","####."],
    'T': ["#####","..#..","..#..","..#..","..#..","..#..","..#.."],
    'U': ["#...#","#...#","#...#","#...#","#...#","#...#",".###."],
    'V': ["#...#","#...#","#...#","#...#","#...#",".#.#.","..#.."],
    'W': ["#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"],
    'X': ["#...#","#...#",".#.#.","..#..",".#.#.","#...#","#...#"],
    'Y': ["#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."],
    'Z': ["#####","....#","...#.","..#..",".#...","#....","#####"],
    '_': [
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
        "#####",
    ],
}

def glyph_to_tile(g):
    rows = []
    rows.append([W]*8)  # top padding
    for r in g:
        out = [W]  # left padding
        for ch in r:
            out.append(B if ch == '#' else W)
        out.extend([W] * (8 - len(out)))
        rows.append(out)
    while len(rows) < 8:
        rows.append([W]*8)
    return encode(rows)

# Build the BG tile bank: tiles 0..127 are font slots (ASCII direct-indexed).
font_tiles = [bytes(16)] * 128   # default = blank
for ch, glyph in FONT.items():
    font_tiles[ord(ch)] = glyph_to_tile(glyph)

# ----------------------------------------------------------------------------
# Map tiles, indexed starting at MAP_TILE_BASE (= 128).
# ----------------------------------------------------------------------------
MAP_TILE_BASE = 128

# Ground: white with a single light-grey dot in a 2-tile checker for texture.
ground_a = [[W]*8 for _ in range(8)]
ground_a[0][0] = L
ground_a[4][4] = L
GROUND = encode(ground_a)

# Path: solid dark grey with a black dotted seam on row 7 to suggest a wire.
path_rows = [[D]*8 for _ in range(8)]
for x in (1, 4, 7):
    path_rows[7][x] = B
PATH = encode(path_rows)

# Computer goal: 2x2. Black bezel + light-grey screen with a dark-grey bar.
def comp_quadrant(tl):
    """Generate the 2x2 computer tile for the given quadrant (tl, tr, bl, br)."""
    rows = [[W]*8 for _ in range(8)]
    for r in range(8):
        for c in range(8):
            # Outer bezel (black) on the outward edges.
            outer_top    = (r == 0)
            outer_bot    = (r == 7)
            outer_left   = (c == 0)
            outer_right  = (c == 7)
            if tl == 'TL':
                bezel = outer_top or outer_left
            elif tl == 'TR':
                bezel = outer_top or outer_right
            elif tl == 'BL':
                bezel = outer_bot or outer_left
            else:  # BR
                bezel = outer_bot or outer_right
            if bezel:
                rows[r][c] = B
                continue
            # Inner ring (dark grey) one pixel inward.
            inner = (
                (tl in ('TL','TR') and r == 1) or
                (tl in ('BL','BR') and r == 6) or
                (tl in ('TL','BL') and c == 1) or
                (tl in ('TR','BR') and c == 6)
            )
            if inner:
                rows[r][c] = D
                continue
            # Screen interior (light grey) — leave white.
            rows[r][c] = L
    # Add a "smile" in the screen center across the 4 tiles.
    if tl == 'TL':
        rows[4][6] = B; rows[4][5] = B
    if tl == 'TR':
        rows[4][1] = B; rows[4][2] = B
    if tl == 'BL':
        rows[3][6] = B
    if tl == 'BR':
        rows[3][1] = B
    return encode(rows)

COMP_TL = comp_quadrant('TL')
COMP_TR = comp_quadrant('TR')
COMP_BL = comp_quadrant('BL')
COMP_BR = comp_quadrant('BR')

# Damaged variants: same shape with extra black "glitch" dots scattered on the
# screen interior.
def comp_damaged(quadrant_idx):
    base = [COMP_TL, COMP_TR, COMP_BL, COMP_BR][quadrant_idx]
    # decode then re-encode is heavy; cheaper: regenerate the rows.
    raw = comp_quadrant(['TL','TR','BL','BR'][quadrant_idx])
    # Re-derive rows by re-running the generator (without final smile)
    rows = [[W]*8 for _ in range(8)]
    name = ['TL','TR','BL','BR'][quadrant_idx]
    for r in range(8):
        for c in range(8):
            if name == 'TL': bezel = (r == 0) or (c == 0)
            elif name == 'TR': bezel = (r == 0) or (c == 7)
            elif name == 'BL': bezel = (r == 7) or (c == 0)
            else: bezel = (r == 7) or (c == 7)
            if bezel:
                rows[r][c] = B; continue
            inner = (
                (name in ('TL','TR') and r == 1) or
                (name in ('BL','BR') and r == 6) or
                (name in ('TL','BL') and c == 1) or
                (name in ('TR','BR') and c == 6)
            )
            rows[r][c] = D if inner else L
    # Glitch dots
    for (r, c) in [(2,3),(3,5),(5,2),(6,4)]:
        if 1 <= r <= 6 and 1 <= c <= 6:
            rows[r][c] = B
    return encode(rows)

COMP_TL_D = comp_damaged(0)
COMP_TR_D = comp_damaged(1)
COMP_BL_D = comp_damaged(2)
COMP_BR_D = comp_damaged(3)

# Tower as a BG tile (towers render on the BG layer to side-step the
# 10-sprites-per-scanline DMG limit). Pixel art mirrors SPR_TOWER but
# uses '.' = ground-grey-equivalent (white here so it tiles cleanly with
# TILE_GROUND).
def tower_bg_tile():
    art = [
        ".%+##+%.",
        "%+####+%",
        "+##++##+",
        "#+#..#+#",
        "#+#..#+#",
        "+##++##+",
        "%+####+%",
        ".%+##+%.",
    ]
    rows = []
    for line in art:
        r = []
        for ch in line:
            if   ch == '.': r.append(W)
            elif ch == '%': r.append(L)
            elif ch == '+': r.append(D)
            elif ch == '#': r.append(B)
            else: raise ValueError(ch)
        rows.append(r)
    return encode(rows)

TOWER_BG = tower_bg_tile()

# Iter-2: Firewall tower BG tile (TILE_TOWER_2). Brick wall pattern with
# a single light-grey "ember" pixel hinting at the fire theme.
def tower2_bg_tile():
    art = [
        ".######.",
        "#+##+##+",
        "########",
        "+##%##++",
        "########",
        "#+##+##+",
        "########",
        ".######.",
    ]
    rows = []
    for line in art:
        r = []
        for ch in line:
            if   ch == '.': r.append(W)
            elif ch == '%': r.append(L)
            elif ch == '+': r.append(D)
            elif ch == '#': r.append(B)
            else: raise ValueError(ch)
        rows.append(r)
    return encode(rows)

TOWER2_BG = tower2_bg_tile()

# ----------------------------------------------------------------------------
# Iter-3 #21: animated/corruption BG tiles. The design doc
# (specs/iter3-21-animations-design.md) provides ASCII grids using:
#   '.' = W (white)
#   ',' = L (light grey)
#   'o' = D (dark grey)
#   '#' = B (black)
# Translate verbatim — no creative re-interpretation.
# ----------------------------------------------------------------------------

def design_tile(art):
    """8x8 tile from design-doc ASCII (.=W, ,=L, o=D, #=B)."""
    rows = []
    for line in art:
        if len(line) != 8:
            raise ValueError("design row must be 8 chars: %r" % line)
        r = []
        for ch in line:
            if   ch == '.': r.append(W)
            elif ch == ',': r.append(L)
            elif ch == 'o': r.append(D)
            elif ch == '#': r.append(B)
            else: raise ValueError("design char: %r" % ch)
        rows.append(r)
    if len(rows) != 8:
        raise ValueError("design tile must be 8 rows")
    return encode(rows)

# Computer corruption tiles -- 7 new BG tiles, per design doc §1.
COMP_TL_C1 = design_tile([
    "########",
    "#ooooooo",
    "#o,,,,,,",
    "#o,,,,,,",
    "#o,,,##,",
    "#o,,,#,,",
    "#o,,,,,,",
    "#o,,,,,,",
])
COMP_TL_C2 = design_tile([
    "########",
    "#ooooooo",
    "#o,,,,,,",
    "#o######",
    "#o,,,##,",
    "#o,,,#,,",
    "#o,,,,,,",
    "#o,,,,,,",
])
COMP_TR_C2 = design_tile([
    "########",
    "ooooooo#",
    ",,,,,,o#",
    "######o#",
    ",##,,,o#",
    ",,,,,,o#",
    ",,,,,,o#",
    ",,,,,,o#",
])
COMP_TL_C3 = design_tile([
    "########",
    "#ooooooo",
    "#o,,,.,,",
    "#o######",
    "#o,,,##,",
    "#o,,,#,,",
    "#o,,#,,,",
    "#o,,,,,,",
])
COMP_TR_C3 = design_tile([
    "########",
    "ooooooo#",
    ",,,,,,o#",
    "######o#",
    ",##,,,o#",
    ",,,,#,o#",
    ",,,,,,o#",
    ",,,,,,o#",
])
COMP_BL_C3 = design_tile([
    "#,,,,,,,",
    "#,,,,#,,",
    "#,,,,,#,",
    "#,,,,,#,",
    "#,,,,,,,",
    "#,,,,,,,",
    "#ooooooo",
    "########",
])
COMP_BR_C3 = design_tile([
    ",,,,,,,#",
    ",,,#,,,#",
    ",,,,,,,#",
    ",#,,,,,#",
    ",,,,,,,#",
    ",,.,,,,#",
    "ooooooo#",
    "########",
])

# Tower idle frame B tiles -- 1-pixel diff from frame A, per design doc §4.
TOWER_BG_B = design_tile([
    ".,o##o,.",
    ",o####o,",
    "o##oo##o",
    "#o#.,#o#",
    "#o#..#o#",
    "o##oo##o",
    ",o####o,",
    ".,o##o,.",
])
TOWER2_BG_B = design_tile([
    ".######.",
    "#o##o##o",
    "########",
    "o##.##oo",
    "########",
    "#o##o##o",
    "########",
    ".######.",
])


# Iter-3 #18: EMP tower BG tiles. Concentric-ring motif suggests a
# Tesla coil / capacitor — distinct from satellite dish (AV) and brick
# wall (FW). TOWER_3_B has a single-pixel LED diff at row 3 col 3
# ('.' → ',') matching the iter-3 #21 idle-blink convention.
TOWER3_BG = design_tile([
    "..o##o..",
    ".o####o.",
    "o##oo##o",
    "##o..o##",
    "##o..o##",
    "o##oo##o",
    ".o####o.",
    "..o##o..",
])
TOWER3_BG_B = design_tile([
    "..o##o..",
    ".o####o.",
    "o##oo##o",
    "##o,.o##",
    "##o..o##",
    "o##oo##o",
    ".o####o.",
    "..o##o..",
])

# Iter-4 #26: L1 (upgraded) tower BG tiles.
# Design: L0 identity preserved + filled corners + denser/brighter center.
# Each alt has a 1-pixel LED diff matching the L0 blink convention.

TOWER_L1_BG = design_tile([
    "#,o##o,#",
    ",o####o,",
    "o##oo##o",
    "#o#,,#o#",
    "#o#..#o#",
    "o##oo##o",
    ",o####o,",
    "#,o##o,#",
])

TOWER_L1_BG_B = design_tile([
    "#,o##o,#",
    ",o####o,",
    "o##oo##o",
    "#o#,.#o#",
    "#o#..#o#",
    "o##oo##o",
    ",o####o,",
    "#,o##o,#",
])

TOWER2_L1_BG = design_tile([
    "########",
    "#o##o##o",
    "########",
    "o##,,#oo",
    "########",
    "#o##o##o",
    "########",
    "########",
])

TOWER2_L1_BG_B = design_tile([
    "########",
    "#o##o##o",
    "########",
    "o##.,#oo",
    "########",
    "#o##o##o",
    "########",
    "########",
])

TOWER3_L1_BG = design_tile([
    ".,o##o.,",
    ".o####o.",
    "o######o",
    "##o,,o##",
    "##o,,o##",
    "o######o",
    ".o####o.",
    ".,o##o.,",
])

TOWER3_L1_BG_B = design_tile([
    ".,o##o.,",
    ".o####o.",
    "o######o",
    "##o.,o##",
    "##o,,o##",
    "o######o",
    ".o####o.",
    ".,o##o.,",
])

# Iter-4 #27: L2 (fully upgraded) tower BG tiles.
# Design: L0 identity preserved + outer ring darkened + center filled.
# Each alt has a 1-pixel LED diff matching the L0/L1 blink convention.

TOWER_L2_BG = design_tile([
    "#oo##oo#",
    "oo####oo",
    "o##oo##o",
    "#o#,,#o#",
    "#o#,,#o#",
    "o##oo##o",
    "oo####oo",
    "#oo##oo#",
])

TOWER_L2_BG_B = design_tile([
    "#oo##oo#",
    "oo####oo",
    "o##oo##o",
    "#o#,.#o#",
    "#o#,,#o#",
    "o##oo##o",
    "oo####oo",
    "#oo##oo#",
])

TOWER2_L2_BG = design_tile([
    "########",
    "#o##o##o",
    "########",
    "o##,,,oo",
    "########",
    "##o##o##",
    "########",
    "########",
])

TOWER2_L2_BG_B = design_tile([
    "########",
    "#o##o##o",
    "########",
    "o##.,,oo",
    "########",
    "##o##o##",
    "########",
    "########",
])

TOWER3_L2_BG = design_tile([
    ",oo##oo,",
    ",o####o,",
    "o######o",
    "##o,,o##",
    "##o,,o##",
    "o######o",
    ",o####o,",
    ",oo##oo,",
])

TOWER3_L2_BG_B = design_tile([
    ",oo##oo,",
    ",o####o,",
    "o######o",
    "##o.,o##",
    "##o,,o##",
    "o######o",
    ",o####o,",
    ",oo##oo,",
])

# Iter-4 #28: Title screen art tiles (17 tiles).
TITLE_B_TL = design_tile([
    "........",
    ".#######",
    ".#######",
    ".###....",
    ".###....",
    ".#######",
    ".#######",
    ".###....",
])

TITLE_B_TR = design_tile([
    "........",
    "####....",
    "#####...",
    "..###...",
    "..###...",
    "####....",
    "#####...",
    "..###...",
])

TITLE_B_BL = design_tile([
    ".###....",
    ".###....",
    ".#######",
    ".#######",
    "........",
    "........",
    "........",
    "........",
])

TITLE_B_BR = design_tile([
    "..###...",
    "..###...",
    "#####...",
    "####....",
    "........",
    "........",
    "........",
    "........",
])

TITLE_U_T = design_tile([
    "........",
    ".###....",
    ".###....",
    ".###....",
    ".###....",
    ".###....",
    ".###....",
    ".###....",
])

TITLE_U_BL = design_tile([
    ".###....",
    ".####...",
    "..######",
    "...#####",
    "........",
    "........",
    "........",
    "........",
])

TITLE_U_BR = design_tile([
    ".###....",
    "####....",
    "###.....",
    "##......",
    "........",
    "........",
    "........",
    "........",
])

TITLE_G_TL = design_tile([
    "........",
    "....####",
    "...#####",
    "..###...",
    ".###....",
    ".###....",
    ".###....",
    ".###....",
])

TITLE_G_TR = design_tile([
    "........",
    "###.....",
    "####....",
    "..###...",
    "........",
    "........",
    "####....",
    "####....",
])

TITLE_G_BL = design_tile([
    ".###....",
    "..###...",
    "...#####",
    "....####",
    "........",
    "........",
    "........",
    "........",
])

TITLE_G_BR = design_tile([
    "..###...",
    "..###...",
    "####....",
    "###.....",
    "........",
    "........",
    "........",
    "........",
])

TITLE_HLINE = design_tile([
    "........",
    "........",
    "........",
    "oooooooo",
    "oooooooo",
    "........",
    "........",
    "........",
])

TITLE_NODE = design_tile([
    "........",
    "...oo...",
    "..oooo..",
    "oooooooo",
    "oooooooo",
    "..oooo..",
    "...oo...",
    "........",
])

TITLE_BUG_T = design_tile([
    "..#..#..",
    "...##...",
    "..####..",
    ".##..##.",
    "########",
    ".##..##.",
    "#.####.#",
    ".######.",
])

TITLE_BUG_B = design_tile([
    ".#.##.#.",
    "########",
    ".##..##.",
    "..####..",
    "...##...",
    "........",
    "........",
    "........",
])

TITLE_TW_T = design_tile([
    "...##...",
    "..####..",
    ".######.",
    ".#.##.#.",
    "...##...",
    "..####..",
    ".#....#.",
    ".######.",
])

TITLE_TW_B = design_tile([
    ".######.",
    ".##..##.",
    "..####..",
    "..####..",
    ".######.",
    ".######.",
    "........",
    "........",
])

# Iter-5: Arrow tile for title menu selector focus indicator.
TILE_ARROW_ART = design_tile([
    "........",
    ".##.....",
    ".####...",
    ".#####..",
    ".#####..",
    ".####...",
    ".##.....",
    "........",
])

# Iter-6: End-screen art tiles.
ENDSCR_STATIC = design_tile([
    "o#,o##.#",
    "#oo#.o#o",
    ".#o#o##,",
    "o#,##o.#",
    "##o.#o#o",
    "#.o#o#,#",
    "o##,o#o.",
    "#o#.#oo#",
])

ENDSCR_BROKEN_HL = design_tile([
    "........",
    ".....o..",
    "........",
    "oo..oo.o",
    "o.oo..oo",
    "........",
    "..o.....",
    "........",
])

ENDSCR_SKULL_T = design_tile([
    "..####..",
    ".######.",
    "##.##.##",
    "########",
    "########",
    ".##..##.",
    "..####..",
    ".##..##.",
])

ENDSCR_SKULL_B = design_tile([
    "..####..",
    "...##...",
    "...##...",
    "........",
    "........",
    "........",
    "........",
    "........",
])

ENDSCR_SHIELD_T = design_tile([
    ".######.",
    "########",
    "########",
    "########",
    "########",
    "########",
    ".######.",
    "..####..",
])

ENDSCR_SHIELD_B = design_tile([
    "...##...",
    "...##...",
    "...##...",
    "........",
    "........",
    "........",
    "........",
    "........",
])

map_tiles = [
    ('TILE_GROUND',   GROUND),
    ('TILE_PATH',     PATH),
    ('TILE_COMP_TL',  COMP_TL),
    ('TILE_COMP_TR',  COMP_TR),
    ('TILE_COMP_BL',  COMP_BL),
    ('TILE_COMP_BR',  COMP_BR),
    ('TILE_COMP_TL_D', COMP_TL_D),
    ('TILE_COMP_TR_D', COMP_TR_D),
    ('TILE_COMP_BL_D', COMP_BL_D),
    ('TILE_COMP_BR_D', COMP_BR_D),
    ('TILE_TOWER',    TOWER_BG),
    ('TILE_TOWER_2',  TOWER2_BG),
    # Iter-3 #21: corruption + tower idle B frames.
    ('TILE_COMP_TL_C1', COMP_TL_C1),
    ('TILE_COMP_TL_C2', COMP_TL_C2),
    ('TILE_COMP_TR_C2', COMP_TR_C2),
    ('TILE_COMP_TL_C3', COMP_TL_C3),
    ('TILE_COMP_TR_C3', COMP_TR_C3),
    ('TILE_COMP_BL_C3', COMP_BL_C3),
    ('TILE_COMP_BR_C3', COMP_BR_C3),
    ('TILE_TOWER_B',    TOWER_BG_B),
    ('TILE_TOWER_2_B',  TOWER2_BG_B),
    # Iter-3 #18: EMP tower base + idle blink.
    ('TILE_TOWER_3',    TOWER3_BG),
    ('TILE_TOWER_3_B',  TOWER3_BG_B),
    # Iter-4 #26: L1 tower tiles.
    ('TILE_TOWER_L1',      TOWER_L1_BG),
    ('TILE_TOWER_L1_B',    TOWER_L1_BG_B),
    ('TILE_TOWER_2_L1',    TOWER2_L1_BG),
    ('TILE_TOWER_2_L1_B',  TOWER2_L1_BG_B),
    ('TILE_TOWER_3_L1',    TOWER3_L1_BG),
    ('TILE_TOWER_3_L1_B',  TOWER3_L1_BG_B),
    # Iter-4 #27: L2 tower tiles.
    ('TILE_TOWER_L2',      TOWER_L2_BG),
    ('TILE_TOWER_L2_B',    TOWER_L2_BG_B),
    ('TILE_TOWER_2_L2',    TOWER2_L2_BG),
    ('TILE_TOWER_2_L2_B',  TOWER2_L2_BG_B),
    ('TILE_TOWER_3_L2',    TOWER3_L2_BG),
    ('TILE_TOWER_3_L2_B',  TOWER3_L2_BG_B),
    # Iter-4 #28: Title screen art tiles.
    ('TILE_TITLE_B_TL',  TITLE_B_TL),
    ('TILE_TITLE_B_TR',  TITLE_B_TR),
    ('TILE_TITLE_B_BL',  TITLE_B_BL),
    ('TILE_TITLE_B_BR',  TITLE_B_BR),
    ('TILE_TITLE_U_T',   TITLE_U_T),
    ('TILE_TITLE_U_BL',  TITLE_U_BL),
    ('TILE_TITLE_U_BR',  TITLE_U_BR),
    ('TILE_TITLE_G_TL',  TITLE_G_TL),
    ('TILE_TITLE_G_TR',  TITLE_G_TR),
    ('TILE_TITLE_G_BL',  TITLE_G_BL),
    ('TILE_TITLE_G_BR',  TITLE_G_BR),
    ('TILE_TITLE_HLINE', TITLE_HLINE),
    ('TILE_TITLE_NODE',  TITLE_NODE),
    ('TILE_TITLE_BUG_T', TITLE_BUG_T),
    ('TILE_TITLE_BUG_B', TITLE_BUG_B),
    ('TILE_TITLE_TW_T',  TITLE_TW_T),
    ('TILE_TITLE_TW_B',  TITLE_TW_B),
    # Iter-5: Arrow tile for title selector focus.
    ('TILE_ARROW',       TILE_ARROW_ART),
    # Iter-6: End-screen art tiles.
    ('TILE_ENDSCR_STATIC',    ENDSCR_STATIC),
    ('TILE_ENDSCR_BROKEN_HL', ENDSCR_BROKEN_HL),
    ('TILE_ENDSCR_SKULL_T',   ENDSCR_SKULL_T),
    ('TILE_ENDSCR_SKULL_B',   ENDSCR_SKULL_B),
    ('TILE_ENDSCR_SHIELD_T',  ENDSCR_SHIELD_T),
    ('TILE_ENDSCR_SHIELD_B',  ENDSCR_SHIELD_B),
]

# ----------------------------------------------------------------------------
# Sprite tiles. Separate VRAM bank in GBDK (set_sprite_data writes to 0x8000
# unsigned region). Sprite tile indices start at 0.
# ----------------------------------------------------------------------------

def sprite_from_art(art):
    """8 rows of 8 chars: '.' '#' for white/black, plus '%' light, '+' dark."""
    rows = []
    for line in art:
        r = []
        for ch in line:
            if   ch == '.': r.append(W)
            elif ch == '%': r.append(L)
            elif ch == '+': r.append(D)
            elif ch == '#': r.append(B)
            else: raise ValueError(ch)
        rows.append(r)
    return encode(rows)

# Cursor frame A (valid): black corner brackets.
SPR_CURSOR_A = sprite_from_art([
    "##....##",
    "#.......",
    "........",
    "........",
    "........",
    "........",
    "#.......",
    "##....##",
])
# Cursor frame B: lighter corners (1 Hz blink off-phase).
SPR_CURSOR_B = sprite_from_art([
    "%%....%%",
    "%.......",
    "........",
    "........",
    "........",
    "........",
    "%.......",
    "%%....%%",
])
# Ghost cursor (invalid): all light grey.
SPR_CURSOR_GA = sprite_from_art([
    "%%....%%",
    "%.......",
    "........",
    "........",
    "........",
    "........",
    "%.......",
    "%%....%%",
])
SPR_CURSOR_GB = sprite_from_art([
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
])
# Tower: octagonal shielded turret.
SPR_TOWER = sprite_from_art([
    ".%+##+%.",
    "%+####+%",
    "+##++##+",
    "#+#..#+#",
    "#+#..#+#",
    "+##++##+",
    "%+####+%",
    ".%+##+%.",
])
# Bug walk frame A.
SPR_BUG_A = sprite_from_art([
    "#.#..#.#",
    ".######.",
    ".##++##.",
    "########",
    ".######.",
    ".######.",
    "#.#..#.#",
    ".......#",
])
SPR_BUG_B = sprite_from_art([
    ".#.##.#.",
    ".######.",
    ".##++##.",
    "########",
    ".######.",
    ".######.",
    ".#.##.#.",
    "#.......",
])
SPR_PROJ = sprite_from_art([
    "........",
    "........",
    "...#....",
    "..###...",
    "...#....",
    "........",
    "........",
    "........",
])

# Iter-2: Robot agent — vertical humanoid silhouette, distinct from the
# horizontal centipede SPR_BUG_*. Two walk frames differ only in row 7
# (legs apart vs. feet together).
SPR_ROBOT_A = sprite_from_art([
    "....#...",
    "..####..",
    "..#..#..",
    "..####..",
    ".######.",
    ".#++++#.",
    ".#++++#.",
    ".##..##.",
])
SPR_ROBOT_B = sprite_from_art([
    "....#...",
    "..####..",
    "..#..#..",
    "..####..",
    ".######.",
    ".#++++#.",
    ".#++++#.",
    "...##...",
])

# Iter-2: Sprite-bank glyph mirror of FONT entries used by the upgrade/sell
# menu. Pixel-identical with the BG HUD font (same FONT dict, same insets).
def glyph_to_sprite(ch):
    return glyph_to_tile(FONT[ch])

SPR_GLYPH_U     = glyph_to_sprite('U')
SPR_GLYPH_P     = glyph_to_sprite('P')
SPR_GLYPH_G     = glyph_to_sprite('G')
SPR_GLYPH_S     = glyph_to_sprite('S')
SPR_GLYPH_E     = glyph_to_sprite('E')
SPR_GLYPH_L     = glyph_to_sprite('L')
SPR_GLYPH_COLON = glyph_to_sprite(':')
# '>' lives in the FONT dict literal above (subtask 1 of iter-3 #20 — was a
# late assignment here, but that left BG tile 62 blank and only the sprite
# bank populated; consolidating into the dict literal keeps BG and sprite
# in lockstep).
SPR_GLYPH_GT    = glyph_to_sprite('>')
SPR_GLYPH_DASH  = glyph_to_sprite('-')
SPR_GLYPH_0     = glyph_to_sprite('0')
SPR_GLYPH_1     = glyph_to_sprite('1')
SPR_GLYPH_2     = glyph_to_sprite('2')
SPR_GLYPH_3     = glyph_to_sprite('3')
SPR_GLYPH_4     = glyph_to_sprite('4')
SPR_GLYPH_5     = glyph_to_sprite('5')
SPR_GLYPH_6     = glyph_to_sprite('6')
SPR_GLYPH_7     = glyph_to_sprite('7')
SPR_GLYPH_8     = glyph_to_sprite('8')
SPR_GLYPH_9     = glyph_to_sprite('9')
# Iter-3 #22 (pause menu): additional letters for PAUSE / RESUME / QUIT.
# Pixel-identical mirrors of the FONT[] entries so they match HUD letters.
SPR_GLYPH_A     = glyph_to_sprite('A')
SPR_GLYPH_R     = glyph_to_sprite('R')
SPR_GLYPH_M     = glyph_to_sprite('M')
SPR_GLYPH_Q     = glyph_to_sprite('Q')
SPR_GLYPH_I     = glyph_to_sprite('I')
SPR_GLYPH_T     = glyph_to_sprite('T')

# Iter-3 #21: hit-flash sprite variants (3-frame override on non-killing
# damage). Per design doc §2/§3 the recipe is "swap B->L, D->W" applied
# to the A walk frame so the sprite reads as a bright/light silhouette
# while keeping its outline. Designed as a verbatim translation of the
# design grids (chars: '.'=W, ','=L, 'o'=D, '#'=B).
SPR_BUG_FLASH = design_tile([
    ",.,..,.,",
    ".,,,,,,.",
    ".,,..,,.",
    ",,,,,,,,",
    ".,,,,,,.",
    ".,,,,,,.",
    ",.,..,.,",
    ".......,",
])
SPR_ROBOT_FLASH = design_tile([
    "....,...",
    "..,,,,..",
    "..,..,..",
    "..,,,,..",
    ".,,,,,,.",
    ".,....,.",
    ".,....,.",
    ".,,..,,.",
])

# Iter-3 #18: Armored bug walk frames. Visually heavier than BUG with a
# crosshatched metal carapace (#/+ alternating). A/B differ by leg row
# (same 32-frame walk cadence as bug/robot).
SPR_ARMORED_A = sprite_from_art([
    "##.##.##",
    ".######.",
    "########",
    "#+#++#+#",
    "########",
    "#+#++#+#",
    ".######.",
    "#.#..#.#",
])
SPR_ARMORED_B = sprite_from_art([
    "##.##.##",
    ".######.",
    "########",
    "#+#++#+#",
    "########",
    "#+#++#+#",
    ".######.",
    ".#.##.#.",
])

# Iter-3 #18: Armored flash (B→L, D→W inversion of ARMORED_A).
SPR_ARMORED_FLASH = design_tile([
    ",,.,,.,,",
    ".,,,,,,,",
    ",,,,,,,,",
    ",.,..,.,",
    ",,,,,,,,",
    ",.,..,.,",
    ".,,,,,,,",
    ",.,..,.,",
])

# Iter-3 #18: Stun overlay sprites. Same outline as walk-A frame with
# light-grey "shock" bands replacing some body pixels so the enemy
# reads as frozen at a glance. Uses design_tile (.=W, ,=L, o=D, #=B).
SPR_BUG_STUN = design_tile([
    "#,#..#,#",
    ".######.",
    ".##oo##.",
    "#,####,#",
    ".######.",
    ".#,##,#.",
    "#.#..#.#",
    ".......#",
])
SPR_ROBOT_STUN = design_tile([
    "....#...",
    "..####..",
    "..#..#..",
    "..#,,#..",
    ".######.",
    ".#,oo,#.",
    ".#oooo#.",
    ".##..##.",
])
SPR_ARMORED_STUN = design_tile([
    "##.##.##",
    ".######.",
    "########",
    "#,#oo#,#",
    "########",
    "#,#oo#,#",
    ".######.",
    "#.#..#.#",
])

# Iter-7: Boss enemy walk frames. 3-point crowned mega-bug, 87.5% fill.
# A/B differ by leg row (same 32-frame walk cadence as other enemies).
SPR_BOSS_A = sprite_from_art([
    "#.####.#",
    "########",
    "#+####+#",
    "########",
    "###++###",
    "########",
    ".######.",
    "##....##",
])
SPR_BOSS_B = sprite_from_art([
    "#.####.#",
    "########",
    "#+####+#",
    "########",
    "###++###",
    "########",
    ".######.",
    ".##..##.",
])

# Iter-7: Boss flash (B→L, D→W per convention).
SPR_BOSS_FLASH = design_tile([
    ",.,,,,.,",
    ",,,,,,,,",
    ",.,,,,.,",
    ",,,,,,,,",
    ",,,..,,,",
    ",,,,,,,,",
    ".,,,,,,.",
    ",,....,,",
])

# Iter-7: Boss stun (spark gaps at rows 3,5).
SPR_BOSS_STUN = design_tile([
    "#.####.#",
    "########",
    "#,####,#",
    "#.####.#",
    "###oo###",
    "#.####.#",
    ".######.",
    "##....##",
])

# Iter-7: Boss HP bar tiles. 2-pixel-tall bar centered vertically.
# Shade 3 (black '#') for filled, shade 1 (light grey ',') for empty.
SPR_BOSS_BAR_1 = design_tile([
    "........",
    "........",
    "........",
    "##,,,,,,",
    "##,,,,,,",
    "........",
    "........",
    "........",
])
SPR_BOSS_BAR_2 = design_tile([
    "........",
    "........",
    "........",
    "####,,,,",
    "####,,,,",
    "........",
    "........",
    "........",
])
SPR_BOSS_BAR_3 = design_tile([
    "........",
    "........",
    "........",
    "######,,",
    "######,,",
    "........",
    "........",
    "........",
])
SPR_BOSS_BAR_4 = design_tile([
    "........",
    "........",
    "........",
    "########",
    "########",
    "........",
    "........",
    "........",
])

sprite_tiles = [
    ('SPR_CURSOR_A',    SPR_CURSOR_A),
    ('SPR_CURSOR_B',    SPR_CURSOR_B),
    ('SPR_CURSOR_GA',   SPR_CURSOR_GA),
    ('SPR_CURSOR_GB',   SPR_CURSOR_GB),
    ('SPR_TOWER',       SPR_TOWER),
    ('SPR_BUG_A',       SPR_BUG_A),
    ('SPR_BUG_B',       SPR_BUG_B),
    ('SPR_PROJ',        SPR_PROJ),
    ('SPR_ROBOT_A',     SPR_ROBOT_A),
    ('SPR_ROBOT_B',     SPR_ROBOT_B),
    ('SPR_GLYPH_U',     SPR_GLYPH_U),
    ('SPR_GLYPH_P',     SPR_GLYPH_P),
    ('SPR_GLYPH_G',     SPR_GLYPH_G),
    ('SPR_GLYPH_S',     SPR_GLYPH_S),
    ('SPR_GLYPH_E',     SPR_GLYPH_E),
    ('SPR_GLYPH_L',     SPR_GLYPH_L),
    ('SPR_GLYPH_COLON', SPR_GLYPH_COLON),
    ('SPR_GLYPH_GT',    SPR_GLYPH_GT),
    ('SPR_GLYPH_DASH',  SPR_GLYPH_DASH),
    ('SPR_GLYPH_0',     SPR_GLYPH_0),
    ('SPR_GLYPH_1',     SPR_GLYPH_1),
    ('SPR_GLYPH_2',     SPR_GLYPH_2),
    ('SPR_GLYPH_3',     SPR_GLYPH_3),
    ('SPR_GLYPH_4',     SPR_GLYPH_4),
    ('SPR_GLYPH_5',     SPR_GLYPH_5),
    ('SPR_GLYPH_6',     SPR_GLYPH_6),
    ('SPR_GLYPH_7',     SPR_GLYPH_7),
    ('SPR_GLYPH_8',     SPR_GLYPH_8),
    ('SPR_GLYPH_9',     SPR_GLYPH_9),
    ('SPR_GLYPH_A',     SPR_GLYPH_A),
    ('SPR_GLYPH_R',     SPR_GLYPH_R),
    ('SPR_GLYPH_M',     SPR_GLYPH_M),
    ('SPR_GLYPH_Q',     SPR_GLYPH_Q),
    ('SPR_GLYPH_I',     SPR_GLYPH_I),
    ('SPR_GLYPH_T',     SPR_GLYPH_T),
    ('SPR_BUG_FLASH',   SPR_BUG_FLASH),
    ('SPR_ROBOT_FLASH', SPR_ROBOT_FLASH),
    # Iter-3 #18: armored walk/flash + stun overlays (3 per enemy type).
    ('SPR_ARMORED_A',     SPR_ARMORED_A),
    ('SPR_ARMORED_B',     SPR_ARMORED_B),
    ('SPR_ARMORED_FLASH', SPR_ARMORED_FLASH),
    ('SPR_BUG_STUN',      SPR_BUG_STUN),
    ('SPR_ROBOT_STUN',    SPR_ROBOT_STUN),
    ('SPR_ARMORED_STUN',  SPR_ARMORED_STUN),
    # Iter-7: boss walk/flash/stun + HP bar (8 tiles).
    ('SPR_BOSS_A',        SPR_BOSS_A),
    ('SPR_BOSS_B',        SPR_BOSS_B),
    ('SPR_BOSS_FLASH',    SPR_BOSS_FLASH),
    ('SPR_BOSS_STUN',     SPR_BOSS_STUN),
    ('SPR_BOSS_BAR_1',    SPR_BOSS_BAR_1),
    ('SPR_BOSS_BAR_2',    SPR_BOSS_BAR_2),
    ('SPR_BOSS_BAR_3',    SPR_BOSS_BAR_3),
    ('SPR_BOSS_BAR_4',    SPR_BOSS_BAR_4),
]

# ----------------------------------------------------------------------------
# Tilemap helpers. Each tile-map cell is a single u8 BG tile index.
# ----------------------------------------------------------------------------

def text_row(s, width=20, pad=' '):
    s = s.ljust(width, pad)[:width]
    return [ord(c) if ord(c) < 128 else 32 for c in s]

def blank_row(width=20):
    return [32] * width

TILE_GROUND_IDX = MAP_TILE_BASE + 0
TILE_PATH_IDX   = MAP_TILE_BASE + 1
TILE_COMP_TL_IDX = MAP_TILE_BASE + 2
TILE_COMP_TR_IDX = MAP_TILE_BASE + 3
TILE_COMP_BL_IDX = MAP_TILE_BASE + 4
TILE_COMP_BR_IDX = MAP_TILE_BASE + 5
TILE_COMP_TL_D_IDX = MAP_TILE_BASE + 6
TILE_COMP_TR_D_IDX = MAP_TILE_BASE + 7
TILE_COMP_BL_D_IDX = MAP_TILE_BASE + 8
TILE_COMP_BR_D_IDX = MAP_TILE_BASE + 9

# Iter-4 #28: Title art tile index constants.
TITLE_B_TL_IDX  = MAP_TILE_BASE + 35
TITLE_B_TR_IDX  = MAP_TILE_BASE + 36
TITLE_B_BL_IDX  = MAP_TILE_BASE + 37
TITLE_B_BR_IDX  = MAP_TILE_BASE + 38
TITLE_U_T_IDX   = MAP_TILE_BASE + 39
TITLE_U_BL_IDX  = MAP_TILE_BASE + 40
TITLE_U_BR_IDX  = MAP_TILE_BASE + 41
TITLE_G_TL_IDX  = MAP_TILE_BASE + 42
TITLE_G_TR_IDX  = MAP_TILE_BASE + 43
TITLE_G_BL_IDX  = MAP_TILE_BASE + 44
TITLE_G_BR_IDX  = MAP_TILE_BASE + 45
TITLE_HLINE_IDX = MAP_TILE_BASE + 46
TITLE_NODE_IDX  = MAP_TILE_BASE + 47
TITLE_BUG_T_IDX = MAP_TILE_BASE + 48
TITLE_BUG_B_IDX = MAP_TILE_BASE + 49
TITLE_TW_T_IDX  = MAP_TILE_BASE + 50
TITLE_TW_B_IDX  = MAP_TILE_BASE + 51
TILE_ARROW_IDX  = MAP_TILE_BASE + 52

# Iter-6: End-screen art tile index constants.
ENDSCR_STATIC_IDX    = MAP_TILE_BASE + 53
ENDSCR_BROKEN_HL_IDX = MAP_TILE_BASE + 54
ENDSCR_SKULL_T_IDX   = MAP_TILE_BASE + 55
ENDSCR_SKULL_B_IDX   = MAP_TILE_BASE + 56
ENDSCR_SHIELD_T_IDX  = MAP_TILE_BASE + 57
ENDSCR_SHIELD_B_IDX  = MAP_TILE_BASE + 58

def title_map():
    S = 32
    # Use Python index constants (must match append order in map_tiles).
    B_TL = TITLE_B_TL_IDX;  B_TR = TITLE_B_TR_IDX
    B_BL = TITLE_B_BL_IDX;  B_BR = TITLE_B_BR_IDX
    U_T  = TITLE_U_T_IDX
    U_BL = TITLE_U_BL_IDX;  U_BR = TITLE_U_BR_IDX
    G_TL = TITLE_G_TL_IDX;  G_TR = TITLE_G_TR_IDX
    G_BL = TITLE_G_BL_IDX;  G_BR = TITLE_G_BR_IDX
    HL   = TITLE_HLINE_IDX
    ND   = TITLE_NODE_IDX
    BG_T = TITLE_BUG_T_IDX
    BG_B = TITLE_BUG_B_IDX
    TW_T = TITLE_TW_T_IDX
    TW_B = TITLE_TW_B_IDX
    CT = TILE_COMP_TL_IDX       # 130 — existing computer tiles
    CR = TILE_COMP_TR_IDX       # 131
    CB = TILE_COMP_BL_IDX       # 132
    CX = TILE_COMP_BR_IDX       # 133

    rows = [blank_row() for _ in range(18)]
    # --- Art area (rows 0-9) ---
    rows[1]  = [S,S,S,S,S,S, B_TL,B_TR, S, U_T,U_T, S, G_TL,G_TR, S,S,S,S,S,S]
    rows[2]  = [S,S,S,S,S,S, B_BL,B_BR, S, U_BL,U_BR, S, G_BL,G_BR, S,S,S,S,S,S]
    rows[3]  = text_row("      DEFENDER      ")
    rows[4]  = [S,S, ND, HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL, ND, S,S]
    rows[6]  = [S,S,S,S, BG_T, HL,HL,HL, S, CT,CR, S, HL,HL,HL, TW_T, S,S,S,S]
    rows[7]  = [S,S,S,S, BG_B, S,S,S,    S, CB,CX, S, S,S,S,    TW_B, S,S,S,S]
    # --- UI area (rows 10-17, selectors overwritten at runtime) ---
    rows[14] = text_row("    PRESS START     ")
    return rows

def win_map():
    S = 32
    HL   = TITLE_HLINE_IDX
    ND   = TITLE_NODE_IDX
    TT   = TITLE_TW_T_IDX
    TB   = TITLE_TW_B_IDX
    CT   = TILE_COMP_TL_IDX
    CR   = TILE_COMP_TR_IDX
    CB   = TILE_COMP_BL_IDX
    CX   = TILE_COMP_BR_IDX
    SH   = ENDSCR_SHIELD_T_IDX
    SS   = ENDSCR_SHIELD_B_IDX

    rows = [blank_row() for _ in range(18)]
    rows[1]  = text_row("      VICTORY!      ")
    rows[3]  = [S,S, ND, HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL, ND, S,S]
    rows[5]  = [S,S,S,S, TT, HL,HL,HL, S, CT,CR, S, HL,HL,HL, TT, S,S,S,S]
    rows[6]  = [S,S,S,S, TB, S,S,S,    S, CB,CX, S, S,S,S,    TB, S,S,S,S]
    rows[8]  = [S,S, ND, HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL, ND, S,S]
    rows[10] = [S,S,S,S,S,S, TT, S,S, SH, S,S, TT, S,S,S,S,S,S,S]
    rows[11] = [S,S,S,S,S,S, TB, S,S, SS, S,S, TB, S,S,S,S,S,S,S]
    return rows

def lose_map():
    S = 32
    HL   = TITLE_HLINE_IDX
    ND   = TITLE_NODE_IDX
    BK   = ENDSCR_BROKEN_HL_IDX
    BT   = TITLE_BUG_T_IDX
    BB   = TITLE_BUG_B_IDX
    DT   = TILE_COMP_TL_D_IDX
    DR   = TILE_COMP_TR_D_IDX
    DB   = TILE_COMP_BL_D_IDX
    DX   = TILE_COMP_BR_D_IDX
    ST   = ENDSCR_STATIC_IDX
    SK   = ENDSCR_SKULL_T_IDX
    SB   = ENDSCR_SKULL_B_IDX

    rows = [blank_row() for _ in range(18)]
    rows[0]  = [ST]*20
    rows[1]  = text_row("     GAME OVER      ")
    rows[2]  = [ST]*20
    rows[4]  = [S,S, ND, BK,BK,BK,BK,BK,BK,BK,BK,BK,BK,BK,BK,BK,BK, ND, S,S]
    rows[6]  = [S,S,S,S, BT, BK,BK,BK, S, DT,DR, S, BK,BK,BK, BT, S,S,S,S]
    rows[7]  = [S,S,S,S, BB, S,S,S,    S, DB,DX, S, S,S,S,    BB, S,S,S,S]
    rows[9]  = [S,S, ND, HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL,HL, ND, S,S]
    rows[10] = [S,S,S,S,S,S, BT, S,S, SK, S,S, BT, S,S,S,S,S,S,S]
    rows[11] = [S,S,S,S,S,S, BB, S,S, SB, S,S, BB, S,S,S,S,S,S,S]
    rows[17] = [ST]*20
    return rows

# ----------------------------------------------------------------------------
# Gameplay map. 20 cols x 17 rows of tile indices for the play field
# (rendered to screen rows 1..17). Plus a parallel class array.
# ----------------------------------------------------------------------------

# Class codes match TC_* in src/map.h.
TC_GROUND   = 0
TC_PATH     = 1
TC_COMPUTER = 2

def build_map(path_segments, computer_tl):
    """Generic map builder.
    path_segments: list of ('h', x0, x1, y) | ('v', x, y0, y1) ; both endpoints
                   are inclusive. Computer cluster cells inside a path segment
                   are reclassified as TC_COMPUTER (path enters cluster at the
                   terminal waypoint).
    computer_tl: (cx, cy) top-left of the 2x2 computer cluster.
    Returns (tile_grid, class_grid).
    """
    class_grid = [[TC_GROUND]*20 for _ in range(17)]

    def set_path(x, y):
        class_grid[y][x] = TC_PATH

    for seg in path_segments:
        if seg[0] == 'h':
            _, x0, x1, y = seg
            lo, hi = (x0, x1) if x0 <= x1 else (x1, x0)
            for c in range(lo, hi + 1):
                set_path(c, y)
        else:
            _, x, y0, y1 = seg
            lo, hi = (y0, y1) if y0 <= y1 else (y1, y0)
            for r in range(lo, hi + 1):
                set_path(x, r)

    cx, cy = computer_tl
    for r in range(cy, cy + 2):
        for c in range(cx, cx + 2):
            class_grid[r][c] = TC_COMPUTER

    tile_grid = [[TILE_GROUND_IDX]*20 for _ in range(17)]
    for r in range(17):
        for c in range(20):
            cls = class_grid[r][c]
            if cls == TC_PATH:
                tile_grid[r][c] = TILE_PATH_IDX
            elif cls == TC_GROUND:
                tile_grid[r][c] = TILE_GROUND_IDX
    tile_grid[cy    ][cx    ] = TILE_COMP_TL_IDX
    tile_grid[cy    ][cx + 1] = TILE_COMP_TR_IDX
    tile_grid[cy + 1][cx    ] = TILE_COMP_BL_IDX
    tile_grid[cy + 1][cx + 1] = TILE_COMP_BR_IDX

    return tile_grid, class_grid


# ---------------------------------------------------------------------------
# Map definitions — see specs/iter3-17-maps-design.md for ASCII grids.
# Waypoint lists are translated literally from the design doc; build_map()
# segments mirror those waypoints (every consecutive pair is purely H or V
# and endpoints inclusive).
# ---------------------------------------------------------------------------

# Map 1 — reference. MUST stay byte-identical to the pre-iter-3-17
# `gameplay_tilemap` / `gameplay_classmap` output.
MAP1_WAYPOINTS = [
    (-1, 2), (0, 2), (8, 2), (8, 9),
    (15, 9), (15, 5), (17, 5), (18, 5),
]
MAP1_SEGMENTS = [
    ('h', 0, 8, 2),
    ('v', 8, 2, 9),
    ('h', 8, 15, 9),
    ('v', 15, 5, 9),
    ('h', 15, 17, 5),
    ('h', 17, 18, 5),  # terminal — last cell is computer cluster
]
MAP1_COMPUTER = (18, 4)

# Map 2 — Maze (10 waypoints — at the budget cap)
MAP2_WAYPOINTS = [
    (-1, 1), (0, 1), (4, 1), (4, 4),
    (2, 4), (2, 8), (11, 8), (11, 5),
    (17, 5), (18, 5),
]
MAP2_SEGMENTS = [
    ('h', 0, 4, 1),
    ('v', 4, 1, 4),
    ('h', 2, 4, 4),
    ('v', 2, 4, 8),
    ('h', 2, 11, 8),
    ('v', 11, 5, 8),
    ('h', 11, 17, 5),
    ('h', 17, 18, 5),  # terminal
]
MAP2_COMPUTER = (18, 4)

# Map 3 — Sprint (8 waypoints, short near-horizontal)
MAP3_WAYPOINTS = [
    (-1, 5), (0, 5), (7, 5), (7, 7),
    (14, 7), (14, 5), (17, 5), (18, 5),
]
MAP3_SEGMENTS = [
    ('h', 0, 7, 5),
    ('v', 7, 5, 7),
    ('h', 7, 14, 7),
    ('v', 14, 5, 7),
    ('h', 14, 17, 5),
    ('h', 17, 18, 5),  # terminal
]
MAP3_COMPUTER = (18, 4)

GAMEPLAY1_TILES, GAMEPLAY1_CLASS = build_map(MAP1_SEGMENTS, MAP1_COMPUTER)
GAMEPLAY2_TILES, GAMEPLAY2_CLASS = build_map(MAP2_SEGMENTS, MAP2_COMPUTER)
GAMEPLAY3_TILES, GAMEPLAY3_CLASS = build_map(MAP3_SEGMENTS, MAP3_COMPUTER)

# ----------------------------------------------------------------------------
# Emit C source.
# ----------------------------------------------------------------------------

def fmt_bytes(data, per_line=16):
    out = []
    for i in range(0, len(data), per_line):
        chunk = data[i:i+per_line]
        out.append("    " + ", ".join("0x%02X" % b for b in chunk) + ",")
    return "\n".join(out)

def emit_tile_array(name, tiles_bytes_list):
    lines = ["const unsigned char %s[] = {" % name]
    for tb in tiles_bytes_list:
        lines.append(fmt_bytes(tb))
    lines.append("};")
    return "\n".join(lines)

def emit_u8_grid(name, grid, sized=False):
    rows = []
    for row in grid:
        rows.append("    " + ", ".join(str(v) for v in row) + ",")
    if sized:
        n = len(grid) * (len(grid[0]) if grid else 0)
        decl = "const unsigned char %s[%d]" % (name, n)
    else:
        decl = "const unsigned char %s[]" % name
    return "%s = {\n%s\n};" % (decl, "\n".join(rows))

def emit_waypoints(name, waypoints):
    """Emit a waypoint_t array. Plain `const` (NOT `static const`) so map.c
    can reference it via an `extern` declaration in res/assets.h."""
    body = ", ".join("{ %d, %d }" % (tx, ty) for (tx, ty) in waypoints)
    return "const waypoint_t %s[%d] = { %s };" % (name, len(waypoints), body)

assets_h = """\
/* Auto-generated by tools/gen_assets.py — do not edit by hand. */
#ifndef GBTD_ASSETS_H
#define GBTD_ASSETS_H

#include "gtypes.h"

/* BG tile bank: tiles 0..127 hold ASCII font glyphs, tiles 128.. hold map
 * tiles (TILE_GROUND, TILE_PATH, TILE_COMP_*). Sprite tiles live in their
 * own VRAM bank starting at sprite tile index 0 (SPR_*). */

#define MAP_TILE_BASE   128
#define TILE_GROUND     (MAP_TILE_BASE + 0)
#define TILE_PATH       (MAP_TILE_BASE + 1)
#define TILE_COMP_TL    (MAP_TILE_BASE + 2)
#define TILE_COMP_TR    (MAP_TILE_BASE + 3)
#define TILE_COMP_BL    (MAP_TILE_BASE + 4)
#define TILE_COMP_BR    (MAP_TILE_BASE + 5)
#define TILE_COMP_TL_D  (MAP_TILE_BASE + 6)
#define TILE_COMP_TR_D  (MAP_TILE_BASE + 7)
#define TILE_COMP_BL_D  (MAP_TILE_BASE + 8)
#define TILE_COMP_BR_D  (MAP_TILE_BASE + 9)
#define TILE_TOWER      (MAP_TILE_BASE + 10)
#define TILE_TOWER_2    (MAP_TILE_BASE + 11)
#define TILE_COMP_TL_C1 (MAP_TILE_BASE + 12)
#define TILE_COMP_TL_C2 (MAP_TILE_BASE + 13)
#define TILE_COMP_TR_C2 (MAP_TILE_BASE + 14)
#define TILE_COMP_TL_C3 (MAP_TILE_BASE + 15)
#define TILE_COMP_TR_C3 (MAP_TILE_BASE + 16)
#define TILE_COMP_BL_C3 (MAP_TILE_BASE + 17)
#define TILE_COMP_BR_C3 (MAP_TILE_BASE + 18)
#define TILE_TOWER_B    (MAP_TILE_BASE + 19)
#define TILE_TOWER_2_B  (MAP_TILE_BASE + 20)
#define TILE_TOWER_3    (MAP_TILE_BASE + 21)
#define TILE_TOWER_3_B  (MAP_TILE_BASE + 22)
#define TILE_TOWER_L1     (MAP_TILE_BASE + 23)
#define TILE_TOWER_L1_B   (MAP_TILE_BASE + 24)
#define TILE_TOWER_2_L1   (MAP_TILE_BASE + 25)
#define TILE_TOWER_2_L1_B (MAP_TILE_BASE + 26)
#define TILE_TOWER_3_L1   (MAP_TILE_BASE + 27)
#define TILE_TOWER_3_L1_B (MAP_TILE_BASE + 28)
#define TILE_TOWER_L2     (MAP_TILE_BASE + 29)
#define TILE_TOWER_L2_B   (MAP_TILE_BASE + 30)
#define TILE_TOWER_2_L2   (MAP_TILE_BASE + 31)
#define TILE_TOWER_2_L2_B (MAP_TILE_BASE + 32)
#define TILE_TOWER_3_L2   (MAP_TILE_BASE + 33)
#define TILE_TOWER_3_L2_B (MAP_TILE_BASE + 34)
#define TILE_TITLE_B_TL   (MAP_TILE_BASE + 35)
#define TILE_TITLE_B_TR   (MAP_TILE_BASE + 36)
#define TILE_TITLE_B_BL   (MAP_TILE_BASE + 37)
#define TILE_TITLE_B_BR   (MAP_TILE_BASE + 38)
#define TILE_TITLE_U_T    (MAP_TILE_BASE + 39)
#define TILE_TITLE_U_BL   (MAP_TILE_BASE + 40)
#define TILE_TITLE_U_BR   (MAP_TILE_BASE + 41)
#define TILE_TITLE_G_TL   (MAP_TILE_BASE + 42)
#define TILE_TITLE_G_TR   (MAP_TILE_BASE + 43)
#define TILE_TITLE_G_BL   (MAP_TILE_BASE + 44)
#define TILE_TITLE_G_BR   (MAP_TILE_BASE + 45)
#define TILE_TITLE_HLINE  (MAP_TILE_BASE + 46)
#define TILE_TITLE_NODE   (MAP_TILE_BASE + 47)
#define TILE_TITLE_BUG_T  (MAP_TILE_BASE + 48)
#define TILE_TITLE_BUG_B  (MAP_TILE_BASE + 49)
#define TILE_TITLE_TW_T   (MAP_TILE_BASE + 50)
#define TILE_TITLE_TW_B   (MAP_TILE_BASE + 51)
#define TILE_ARROW        (MAP_TILE_BASE + 52)
#define TILE_ENDSCR_STATIC    (MAP_TILE_BASE + 53)
#define TILE_ENDSCR_BROKEN_HL (MAP_TILE_BASE + 54)
#define TILE_ENDSCR_SKULL_T   (MAP_TILE_BASE + 55)
#define TILE_ENDSCR_SKULL_B   (MAP_TILE_BASE + 56)
#define TILE_ENDSCR_SHIELD_T  (MAP_TILE_BASE + 57)
#define TILE_ENDSCR_SHIELD_B  (MAP_TILE_BASE + 58)
#define MAP_TILE_COUNT  59

#define SPR_CURSOR_A    0
#define SPR_CURSOR_B    1
#define SPR_CURSOR_GA   2
#define SPR_CURSOR_GB   3
#define SPR_TOWER       4
#define SPR_BUG_A       5
#define SPR_BUG_B       6
#define SPR_PROJ        7
#define SPR_ROBOT_A     8
#define SPR_ROBOT_B     9
#define SPR_GLYPH_U     10
#define SPR_GLYPH_P     11
#define SPR_GLYPH_G     12
#define SPR_GLYPH_S     13
#define SPR_GLYPH_E     14
#define SPR_GLYPH_L     15
#define SPR_GLYPH_COLON 16
#define SPR_GLYPH_GT    17
#define SPR_GLYPH_DASH  18
#define SPR_GLYPH_0     19
#define SPR_GLYPH_1     20
#define SPR_GLYPH_2     21
#define SPR_GLYPH_3     22
#define SPR_GLYPH_4     23
#define SPR_GLYPH_5     24
#define SPR_GLYPH_6     25
#define SPR_GLYPH_7     26
#define SPR_GLYPH_8     27
#define SPR_GLYPH_9     28
#define SPR_GLYPH_A     29
#define SPR_GLYPH_R     30
#define SPR_GLYPH_M     31
#define SPR_GLYPH_Q     32
#define SPR_GLYPH_I     33
#define SPR_GLYPH_T     34
#define SPR_BUG_FLASH   35
#define SPR_ROBOT_FLASH 36
#define SPR_ARMORED_A     37
#define SPR_ARMORED_B     38
#define SPR_ARMORED_FLASH 39
#define SPR_BUG_STUN      40
#define SPR_ROBOT_STUN    41
#define SPR_ARMORED_STUN  42
#define SPR_BOSS_A        43
#define SPR_BOSS_B        44
#define SPR_BOSS_FLASH    45
#define SPR_BOSS_STUN     46
#define SPR_BOSS_BAR_1    47
#define SPR_BOSS_BAR_2    48
#define SPR_BOSS_BAR_3    49
#define SPR_BOSS_BAR_4    50
#define SPRITE_TILE_COUNT 51

extern const unsigned char font_tiles[];   /* 128 tiles * 16 bytes */
extern const unsigned char map_tile_data[]; /* MAP_TILE_COUNT * 16 bytes */
extern const unsigned char sprite_tile_data[]; /* SPRITE_TILE_COUNT * 16 bytes */

extern const unsigned char title_tilemap[20*18];
extern const unsigned char win_tilemap[20*18];
extern const unsigned char lose_tilemap[20*18];

/* Iter-3 #17: three selectable maps. Map 1 is the canonical replacement
 * for the pre-iter-3-17 `gameplay_tilemap` / `gameplay_classmap` /
 * waypoint set (Map 1 bytes are byte-identical to the legacy data).
 * Plain `const` (NOT `static const`) so external linkage is preserved. */
#include "map.h"   /* waypoint_t */

extern const unsigned char gameplay1_tilemap[20*17];
extern const unsigned char gameplay1_classmap[20*17];
extern const unsigned char gameplay2_tilemap[20*17];
extern const unsigned char gameplay2_classmap[20*17];
extern const unsigned char gameplay3_tilemap[20*17];
extern const unsigned char gameplay3_classmap[20*17];

extern const waypoint_t gameplay1_waypoints[8];
extern const waypoint_t gameplay2_waypoints[10];
extern const waypoint_t gameplay3_waypoints[8];

#endif
"""

# Concat font into one byte stream of 128*16 bytes.
font_blob = b"".join(font_tiles)
map_blob  = b"".join(t for _, t in map_tiles)
spr_blob  = b"".join(t for _, t in sprite_tiles)

assets_c = []
assets_c.append("/* Auto-generated by tools/gen_assets.py — do not edit by hand. */")
assets_c.append('#include "assets.h"')
assets_c.append("")
assets_c.append(emit_tile_array("font_tiles",
    [font_blob[i:i+16] for i in range(0, len(font_blob), 16)]))
assets_c.append("")
assets_c.append(emit_tile_array("map_tile_data",
    [map_blob[i:i+16] for i in range(0, len(map_blob), 16)]))
assets_c.append("")
assets_c.append(emit_tile_array("sprite_tile_data",
    [spr_blob[i:i+16] for i in range(0, len(spr_blob), 16)]))
assets_c.append("")
assets_c.append(emit_u8_grid("title_tilemap", title_map()))
assets_c.append("")
assets_c.append(emit_u8_grid("win_tilemap", win_map()))
assets_c.append("")
assets_c.append(emit_u8_grid("lose_tilemap", lose_map()))
assets_c.append("")
assets_c.append(emit_u8_grid("gameplay1_tilemap",  GAMEPLAY1_TILES, sized=True))
assets_c.append("")
assets_c.append(emit_u8_grid("gameplay1_classmap", GAMEPLAY1_CLASS, sized=True))
assets_c.append("")
assets_c.append(emit_u8_grid("gameplay2_tilemap",  GAMEPLAY2_TILES, sized=True))
assets_c.append("")
assets_c.append(emit_u8_grid("gameplay2_classmap", GAMEPLAY2_CLASS, sized=True))
assets_c.append("")
assets_c.append(emit_u8_grid("gameplay3_tilemap",  GAMEPLAY3_TILES, sized=True))
assets_c.append("")
assets_c.append(emit_u8_grid("gameplay3_classmap", GAMEPLAY3_CLASS, sized=True))
assets_c.append("")
assets_c.append(emit_waypoints("gameplay1_waypoints", MAP1_WAYPOINTS))
assets_c.append(emit_waypoints("gameplay2_waypoints", MAP2_WAYPOINTS))
assets_c.append(emit_waypoints("gameplay3_waypoints", MAP3_WAYPOINTS))
assets_c.append("")

with open(os.path.join(OUT_DIR, "assets.h"), "w") as f:
    f.write(assets_h)
with open(os.path.join(OUT_DIR, "assets.c"), "w") as f:
    f.write("\n".join(assets_c))

print("wrote %s/assets.{c,h} (font=%d B, map_tiles=%d B, sprites=%d B)" %
      (OUT_DIR, len(font_blob), len(map_blob), len(spr_blob)))
