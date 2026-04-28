# QA Report — GBTD MVP

## Verdict: SHIP (with one cosmetic note)

All developer workflows pass. ROM builds to a valid 32 KB DMG image
with correct Nintendo logo, header checksum, and global checksum.
mGBA boots the ROM cleanly. Interactive gameplay verification (title →
play → win/lose loop, tower placement, enemy/projectile behaviour)
remains MANUAL-REQUIRED — mGBA on macOS exposes no headless input
driver, scripting bridge, or CLI screenshot facility, so the
acceptance scenarios in `tests/manual.md` cannot be automated from
this environment.

---

## Layer 1 — Developer workflow

Environment: macOS / arm64 (Apple Silicon), `just` from Homebrew,
`mgba 0.10.5_2` from Homebrew, GBDK-2020 4.2.0 already cached in
`vendor/gbdk/`. Rosetta 2 verified by `arch -x86_64 /usr/bin/true`
(success).

| # | Check | Result | Evidence |
|---|-------|--------|----------|
| 1 | `just --list` shows recipes | ✅ PASS | Lists `assets, build, check, clean, clean-all, default, emulator, run, setup, test`. Matches README table. |
| 2 | `just setup` succeeds | ✅ PASS | `Toolchain OK: .../vendor/gbdk/bin/lcc ... 2.0`. Rosetta probe passes; mgba check passes; GBDK already cached. |
| 3 | `just build` produces ≤ 32 KB ROM with valid header | ✅ PASS | `Built .../build/gbtd.gb (32768 bytes)`. Header verified (see ROM metadata below). |
| 4 | `just test` runs host-side tests | ✅ PASS | `test_math: all assertions passed`. |
| 5 | `just check` passes (build + test + size/header) | ✅ PASS | `ROM check OK (32768 bytes, cart=0x00, ≤ 32 KB)`. |
| 6 | `just clean` removes `build/` | ✅ PASS | `rm -rf .../build`; `ls build` → No such file or directory. Re-run after rebuild also clean. |
| 7 | `just run` builds and launches mGBA | ✅ PASS | Process started, alive >4 s, killed cleanly. Only log line is a benign Qt HiDPI backing-store warning unrelated to the ROM. |

### Cosmetic note (low severity, not blocking)

`just --list` renders the description for the `build` recipe as the
last line of its preceding multi-line `#` comment block:

```
build     # "1 bank = 16 KB" in makebin — see memory/decisions.md.)
```

Functionally `build` works perfectly; only the listing text is
misleading. Fix would be a one-line `# Build ROM` comment immediately
above the recipe. **Severity: low / cosmetic.**

---

## ROM metadata

| Field | Offset | Value | Notes |
|-------|--------|-------|-------|
| File size | — | 32 768 bytes | == 2 × 16 KB banks, spec maximum |
| Nintendo logo | 0x104–0x133 | matches canonical bytes | ✅ boot ROM will accept |
| Title | 0x134–0x143 | all `0x00` | Empty — acceptable for MVP, not a defect; consider populating "GBTD" later |
| CGB flag | 0x143 | `0x00` | DMG-only, as specified |
| Cartridge type | 0x147 | `0x00` | ROM-only, no MBC ✅ |
| ROM size | 0x148 | `0x00` | 32 KB / 2 banks ✅ |
| RAM size | 0x149 | `0x00` | none ✅ |
| Destination | 0x14A | `0x00` | Japan (default; not relevant) |
| Header checksum | 0x14D | `0x53` | computed `0x53` ✅ MATCH |
| Global checksum | 0x14E–0x14F | `0xC277` | computed `0xC277` ✅ MATCH (not enforced by hardware but correct) |

---

## Layer 2 — Game functionality

mGBA was launched with `mgba -3 build/gbtd.gb` and confirmed to keep
running for the full sample window (>4 s) with no error output to
stderr or to its log. ROM boot is therefore confirmed.

Beyond boot, the macOS build of mGBA exposes no CLI flags for input
injection, scripted frame-stepping, or headless screenshots, and
there is no Lua bridge available without launching the GUI. Driving
the gameplay loop from this QA environment is therefore not feasible.

| # | Acceptance scenario | Result |
|---|--------------------|--------|
| 0 | ROM boots in mGBA without crash, no error in log | ✅ AUTOMATED-PASS |
| 1 | Title renders → START → enters PLAYING | 🟡 MANUAL-REQUIRED |
| 2 | HUD shows HP/Energy/Wave | 🟡 MANUAL-REQUIRED |
| 3 | D-pad moves cursor; A places tower (buildable tile + cost) | 🟡 MANUAL-REQUIRED |
| 4 | Bug enemies spawn and follow path | 🟡 MANUAL-REQUIRED |
| 5 | Towers fire, kill bugs, award bounty | 🟡 MANUAL-REQUIRED |
| 6 | Cleared 3 waves with HP > 0 → WIN screen | 🟡 MANUAL-REQUIRED |
| 7 | 5 enemies reach computer → LOSE screen | 🟡 MANUAL-REQUIRED |
| 8 | START on game-over returns to title | 🟡 MANUAL-REQUIRED |

### Manual-only checks from `tests/manual.md` still outstanding

All of the following remain unaddressed by automation and require a
human play-test on real DMG or via mGBA GUI:

- **F1** — VRAM/BG writes during active scan (no tearing on
  title/play transition, tower placement, computer-damage swap,
  win/lose screens).
- **F3** — 10-sprites-per-scanline limit (5+ towers in one row,
  multiple enemies on that row in wave 3, no per-row sprite flicker).
- **F5** — D-pad auto-repeat reset on direction change (hold RIGHT
  → press UP → release RIGHT; UP must wait full 12-frame initial
  delay before repeating).
- **F6** — Projectile retargets when its enemy slot is recycled
  (kill a bug while a projectile is in flight; new bug spawning into
  the slot must NOT receive a "free kill").

The boot smoke test prescribed by `tests/manual.md`
(`timeout 5 mgba -3 build/gbtd.gb`) was performed and passed.

---

## Defects

None blocking. One cosmetic finding only:

### D1 — `just --list` description for `build` recipe is misleading
- **Severity:** low (cosmetic; does not affect any workflow)
- **Steps to reproduce:** `just --list` in repo root.
- **Expected:** A short, sensible description such as
  `build  # Compile and link the ROM`.
- **Actual:** The displayed description is the last sentence of the
  multi-line comment block above the recipe:
  `build  # "1 bank = 16 KB" in makebin — see memory/decisions.md.)`
- **Root cause:** `just` only picks the comment line immediately
  preceding the recipe as its description; the recipe's intended
  one-line summary is buried mid-paragraph.

---

## Summary

- Layer 1 dev workflow checks: **7 PASS / 0 FAIL**
- Layer 2 automated checks: **1 PASS / 0 FAIL** (boot)
- Layer 2 manual-required scenarios: **8** (per spec) + **4** (manual.md
  regression items F1/F3/F5/F6) — deferred to human playtest.
- Defects: **0 blocking**, **1 cosmetic** (D1).
- ROM header: **valid** (logo OK, header checksum OK, global checksum
  OK, cart type 0x00, 32 768 bytes).

**Final verdict: SHIP.** Automated coverage is exhausted; the
remaining acceptance criteria require a human at a Game Boy (real or
emulated GUI) and are explicitly scoped that way by `tests/manual.md`.
