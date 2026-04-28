# GBTD — Iteration 2 ("Core Depth") Implementation Spec

> Visual spec sibling: **`specs/iter2-design.md`** (robot sprite, firewall tile, menu glyphs, HUD layout).
> Roadmap scope: roadmap items #10–15 (second enemy, second tower, upgrade/sell, passive income, expanded waves, SFX).
> Builds on `specs/mvp.md` and the as-shipped MVP code in `src/`.

This spec is the single source of truth for the iteration-2 coder pass. Every numeric value, struct shape, file impact, and decision is fixed here — no TBDs, no open questions.

---

## 1. Problem

Add depth to the MVP playable slice without breaking any decision in `memory/decisions.md`. The result must remain a single 32 KB no-MBC DMG ROM that builds via `just run` on a fresh macOS clone, plays through 10 waves of mixed enemies with two tower types, an upgrade/sell loop, passive economy, and audible SFX.

Out of scope (defer to iteration 3): music, second map, third enemy/tower, SRAM, pause menu.

---

## 2. Module Impact Map

| File | Status | Change |
|------|--------|--------|
| `src/main.c`            | TOUCHED | call `audio_init()` after `gfx_init()`. |
| `src/gtypes.h`          | TOUCHED | new tuning constants (see §10), enemy/tower type enums, `MAX_WAVES = 10`. |
| `src/game.{h,c}`        | TOUCHED | `playing_update` calls `economy_tick()`, `audio_tick()`, `menu_update()` and gates entity updates while menu open; `playing_render` calls `menu_render()`. |
| `src/towers.{h,c}`      | TOUCHED | tower has `u8 type, level`; stats lookup; `towers_try_place(tx,ty,type)`; new `towers_index_at(tx,ty)`, `towers_get(idx)`, `towers_upgrade(idx)`, `towers_sell(idx)`. |
| `src/enemies.{h,c}`     | TOUCHED | enemy has `u8 type`; stats lookup; `enemies_spawn(type)`; new `enemies_bounty(idx)`. Robot uses sprite tiles `SPR_ROBOT_A/B`. |
| `src/waves.{h,c}`       | TOUCHED | new event-list wave format (10 waves, mixed comp); HUD shows `W:NN/NN`. |
| `src/economy.{h,c}`     | TOUCHED | `economy_tick()` — passive income trickle. Tracks per-tower spent for sell refund (via towers calling `economy_award()`). |
| `src/hud.{h,c}`         | TOUCHED | new layout `HP:5 E:030 W:01/10 X` (X = selected tower indicator); `hud_mark_t_dirty()`; wave field widened to 2 digits. |
| `src/cursor.{h,c}`      | TOUCHED | input handling stays in `cursor_update` for movement; A-press routing moved into `game.c::playing_update` (already there); cursor exposes `cursor_blink_pause(bool)` so menu can show steady highlight. |
| `src/projectiles.{h,c}` | TOUCHED (minimal) | `projectiles_fire()` gains `u8 damage` parameter (was hard-coded `PROJ_DAMAGE`); existing call sites pass tower-stat damage. |
| `src/audio.{h,c}`       | **NEW**  | SFX engine: 6 effects on channels 1/2/4. |
| `src/menu.{h,c}`        | **NEW**  | Upgrade/sell sprite overlay state machine. |
| `tools/gen_assets.py`   | TOUCHED | add `TILE_TOWER_2` BG tile, `SPR_ROBOT_A/B` sprite tiles, `SPR_GLYPH_*` sprite-bank font subset (18 glyphs). |
| `res/assets.{c,h}`      | REGENERATED | output of `just assets`. |
| `src/map.{h,c}`         | UNTOUCHED | map geometry, classes, waypoints unchanged. |
| `src/input.{h,c}`       | UNTOUCHED | reuses existing edge / repeat API; B-button polled by game.c. |
| `src/gfx.{h,c}`         | UNTOUCHED | tile bank counts auto-flow through `MAP_TILE_COUNT` / `SPRITE_TILE_COUNT` defines emitted by `gen_assets.py`. |
| `src/title.{h,c}`       | UNTOUCHED | |
| `src/gameover.{h,c}`    | UNTOUCHED | (win/lose stingers play via `audio_play()` from `game.c::enter_gameover`, no gameover.c change.) |
| `tests/test_math.c`     | TOUCHED | new tests: sell refund, passive income tick, wave-event indexing, tower-stats lookup. |
| `justfile`              | UNTOUCHED | source-glob covers new files automatically. |

**Parallelism breakdown** — see §13.

---

## 3. Robot Agent (#10)

**Sprite**: 8×8, 2 walk-anim frames `SPR_ROBOT_A/B` per `iter2-design.md §1`. Animate every 16 frames (same cadence as bug).

**Stats** (constants in `gtypes.h`):
| Const | Value | Note |
|---|---|---|
| `ROBOT_HP`     | 6      | 2× bug |
| `ROBOT_SPEED`  | `0x00C0` (0.75 px/frame) | 1.5× bug |
| `ROBOT_BOUNTY` | 5      | vs bug = 3 |
| `BUG_BOUNTY`   | 3      | unchanged (was hard-coded `KILL_BOUNTY`) |

**Capture-before-damage rule (F5)**: `enemies_apply_damage()` already clears `alive` on kill (and slots can be reused by `enemies_spawn()` later in the same frame, retargeting the type byte). Therefore the bounty MUST be read **before** the damage call:
```c
/* projectiles.c kill path (post-fix): */
u8 b = enemies_bounty(p->target);
if (enemies_apply_damage(p->target, p->damage)) {
    economy_award(b);
    audio_play(SFX_ENEMY_DEATH);
} else {
    audio_play(SFX_ENEMY_HIT);
}
```

**Type plumbing** — table-driven, keyed by `u8 type`:
```c
/* enemies.c */
enum { ENEMY_BUG = 0, ENEMY_ROBOT = 1, ENEMY_TYPE_COUNT };
typedef struct { u8 hp; fix8 speed; u8 bounty;
                 u8 spr_a; u8 spr_b; } enemy_stats_t;
static const enemy_stats_t s_enemy_stats[ENEMY_TYPE_COUNT] = {
    { BUG_HP,   BUG_SPEED,   BUG_BOUNTY,   SPR_BUG_A,   SPR_BUG_B   },
    { ROBOT_HP, ROBOT_SPEED, ROBOT_BOUNTY, SPR_ROBOT_A, SPR_ROBOT_B },
};
```

`enemy_t` adds **one byte**: `u8 type`. `enemies_spawn(u8 type)` returns `bool`. Existing `step_enemy()` reads speed/sprites from `s_enemy_stats[e->type]`.

`enemies_bounty(u8 idx)` returns `s_enemy_stats[s_enemies[idx].type].bounty`. `projectiles.c` calls this on kill instead of the hard-coded `KILL_BOUNTY`.

**Pool growth (F6)**: `MAX_ENEMIES` is **bumped from 12 → 14** to accommodate W10's worst-case alive count (with revised 50-frame spawn interval, 480-frame bug traversal → ~10 alive; 14 leaves margin). OAM re-allocation:
| Slots | Owner |
|---|---|
| 0       | cursor |
| 1..14   | menu (active only when menu open) |
| 15..16  | reserved |
| 17..30  | enemies (14 slots) |
| 31..38  | projectiles (8 slots) |
| 39      | reserved |

Total worst-case OAM: 1 + 14 enemies + 8 projectiles = 23 (non-menu) or 1 + 14 menu = 15 (menu). Both ≤ 40.

`enemies.c`: `OAM_ENEMIES_BASE` changes from 17 to 17 (unchanged), but pool size grows from 12 → 14. `gtypes.h`: update `MAX_ENEMIES = 14`, `OAM_PROJ_BASE = 31`. `projectiles.c`: `OAM_PROJ_BASE` already drives off the macro — no other change.

**Resource budget**: 2 new sprite tiles (32 B). Per-scanline check: robot is vertical (cols 1..6), bug is horizontal — combined max on one scanline still ≤ 8 (well under the 10 limit). With 14-enemy pool the per-scanline math from MVP §17 still holds (bugs spaced ≥ 30 px on path → max ≈ 6 on the same horizontal segment).

---

## 4. Second Tower — Firewall (#11)

**Decision: single-target, slow, high-damage, longer range.** *Not* AoE.

**Justification.** AoE requires either (a) a per-projectile multi-target damage loop and an area-flash sprite, expanding `projectiles.c` non-trivially, or (b) instant-hit damage with no projectile, breaking the existing visual feedback model. Both push iter-2 scope. A single-shot heavy projectile is a *meaningful* counterpart to the antivirus (high burst vs. high DPS, longer reach vs. shorter cooldown) and keeps `projectiles.c` change to a one-parameter API extension. Combined with the new fast Robot enemy (which is hardest to chip down with the antivirus's 1-damage shots), the firewall fills a clear strategic role: anti-robot sniper.

**Tower stats table** (`towers.c`):
```c
enum { TOWER_AV = 0, TOWER_FW = 1, TOWER_TYPE_COUNT };
typedef struct {
    u8  cost;        /* placement cost */
    u8  upgrade_cost;
    u8  cooldown;    /* level 0 */
    u8  cooldown_l1; /* level 1 (upgraded) */
    u8  damage;      /* level 0 */
    u8  damage_l1;   /* level 1 */
    u8  range_px;    /* level 0 (== level 1; upgrade is dmg+rate, not range) */
    u8  bg_tile;     /* TILE_TOWER or TILE_TOWER_2 */
    u8  hud_letter;  /* 'A' or 'F' */
} tower_stats_t;

static const tower_stats_t s_tower_stats[TOWER_TYPE_COUNT] = {
    /* AV */ { .cost=10, .upgrade_cost=15, .cooldown=60, .cooldown_l1=40,
               .damage=1, .damage_l1=2, .range_px=24,
               .bg_tile=TILE_TOWER,   .hud_letter='A' },
    /* FW */ { .cost=15, .upgrade_cost=20, .cooldown=120, .cooldown_l1=90,
               .damage=3, .damage_l1=4, .range_px=40,
               .bg_tile=TILE_TOWER_2, .hud_letter='F' },
};
```

**Numbers rationale**:
- AV DPS L0 = 1·(60/60) = 1.0/s. AV DPS L1 = 1.5/s.
- FW DPS L0 = 3·(60/120) = 1.5/s. FW DPS L1 ≈ 2.67/s.
- AV TTK on Robot (HP6) ≈ 6 s; FW TTK ≈ 4 s. FW is the answer to robots.
- FW range 40 px = 5 tiles vs. AV's 3 tiles → coverage trade-off (you build fewer FWs).

**Tower struct extension** (`towers.c`):
```c
typedef struct {
    u8 tx, ty;
    u8 cooldown;
    u8 alive;
    u8 dirty;
    u8 type;     /* TOWER_AV | TOWER_FW */
    u8 level;    /* 0 | 1 */
    u8 spent;    /* cumulative energy spent (cost + upgrade if any).
                    Used by sell refund. Max value = 15 + 20 = 35, fits u8. */
} tower_t;
```
Per-tower size grows from 5 → 8 bytes; pool RAM 16×8 = 128 B (was 80 B). Trivial.

**API changes**:
```c
bool towers_try_place(u8 tx, u8 ty, u8 type);
i8   towers_index_at(u8 tx, u8 ty);   /* -1 if none */
const tower_stats_t *towers_stats(u8 type);
u8   towers_get_type(u8 idx);
u8   towers_get_level(u8 idx);
u8   towers_get_spent(u8 idx);
bool towers_can_upgrade(u8 idx);      /* level==0 && energy ≥ upgrade_cost */
bool towers_upgrade(u8 idx);          /* spends, sets level=1, +spent */
void towers_sell(u8 idx);             /* refunds spent/2, frees slot, schedules
                                         BG tile clear back to TILE_GROUND */
```

`towers_render()` extension: when freeing a slot via sell, push a one-shot BG write to restore `TILE_GROUND` at that tile (1 extra `set_bkg_tile_xy` per sell; fits VBlank budget). Implemented via a 16-entry `s_clear_pending` bitmap of slot indices plus an `s_cleared_tiles[16]` ring of `(tx,ty)` recorded at sell time.

**Sell-pending data structure (R2 fix)** — exact representation and lifecycle:
```c
/* towers.c — file scope */
typedef struct { u8 tx, ty; } clear_tile_t;
static u16          s_pending_mask;            /* bit i = clear pending for slot i */
static clear_tile_t s_pending_tiles[MAX_TOWERS]; /* recorded at sell time */
```
- `towers_init()` MUST zero both: `s_pending_mask = 0; memset(s_pending_tiles, 0, sizeof s_pending_tiles);` — guarantees no stale clears across game-state resets (lose → title → playing).
- `towers_sell(idx)`: set bit `idx` in `s_pending_mask`; record `s_pending_tiles[idx] = {tx,ty}`; mark `s_towers[idx].alive = 0`; refund energy.
- `towers_try_place(tx,ty,type)`: before slot allocation, scan `s_pending_mask` and for any set bit whose `s_pending_tiles[i]` matches `(tx,ty)`, clear that bit (the new placement supersedes the clear).
- `towers_render()`: drain phase — find the lowest set bit in `s_pending_mask`, write `TILE_GROUND` at its recorded coord, clear the bit. Drain at most 1 per frame. Then iterate alive slots and write `dirty` ones, also at most 1 per frame (BG-write budget §9).

**Tower-select UI** (B button cycles type):
- B button pressed (edge) → `s_selected_type ^= 1` (toggle AV ↔ FW).
- HUD letter at col 19 reflects the selection (`'A'` / `'F'`); marked dirty by `hud_mark_t_dirty()`.
- A press uses `s_selected_type` for placement OR opens menu if cursor is on a tower (see §5).
- B as cycler is the natural fit per `mvp-design.md §5` ("B reserved for future menu") — it was deliberately a no-op in MVP.

`game.c::playing_update` becomes:
```c
if (menu_is_open()) {
    menu_update();
} else {
    cursor_update();
    if (input_is_pressed(J_B)) { game_cycle_tower_type(); }
    if (input_is_pressed(J_A)) {
        i8 idx = towers_index_at(cursor_tx(), cursor_ty());
        if (idx >= 0) {
            menu_open(idx);
            /* Early return so the just-opened menu freezes gameplay
             * for THIS frame too — otherwise enemies/projectiles/waves
             * would tick once and re-emit OAM after menu_open hid them. */
            economy_tick();
            audio_tick();
            return;
        }
        towers_try_place(cursor_tx(), cursor_ty(), s_selected_type);
    }
    towers_update();
    enemies_update();
    projectiles_update();
    waves_update();
}
economy_tick();   /* runs even with menu open — design choice §6, D19 */
audio_tick();     /* runs always so SFX continue during menu */
```

**Why entities are gated by menu state**: opening the menu draws 14 sprites in OAM 1..14, the same range as the now-reserved tower OAM slots. With enemies (17..28) and projectiles (29..36) still moving, sprites-per-scanline could spike (menu is 2 rows tall × 7 cols → 7 sprites per menu scanline) and combine with enemies at the same screen Y to exceed 10/scanline. **Freezing enemies + projectiles while the menu is open eliminates this risk and is invisible to the player as a "pause" because the menu interaction is intentionally modal.** This is *not* the iter-3 pause feature (no pause UI, no Start binding, no resume affordance — closing the menu resumes automatically).

---

## 5. Upgrade / Sell Menu (#12)

**Trigger**: A on cursor tile that contains a tower (`towers_index_at >= 0`).

**Visual**: 14 sprite OAM slots (**slots 1..14**, formerly tower-reserved per MVP convention) arranged 2×7 floating adjacent to the selected tower. See `iter2-design.md §5` for layout. *(F1 resolution: this spec and `iter2-design.md` were inconsistent — slots 1..14 is the chosen range. `iter2-design.md §5` will be updated to match in subtask C3.)*

```
row 0:  > U P G : N N
row 1:    S E L : N N
```

`>` is the selection cursor (toggles row with UP/DOWN). `NN` digits:
- UPG line: upgrade cost (`s_tower_stats[type].upgrade_cost`, e.g. 15 / 20).
  - If already at level 1, the UPG line text becomes `UPG:--` (using a new `-` glyph; see asset additions below). Pressing A on UPG line in this state is ignored (menu stays open — the player can still press DOWN to SEL or B to cancel).
- SEL line: refund value = `towers_get_spent(idx) / 2`.

**Glyph set (R2 fix)**: 19 sprite-bank glyphs total — `U P G S E L : > 0..9` (18) plus `-` (`SPR_GLYPH_DASH`, used for the `UPG:--` max-level state). `gen_assets.py`'s FONT dict already has `-`; the new `glyph_to_sprite('-')` call adds 1 tile. Updates §11 sprite-tile total: 28 → 29 (still trivially within 256-bank).

**Anchor rule** (per `iter2-design.md §5`):
```
Mx = clamp(tower_screen_px_x - 24, 0, 160-56)        /* center 56-wide menu */
My = (pf_row >= 2) ? (tower_screen_px_y - 16)        /* above */
                   : (tower_screen_px_y + 8)         /* below if near top */
```

**Inputs (menu mode)**:
- `UP/DOWN` (with input-repeat): toggle selection between UPG and SEL lines.
- `A`: confirm selected action.
  - UPG: if `towers_can_upgrade(idx)` → `towers_upgrade(idx)`; play `SFX_TOWER_PLACE`; close menu. If already L1 (text shows `UPG:--`): A is silently ignored, menu stays open.
  - SEL: `towers_sell(idx)`; play `SFX_TOWER_PLACE` (reused — short positive blip); close menu.
- `B`: cancel; close menu.

**`menu.{h,c}` API**:
```c
void menu_init(void);
void menu_open(u8 tower_idx);
void menu_close(void);
bool menu_is_open(void);
void menu_update(void);   /* input + state */
void menu_render(void);   /* sprite OAM writes; idempotent */
```

Internal state:
```c
static bool s_open;
static u8   s_tower_idx;
static u8   s_sel;        /* 0 = UPG, 1 = SEL */
```

**Sprite-tile content** (per `iter2-design.md §3`): 18 new sprite-bank glyph tiles `SPR_GLYPH_U`, `..._P`, `..._G`, `..._S`, `..._E`, `..._L`, `..._COLON`, `..._GT`, `..._0`..`..._9`. `gen_assets.py` reuses the existing BG `FONT` bitmaps via a new `glyph_to_sprite(ch)` helper — no manual redraw. ROM cost: 18 × 16 B = 288 B.

**Opening (F8)** — `menu_open(idx)` MUST also hide entity OAM so frozen-but-still-positioned enemies/projectiles don't share a scanline with the menu sprites:
```c
void menu_open(u8 idx) {
    s_open = true; s_tower_idx = idx; s_sel = 0;
    enemies_hide_all();      /* writes Y=0 for OAM 17..30; preserves logical state */
    projectiles_hide_all();  /* writes Y=0 for OAM 31..38;     "                  */
    cursor_blink_pause(true);
}
```
Add to `enemies.h`/`projectiles.h`: `void enemies_hide_all(void);` / `void projectiles_hide_all(void);`. Their internal `s_enemies[i]` / `s_proj[i]` arrays are untouched — the next post-close `enemies_update()` re-emits each alive slot via the existing `move_sprite` call inside `step_enemy`.

**Closing**: `menu_close()` hides OAM slots 1..14 (write Y=0); resumes entity updates (gating in `game.c` reads `menu_is_open()`); calls `cursor_blink_pause(false)`. Entities re-emit their OAM on the very next update tick.

**Wave gating clarification (F6 / D19)**: while `menu_is_open()`, `waves_update()` is **also** gated off (alongside enemies/projectiles). Otherwise the wave-spawn timer would tick down and queue spawns whose `enemies_spawn(type)` succeeds but produce no visible enemy until close, causing a pile-up at the spawn waypoint. Only `economy_tick()` and `audio_tick()` continue.

---

## 6. Passive Energy Income (#13)

**Rate**: +1 energy every **180 frames** (3 s).

**Math**: Avg wave length ≈ 30 s (10-wave script in §7 spans ~5 min total play). 5 min × (1/3) = 100 passive energy across the run. Combined with kill bounties (rough: 100 enemies × ~3.5 avg bounty = 350) the player can afford ~25 placements + several upgrades — comfortably above the 16-tower pool ceiling, but tight enough that *spending* timing still matters. Passive trickle removes the "dead time between waves" feel.

**Implementation** (`economy.c`):
```c
static u16 s_passive_timer;

void economy_init(void) {
    /* ... existing ... */
    s_passive_timer = 0;
}

void economy_tick(void) {
    s_passive_timer++;
    if (s_passive_timer >= PASSIVE_INCOME_PERIOD) {
        s_passive_timer = 0;
        economy_award(1);   /* reuses existing dirty-flag path */
    }
}
```

`gtypes.h`: `#define PASSIVE_INCOME_PERIOD 180`.

`game.c::playing_update` calls `economy_tick()` every frame, **including while menu is open** — passive economy continues to advance during a menu interaction so the player doesn't feel punished for opening it.

---

## 7. Expanded Wave Script (#14)

**10 waves. Final wave is "boss-ish" (heaviest mixed composition; no actual boss enemy).**

Composition table:
| W | Bugs | Robots | Order | Spawn interval (frames) | Inter-wave delay (frames) |
|---|------|--------|-------|------|------|
| 1 |  5 | 0 | B×5 | 90 | 180 |
| 2 |  8 | 0 | B×8 | 75 | 180 |
| 3 |  5 | 2 | B×3, R, B×2, R | 75 | 180 |
| 4 | 10 | 2 | R, B×5, R, B×5 | 60 | 180 |
| 5 |  8 | 4 | B×2, R, B×2, R, B×2, R, B×2, R | 60 | 180 |
| 6 | 12 | 4 | R×2, B×6, R×2, B×6 | 50 | 180 |
| 7 |  6 | 8 | R, R, B, R, R, B, R, R, B, R, R, B, R, R | 50 | 180 |
| 8 | 14 | 6 | (B,B,R)×6, B, B = 14B + 6R | 50 | 180 |
| 9 | 10 | 10 | strict alternating starting with B: B,R,B,R,...×10 = 10B+10R | 50 | 240 (long breath before finale) |
| 10 | 16 | 12 | R×4, B×6, R×4, B×6, R×4, B×4 ("boss") | 50 | — |

**(F6 resolution)** — Spawn intervals at 35/40/45 f would overflow the 12-enemy pool (worst-case bugs alive = 480-f traversal / 35-f spawn ≈ 13). With `MAX_ENEMIES` bumped to 14 (§3) AND a 50-frame floor on spawn intervals, worst-case alive = 480/50 ≈ 10, fitting in 14 with margin. The "boss" feel is delivered by total enemy count (28 in W10, vs. 5–14 in earlier waves) and the dense bug+robot mix, not by a faster cadence.

Total enemies: 94 bugs + 48 robots = 142.

**Wave script format**:
```c
typedef struct { u8 type; u8 delay; } spawn_event_t;
typedef struct { const spawn_event_t *events; u8 count;
                 u16 inter_delay; } wave_t;

#define MAX_WAVES 10
static const spawn_event_t W1_EV[] = { {ENEMY_BUG,90}, {ENEMY_BUG,90}, ... };
/* ... W2_EV ... W10_EV ... */
static const wave_t s_waves[MAX_WAVES] = {
    { W1_EV,  5, 180 }, { W2_EV,  8, 180 }, { W3_EV,  7, 180 },
    { W4_EV, 12, 180 }, { W5_EV, 12, 180 }, { W6_EV, 16, 180 },
    { W7_EV, 14, 180 }, { W8_EV, 20, 180 }, { W9_EV, 20, 240 },
    { W10_EV, 28, 0   },
};
```

Total ROM cost for events: ~142 × 2 B = 284 B + 10 × 4 B wave headers = ~324 B. Acceptable.

**Interpreter** (replaces existing `s_waves[3]` machinery):
```c
static u8  s_state;       /* WS_DELAY | WS_SPAWNING | WS_DONE */
static u8  s_cur;         /* 0..MAX_WAVES */
static u8  s_event_idx;   /* index into current wave's events array */
static u16 s_timer;

waves_update():
  if WS_DELAY:
    if (--s_timer == 0) { s_state = WS_SPAWNING; s_event_idx = 0; s_timer = 0; }
  else if WS_SPAWNING:
    if (s_timer) { s_timer--; return; }
    const spawn_event_t *e = &s_waves[s_cur].events[s_event_idx];
    if (enemies_spawn(e->type)) {
        s_event_idx++;
        if (s_event_idx >= s_waves[s_cur].count) {
            s_cur++;
            hud_mark_w_dirty();
            if (s_cur >= MAX_WAVES) { s_state = WS_DONE; }
            else { s_state = WS_DELAY; s_timer = s_waves[s_cur-1].inter_delay; }
        } else {
            s_timer = s_waves[s_cur].events[s_event_idx].delay;
        }
    } else {
        s_timer = 8;  /* pool full retry — same as MVP */
    }
```

`waves_get_current()` returns 1..10. `waves_get_total()` returns `MAX_WAVES`. HUD uses both for `W:NN/NN`.

**Win condition**: unchanged (`waves_all_cleared() && enemies_count() == 0`).

---

## 8. SFX (#15) — `audio.{h,c}`

**Channels used**: ch1 (square + sweep), ch2 (square), ch4 (noise). Ch3 (wave) reserved for iter-3 music.

**Hardware register init** (`audio_init()` called from `main.c` after `gfx_init()`):
```c
NR52_REG = 0x80;   /* sound master ON */
NR50_REG = 0x77;   /* both stereo terminals at max volume */
NR51_REG = 0xFF;   /* all 4 channels routed to L+R */
```

**DMG audio gotchas (F2/F3/F4 — incorporated)**:
- Trigger bit is NRx4 bit 7, **write-only**: setting bit 7 starts the channel; writing 0 to NRx4 does *not* silence it. To silence: write `NRx2 = 0x08` (DAC off — top nibble = 0, bit 3 set keeps the envelope-direction bit at "increase" so NR52's channel-on bit clears cleanly).
- NRx1 must be written before NRx4: it sets duty (square channels, top 2 bits) and length-counter load. `0x80` = 50% duty, length = 0 (length-counter unused; we shut off via DAC instead).
- For ch4 noise, NRx1 has no duty; only the bottom 6 bits as length. Use `0x3F` (length max — irrelevant since we shut off via DAC).
- Each channel's DAC must be enabled *before* trigger or the trigger is no-op: write NRx2 with non-zero top nibble (envelope volume).

**SFX type system** — two struct flavours, dispatched by channel:

```c
/* audio.h */
enum { SFX_TOWER_PLACE = 0, SFX_TOWER_FIRE, SFX_ENEMY_HIT,
       SFX_ENEMY_DEATH, SFX_WIN, SFX_LOSE, SFX_COUNT };

void audio_init(void);
void audio_play(u8 sfx_id);
void audio_tick(void);          /* call once per frame */
```

```c
/* audio.c — internal */
typedef struct {
    u8  channel;        /* 1, 2, 4 */
    u8  priority;       /* higher preempts on same channel */
    u8  nrx1;           /* duty + length (square: 0x80 = 50%; noise: 0x3F) */
    u8  envelope;       /* NRx2 — top nibble must be non-zero */
    u8  duration;       /* frames; 0 → derive from note_count*frames_per_note */
    u8  is_noise;       /* 1 = ch4 (poly counter in 'pitch') */
    u8  sweep;          /* NR10 (ch1 only); 0 if unused */
    u8  note_count;     /* 1 = single tone */
    u8  frames_per_note;
    const u16 *pitches; /* 11-bit gb freq for ch1/2; or NR43 byte for ch4 */
} sfx_def_t;

static const u16 PITCH_LO[]  = { 0x06A4 };  /* ~262 Hz, low blip */
static const u16 PITCH_FIRE[] = { 0x0783 };  /* ~440 Hz */
static const u16 NOISE_HIT[]  = { 0x0050 };  /* short sharp */
static const u16 NOISE_DEATH[]= { 0x0070 };  /* longer rumble */
static const u16 WIN_NOTES[]  = { 0x06A4, 0x0721, 0x0783, 0x07C2 }; /* C-E-G-C asc */
static const u16 LOSE_NOTES[] = { 0x07C2, 0x0721, 0x06A4, 0x05F2 }; /* desc */

static const sfx_def_t S_SFX[SFX_COUNT] = {
 [SFX_TOWER_PLACE] = { .channel=1, .priority=2, .nrx1=0x80, .envelope=0xF1, .duration=8,
                       .sweep=0x16, .note_count=1, .pitches=PITCH_LO },
 [SFX_TOWER_FIRE]  = { .channel=2, .priority=1, .nrx1=0x80, .envelope=0xA1, .duration=4,
                       .note_count=1, .pitches=PITCH_FIRE },
 [SFX_ENEMY_HIT]   = { .channel=4, .priority=1, .nrx1=0x3F, .envelope=0x71, .duration=3,
                       .is_noise=1, .pitches=NOISE_HIT },
 [SFX_ENEMY_DEATH] = { .channel=4, .priority=2, .nrx1=0x3F, .envelope=0xC2, .duration=8,
                       .is_noise=1, .pitches=NOISE_DEATH },
 [SFX_WIN]         = { .channel=1, .priority=3, .nrx1=0x80, .envelope=0xF3, .duration=0,
                       .note_count=4, .frames_per_note=10, .pitches=WIN_NOTES },
 [SFX_LOSE]        = { .channel=1, .priority=3, .nrx1=0x80, .envelope=0xF3, .duration=0,
                       .note_count=4, .frames_per_note=10, .pitches=LOSE_NOTES },
};
```

**Per-channel state**:
```c
typedef struct {
    const sfx_def_t *cur;
    u8  prio;       /* 0 = idle */
    u8  frames_left;
    u8  note_idx;
    u8  note_frames;
} channel_state_t;
static channel_state_t s_ch[3]; /* indexed 0..2 -> channels 1,2,4 */
```

**Preemption rule**: `audio_play(id)` inspects the channel for that SFX.
- If `prio == 0` (idle): start immediately.
- If new `priority >= cur->priority`: preempt and restart.
- Else: drop the request silently.

This protects the long WIN/LOSE jingle (priority 3) from being squashed by a routine FIRE (priority 1).

**Hot-path mappings** (call sites):
- `towers_try_place` (success): `audio_play(SFX_TOWER_PLACE)`.
- `towers_upgrade` / `towers_sell`: `audio_play(SFX_TOWER_PLACE)` (reused).
- `projectiles_fire` (success): `audio_play(SFX_TOWER_FIRE)`.
- `projectiles.c` post-hit: see §3 capture-before-damage block — `audio_play(SFX_ENEMY_DEATH)` on kill, `SFX_ENEMY_HIT` otherwise.
- `game.c::enter_gameover(true)`: `audio_play(SFX_WIN)` is invoked **before** `gameover_enter()` is called (F9). Rationale: `gameover_enter()` does `DISPLAY_OFF` + a 360-tile BG redraw, which blocks the main loop for several frames. Triggering the SFX first lets the channel's hardware sequencer hold the first note while `audio_tick` is paused; once the redraw completes and the main loop resumes, subsequent ticks advance the multi-note sequence. The win/lose jingle's first note may sound slightly elongated (~5–8 frames longer than 10) — acceptable.
- `game.c::enter_gameover(false)`: same — `audio_play(SFX_LOSE)` before `gameover_enter()`.

**Start sequence** (called from `audio_play` when starting/preempting):
```
ch1: NR10 = sfx.sweep; NR11 = sfx.nrx1; NR12 = sfx.envelope;
     NR13 = pitch & 0xFF; NR14 = 0x80 | ((pitch >> 8) & 0x07);   /* trigger */
ch2: NR21 = sfx.nrx1; NR22 = sfx.envelope;
     NR23 = pitch & 0xFF; NR24 = 0x80 | ((pitch >> 8) & 0x07);   /* trigger */
ch4: NR41 = sfx.nrx1; NR42 = sfx.envelope;
     NR43 = (u8)(pitch & 0xFF);  NR44 = 0x80;                    /* trigger */
```

For multi-note SFX, the same sequence runs again on each note boundary (`note_idx++`, write new pitch + re-trigger via NRx4 bit 7). Sweep (NR10) is only re-written for ch1.

**Stop sequence** (frames_left == 0): write `NRx2 = 0x08` to the channel's NRx2 register (DAC off — top nibble 0, bit 3 set so direction bit reads "increase" cleanly). NR52's channel-active bit then clears on the next read. Set `s_ch[i].prio = 0` and `s_ch[i].cur = NULL`.

**`audio_tick()`** (each frame): for each channel, decrement `frames_left`; on note boundary (`frames_left == frames_per_note * (remaining_notes)`) write next pitch via the start-sequence above (re-triggering); on `frames_left == 0` invoke the stop sequence.

**Worst-case CPU**: 3 channel state-machines × ~10 cycles/frame ≈ 30 cycles. Negligible at ~70 000 cycles/frame budget.

**ROM cost**: ~600 B code + ~120 B data.

---

## 9. HUD Changes (per `iter2-design.md §4`)

New 20-char layout (row 0):
```
col:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9
text: H P : N _ E : N N N _ W : N N / N N _ X
```
where `_` = space, `N` = digit, `X` = `'A'` or `'F'`.

- `HP:N`     cols 0..3 (1-digit HP, max 5)
- `E:NNN`    cols 5..9 (3-digit energy, zero-padded)
- `W:NN/NN`  cols 11..17 (2-digit current/total wave)
- `X`        col 19 (selected tower indicator)

Cols 4, 10, 18 are spaces.

**`hud.c` updates**:
- New `put_dec2_zero(x, y, v)` helper (already exists conceptually as `put_dec2`; OK).
- Wave field redraw writes 5 tiles (`NN/NN`) when dirty.
- New flag `s_dirty_t`; `hud_mark_t_dirty()` set by `game_cycle_tower_type()`. Render writes 1 BG tile at col 19.
- `hud_init()` reflows static labels per the layout above.

**VBlank-safety check (F10)**: worst-case render frame writes:
- HUD: 1 (HP) + 3 (E) + 5 (W) + 1 (T) + 4 (computer-damaged swap) = 14 BG writes.
- + at most 1 tower BG write per frame: `towers_render` writes EITHER one place OR one sell-clear per frame, never both. Place-then-clear race is eliminated by the de-dup in `towers_try_place` (§4 F7 resolution); a frame with a pending clear AND a new place will execute the place (skipping the cancelled clear).
- = **15 BG writes worst case**, ≤ 16 budget. Per `set_bkg_tile_xy` ≈ 70 cycles, 15 × 70 = 1050 cycles, within ~1080-cycle VBlank budget.

---

## 10. New Constants (`gtypes.h`)

```c
/* Iter 2 additions */
#define PASSIVE_INCOME_PERIOD 180   /* +1 energy every 3 s */
#define MAX_WAVES             10

#define ROBOT_HP              6
#define ROBOT_SPEED           0x00C0   /* 0.75 px/frame */
#define ROBOT_BOUNTY          5
#define BUG_BOUNTY            3        /* renames the role of KILL_BOUNTY */

#define TOWER_AV_COST         10
#define TOWER_AV_UPG_COST     15
#define TOWER_FW_COST         15
#define TOWER_FW_UPG_COST     20
#define TOWER_FW_RANGE_PX     40
#define TOWER_FW_RANGE_SQ     (TOWER_FW_RANGE_PX * TOWER_FW_RANGE_PX)
```

`TOWER_COST`, `KILL_BOUNTY`, `TOWER_RANGE_PX/SQ`, `TOWER_COOLDOWN`, `TOWER_DAMAGE` are kept as compatibility aliases for the AV stats so the diff to existing call sites stays surgical (or they may be removed in this pass — coder's choice; both are fine).

---

## 11. Resource Budget Update

| Resource | Iter-1 used | Iter-2 added | Iter-2 total | Limit |
|---|---|---|---|---|
| BG tiles  | 11 (font 96 + map 11) | +1 (TILE_TOWER_2) | 12 + 96 font = 108 | 256 |
| Sprite tiles | 8 | +2 (robot) +18 (menu glyphs) = +20 | 28 | 256 |
| OAM (worst case) | 21 (1 cur + 12 enem + 8 proj) | enemies bumped to 14 (F6); menu open: +14 (slots 1..14), but enemies & proj **frozen and OAM-hidden** during menu (§5 F8) so they don't count → menu mode = 1 + 14 = 15 OAM. Non-menu mode = 1 + 14 + 8 = 23 OAM. | 23 (max non-menu) / 15 (menu) | 40 |
| Sprites/scanline | ≤7 | menu mode is exclusive of moving entities → ≤7 worst case (menu row holds 7 sprites; no enemies on that scanline since hidden). Non-menu: 14 enemies max but spaced ≥ 30 px on path → ≤6/scanline; +1 cursor = ≤7. | ≤7 | 10 |
| ROM | ~14–18 KB est. | +audio ~720 B + menu ~500 B + waves ~450 B + tower stats ~250 B + econ tick ~80 B + 20 sprite tiles 320 B + 1 bg tile 16 B = **~2.4 KB** | ~17–21 KB | 32 KB |
| WRAM | ~920 B | tower struct +3 B/×16 = +48 B; enemy +1 B/×14 + 2 extra slots = +26 B; menu state ~16 B; audio state ~30 B | ~1.0 KB | 8 KB |

**MBC1 verdict: NOT NEEDED.** Even the high-end 21 KB estimate leaves 11 KB headroom. Iter-2 stays on 32 KB no-MBC. (MBC1 deferred to iter-3 as the roadmap intends.)

---

## 12. OAM Slot Allocation (revised; supersedes MVP `memory/conventions.md`)

| Slots | Owner | Notes |
|---|---|---|
| 0       | cursor | |
| 1..14   | menu sprites | active only when `menu_is_open()` |
| 15..16  | reserved | |
| 17..30  | enemies | 14 slots (was 12; F6 fix) |
| 31..38  | projectiles | 8 slots (shifted down 2 from MVP's 29..36 to make room for enemy growth) |
| 39      | reserved | |

`menu_close()` MUST hide slots 1..14. `menu_open()` MUST hide slots 17..38 (via `enemies_hide_all`/`projectiles_hide_all`) — see §5 F8. `cursor_init()`, `menu_init()`, `enemies_init()`, `projectiles_init()` each hide their own slots per the per-`_init` OAM-hide convention.

**`gtypes.h` deltas**:
```c
#define MAX_ENEMIES      14   /* was 12 */
#define OAM_ENEMIES_BASE 17   /* unchanged */
#define OAM_PROJ_BASE    31   /* was 29 */
```

---

## 13. Subtask Breakdown (ordered, with parallelism notes)

Coders may pick up parallel groups in any order within a group. Cross-group ordering must be sequential because of shared file edits (esp. `gtypes.h`, `gen_assets.py`, `game.c`).

### Group A — Foundations (sequential)
1. ⬜ **A1: gtypes.h constants + enemy/tower type enums** — Add all §10 constants. Done when project compiles unchanged.
2. ⬜ **A2: Asset additions in `tools/gen_assets.py`** — Add `TILE_TOWER_2`, `SPR_ROBOT_A/B`, `SPR_GLYPH_*` (18 tiles via `glyph_to_sprite()`). Update `MAP_TILE_COUNT` / `SPRITE_TILE_COUNT` defines. Done when `just assets && just build` succeeds and `res/assets.h` lists all new symbols.

### Group B — Independent feature modules (parallelizable after Group A)
3. ⬜ **B1: enemies.c — type plumbing + robot stats** *(touches: enemies.{h,c}, projectiles.c kill bounty path)* — Add `u8 type`, stats table, `enemies_spawn(type)`, `enemies_bounty(idx)`. Migrate existing `enemies_spawn()` callers (only `waves.c`) to pass `ENEMY_BUG`. Done when test harness can spawn one robot via `enemies_spawn(ENEMY_ROBOT)` and it walks the path with correct sprite + speed + HP.
4. ⬜ **B2: towers.c — type/level/spent plumbing + firewall stats** *(touches: towers.{h,c}, projectiles.c damage param)* — Add `tower_stats_t` table; `towers_try_place` takes `type`; `towers_index_at`, `towers_upgrade`, `towers_sell`, BG-clear-on-sell. Update fire site to use stat damage via `projectiles_fire(x,y,target,damage)`. Done when both tower types can be placed (via temporary B-cycle stub in game.c) and sell restores `TILE_GROUND`.
5. ⬜ **B3: audio.{h,c}** *(NEW files; disjoint)* — Implement engine + `S_SFX[]` table per §8. Done when `audio_play(SFX_TOWER_PLACE)` audibly emits a blip in mGBA from a temporary debug binding.
6. ⬜ **B4: economy.c — passive income tick** *(touches: economy.{h,c} only)* — Add `economy_tick`, `s_passive_timer`. Done when host-side test_math passes new tick test (§14).

B1, B2, B3, B4 touch disjoint files (with the exception of `projectiles.c` which is shared between B1 and B2 — see "merge note" below). They are coder-parallelizable.

**Merge note (B1 ↔ B2 in `projectiles.c`)**: B2 adds a `damage` parameter to `projectiles_fire()`. B1 changes the kill-bounty award path to read from `enemies_bounty(target_idx)`. These are non-overlapping line edits but in the same file — the second-merging coder must rebase. Estimated conflict surface: < 5 lines.

### Group C — Integrators (sequential after Group B)
7. ⬜ **C1: waves.c — 10-wave event-list interpreter** *(touches: waves.{h,c}; depends on B1)* — Replace MVP wave struct with `spawn_event_t`/`wave_t`; author all 10 wave event arrays per §7; widen HUD wave to 2 digits. Done when `waves_get_total() == 10` and waves W3..W10 spawn the documented bug/robot mix in order.
8. ⬜ **C2: hud.c — new layout + tower indicator** *(touches: hud.{h,c}; depends on A1, B2)* — Reflow row to §9 layout; add `s_dirty_t`, `hud_mark_t_dirty`, `put_dec2_zero` for wave digits. Done when scenario 5 (§15) shows correct HUD on entry to PLAYING.
9. ⬜ **C3: menu.{h,c}** *(NEW files; depends on A2, B2)* — Implement upgrade/sell sprite overlay per §5. Done when scenario 9 (§15) passes.
10. ⬜ **C4: game.c integration** *(touches: game.{h,c}, main.c; depends on B1..B4, C1..C3)* — Wire `audio_init` in main, `menu_init` in `enter_playing()` (and `enter_title()` for OAM hygiene), `economy_tick`/`audio_tick`/`menu_update` in `playing_update`, `menu_render` in `playing_render`, B-cycle, A-routes-to-menu-or-place with early-return-on-open, win/lose audio plays. Done when all scenarios in §15 pass.

### Group D — Verification
11. ⬜ **D1: Host-side regression tests** *(touches: tests/test_math.c only)* — Add tests per §14. Done when `just test` (and thus `just check`) is green.
12. ⬜ **D2: Acceptance run** — Manual playthrough on a fresh checkout. Done when §15 checklist is green.

---

## 14. Regression Tests (host-side, `tests/test_math.c`)

All new logic with non-trivial arithmetic gets a host-side test. The existing harness (cc -std=c99 -Wall) runs via `just test`. New tests:

1. **`test_sell_refund`** — `refund = spent / 2`. Cases: `spent=10 → 5`, `spent=15 → 7`, `spent=25 (10 cost + 15 upg) → 12`, `spent=35 (15 + 20) → 17`. Confirms integer division rounding.
2. **`test_passive_income_tick`** — 180 calls produce exactly 1 award; 359 calls produce 1 award; 360 calls produce 2 awards; 1800 calls produce 10 awards.
3. **`test_wave_event_index_advance`** — small synthetic wave (4 events). Verify `s_event_idx` increments per spawn, transitions to next wave at count, and respects `inter_delay`.
4. **`test_tower_stats_lookup`** — `s_tower_stats[TOWER_AV].cost == 10`, `[TOWER_FW].damage_l1 == 4`, `[TOWER_FW].range_px == 40`. (Compile-time asserts also acceptable.)
5. **`test_enemy_bounty_lookup`** — `bounty(BUG) == 3`, `bounty(ROBOT) == 5`.
6. **`test_sell_then_place_same_tile`** *(R2 fix)* — see above.
7. **`test_dist_squared_extended_range`** — TOWER_FW range 40 px; verify a target at (40,0) is in range, (41,0) out, (24,32) at exactly 40² in range, (25,33) out. Locks in the squared-distance check at the new range.

Mirror the existing test pattern (small `static` helpers + `EXPECT(cond)` macro + final `failures` exit code).

---

## 15. Acceptance Criteria

### Setup (unchanged from MVP)
```sh
brew install just mgba
softwareupdate --install-rosetta --agree-to-license   # Apple Silicon
git clone <repo> && cd gbtd
just setup
just check    # builds + runs host tests + ROM size guard
just run
```

### Scenarios

| # | Scenario | Steps | Expected |
|---|---|---|---|
| 1 | Build & host tests | `just check` | Exit 0; ROM ≤ 32 KB; host tests all pass. |
| 2 | Boot flow | `just run`; press START | Title → playing; HUD shows `HP:5 E:030 W:01/10 A`. |
| 3 | Place AV tower | Cursor on buildable tile, A | Tower appears (octagon BG tile); `E:020`; **SFX_TOWER_PLACE audible** *(MANUAL-REQUIRED)*. |
| 4 | Cycle tower type | Press B | HUD col 19 toggles `A`↔`F`; no other state change. |
| 5 | Place FW tower | Cycle to F, A on buildable tile | Firewall BG tile (brick pattern) appears; `E:005` (after also placing AV in #3); cost = 15. |
| 6 | Insufficient energy | Spend below 15, press A on valid tile with F selected | No placement; HUD unchanged. |
| 7 | Robot spawns | Idle through W1 (bugs only), then W3 | W3 includes 2 robots; robots are visually distinct (vertical silhouette + antenna), faster than bugs. |
| 8 | Bounty differentiates | Kill 1 bug → +3 energy; kill 1 robot → +5 energy | HUD energy ticks accordingly *(verify via mGBA frame counter)*. |
| 9 | Open upgrade menu | Cursor on existing tower, A | 2×7 sprite menu appears next to tower with `>UPG:NN` / ` SEL:NN`; entities visibly freeze. |
| 10 | Upgrade tower | Open menu, A on UPG | Cost deducted; menu closes; tower's fire visibly faster (verify cooldown changed via repeated shots). Re-opening menu shows `MAX :--` on UPG line. |
| 11 | Sell tower | Open menu, DOWN, A | Energy +`spent/2`; menu closes; tower BG tile reverts to `TILE_GROUND`; OAM clean (no stale menu sprites). |
| 12 | Cancel menu | Open menu, B | Menu closes; no state change; entities resume. |
| 13 | Passive income | Idle continuously for 1080 frames (= 18 s, spans the W3→W4 inter-wave delay plus partial W4 spawns) | Energy increments by exactly 6 over that span (1080 / 180 = 6), verifiable via mGBA frame counter snapshots before/after. |
| 14 | Wave progression to 10 | Play through | HUD shows `W:01/10` ... `W:10/10`; W10 features dense bug+robot mix. |
| 15 | Win | Survive all 10 waves | Win screen; **SFX_WIN jingle audible** *(MANUAL-REQUIRED)*. |
| 16 | Lose | Let HP reach 0 | Lose screen; **SFX_LOSE descending tone audible** *(MANUAL-REQUIRED)*. |
| 17 | Replay reset | START from gameover | Energy/HP/wave/tower-pool/menu-state all reset; selected tower type resets to AV. |
| 18 | OAM hygiene | Open menu, then trigger lose (HP=0 from queued damage); START to title | No stale menu sprites on title screen. |
| 19 | ROM size guard | `wc -c build/gbtd.gb` | ≤ 32768 B. |
| 20 | E2E win run | `just run`; play through to win | Both tower types placed + at least one upgrade + at least one sell during the run; both enemy types defeated; the 5 SFX triggered by win-path actions are heard: PLACE, FIRE, HIT, DEATH, WIN. |
| 21 | E2E lose run | `just run`; let HP reach 0 (don't place enough towers) | LOSE SFX heard; lose screen reached. |

**Visual checks (mGBA Tools)**:
- `View tiles` — confirm `TILE_TOWER_2` present, distinct from `TILE_TOWER`.
- `View sprites` — during menu, slots 1..14 populated; enemies (17..28) hidden.
- Frame counter — passive income period = 180 ± 2 frames between awards.

---

## 16. Constraints

- All MVP constraints from `specs/mvp.md §20` apply.
- Existing decisions in `memory/decisions.md` (BG-tile towers, frame loop split, `lcc` defaults, hand-coded asset pipeline, shebang recipes, MVP cartridge type) are **not revisited**.
- Per-frame BG writes stay ≤ 16 (§9 worst-case = 15).
- Per-scanline sprite count stays ≤ 8 (menu mode freezes enemies/projectiles to guarantee this).
- 32 KB ROM holds (§11 budget).
- No `malloc`, no float, no SRAM, no MBC, no music, no second map, no general pause, no third enemy/tower.

---

## 17. Decisions

| # | Decision | Options | Choice | Rationale |
|---|---|---|---|---|
| D1 | Firewall behavior | AoE / single-target high-damage / chain / slow | **Single-target high-damage, longer range** | AoE expands `projectiles.c` non-trivially; high-damage single-shot keeps API surface to one new param and serves the strategic role of "anti-robot sniper" — meaningful counterpart to AV's high-DPS short-range. |
| D2 | Tower-select input | B-cycle / Start panel / Select / cursor menu | **B cycles AV↔FW** | B was deliberately a no-op in MVP per `mvp-design.md §5` ("reserved for future menu"). Cycle is zero-friction for 2 types; a panel is overkill until ≥ 3 types. |
| D3 | Upgrade/sell menu rendering | BG-with-restore / DISPLAY_OFF / sprite overlay / HUD-mode reuse | **Sprite overlay (14 OAM) with entities frozen** | No display blink; uses already-reserved tower OAM slots; freezing entities removes any per-scanline risk. Not a "pause feature" — modal-only-while-menu-open. |
| D4 | Upgrade levels in scope | 0 / 1 / 2+ | **One upgrade level (L0 → L1)** | Roadmap phrases #12 as upgrade-or-sell with one MVP level; multi-level adds a "level number" UI and balance pass without iter-2 payoff. |
| D5 | Sell refund formula | 50% / 75% / spent − 5 / type-specific | **`spent / 2` (50% of cumulative)** | Standard TD convention; integer-only; trivially testable. |
| D6 | Passive income period | 60 / 120 / 180 / 240 frames | **180 frames (3 s, +1 energy)** | Math in §6: ~100 passive over a full run keeps spending decisions meaningful without trivializing economy. |
| D7 | Robot sprite size | 8×8 / 8×16 | **8×8** | Mixing 8×16 sprites flips global LCDC bit and re-jiggers OAM y-offset for every existing sprite. Out of scope for an iter-2 pass. |
| D8 | Robot stats | various | **HP=6, speed=0.75 px/f, bounty=5** | 2× HP and 1.5× speed makes AV inefficient (justifies FW); bounty 5 funds replacement firepower without making robots a free farm. |
| D9 | Wave script format | hard-coded counts (MVP) / event list / external file | **Const event-list per wave** | Supports interleaved comp without parser; pure const data; ROM cost ~324 B. |
| D10 | Wave count | 8 / 10 / 12 | **10** | Even round number; ramps cleanly across the bug→robot transition; final "boss-ish" wave fits in pool budget (W10's worst-case alive ≤ 8 per spawn-interval/path-traversal math). |
| D11 | Boss-ish final wave shape | dedicated boss enemy / dense mix | **Dense mix (12 robots + 16 bugs at fast spawn)** | Item #18 (dedicated boss) is iter-3. Density alone delivers the "finale" feel within current entity types. |
| D12 | SFX engine shape | hUGEDriver / hand-coded register writes / GBT-Player | **Hand-coded `audio.c` with const SFX table** | hUGE/GBT pull in a tracker runtime + bank-switching infra better suited for music in iter-3; for 6 SFX hand-rolled is ~600 B and trivially testable. |
| D13 | Channel reservation | use ch3 / leave for music | **Skip ch3 (wave) entirely; ch1/2/4 only** | Reserves ch3 + waveform RAM for iter-3 music without forcing a refactor when music lands. |
| D14 | SFX preemption | none / FIFO / priority | **Per-channel priority preempt** | Prevents WIN/LOSE jingle from being clobbered by a fire SFX while remaining 1-line of logic. |
| D15 | HUD wave format | `W:N/N` (1-digit) / `W:NN/NN` (2-digit) | **`W:NN/NN`** | 10 waves needs 2 digits; widening once (vs reflowing at iter 3 if more waves added) is cheap. |
| D16 | Tower indicator placement | icon BG tile / dedicated sprite / single letter | **Single BG-font letter at HUD col 19** | Zero new tile cost; survives DMG contrast loss; consistent with the rest of the HUD. |
| D17 | Menu glyphs source | redraw / reuse BG font | **Reuse via `glyph_to_sprite(ch)` in gen_assets.py** | Pixel-identical with HUD digits; one-line generator addition. |
| D18 | MBC1 in iter-2 | yes / no | **No** | Budget §11 shows ~21 KB worst case ≤ 32 KB; defer MBC1 to iter-3 as roadmap intends. |
| D19 | Where `economy_tick` runs | playing only / playing + menu | **Playing + menu (always when in PLAYING state)** | Player shouldn't lose passive income for opening a menu; the trickle is an *over-time* mechanic. |

---

## 18. Memory updates

**Append to `memory/decisions.md`**:
- "Iter-2 firewall = single-target high-damage" (D1)
- "B-button cycles tower type" (D2)
- "Upgrade/sell menu = sprite overlay with entity freeze" (D3)
- "Wave script = const event lists per wave; 10-wave script" (D9, D10)
- "SFX engine: hand-coded `audio.c`, ch1/2/4 only, per-channel priority preempt" (D12, D13, D14)
- "Iter-2 stays on 32 KB no-MBC" (D18)

**Append to `memory/conventions.md`**:
- HUD col 19 is the selected-tower indicator (`A`/`F`).
- OAM 1..14 are owned by `menu.c` when menu is open; `menu_close()` MUST hide them.
- While `menu_is_open()`, `enemies_update()` and `projectiles_update()` are gated off in `game.c::playing_update`.
- Sprite-bank glyph reuse: any glyph rendered in BG via the FONT dict may be mirrored into the sprite bank by `glyph_to_sprite(ch)` in `gen_assets.py`; the bitmap is identical.
- New `_render()` modules added in iter-2: `menu_render()` (sprite-only — does NOT count against the per-frame BG-write budget).
- Per-frame BG-write budget tightened: worst-case = 15 (HUD 10 + computer-damaged 4 + tower place/clear 1).

---

## 19. Review

### Adversarial review (round 1) — addressed
| # | Severity | Finding | Resolution |
|---|---|---|---|
| F1 | Critical | OAM slot range for menu disagreed between iter2.md (1..14) and iter2-design.md (17..30). | §5 + §12: pinned to **1..14**. `iter2-design.md §5` will be updated to match in subtask C3. |
| F2 | High | Writing `NRx4 = 0x00` does not silence a DMG channel — trigger bit is write-1-only. | §8: stop sequence now writes `NRx2 = 0x08` (DAC off). |
| F3 | Medium | NRx1 (duty + length) was missing from `sfx_def_t`; sound timbre would be undefined. | §8: added `nrx1` field; populated for all 6 SFX (`0x80` square, `0x3F` noise). |
| F4 | Medium | Start sequence (NR10/NRx1/NRx2/NRx3/NRx4-with-trigger) was implicit. | §8: added explicit start-sequence pseudocode per channel family. |
| F5 | High | `enemies_bounty(target)` read after `enemies_apply_damage` could see a reused-slot's type. | §3: added capture-before-damage rule; bounty is read before the damage call and held in a local. |
| F6 | High | W10 35-frame cadence overflows `MAX_ENEMIES=12` (worst-case ~13 alive). | §3 + §7 + §12: `MAX_ENEMIES → 14`, OAM re-allocated (projectiles → 31..38), W8/9/10 spawn floor raised to 50 f. Boss-feel via count, not cadence. |
| F7 | Medium | Sell-then-place same-tile same-frame could overwrite the new tower. | §4: `towers_try_place` cancels any matching pending clear; render drains clears first then places, capped 1 of each per frame. |
| F8 | Medium | Skipped `_update()` does not clear shadow OAM — frozen entities still on screen. | §5: `menu_open()` calls `enemies_hide_all()` and `projectiles_hide_all()` (new APIs); restored on next post-close update. |
| F9 | Medium | `audio_play(SFX_WIN/LOSE)` ordering vs. `gameover_enter()`'s DISPLAY_OFF was unspecified. | §8: SFX play is invoked **before** `gameover_enter()`; documented that first note may elongate by ≤ 8 f during the redraw blocking window. |
| F10 | Medium | Worst-case BG-write ceiling could hit 16 with simultaneous place + clear. | §9 + §4: enforced 1 tower BG write per frame (place or clear, not both). Worst case 15. |
| F11 | Medium | Function name `towers_at_index` vs. `towers_index_at` inconsistent across §2/§4/§5. | Renamed all sites to `towers_index_at`. |

### Cross-model validation (round 2, gpt-5.4) — addressed
| # | Severity | Finding | Resolution |
|---|---|---|---|
| R1 | High | `menu_open()` happened mid-frame but subsequent `enemies_update`/`projectiles_update`/`waves_update` still ran in the same frame, undoing OAM hide and ticking the wave timer once. | §4 pseudocode: added explicit early `return` after `menu_open(idx)` so the opening frame freezes immediately (only `economy_tick` + `audio_tick` after the return). |
| R2 | High | `MAX :--` text used glyphs (`M`,`A`,`X`,`-`) not in the sprite-bank glyph set; max-state A behavior unspecified. | §5: changed text to `UPG:--`, added `SPR_GLYPH_DASH` (1 new sprite tile, total now 29), specified A is silently ignored in max state and menu stays open. |
| R3 | Medium | W8 ("mixed pairs") and W9 ("alternating") spawn order under-specified for coder. | §7: replaced labels with explicit event sequences; W8 = `(B,B,R)×6, B, B`; W9 = strict alternating starting B. |
| R4 | Medium | `s_clear_pending` representation, lifecycle, and reset on game-state change were vague; no test for the de-dup race. | §4: pinned exact types (`u16 s_pending_mask`, `clear_tile_t s_pending_tiles[MAX_TOWERS]`); required `towers_init()` to zero them; added `test_sell_then_place_same_tile` host test (§14 #6). |
| R5 | Medium | `menu_init()` was defined but not added to any state-reset path → `s_open` could persist across sessions. | §13 C4: explicit requirement to call `menu_init()` from `enter_playing()` (and `enter_title()` for OAM hygiene). |
| R6 | Medium | Scenario 13 timing math impossible (claimed 18 s idle within an inter-wave delay, but max delay is 4 s); scenario 20 listed LOSE SFX in a winning run. | §15: rewrote scenario 13 to a 1080-frame idle span across multiple game phases; split scenario 20 (win run) and added scenario 21 (lose run). |
