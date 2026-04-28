# Iter-3 #16 — Music: Designer Notation

Designer output for the spec at `specs/iter3-16-music.md`. Authoritative
source for the row tables coded into `src/music.c`. Hand-authored — no
external tracker — per D-MUS-1.

## Common parameters

- Frame rate: 60 Hz (16.7 ms/frame).
- One row = one note + one percussion tick. Duration in frames.
- All songs share **wave pattern 0** (mellow triangle, see below).
- CH3 NR32 output level: `1` (100%) — `0x20`. Same for every song.
- CH4 envelope: per song (`ch4_env`).
- CH3 pitch values use the formula `x = 2048 - 65536 / freq_Hz` (DMG
  CH3 frequency formula, NR33+NR34 11-bit).

### Wave pattern 0 (16 bytes, mellow triangle)

```
01 23 45 67 89 AB CD EF FE DC BA 98 76 54 32 10
```

32 4-bit samples, two per byte: `0,1,2,3,…,F,F,E,D,…,1,0`. Symmetric
triangle. Mellow timbre, no harsh harmonics.

### Pause-duck NR50 value

- Normal: `NR50 = 0x77` (vol 7/7 both terminals).
- Ducked: `NR50 = 0x33` (vol 3/7, ≈43% loudness). Single register write
  on `pause_open` / `pause_close`. (D-MUS-4.)

### Pitch table (CH3 11-bit form)

| Note | Hz   | Hex   | Note | Hz   | Hex   |
|------|------|-------|------|------|-------|
| C4   | 262  | 0x706 | C5   | 523  | 0x783 |
| D4   | 294  | 0x721 | D5   | 587  | 0x790 |
| E4   | 330  | 0x739 | E5   | 659  | 0x79D |
| F4   | 349  | 0x744 | F5   | 698  | 0x7A2 |
| G4   | 392  | 0x759 | G5   | 784  | 0x7AC |
| A4   | 440  | 0x76B | A5   | 880  | 0x7B5 |
| B4   | 494  | 0x77B | C6   | 1047 | 0x7C1 |

### Percussion noise bytes (CH4 NR43)

- `KICK`  = `0x55` — low rumble (5-bit poly, period 5)
- `SNAR`  = `0x32` — bright snap (7-bit poly, period 2)
- `HAT`   = `0x21` — high tick   (7-bit poly, period 1)
- `0x00`  = silent (rest)

---

## MUS_TITLE — calm intro arpeggio (~80 BPM, looping)

- `ch3_vol = 0x20` (100%)
- `ch4_env = 0x82` (vol 8, decay 2)
- 24 rows × 24 frames each = 576 frames ≈ 9.6 s loop
- `loop_idx = 0` (full loop)

| # | Pitch | Noise | Frames | Notes |
|---|-------|-------|--------|-------|
|  0 | C5 | KICK | 24 |
|  1 | E5 | 0    | 24 |
|  2 | G5 | HAT  | 24 |
|  3 | E5 | 0    | 24 |
|  4 | C5 | KICK | 24 |
|  5 | E5 | 0    | 24 |
|  6 | G5 | HAT  | 24 |
|  7 | E5 | 0    | 24 |
|  8 | A4 | KICK | 24 |
|  9 | C5 | 0    | 24 |
| 10 | E5 | HAT  | 24 |
| 11 | C5 | 0    | 24 |
| 12 | A4 | KICK | 24 |
| 13 | C5 | 0    | 24 |
| 14 | E5 | HAT  | 24 |
| 15 | C5 | 0    | 24 |
| 16 | F4 | KICK | 24 |
| 17 | A4 | 0    | 24 |
| 18 | C5 | HAT  | 24 |
| 19 | A4 | 0    | 24 |
| 20 | G4 | KICK | 24 |
| 21 | B4 | 0    | 24 |
| 22 | D5 | HAT  | 24 |
| 23 | B4 | SNAR | 24 |

---

## MUS_PLAYING — driving in-game loop (~120 BPM, looping)

- `ch3_vol = 0x20` (100%)
- `ch4_env = 0xA1` (vol 10, decay 1 — punchy)
- 16 rows × 16 frames each = 256 frames ≈ 4.3 s loop
- `loop_idx = 0` (full loop)

| # | Pitch | Noise | Frames | Notes |
|---|-------|-------|--------|-------|
|  0 | C5 | KICK | 16 | beat 1 |
|  1 | C5 | HAT  | 16 |
|  2 | E5 | 0    | 16 |
|  3 | C5 | SNAR | 16 | beat 2 |
|  4 | G4 | KICK | 16 | beat 3 |
|  5 | G4 | HAT  | 16 |
|  6 | B4 | 0    | 16 |
|  7 | G4 | SNAR | 16 | beat 4 |
|  8 | A4 | KICK | 16 |
|  9 | A4 | HAT  | 16 |
| 10 | C5 | 0    | 16 |
| 11 | A4 | SNAR | 16 |
| 12 | F4 | KICK | 16 |
| 13 | F4 | HAT  | 16 |
| 14 | A4 | 0    | 16 |
| 15 | F4 | SNAR | 16 |

---

## MUS_WIN — victory stinger (one-shot)

- `ch3_vol = 0x20` (100%)
- `ch4_env = 0xC1` (vol 12, decay 1)
- 6 rows, durations vary, total ≈ 90 frames ≈ 1.5 s
- `loop_idx = 0xFF` (stinger; silence on end)

| # | Pitch | Noise | Frames |
|---|-------|-------|--------|
|  0 | C5 | KICK | 12 |
|  1 | E5 | HAT  | 12 |
|  2 | G5 | HAT  | 12 |
|  3 | C6 | SNAR | 18 |
|  4 | G5 | 0    | 12 |
|  5 | C6 | SNAR | 24 |

---

## MUS_LOSE — defeat stinger (one-shot)

- `ch3_vol = 0x20` (100%)
- `ch4_env = 0xC2` (vol 12, decay 2 — slow rumble)
- 6 rows, total ≈ 96 frames ≈ 1.6 s
- `loop_idx = 0xFF` (stinger; silence on end)

| # | Pitch | Noise | Frames |
|---|-------|-------|--------|
|  0 | C5 | KICK | 14 |
|  1 | A4 | 0    | 14 |
|  2 | F4 | KICK | 14 |
|  3 | D4 | 0    | 14 |
|  4 | C4 | KICK | 16 |
|  5 | C4 | SNAR | 24 |

---

## ROM size projection

- 4 songs × ≤ 24 rows × 4 B = 384 B song data
- 4 song headers × 6 B = 24 B
- 1 wave pattern × 16 B = 16 B
- Engine code (music.c) ≈ 600 B
- Total ≈ 1.0 KB. Within the spec's 2.5–3 KB headroom.
