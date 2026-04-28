# GBTD — Game Boy Tower Defense

A monochrome Game Boy (DMG) tower-defense game where the player defends a
desktop computer from waves of robot agents and "bugs" sent by a runaway AI.
Compiles to a real `.gb` ROM that runs on any DMG emulator (or real hardware
via flashcart).

## Prerequisites (macOS)

```sh
brew install just mgba
# Apple Silicon only — GBDK ships x86_64 binaries; needs Rosetta 2:
softwareupdate --install-rosetta --agree-to-license
```

`just setup` downloads GBDK-2020 (4.2.0) into `vendor/gbdk/` on first run.

## Build & play

```sh
just setup     # one-time: download GBDK
just run       # build then launch mGBA on the ROM
```

Other recipes:

| Recipe         | Purpose                                   |
|----------------|-------------------------------------------|
| `just build`   | Compile sources → `build/gbtd.gb`         |
| `just check`   | Validate ROM exists, ≤ 32 KB, cart byte 0 |
| `just emulator`| Launch mGBA on existing ROM (no rebuild)  |
| `just assets`  | Regenerate `res/assets.{c,h}` (Python)    |
| `just clean`   | Remove `build/`                           |
| `just clean-all` | Also remove `vendor/gbdk/`              |

## Controls

- **D-pad** — move placement cursor (12-frame initial / 6-frame repeat)
- **A** — place selected tower (or open upgrade/sell menu on an existing tower)
- **B** — cycle selected tower type (Antivirus ↔ Firewall)
- **Start** — begin game / return to title from game over

In the upgrade/sell menu: **D-pad up/down** picks Upgrade or Sell,
**A** confirms, **B** cancels. Gameplay freezes while the menu is open.

## How it plays

You start with 5 HP and 30 energy, plus a slow passive trickle of
+1 energy every 3 s. Two tower types defend your computer:

- **Antivirus** (`A`, 10 E, 3-tile range): fast, low-damage chip-shots.
- **Firewall** (`F`, 15 E, 5-tile range): slow but heavy hitter — the
  answer to robots.

Each tower can be upgraded once (faster + harder hits) or sold for
half of what you've sunk into it. Killing a bug awards 3 energy; a
robot awards 5. Survive 10 escalating waves (mix of bugs and robots,
final wave is a 28-enemy "boss" mix) without letting HP hit zero.
Win → "SYSTEM CLEAN :)". Lose → "KERNEL PANIC X_X". Sound effects
play on channels 1, 2 and 4 throughout.

## Project layout

```
src/        C99 sources, one module per concern
res/        Generated tile + tilemap data (committed)
tools/      Asset generator (Python)
specs/      Design + implementation specs
memory/     Decisions + conventions
```

See `specs/mvp.md` for the full implementation spec and
`specs/mvp-design.md` for the visual/UX spec.
