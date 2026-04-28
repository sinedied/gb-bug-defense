# GBTD MVP — Iteration 1 Implementation Spec

> Visual/UX reference: `specs/mvp-design.md` (palette, screen mockups, sprites, HUD, map waypoints).
> Roadmap & scope: `specs/roadmap.md` (Iteration 1, items 1–10).
> Toolchain decisions: `memory/decisions.md`.

This spec is the single source of truth for the coder. Every concrete value (struct shapes, pool sizes, fixed-point format, costs, HP, fire rates, build commands) is fixed here.

---

## 1. Problem

Deliver a single-file `.gb` ROM that boots in mGBA on a fresh macOS clone (with only `brew` and `just` preinstalled), shows a title screen, lets the player play one map / one tower / one enemy / three waves with an energy economy, and reaches a win or lose game-over screen. Scope is exactly the 10 items in the roadmap's Iteration 1 — no more.

---

## 2. Project Layout

```
gbtd/
├── AGENTS.md
├── README.md                 # short: how to build/run
├── justfile                  # setup, build, run, clean, check, emulator
├── .gitignore                # build/, vendor/gbdk/
├── memory/
├── specs/
├── vendor/
│   └── gbdk/                 # downloaded by `just setup`, gitignored
│       └── bin/lcc, bin/png2asset, ...
├── res/                      # source art (PNG) + raw asset sources
│   ├── bg_tiles.png          # background tileset (1 row of 8×8 tiles)
│   ├── sprites.png           # 8×8 sprite sheet: cursor×2, ghost, tower, bug×2, projectile
│   ├── title_map.png         # full-screen 20×18 BG map for title
│   ├── win_map.png           # full-screen 20×18 BG map for win
│   ├── lose_map.png          # full-screen 20×18 BG map for lose
│   └── README.md             # describes asset conventions (see §16)
├── build/                    # generated, gitignored
│   ├── obj/                  # *.o, *.asm
│   ├── gen/                  # png2asset outputs (*.c/*.h)
│   └── gbtd.gb
└── src/
    ├── main.c                # entry: hardware init, palette, dispatch to game loop
    ├── types.h               # u8/u16/i8/i16 typedefs, fixed-point macros, common consts
    ├── game.h / game.c       # state machine (TITLE/PLAYING/GAME_OVER) + main loop
    ├── input.h / input.c     # joypad polling, edge-detection (pressed-this-frame)
    ├── gfx.h / gfx.c         # tile/sprite VRAM loading helpers, palette setup
    ├── map.h / map.c         # MVP map data, tile classification (path/build/computer), waypoints
    ├── hud.h / hud.c         # HUD draw + dirty-flag redraw
    ├── cursor.h / cursor.c   # placement cursor (position, blink, valid/invalid state)
    ├── towers.h / towers.c   # tower pool, placement, targeting, firing
    ├── enemies.h / enemies.c # bug pool, waypoint movement, damage application
    ├── projectiles.h / projectiles.c # projectile pool, motion, hit detection
    ├── waves.h / waves.c     # wave script interpreter
    ├── economy.h / economy.c # energy + computer HP state + accessors
    ├── title.h / title.c     # title screen
    └── gameover.h / gameover.c # win + lose screens
```

**Header conventions**:
- Include guards: `#ifndef GBTD_<MODULE>_H`.
- Each module exposes `void <module>_init(void);` and `void <module>_update(void);` where it makes sense.
- No `extern` global structs; modules expose accessors (`u8 economy_get_hp(void);`).
- Shared typedefs (`u8`, `u16`, `i8`, `i16`, `fix8` = `i16` for 8.8 fixed-point) in `types.h`.

---

## 3. Toolchain Setup

### GBDK-2020 — pinned tarball into `vendor/gbdk/`

**Decision**: download a pinned GBDK-2020 release tarball into `vendor/gbdk/` (gitignored). **Not** a Homebrew tap.

**Rationale**:
- No Homebrew tap for GBDK-2020 is officially maintained → tap installs are fragile.
- Pinning the version inside the justfile makes builds byte-reproducible across machines and CI.
- `vendor/gbdk/` is self-contained; `just clean-all` can wipe it without touching system state.

**Pinned version**: GBDK-2020 **4.2.0** (release asset `gbdk-macos.tar.gz` from `https://github.com/gbdk-2020/gbdk-2020/releases/download/4.2.0/gbdk-macos.tar.gz`). The tarball's top-level directory is `gbdk/`, which extracts to `vendor/gbdk/`.

The macOS release ships universal x86_64 binaries; on Apple Silicon they run via Rosetta 2. `just setup` checks for Rosetta and prints a hint to run `softwareupdate --install-rosetta --agree-to-license` if missing on `arm64`.

### mGBA — Homebrew formula

`brew install mgba` (as decided in `memory/decisions.md`). `just setup` verifies `mgba` is on `PATH` and instructs the user otherwise.

### `just` itself

Assumed preinstalled (`brew install just`). The justfile's first line documents this.

---

## 4. Justfile

```make
# justfile — GBTD build/run on macOS
# Prereqs: `brew install just mgba`

GBDK_VERSION := "4.2.0"
GBDK_URL     := "https://github.com/gbdk-2020/gbdk-2020/releases/download/" + GBDK_VERSION + "/gbdk-macos.tar.gz"
GBDK_DIR     := justfile_directory() + "/vendor/gbdk"
LCC          := GBDK_DIR + "/bin/lcc"
PNG2ASSET    := GBDK_DIR + "/bin/png2asset"
BUILD        := justfile_directory() + "/build"
GEN          := BUILD + "/gen"
OBJ          := BUILD + "/obj"
ROM          := BUILD + "/gbtd.gb"

default: run

# Install/verify GBDK + mGBA
setup:
    #!/usr/bin/env bash
    set -euo pipefail
    command -v mgba >/dev/null || { echo "mGBA missing — run: brew install mgba"; exit 1; }
    if [[ "$(uname -m)" == "arm64" ]] && ! /usr/bin/pgrep -q oahd; then
      echo "Apple Silicon detected without Rosetta. Install with:"
      echo "  softwareupdate --install-rosetta --agree-to-license"
      exit 1
    fi
    if [ ! -x "{{LCC}}" ]; then
      echo "Downloading GBDK-2020 {{GBDK_VERSION}}..."
      mkdir -p "{{GBDK_DIR}}/.."
      TMP="$(mktemp -d)"
      curl -fL "{{GBDK_URL}}" -o "$TMP/gbdk.tar.gz"
      tar -xzf "$TMP/gbdk.tar.gz" -C "{{GBDK_DIR}}/.."
      rm -rf "$TMP"
    fi
    test -x "{{LCC}}"
    echo "Toolchain OK: $({{LCC}} -v 2>&1 | head -1)"

# Convert PNG assets to C sources.
# bg_tiles.png is processed first; the three full-screen maps reference it via
# -source_tileset so they emit only tilemap arrays (single shared BG tileset).
assets: setup
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p "{{GEN}}"
    "{{PNG2ASSET}}" res/bg_tiles.png  -c "{{GEN}}/bg_tiles.c"  -keep_palette_order -noflip
    "{{PNG2ASSET}}" res/sprites.png   -c "{{GEN}}/sprites.c"   -spr8x8 -keep_palette_order -noflip
    "{{PNG2ASSET}}" res/title_map.png -c "{{GEN}}/title_map.c" -map -source_tileset res/bg_tiles.png -keep_palette_order -noflip
    "{{PNG2ASSET}}" res/win_map.png   -c "{{GEN}}/win_map.c"   -map -source_tileset res/bg_tiles.png -keep_palette_order -noflip
    "{{PNG2ASSET}}" res/lose_map.png  -c "{{GEN}}/lose_map.c"  -map -source_tileset res/bg_tiles.png -keep_palette_order -noflip

# Compile and link
# -Wm-yC : compute Nintendo logo + header checksum (real-hardware safe).
# -Wm-yt0x00 : cartridge type = ROM only (no MBC).
# -Wm-yo1   : 1 ROM bank (32 KB).
build: assets
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p "{{OBJ}}"
    "{{LCC}}" -Wa-l -Wl-m -Wm-yC -Wm-yt0x00 -Wm-yo1 \
      -I src -I "{{GEN}}" \
      -o "{{ROM}}" \
      src/*.c "{{GEN}}"/*.c
    SIZE=$(wc -c < "{{ROM}}")
    echo "Built {{ROM}} ($SIZE bytes)"

# Validate ROM
check: build
    #!/usr/bin/env bash
    set -euo pipefail
    test -f "{{ROM}}"
    SIZE=$(wc -c < "{{ROM}}")
    test "$SIZE" -le 32768 || { echo "ROM > 32KB: $SIZE"; exit 1; }
    CT=$(xxd -s 0x147 -l 1 -p "{{ROM}}")
    test "$CT" = "00" || { echo "Cart type != 0x00 (got $CT)"; exit 1; }
    echo "ROM check OK ($SIZE bytes, cart=0x$CT, ≤ 32 KB)"

# Build + launch emulator
run: build
    mgba -3 "{{ROM}}"

# Launch emulator on existing ROM
emulator:
    mgba -3 "{{ROM}}"

clean:
    rm -rf "{{BUILD}}"

clean-all: clean
    rm -rf "{{GBDK_DIR}}"
```

Notes:
- `-Wa-l -Wl-m` keep listing/map files in `build/` for debugging.
- Cartridge type / bank count / header checksum are forced explicitly via `-Wm-yt0x00 -Wm-yo1 -Wm-yC` so the ROM passes strict header validation (real DMG hardware boots only if the header checksum at 0x14D is correct — mGBA is permissive but we don't rely on that).
- Source globs (`src/*.c`) are intentional — adding a new module needs no justfile edit.
- Recipes use `just`'s **shebang recipe** form (`#!/usr/bin/env bash` + `set -euo pipefail`) so each recipe runs in a single bash process — multi-line `if`/loops behave as written and failures abort. (`set shell` alone does NOT collapse a recipe into one shell; only shebang recipes do.)

---

## 5. Game Architecture

### 5.1 Main loop

```
main():
  hw_init()              # disable LCD, set BGP=0xE4, OBP0=0xE4, init pools
  gfx_load_common()      # font + sprite tiles into VRAM
  game_set_state(TITLE)
  enable LCD + VBlank interrupt
  while (1):
      input_poll()       # read joypad, compute edges
      game_update()      # dispatch by state
      wait_vbl_done()    # GBDK; sleeps until VBlank
```

Frame budget: one logical "tick" per VBlank (~60 Hz). All timers are frame counts.

### 5.2 State machine (in `game.c`)

```
enum game_state { GS_TITLE, GS_PLAYING, GS_WIN, GS_LOSE };
static enum game_state s_state;

game_update():
  switch (s_state):
    case GS_TITLE:    title_update();    if (input_pressed(START)) enter_playing();
    case GS_PLAYING:  playing_update();
    case GS_WIN:
    case GS_LOSE:     gameover_update(); if (input_pressed(START)) enter_title();

enter_playing():
  map_load(); hud_init(); cursor_init(); towers_init();
  enemies_init(); projectiles_init(); waves_init(); economy_init();
  s_state = GS_PLAYING;

playing_update():
  cursor_update();
  towers_update();        # find target, maybe fire
  enemies_update();       # walk waypoints, damage computer at end
  projectiles_update();   # move, collide, kill
  waves_update();         # spawn per script, advance wave
  hud_update();           # redraw only dirty fields
  if economy_get_hp() == 0:  s_state = GS_LOSE; gameover_enter(LOSE);
  elif waves_all_cleared() && enemies_count() == 0: s_state = GS_WIN; gameover_enter(WIN);
```

### 5.3 Module interfaces (no globals soup)

Each module owns its data as `static` and exposes a small API. Examples:

```c
// economy.h
void economy_init(void);
u8   economy_get_hp(void);          // 0..5
u8   economy_get_energy(void);      // 0..255
void economy_damage(u8 dmg);        // computer takes dmg
bool economy_try_spend(u8 cost);    // returns false if not enough
void economy_award(u8 amount);      // kill bounty

// enemies.h
void enemies_init(void);
bool enemies_spawn(void);           // returns false if pool full
void enemies_update(void);          // moves all, applies damage to computer
u8   enemies_count(void);
// internal: damage callback used by projectiles
bool enemies_apply_damage(u8 idx, u8 dmg); // returns true if killed

// projectiles.h
void projectiles_init(void);
bool projectiles_fire(fix8 x, fix8 y, u8 target_idx);
void projectiles_update(void);

// towers.h, waves.h, hud.h, cursor.h, map.h, input.h, title.h, gameover.h — same pattern
```

Cross-module dependencies (acyclic):
```
game → {title, gameover, cursor, towers, enemies, projectiles, waves, hud, economy, map, input}
towers → {enemies, projectiles, economy, map}
projectiles → {enemies}
enemies → {map (waypoints), economy}
waves → {enemies}
hud → {economy, waves}
cursor → {map, towers, economy, input}
```

---

## 6. Map (one MVP map)

Per design spec (`specs/mvp-design.md` §3):

- **Play field**: 20 cols × 17 rows (HUD occupies row 0).
- **Map storage**: `static const u8 map_tiles[20*17]` of BG tile indices (uses `bg_tiles.png` indices 0..N).
- **Tile types**: classified at compile time into a parallel `static const u8 map_class[20*17]`:
  - `TC_GROUND  = 0` (buildable)
  - `TC_PATH    = 1` (not buildable, enemies walk here visually)
  - `TC_COMPUTER= 2` (not buildable, goal marker — 2×2 at cols 18–19, rows 4–5)
- **Waypoints** — authoritative list lives in `specs/mvp-design.md` §3 (play-field-local tile coords). For the coder's convenience, reproduced here verbatim:
  ```c
  // Play-field-local tile coords. Convert to 8.8 fixed-point screen pixels with:
  //   x_fix = FIX8(tx*8 + 4)
  //   y_fix = FIX8((ty + HUD_ROWS)*8 + 4)   // HUD_ROWS = 1
  static const struct { i8 tx, ty; } waypoints[] = {
      { -1,  2 },  // off-screen spawn
      {  0,  2 },  // enter screen
      {  8,  2 },  // turn 1: right -> down
      {  8,  9 },  // turn 2: down  -> right
      { 15,  9 },  // turn 3: right -> up
      { 15,  5 },  // turn 4: up    -> right
      { 17,  5 },  // last walkable path tile
      { 18,  5 },  // collision with computer (damage trigger)
  };
  ```
- **Computer goal**: 2×2 BG tile cluster at play-field cols 18–19, rows 4–5 (screen rows 5–6). At `economy_get_hp() <= 2` swap to "damaged" tile variant (per design spec).

`map_load()` copies `map_tiles` to BG tilemap (0,1)..(19,17) with `set_bkg_tiles()`.

**Cursor initial position**: play-field-local tile `(1, 0)` (screen tile `(1, 1)`). Authored map guarantees this is `TC_GROUND` (top-left corner of play field, well clear of the row-2 path), so the cursor renders in its valid (black-bracket, 1 Hz) state on entry.

---

## 7. Enemy ("bug")

- **Sprite**: 8×8, 2 walk-anim frames (animate every 16 frames).
- **HP**: 3.
- **Speed**: `0x0080` = 0.5 px/frame (8.8 fixed point) → ~30 px/sec.
- **Movement**: state = `{x, y, hp, wp_idx, anim, alive}` in 8.8 fixed-point.
  - Each tick, compute delta toward `waypoints[wp_idx+1]`; advance by speed along the dominant axis (path is axis-aligned, so no diagonals).
  - When within `speed` of next waypoint, snap to it and `wp_idx++`.
  - When `wp_idx` reaches the last waypoint: call `economy_damage(1)`, free slot.
- **Pool**: `MAX_ENEMIES = 12` static array. Free-list via `alive` flag (linear scan — pool is tiny, no heap). Sized for the worst case: path = 30 tiles × 8 px = 240 px; at 0.5 px/frame traversal = 480 frames; W3 spawns every 60 f → max simultaneously alive = 480/60 = **8**, plus a 4-slot safety margin.
- **OAM use**: 1 OAM per enemy. Worst case 8 visible.
- **Per-scanline check**: path is purely horizontal/vertical with one row/col at a time, so worst case ~6 enemies on the same horizontal segment → 6 sprites/scanline (under the 10 limit).

```c
typedef struct { fix8 x, y; u8 hp; u8 wp_idx; u8 anim; u8 alive; } enemy_t;
static enemy_t s_enemies[MAX_ENEMIES];
```

---

## 8. Tower (single type — "Antivirus Turret")

- **Sprite**: 8×8, single frame.
- **Placement rules**: only on `TC_GROUND` tile under the cursor; only if `economy_try_spend(TOWER_COST)` succeeds; not on a tile already occupied by another tower.
- **Cost**: `TOWER_COST = 10` energy.
- **Range**: 24 px (3 tiles). Squared-distance check (`dx*dx + dy*dy <= 24*24`) using integer pixels (high byte of fixed-point).
- **Fire rate**: `TOWER_COOLDOWN = 60` frames (1 shot/sec).
- **Damage per hit**: 1.
- **Targeting**: scan enemies, pick the **first alive enemy in range with the highest `wp_idx`** (closest to the computer — classic TD "first" policy). Tie-break: lower array index.
- **Pool**: `MAX_TOWERS = 16` static array. State `{tx, ty, cooldown, alive}` in tile coords. Occupancy check on placement is a linear scan over the pool (16 entries, trivial).
- **Render**: BG tile is *not* used; tower is drawn as **1 OAM sprite** centered on its tile. (Keeps map data immutable — no need to rewrite BG when a tower is placed.)
- **OAM cost**: up to 16, but realistically 4–8 placed during a 3-wave game.

---

## 9. Projectile

- **Sprite**: 8×8, single frame (3×3 black square per design).
- **Pool**: `MAX_PROJECTILES = 8` static array.
- **State**: `{x, y, target_idx, alive}` in 8.8 fixed-point. Speed = `0x0200` (2 px/frame).
- **Update**: each frame, move toward `enemies[target_idx]` position. If target died (`!alive` or `enemies[target_idx].alive==0`), free the projectile (no homing onto a new target — keeps logic trivial).
- **Hit detection**: when distance to target ≤ 4 px (squared check), call `enemies_apply_damage(target_idx, 1)`, free the projectile. If kill returned, call `economy_award(KILL_BOUNTY)`.
- **OAM cost**: up to 8.

---

## 10. Wave System

```c
typedef struct {
    u8  count;            // total bugs in this wave
    u8  spawn_interval;   // frames between spawns
} wave_t;

static const wave_t waves[3] = {
    { 5,  90 },   // wave 1: 5 bugs, 1 every 1.5 s
    { 8,  75 },   // wave 2: 8 bugs, 1 every 1.25 s
    { 12, 60 },   // wave 3: 12 bugs, 1 every 1.0 s
};
#define INTER_WAVE_DELAY 180  // 3 seconds
```

State machine inside `waves.c`:
```
enum { WS_DELAY, WS_SPAWNING, WS_DONE };
static u8 cur_wave;       // 0..2
static u8 spawned;        // count spawned in current wave
static u16 timer;         // frames

waves_update():
  if WS_DELAY: if --timer == 0: state = WS_SPAWNING; spawned = 0; timer = 0
  if WS_SPAWNING:
    if --timer == 0:
      if enemies_spawn(): spawned++; timer = waves[cur_wave].spawn_interval
      else: timer = 8        # pool full, retry shortly
    if spawned == waves[cur_wave].count:
      cur_wave++
      if cur_wave == 3: state = WS_DONE
      else: state = WS_DELAY; timer = INTER_WAVE_DELAY

waves_all_cleared():  return state == WS_DONE
waves_get_current():  return min(cur_wave + 1, 3)   # for HUD: 1..3
```

Initial `state = WS_DELAY, timer = 60` (1 s grace before first wave).

---

## 11. HUD

Per design spec (`mvp-design.md` §6):
- **Row 0 only** (top of screen, 20 tiles wide).
- **Format**: `HP:05  E:030  W:1/3` (17 chars, cols 0–16; cols 17–19 reserved blank).
- **Redraw strategy**: each value has a `dirty` flag set by economy/waves writes. `hud_update()` checks flags and rewrites only the affected 2–3 tiles via `set_bkg_tiles()` during VBlank.
- **Glyphs**: GBDK default 8×8 ASCII font (loaded by `gfx_load_common`).

```c
static u8 dirty_hp, dirty_e, dirty_w;
hud_mark_hp_dirty();   // called from economy_damage
hud_mark_e_dirty();    // called from economy_try_spend / economy_award
hud_mark_w_dirty();    // called from waves on cur_wave++
```

---

## 12. Input

`input.c` polls once per frame and exposes:
```c
extern u8 input_held;       // current frame
extern u8 input_pressed;    // edge: held & ~prev
bool input_is_held(u8 mask);
bool input_is_pressed(u8 mask);
```

Bindings (per roadmap §item 4):
- **D-pad**: move cursor by 1 tile per press (edge-triggered, with **6-frame auto-repeat after a 12-frame initial hold** — matches `mvp-design.md` §5).
- **A**: place tower at cursor (no-op if invalid tile or insufficient energy).
- **B**: no-op in MVP (reserved for future menu).
- **START**: TITLE → PLAYING; GAME_OVER → TITLE. **No pause in MVP** (deferred to iteration 3 item 22 — confirmed in roadmap).
- **SELECT**: unused.

---

## 13. Energy Economy

| Constant | Value |
|---|---|
| `START_ENERGY`  | 30 |
| `TOWER_COST`    | 10 |
| `KILL_BOUNTY`   | 3  |
| `START_HP`      | 5  |
| `MAX_ENERGY`    | **255** (`u8` max — no clamp logic needed; HUD displays zero-padded 3-digit per design `E:NNN`) |

Total awardable in a perfect run: `30 + (5+8+12)×3 = 105` energy. Cap of 255 is comfortably above this; no overflow possible.

Initial: `hp=5, energy=30`. First tower is affordable from frame 1; players must kill ~3 bugs to afford a second tower.

---

## 14. Win / Lose

- **Lose**: `economy_get_hp() == 0` → enter `GS_LOSE`.
- **Win**: `waves_all_cleared() && enemies_count() == 0` → enter `GS_WIN`.
- Game-over screens defined in design spec §2 (KERNEL PANIC `X_X` / SYSTEM CLEAN `:)`). START returns to title and re-runs full init.

---

## 15. Title & Game-Over Screens

- Each is a static 20×18 BG **tilemap** authored as a PNG (`title_map.png`, `win_map.png`, `lose_map.png`) and converted by `png2asset` using `-source_tileset res/bg_tiles.png` so the maps emit **only** a tile-index array referencing the single shared BG tileset already loaded in VRAM (`gfx_load_common()` loads it once at boot).
- Loading a screen: `gfx_hide_all_sprites(); set_bkg_tiles(0,0, 20,18, <map>_tiles);`
- **"PRESS START" blink**: every 30 frames, toggle the 12 BG tiles holding the literal `PRESS START` glyphs (per design spec coords, e.g. row 13 cols 4..15) between their glyph indices and the blank tile (tile 0) via `set_bkg_tiles` during VBlank. Implemented in `title.c` and reused by `gameover.c`. Period = 60 f = 1 Hz, verifiable with mGBA's frame counter.

**OAM hygiene on every screen transition**: each module's `_init()` MUST call `hide_sprite(slot)` (write Y=0xF0) for every OAM slot it owns. OAM slot allocation is fixed:
| Slots | Owner |
|---|---|
| 0       | cursor |
| 1..16   | towers (slot = 1 + tower index) |
| 17..28  | enemies (slot = 17 + enemy index) |
| 29..36  | projectiles (slot = 29 + projectile index) |
| 37..39  | reserved |

This guarantees scenario 13 (replay re-entrancy) leaves no stale sprites.

---

## 16. Asset Pipeline

- Tool: **`png2asset`** (ships with GBDK-2020). Invoked by `just assets`.
- Source PNGs in `res/`. Naming: snake_case + purpose suffix (`_tiles.png` for tilesets, `_map.png` for full screen maps).
- PNGs use **exactly 4 colors** matching the DMG palette: white `#FFFFFF`, light grey `#AAAAAA`, dark grey `#555555`, black `#000000`. `-keep_palette_order` ensures palette index matches BGP index.
- Generated `*.c`/`*.h` written to `build/gen/` and added to the link automatically by the source-glob in `just build`.
- `res/README.md` documents naming + the 4 hex colors so designers/coders can produce conformant PNGs.

**Asset list for MVP**:
| File | Contents | Tiles |
|---|---|---|
| `res/bg_tiles.png` | font (~96 ASCII glyphs) + map tiles (ground, path H, path V, 4 corners, computer ×4 full + ×4 damaged, HUD chrome) | ≤ 120 |
| `res/sprites.png`  | cursor frame A, cursor frame B, ghost cursor, tower, bug walk A, bug walk B, projectile, (1 spare) | 8 |
| `res/title_map.png` | full 20×18 title screen (uses bg_tiles font + decorative tiles) | — |
| `res/win_map.png`   | full 20×18 "SYSTEM CLEAN :)" screen | — |
| `res/lose_map.png`  | full 20×18 "KERNEL PANIC X_X" screen | — |

---

## 17. Resource Budget

| Resource | Budget | Used (estimate) | Headroom |
|---|---|---|---|
| BG tiles (VRAM `0x8800–0x97FF`) | 256 | 120 (font 96 + map 24) | 136 |
| Sprite tiles (`0x8000–0x8FFF`) | 256 (shared 384) | 8 | huge |
| **Total VRAM tiles** | **384** | **128** | **256** |
| OAM | 40 | 1 cursor + 12 enemies + 8 projectiles + 16 towers = **37 worst case** | 3 |
| Sprites/scanline | 10 | ≤ 7 (path geometry + tower row) | 3 |
| ROM | 32 KB | est. 12–18 KB (code 6–10 KB + assets 6–8 KB) | comfortable |
| WRAM | 8 KB | est. <1 KB (pools+state ~600 B) | huge |

OAM stress-test scenario (wave 3 stalled, 12 bugs alive + 8 in-flight projectiles + 16 placed towers + cursor) = 37 OAM, ≤ 40. Per-scanline limit (10 sprites) is held by the path geometry: on any given scanline only one path-row segment is visible, so even a saturated row holds ≤ 12 enemy sprites *spread across columns* — but two enemies share a scanline only when on the same row. The 60-frame W3 spawn interval combined with 0.5 px/frame speed means consecutive bugs are spaced ≥ 30 px apart on the path; with the ≥ 8-tile-wide horizontal segments, a worst case row holds ~5 bugs on the same scanline (≤ 7 OAM/scanline including a tower on the row).

---

## 18. Subtask Breakdown (ordered)

Each subtask is small (≈ ½–1 day), independently reviewable, and ends with a testable artifact.

1. ⬜ **Repo skeleton & gitignore** — Create dirs, `.gitignore` (`build/`, `vendor/gbdk/`), `README.md`. Done when `tree -L 2` matches §2.
2. ⬜ **Justfile + GBDK download (`just setup`)** — Implement `setup`, `clean`, `clean-all` per §4 (using shebang recipes). Done when `just setup` on a fresh checkout downloads GBDK 4.2.0 to `vendor/gbdk/` and prints `lcc -v` output.
3. ⬜ **Placeholder assets** — Create the five required PNGs in `res/` as minimal valid stubs using exactly the 4 DMG palette colors (§16): `bg_tiles.png` (single 8×8 white tile), `sprites.png` (one 8×8 black square), `title_map.png` / `win_map.png` / `lose_map.png` (160×144 all-white). These exist solely so `just assets` succeeds during early subtasks; they are replaced by final art in subtask 17. Done when `just assets` produces all `build/gen/*.c` files without error.
4. ⬜ **Hello-world ROM (`just build` + `just run`)** — Minimal `src/main.c` that sets BGP and prints "GBTD" via `printf`. Wire `build`/`run`/`check` justfile targets. Done when `just run` opens mGBA showing "GBTD" and `just check` passes (size ≤ 32 KB, cart byte = 0x00).
5. ⬜ **`types.h` + `gfx.c` + palette init** — Define `u8/u16/i8/i16/fix8` per Appendix A. Implement `gfx_load_common()` (loads bg_tiles + sprites into VRAM) and `gfx_hide_all_sprites()`. Done when main calls these instead of `printf` and the screen still shows the placeholder content. **Drop `<stdio.h>` from the project after this subtask** (only `main.c` may include it during subtask 4).
6. ⬜ **`input.c`** — Joypad polling with edge detection + 12-frame initial / 6-frame repeat. Done when a test sprite moves around with D-pad in mGBA.
7. ⬜ **Final `bg_tiles.png` + `map.c`** — Replace placeholder `bg_tiles.png` with the real shared tileset (font ~96 + map tiles + computer + HUD chrome per §17). Author `map_tiles[]` + `map_class[]` + waypoints array (verbatim from §6 / `mvp-design.md` §3). Implement `map_load()`. Done when entering a stub PLAYING state shows the map per design.
8. ⬜ **`game.c` state machine + `title.c`** — TITLE state with final `title_map.png` rendered, START → PLAYING (renders the map from subtask 7). "PRESS START" blink per §15. Done when boot shows title and START enters PLAYING with the map visible. *Depends on subtask 7 (shares `bg_tiles.png`).*
9. ⬜ **`hud.c` + `economy.c`** — Render HUD row, init HP=5/E=30, dirty-flag redraw. Done when HUD shows `HP:05  E:030  W:1/3` and updates when test code calls `economy_damage(1)`.
10. ⬜ **`cursor.c`** — Move cursor on grid; valid/invalid visual; respect `map_class`; initial position (1,0) play-field-local. Done when D-pad moves cursor over the map and brackets dim/blink-faster on path tiles.
11. ⬜ **`towers.c`** (placement only, no firing yet) — A places a tower sprite (OAM slot 1+i), deducts 10 energy, rejects invalid tiles / insufficient energy / occupied tiles. Done when towers are visually placed and HUD energy ticks down by 10.
12. ⬜ **`enemies.c`** — Pool of 12, spawn API, waypoint walk, damage computer at end. Test harness: spawn 1 bug on entering PLAYING. Done when 1 bug walks the full path and HP decrements by 1 on arrival.
13. ⬜ **`projectiles.c`** — Pool of 8, motion, hit detection, kill/bounty callback. Done when manually firing from a fixed point reduces enemy HP and (on kill) awards 3 energy.
14. ⬜ **Tower firing** — Wire towers to projectiles + targeting (first-by-`wp_idx` policy). Done when a placed tower auto-kills a passing bug.
15. ⬜ **`waves.c`** — Wave script + delays + HUD wave update. Done when leaving the player idle plays out W1→W2→W3 with correct counts and timings.
16. ⬜ **Win/lose transitions + `gameover.c`** — Detect win/lose, render `win_map.png`/`lose_map.png` (final art), START → TITLE with full reset (every `*_init()` calls its OAM-hide routine per §15). Done when both endings are reachable and looping back to TITLE works with no stale sprites.
17. ⬜ **Final asset polish** — Replace remaining placeholder PNGs (`sprites.png`, `title_map.png`, `win_map.png`, `lose_map.png`) with final art per design spec; verify VRAM table in §17 against `lcc -Wl-m` map output.
18. ⬜ **Acceptance run** — Fresh-clone dry-run on a second macOS account/path; record any setup friction. Done when checklist in §19 passes end-to-end.

**Parallelization**: subtasks here are written as a strict sequential chain because the shared-tileset asset pipeline (§4 / subtask 7) creates a hard dependency between most graphical work. After subtask 9, subtasks 10/11 (cursor/towers) and 12/13 (enemies/projectiles) operate on disjoint files and pools and may be tackled in parallel by two coders.

---

## 19. Acceptance Criteria

### Setup
On a macOS machine:
```sh
brew install just mgba                          # both required
softwareupdate --install-rosetta --agree-to-license   # Apple Silicon only
git clone <repo> && cd gbtd
just setup    # downloads GBDK 4.2.0 to vendor/gbdk/
just run      # builds and opens mGBA
```
**Apple Silicon prerequisite**: GBDK-2020 macOS binaries are x86_64-only (run via Rosetta 2). `just setup` aborts with an install hint if Rosetta is missing. On Intel Macs Rosetta is not needed.

### Scenarios

| # | Scenario | Steps | Expected |
|---|---|---|---|
| 1 | Fresh-clone build | `just setup && just build` | `build/gbtd.gb` produced, ≤ 32 KB, `just check` exits 0 |
| 2 | Boot to title | `just run` | mGBA opens at 3× scale; title screen shown per `mvp-design.md` §2; "PRESS START" toggles visible/hidden every 30 frames (verify with mGBA frame counter, period = 60 ± 2 f) |
| 3 | Title → play | Press START on title | Transitions to PLAYING; map + HUD visible; `HP:05 E:030 W:1/3`; cursor visible at default tile |
| 4 | Cursor invalid feedback | D-pad cursor onto a path tile | Brackets dim to light grey, blink doubles to 2 Hz |
| 5 | Place a tower | Move cursor to a buildable tile, press A | Tower sprite appears; HUD shows `E:020`; cannot place a second tower on the same tile |
| 6 | Insufficient energy | Spend energy below 10, press A on valid tile | No tower placed; HUD unchanged |
| 7 | Wave plays out | Idle for ~12 s | W1 spawns 5 bugs at the spawn edge; bugs walk the path; reaching computer decrements HP |
| 8 | Tower kills bug | Place a tower within 3 tiles of the path before W1 spawns | Tower fires every 60 frames; bugs lose HP; on death HUD energy increases by 3 |
| 9 | Wave advance | After W1's last bug despawns | After 3 s delay HUD shows `W:2/3`; W2 begins |
| 10 | Win | Survive all 3 waves | Win screen ("SYSTEM CLEAN :)") appears; START returns to title with full reset |
| 11 | Lose | Let HP reach 0 | Lose screen ("KERNEL PANIC X_X") appears; START returns to title with full reset |
| 12 | Replay | From game-over, START → play again | Energy/HP/wave reset to start values; previously placed towers cleared |
| 13 | Re-entrancy | Lose, return to title, immediately START | No state leakage (no leftover sprites, HP=5 not 0) |
| 14 | OAM ceiling (deterministic) | Place towers at play-field tiles `(7,3)`, `(9,3)`, `(14,8)`, `(16,7)` (all buildable, each within 3 tiles of the path); idle through W3 | No sprite flicker on path rows (play-field rows 2, 9, and col 8/15 columns); verify in mGBA `Tools → View sprites` that ≤ 8 sprites are on any one scanline |
| 15 | ROM size guard | `wc -c build/gbtd.gb` | ≤ 32768 bytes |

Visual checks (mGBA):
- Use `Tools → View palette` to confirm BGP = `0xE4` (0,1,2,3 → light to dark per design).
- Use `Tools → View tiles` to confirm BG tile bank ≤ 128 tiles used.
- Use `Tools → Frame counter` to verify wave timings (180-frame inter-wave delay; first spawn ≈ 60 frames after entering PLAYING).

---

## 20. Constraints

- DMG only (no CGB-specific registers).
- 32 KB ROM no-MBC; if the linker fails to fit, **first** strip unused font glyphs (font is 96 tiles — drop tiles for unused punctuation), **then** revisit only as a follow-up spec.
- All movement/physics in 8.8 fixed-point (`fix8 = i16`); avoid 32-bit math in tight loops.
- No `malloc` — all pools are `static`.
- No floating point.
- All draw calls (`set_bkg_tiles`, `set_sprite_tile`, OAM updates) must run during VBlank — done implicitly because update happens before `wait_vbl_done()` and GBDK shadow-OAM is flushed by the lib.
- Code style: K&R braces, snake_case, `static` everything that isn't in a header.

---

## 21. Decisions (made in this spec)

| Decision | Options | Choice | Rationale |
|---|---|---|---|
| GBDK install | Homebrew tap / manual tarball into `vendor/` | Manual tarball into `vendor/gbdk/` | No official tap; pinning ensures reproducibility; self-contained / removable |
| GBDK version | 4.2.0 / latest | **4.2.0 pinned** | Reproducible builds; bump deliberately |
| Sprite size | 8×8 / 8×16 | 8×8 | Bug/cursor/projectile fit in 8×8; mixing sizes complicates LCDC and OAM math |
| Tower rendering | BG-tile swap / OAM sprite | OAM sprite | Keeps map data immutable; trivial to remove on game reset |
| Targeting policy | nearest / first / last / weakest | First (highest `wp_idx`) | Classic TD policy; easiest to reason about; zero ties in axis-aligned path |
| Projectile homing | follow target / fire-and-forget | Fire-and-forget after target death | Simpler logic; avoids re-acquisition cost; aesthetic loss negligible |
| Pool sizes | various | Enemies 12 / Projectiles 8 / Towers 16 | Enemies 12 covers the worst-case wave-3 stall (8 alive + 4 margin) without losing waves; projectiles 8 covers all towers firing on the same frame; towers 16 is generous given the 30/10 = 3 starting placements + ~10 affordable from kill bounties |
| Tick rate | per-frame / fixed-step decoupled | Per VBlank, no decoupling | Simpler; DMG framerate is rock-stable |
| Pause in MVP | include / defer | Defer (iteration 3 item 22) | Roadmap explicitly scopes pause out of MVP |
| Bounty per kill | 1/2/3/5 | **3** | 5 kills funds a second tower; matches W1 size — creates one mid-W1 placement decision |
| Computer HP | 3/5/10 | **5** | Allows a few path-throughs before loss; teaches consequences without harshness |
| Tower cost | 5/10/15 | **10** | Start energy 30 → 3 towers max upfront; forces sequencing |
| Wave script | designer-tweakable file / hardcoded array | Hardcoded `const wave_t[3]` | MVP scope; no runtime parser needed |
| Map encoding | RLE / metatiles / raw | Raw `u8[20*17]` + parallel class array | 680 B is trivial; clearer for one map |
| Asset tool | png2asset / hand-coded `.c` arrays | `png2asset` for everything | Ships with GBDK; uniform pipeline; designer-iterable |
| HUD position | top / bottom | **Top** (per design) | Design rationale: keeps eye sweeping into action toward bottom-right computer |

---

## 22. Review

### Adversarial review (round 1) — addressed
| # | Severity | Finding | Resolution |
|---|---|---|---|
| 1 | Critical | GBDK macOS asset is `.tar.gz`, not `.zip`; `unzip` would fail | §3, §4: switched URL to `gbdk-macos.tar.gz`, replaced `unzip` with `tar -xzf` |
| 2 | High | `MAX_ENEMIES=8` exactly equals worst-case alive in W3 (path = 30 tiles, not 19) | §7: bumped to 12 with explicit math; OAM table updated (worst case 33/40) |
| 3 | High | png2asset flags partly invalid (`-map`, `-sw/-sh` redundant with `-spr8x8`, `-use_map_attributes` is CGB-only) | §4: cleaned to `-keep_palette_order -noflip` for BG, `-spr8x8` for sprites, `-map -source_tileset` for full-screen maps |
| 4 | High | Title/win/lose maps each generated their own tileset → VRAM blowup or garbled screens | §4 + §15: all three full-screen maps now share `bg_tiles.png` via `-source_tileset`, emitting only tilemap arrays |
| 5 | Medium | "PRESS START" blink described as both BG and sprite, no measurable test | §15 + scenario 2: BG-tile toggle every 30 f; scenario verifies via mGBA frame counter |
| 6 | Medium | OAM hygiene on game reset unspecified; stale sprites would persist | §15: explicit per-module OAM slot ranges + `_init()` must call `hide_sprite` |
| 7 | Medium | Cursor initial position unspecified | §6: cursor starts at play-field `(2,2)`, guaranteed `TC_GROUND` |
| 8 | Medium | `lcc` defaults don't write a valid header checksum | §4: added `-Wm-yC -Wm-yt0x00 -Wm-yo1`; `just check` validates cart-type byte |
| 9 | Medium | `just` recipes need `$$VAR` to avoid interpolation | §4: added `set shell := ["bash","-ceu","-o","pipefail"]` so recipes run in a single shell where `$VAR` works directly |
| 10 | Medium | Energy cap stated three different ways | §13: settled on `MAX_ENERGY = 255` |
| 11 | Minor | `TILE(n)` macro signed-promotion risk | Appendix A: macro casts `n` to `i16` first |
| 12 | Minor | Subtask 14 OAM check not deterministic | Scenario 14: now lists exact tower coords + which scanlines to inspect |
| 13 | Minor | `<stdio.h>` would inflate ROM if leaked | §22 self-review note: only `main.c` may include `<stdio.h>` and only during subtask 3; removed by subtask 4 |

### Self-review (round 0) — addressed
- *Risk*: 12 bugs in W3 with 8-pool overflow → wave never finishes. **Resolved**: spawn interval 60 f vs. ~40 f path traversal at 0.5 px/frame over ~150 px = 300 f → max ~5 alive simultaneously; pool of 8 is safe with margin. Pool-full path retries every 8 frames (no deadlock).
- *Risk*: Tower drawn as OAM means up to 16 OAM consumed. **Resolved**: budget table verified; even worst-case 31 OAM ≤ 40.
- *Risk*: GBDK 4.2.0 macOS download URL may change. **Resolved**: pinned by exact version+filename; if 404, `just setup` exits with a clear error, and bumping the constant is a 1-line change.
- *Risk*: Apple Silicon Rosetta dependency. **Resolved**: `just setup` checks and prints a one-line install hint.
- *Risk*: `printf` pulls in heavy stdio. **Resolved**: use only in subtask 3 (hello-world); replaced by direct `set_bkg_tiles` from subtask 4 onward. Only `src/main.c` may `#include <stdio.h>`, and only until subtask 4 lands.

### Cross-model validation (round 2) — addressed
| # | Severity | Finding | Resolution |
|---|---|---|---|
| 1 | Critical | `set shell` does not collapse `just` recipes into one shell — multi-line `if` blocks would break | §4: rewrote `setup`/`assets`/`build`/`check` as **shebang recipes** (`#!/usr/bin/env bash` + `set -euo pipefail`); explanatory note updated |
| 2 | High | Apple Silicon Rosetta requirement understated as "hint only" | §3 + §4 + §19: `just setup` now hard-fails on arm64 without Rosetta; §19 adds `softwareupdate --install-rosetta` as an explicit prereq |
| 3 | High | Map waypoints / cursor start in §6 contradicted `mvp-design.md` (path is on play-field row 2) | §6: replaced with verbatim waypoints from design spec; cursor start moved to play-field `(1,0)` (guaranteed buildable) |
| 4 | High | Subtask 3 (hello-world) needed assets but they didn't exist yet; subtasks 6/7 were marked parallel despite shared `bg_tiles.png` dependency | §18: inserted new subtask 3 "Placeholder assets" (minimal stub PNGs); rewrote sequence to make the shared-tileset dependency explicit; removed false parallelization claims |
| 5 | Medium | Pool sizes inconsistent across §7/§9/§17/§21/Appendix B and `memory/conventions.md` | Settled on 12/8/16 enemies/projectiles/towers; updated §17 budget (37 OAM worst case), §21 decisions table, Appendix B; `memory/conventions.md` updated separately |
| 6 | Medium | Input auto-repeat timing diverged between mvp.md (24/12) and mvp-design.md (12/6) | §12: switched mvp.md to design's 12/6 |

---

## Appendix A — `types.h` sketch

```c
#ifndef GBTD_TYPES_H
#define GBTD_TYPES_H
#include <gbdk/platform.h>
typedef UINT8  u8;
typedef UINT16 u16;
typedef INT8   i8;
typedef INT16  i16;
typedef i16    fix8;          // 8.8 fixed point
#define FIX8(n)        ((fix8)((i16)(n) << 8))
#define FIX8_INT(f)    ((i8)((f) >> 8))
#define TILE_TO_FIX(t) FIX8((i16)(t)*8 + 4)
typedef struct { fix8 x, y; } fix8_point;
#endif
```

## Appendix B — Pool size cheat sheet

| Pool | Size | Bytes/entry | Total bytes |
|---|---|---|---|
| enemies | 12 | 8 | 96 |
| projectiles | 8 | 7 | 56 |
| towers | 16 | 4 | 64 |
| waypoints | 6 | 4 | 24 |
| **Total pool RAM** | | | **208 B** |

Plus map (340 B tiles + 340 B class) + HUD state (~8 B) + economy/wave state (~16 B) ≈ **920 B** of WRAM.
