# Iter-7: Difficulty Scaling, Boss Enemies & Title Spacing

## Problem

Three user-feedback items bundled into a single iteration:

1. **Title spacing**: "Press start and hi score should be moved down a line in title menu, to keep space after the map select text." Currently PRESS START (row 13) is immediately below MAP SELECT (row 12) with no gap.
2. **Difficulty scaling**: "Difficulty doesn't scale well with map progression: it starts ok (all modes), but becomes too easy as waves progress (all modes). Game should become more challenging and difficult as waves progress." Enemy HP is flat across all 10 waves — tower DPS scales far faster than enemy durability, making late waves trivial.
3. **Boss enemies**: "What about a special boss appearing at end of waves 5 and 10? To add more challenge, with a single high HP enemy." No boss mechanic exists.

## Section 1 — Investigation Findings

### 1.1 Current title layout (rows 10–17)

| Row | Content | Source |
|-----|---------|--------|
| 10 | Difficulty selector `▶ LABEL` | Runtime (`draw_diff_now`, DIFF_ROW=10) |
| 11 | blank | |
| 12 | Map selector `▶ LABEL` | Runtime (`draw_map_now`, MAP_ROW=12) |
| 13 | `PRESS START` prompt | gen_assets.py tilemap + runtime blink (PROMPT_ROW=13) |
| 14 | blank | |
| 15 | `HI: NNNNN` | Runtime (`draw_hi_now`, HI_ROW=15) |
| 16 | blank | |
| 17 | blank | |

Row 12→13 is zero gap. User wants a blank row between map selector and PRESS START.

### 1.2 Current difficulty balance (why late waves are too easy)

**Root cause**: Enemy HP is constant across all 10 waves. The only progression levers are enemy count, type mix, and spawn interval. But tower DPS scales dramatically via upgrades:

| Tower | L0 DPS (dmg/frame) | L2 DPS (dmg/frame) | Multiplier |
|-------|--------------------|--------------------|------------|
| AV | 1/60 = 0.017 | 3/30 = 0.100 | **6×** |
| FW | 3/120 = 0.025 | 6/70 = 0.086 | **3.4×** |

By wave 7–10, a Normal player typically has 6–10 towers (many L1/L2). Combined DPS of ~0.3–0.5/frame kills Normal bugs (HP=3) in 6–10 frames — enemies evaporate before reaching mid-path.

**Current HP table** (`DIFF_ENEMY_HP[3][3]`):

| Diff | Bug | Robot | Armored |
|------|-----|-------|---------|
| Casual | 1 | 3 | 6 |
| Normal | 3 | 6 | 12 |
| Veteran | 4 | 7 | 14 |

These values are identical at wave 1 and wave 10. No wave-based scaling exists.

**Spawn intervals** (base values from wave script, then `difficulty_scale_interval` applied):

| Wave | Base interval | Casual (×1.75) | Normal (×1.0) | Veteran (×0.75) | Count |
|------|-------------|-----------|---------|----------|-------|
| 1 | 90 | 157 | 90 | 67 | 5 |
| 3 | 75 | 131 | 75 | 56 | 7 |
| 5 | 60 | 105 | 60 | 45 | 12 |
| 7 | 50 | 87 | 50 | 37 | 15 |
| 9 | 50/80 | 87/140 | 50/80 | 37/60 | 22 |
| 10 | 50/75 | 87/131 | 50/75 | 37/56 | 31 |

Spawn pressure is adequate; the problem is that enemies die too quickly to create any pressure.

**Diagnosis**: Per-wave HP scaling is the highest-impact, lowest-complexity fix. By making enemies tougher each wave, the kill rate drops relative to spawn rate, naturally creating late-game pressure. This also implicitly reduces effective income (enemies take longer to kill → bounties arrive slower).

### 1.3 Boss feasibility

- **OAM budget**: 14 enemy slots (OAM 17–30). Boss uses 1 slot. As the last spawn of a wave, most regular enemies should be dead or dying. Even worst case (wave 5 boss + wave 6 enemies): 1 boss + ~9 alive = 10 < 14. ✓
- **Sprite tile budget**: 43/256 used. Adding 8 tiles (4 boss + 4 HP bar) → 51/256. ✓
- **OAM slot 39**: Currently reserved-unused. Perfect for the boss HP bar.
- **Pool size**: No expansion needed. Boss takes 1 of 14 slots.
- **Win condition**: `waves_all_cleared() && enemies_count() == 0` already requires all enemies (including the boss) to be dead. No change needed.

## Architecture

### Per-wave HP scaling

Add a 10-entry `WAVE_HP_SCALE[10]` lookup table (numerator/8) to `difficulty_calc.h`. New pure function `difficulty_wave_enemy_hp(type, diff, wave_1based)` = `max(1, (base_hp * scale) >> 3)`. Enemies use wave-scaled HP at spawn time.

**Curve** (chosen for gentle early, steep late):

```
WAVE_HP_SCALE[10] = { 8, 8, 9, 10, 11, 13, 15, 17, 20, 24 }
```

Multipliers: ×1.0, ×1.0, ×1.125, ×1.25, ×1.375, ×1.625, ×1.875, ×2.125, ×2.5, ×3.0

### Boss enemies

A boss is a regular enemy pool slot with the `is_boss` flag set. Module-level statics in `enemies.c` hold the boss's unique config (speed, bounty, max HP, bar thresholds) — safe because at most 1 boss exists at a time.

Boss spawns are encoded in the wave script as `{SPAWN_BOSS, delay}` sentinel events appended to waves 5 and 10. `waves_update()` branches on the sentinel to call `enemies_spawn_boss()` instead of `enemies_spawn()`.

Boss HP bar uses OAM slot 39 (reserved-unused) with 4 sprite tiles showing fill levels. Position tracks the boss sprite each frame in `step_enemy()`. Tile updates on damage via `enemies_apply_damage()`.

### Data flow

```
waves.c                    enemies.c               difficulty_calc.h
────────                   ─────────               ─────────────────
spawn_event_t              enemies_spawn(type, w)   difficulty_wave_enemy_hp()
  .type == SPAWN_BOSS? ──► enemies_spawn_boss(w)    difficulty_boss_hp()
  .delay ─► scale_interval                          WAVE_HP_SCALE[]
                           enemy_t.is_boss
                           s_boss_speed, s_boss_bounty  ◄── tuning.h constants
                           
projectiles.c              score.c                  score_calc.h
─────────────              ───────                  ────────────
  is_boss? ──► score_add_boss_kill()                SCORE_KILL_BOSS
  else     ──► score_add_kill(type)
```

## Section 2 — Subtasks

### 2.1 ✅ **Title menu spacing** — Move PRESS START and HI down 1 row each

**Scope**: Change 2 row-position constants in `title.c` and 1 row assignment in `gen_assets.py`.

**Changes**:
- `src/title.c`: `PROMPT_ROW 13 → 14`, `HI_ROW 15 → 16`
- `tools/gen_assets.py`: In `title_map()`, change `rows[13] = text_row("    PRESS START     ")` to `rows[14] = text_row("    PRESS START     ")` (row 13 becomes blank)

**New layout (rows 10–17)**:

| Row | Content |
|-----|---------|
| 10 | Difficulty selector |
| 11 | blank |
| 12 | Map selector |
| 13 | **blank** ← new gap |
| 14 | PRESS START ← moved from 13 |
| 15 | **blank** ← new gap |
| 16 | HI: NNNNN ← moved from 15 |
| 17 | blank |

**Verification**: All 18 rows used, nothing off-screen. BG-write priority chain unchanged (max 12 writes/frame for prompt at row 14). Focus arrow stays at rows 10/12. No motif/art collision.

**Done when**: Title screen shows blank row between MAP selector and PRESS START, and blank row between PRESS START and HI line. All blink/selector interactions work as before.

**Dependencies**: None.
**Risk**: LOW.

---

### 2.2 ✅ **Difficulty rescale** — Per-wave HP scaling via pure-helper curve

**Scope**: New math in `difficulty_calc.h`, API change to `enemies_spawn`, tests.

**Changes**:

#### difficulty_calc.h
Add `WAVE_HP_SCALE` and `difficulty_wave_enemy_hp`:
```c
static const uint8_t WAVE_HP_SCALE[10] = { 8, 8, 9, 10, 11, 13, 15, 17, 20, 24 };

static inline uint8_t difficulty_wave_enemy_hp(uint8_t enemy_type, uint8_t diff, uint8_t wave_1based) {
    uint8_t base = difficulty_enemy_hp(enemy_type, diff);
    uint8_t idx = (wave_1based >= 1 && wave_1based <= 10) ? (uint8_t)(wave_1based - 1u) : 0u;
    uint16_t scaled = ((uint16_t)base * WAVE_HP_SCALE[idx]) >> 3;
    if (scaled > 255u) scaled = 255u;
    if (scaled < 1u) scaled = 1u;
    return (uint8_t)scaled;
}
```

#### enemies.h / enemies.c
Change `enemies_spawn(u8 type)` → `enemies_spawn(u8 type, u8 wave_1based)`.

In `enemies_spawn`, replace:
```c
s_enemies[i].hp = difficulty_enemy_hp(type, game_difficulty());
```
with:
```c
s_enemies[i].hp = difficulty_wave_enemy_hp(type, game_difficulty(), wave_1based);
```

#### tests/test_enemies.c
Update all 8 call sites from `enemies_spawn(TYPE)` to `enemies_spawn(TYPE, 1)` (wave 1 — matches existing Normal-difficulty test stubs). The wave parameter is irrelevant to the stun/flash tests; using wave 1 preserves the pre-change HP values (scale=8 → ×1.0 identity).

#### waves.c
Update the single call site:
```c
if (enemies_spawn(e->type, s_cur + 1)) {
```

#### Full HP tables (computed: `(base * scale) >> 3`, floor 1)

**Normal** (base: Bug=3, Robot=6, Armored=12):

| Wave | Scale | Bug | Robot | Armored |
|------|-------|-----|-------|---------|
| 1 | ×1.0 | 3 | 6 | 12 |
| 2 | ×1.0 | 3 | 6 | 12 |
| 3 | ×1.125 | 3 | 6 | 13 |
| 4 | ×1.25 | 3 | 7 | 15 |
| 5 | ×1.375 | 4 | 8 | 16 |
| 6 | ×1.625 | 4 | 9 | 19 |
| 7 | ×1.875 | 5 | 11 | 22 |
| 8 | ×2.125 | 6 | 12 | 25 |
| 9 | ×2.5 | 7 | 15 | 30 |
| 10 | ×3.0 | 9 | 18 | 36 |

**Casual** (base: Bug=1, Robot=3, Armored=6):

| Wave | Bug | Robot | Armored |
|------|-----|-------|---------|
| 1 | 1 | 3 | 6 |
| 5 | 1 | 4 | 8 |
| 7 | 1 | 5 | 11 |
| 10 | 3 | 9 | 18 |

**Veteran** (base: Bug=4, Robot=7, Armored=14):

| Wave | Bug | Robot | Armored |
|------|-----|-------|---------|
| 1 | 4 | 7 | 14 |
| 5 | 5 | 9 | 19 |
| 7 | 7 | 13 | 26 |
| 10 | 12 | 21 | 42 |

**Balance check**: Normal W10 robot (HP=18) vs AV L2 (dmg 3, cd 30) = 6 shots = 180f (3 sec). With 2–3 towers in range, kill time ≈ 60–90 frames. Spawn interval 50f — enemies accumulate. Creates genuine pressure. ✓

**Casual W10**: Bug HP=3 (same as old Normal W1). Still very manageable with a decent tower setup. ✓

**Veteran W7**: Bug HP=7, Robot HP=13. With 5–6 towers, DPS ~0.2/frame. Robot kill time ≈ 65 frames. Spawn interval 37f. Multiple enemies in flight. Hard. ✓

**Done when**: `difficulty_wave_enemy_hp()` returns correct values for all 10 waves × 3 difficulties × 3 types. Host tests pass. Wave-1 balance is identical to pre-change (scale=8 → ×1.0). Late-wave enemies visibly take more hits to kill.

**Dependencies**: None (subtask 2.3 builds on the API change but can be implemented in sequence).
**Risk**: MEDIUM. API change to `enemies_spawn` has exactly 1 call site (waves.c). Pure-helper math is fully testable. Risk is in the balance feel — numbers may need post-QA tweaking (all values are in `difficulty_calc.h` constants, easy to adjust).

---

### 2.3 ✅ **Boss enemies at waves 5 and 10** — New entity behavior, sprites, HP bar

**Scope**: `enemies.{h,c}`, `waves.c`, `projectiles.c`, `score.{h,c}`, `gen_assets.py`, `tuning.h`, `difficulty_calc.h`, `score_calc.h`, `gtypes.h`.

**Dependencies**: Subtask 2.2 (enemies_spawn API already changed).
**Risk**: MEDIUM-HIGH. Touches the enemy struct, multiple render paths, OAM management, wave script.

#### 2.3.1 Constants

**tuning.h** — add:
```c
/* Boss enemies (iter-7) */
#define BOSS_LEAK_DAMAGE    3      /* HP damage to computer if boss leaks */
#define BOSS_BOUNTY_W5     30      /* energy awarded for killing W5 boss */
#define BOSS_BOUNTY_W10    50      /* energy awarded for killing W10 boss */
#define BOSS_SPEED       0x0060    /* 0.375 px/frame (between armored 0.25 and bug 0.5) */
#define BOSS_SPAWN_DELAY   120     /* base delay (frames) before boss appears after last regular enemy */
```

**difficulty_calc.h** — add:
```c
/* Boss HP per (wave-tier, difficulty). Fixed values, NOT scaled by WAVE_HP_SCALE. */
static const uint8_t BOSS_HP[2][3] = {
    /* W5  */ { 20, 30, 40 },   /* Casual, Normal, Veteran */
    /* W10 */ { 40, 60, 80 },   /* Casual, Normal, Veteran */
};

static inline uint8_t difficulty_boss_hp(uint8_t wave_1based, uint8_t diff) {
    if (diff >= 3u) diff = 1u;
    uint8_t tier = (wave_1based >= 10u) ? 1u : 0u;
    return BOSS_HP[tier][diff];
}
```

**score_calc.h** — add:
```c
#define SCORE_KILL_BOSS   300u   /* base score for killing any boss (pre-multiplier) */
```

**gtypes.h** — add:
```c
#define OAM_BOSS_BAR     39     /* HP bar sprite above boss enemy */
```

#### 2.3.2 Enemy struct extension

In `enemies.c`, add field to `enemy_t`:
```c
u8 is_boss;    /* 0=normal, 1=boss */
```

Add module-level statics:
```c
static fix8 s_boss_speed;          /* set at spawn */
static u8   s_boss_bounty;         /* set at spawn */
static u8   s_boss_max_hp;         /* for bar threshold computation */
static u8   s_boss_bar_thr[3];     /* 75%, 50%, 25% thresholds */
```

WRAM growth: 14 bytes (is_boss × 14) + 2 (speed) + 1 (bounty) + 1 (max_hp) + 3 (thresholds) = **21 bytes**.

#### 2.3.3 Public API additions (`enemies.h`)

```c
bool enemies_spawn_boss(u8 wave_1based);   /* spawn boss for given wave */
bool enemies_is_boss(u8 idx);              /* read-only boss flag accessor */
```

#### 2.3.4 enemies_spawn changes

In `enemies_spawn(type, wave_1based)` — add `s_enemies[i].is_boss = 0;` to the init block (defensive clear).

New `enemies_spawn_boss(wave_1based)`:
- Scan for free slot (same as regular spawn).
- Guard: if any alive enemy has `is_boss == 1`, return false (at most 1 boss).
- **Perform the full regular-spawn init block first**: set position from `map_waypoints()[0]` via `wp_x_fix/wp_y_fix`, `wp_idx = 0`, `anim = 0`, `alive = 1`, `type = ENEMY_BUG` (unused for boss logic), `flash_timer = 0`, `stun_timer = 0`, `gen++`. This is identical to the non-boss path in `enemies_spawn`. Missing `gen++` would break projectile slot-reuse detection.
- **Then apply boss-specific overrides**:
  - `is_boss = 1`
  - HP from `difficulty_boss_hp(wave_1based, game_difficulty())`.
  - `s_boss_speed = BOSS_SPEED`.
  - `s_boss_bounty = (wave_1based >= 10) ? BOSS_BOUNTY_W10 : BOSS_BOUNTY_W5`.
  - Compute bar thresholds: `s_boss_max_hp = hp; thr[0] = hp*3/4; thr[1] = hp/2; thr[2] = hp/4`.
- Set HP bar tile only: `set_sprite_tile(OAM_BOSS_BAR, SPR_BOSS_BAR_4)`. Do NOT call `move_sprite` here — the bar position is set by `step_enemy` on the **next** entity tick (in `game.c::entity_tick`, `enemies_update()` runs BEFORE `waves_update()`, so the newly-spawned boss won't get a `step_enemy` call this tick). In fast mode the second `entity_tick` positions the bar same-frame; in normal mode it appears next frame (~16ms delay). Both are imperceptible. This avoids duplicating the `sx/sy` offset math.

#### 2.3.5 step_enemy changes

**Speed**: `fix8 step = e->is_boss ? s_boss_speed : s_enemy_stats[e->type].speed;`

**Leak damage**: Replace `economy_damage(1)` with:
```c
economy_damage(e->is_boss ? BOSS_LEAK_DAMAGE : 1);
if (e->is_boss) {
    move_sprite(OAM_BOSS_BAR, 0, 0);
    e->is_boss = 0;
}
```

**Sprite selection**: Before the existing `if (had_flash)` chain, add boss branch:
```c
if (e->is_boss) {
    if (had_flash) tile = SPR_BOSS_FLASH;
    else if (was_stunned) tile = SPR_BOSS_STUN;
    else {
        e->anim++;   /* MUST increment anim — same as the normal walk branch */
        tile = ((e->anim >> 4) & 1) ? SPR_BOSS_B : SPR_BOSS_A;
    }
} else {
    /* existing type-based sprite selection (unchanged) */
}
```

**HP bar position** (after `move_sprite` for the enemy itself):
```c
if (e->is_boss) {
    move_sprite(OAM_BOSS_BAR, sx, sy - 8);
}
```

#### 2.3.6 enemies_apply_damage changes

On killing hit (after setting alive=0 and hiding enemy sprite):
```c
if (s_enemies[idx].is_boss) {
    move_sprite(OAM_BOSS_BAR, 0, 0);
    s_enemies[idx].is_boss = 0;
}
```

On non-killing hit (after reducing HP):
```c
if (s_enemies[idx].is_boss) {
    boss_update_bar(s_enemies[idx].hp);
}
```

New static helper:
```c
static void boss_update_bar(u8 hp) {
    u8 tile;
    if (hp > s_boss_bar_thr[0])      tile = SPR_BOSS_BAR_4;
    else if (hp > s_boss_bar_thr[1]) tile = SPR_BOSS_BAR_3;
    else if (hp > s_boss_bar_thr[2]) tile = SPR_BOSS_BAR_2;
    else                              tile = SPR_BOSS_BAR_1;
    set_sprite_tile(OAM_BOSS_BAR, tile);
}
```

#### 2.3.7 enemies_set_flash changes

`enemies_set_flash()` (lines 103–122) immediately writes the flash sprite tile on hit to sync visual + audio on the same frame (iter-3 #21 F1). Currently it branches on `e->type` to select `SPR_BUG_FLASH` / `SPR_ROBOT_FLASH` / `SPR_ARMORED_FLASH`. Since the boss sets `type = ENEMY_BUG`, a hit would incorrectly write `SPR_BUG_FLASH`.

**Fix**: Add boss branch at the top of the tile-selection block:
```c
u8 tile;
if (s_enemies[idx].is_boss)
    tile = SPR_BOSS_FLASH;
else if (s_enemies[idx].type == ENEMY_BUG)
    tile = SPR_BUG_FLASH;
else if (s_enemies[idx].type == ENEMY_ROBOT)
    tile = SPR_ROBOT_FLASH;
else
    tile = SPR_ARMORED_FLASH;
set_sprite_tile(OAM_ENEMIES_BASE + idx, tile);
```

#### 2.3.8 enemies_bounty / enemies_init / enemies_hide_all

`enemies_bounty`: `if (s_enemies[idx].is_boss) return s_boss_bounty;` before existing return.

`enemies_init`: Add `s_enemies[i].is_boss = 0;` to init loop. Add `move_sprite(OAM_BOSS_BAR, 0, 0);` after the loop.

`enemies_hide_all`: Add `move_sprite(OAM_BOSS_BAR, 0, 0);` after the loop.

#### 2.3.9 Wave script changes

In `waves.c`, add sentinel:
```c
#define SPAWN_BOSS 0xFF
```

Append to W5_EV:
```c
static const spawn_event_t W5_EV[] = {
    {B,60},{B,60},{R,60},
    {B,60},{B,60},{R,60},
    {B,60},{B,60},{R,60},
    {B,60},{B,60},{R,60},
    {SPAWN_BOSS, BOSS_SPAWN_DELAY}
};
```

Append to W10_EV:
```c
/* existing 31 events unchanged */
{SPAWN_BOSS, BOSS_SPAWN_DELAY}
```

Update `s_waves` counts — these are **hard-coded literals** (not sizeof-derived):
- `s_waves[4]`: change `.count` from `12` to `13`
- `s_waves[9]`: change `.count` from `31` to `32`

Both the `_EV` array extension AND the `s_waves` count literal must be updated in lockstep — missing either silently skips the boss event.

#### 2.3.10 waves_update spawn branch
Replace:
```c
if (enemies_spawn(e->type)) {
```
with:
```c
bool spawned;
if (e->type == SPAWN_BOSS) {
    spawned = enemies_spawn_boss(s_cur + 1);
} else {
    spawned = enemies_spawn(e->type, s_cur + 1);
}
if (spawned) {
```

The difficulty_scale_interval still applies to the boss delay event. Effective boss delays:
- Casual: max(120 × 14/8, 30) = 210 frames (3.5 sec)
- Normal: 120 frames (2 sec)
- Veteran: max(120 × 6/8, 30) = 90 frames (1.5 sec)

**Spawn failure retry**: If `enemies_spawn_boss` returns false (pool full OR another boss alive), the existing `else { s_timer = 8; }` branch in `waves_update` retries after 8 frames. This is the correct behavior — the wave does NOT advance past the boss event until the boss successfully spawns. No new retry logic needed.

#### 2.3.11 Projectile kill path

In `projectiles.c::step_proj`, all state must be captured BEFORE `enemies_apply_damage` per the bounty-capture convention. `enemies_apply_damage` clears `is_boss` on kill, and the slot may be reused same-frame. The complete capture + branch:

```c
/* Capture ALL enemy state before damage — per bounty-capture convention.
 * enemies_apply_damage may free the slot + clear is_boss on kill. */
u8   bounty  = enemies_bounty(p->target);
u8   etype   = enemies_type(p->target);
bool is_boss = enemies_is_boss(p->target);
bool killed  = enemies_apply_damage(p->target, p->damage);
if (killed) {
    economy_award(bounty);
    if (is_boss) {
        score_add_boss_kill();
    } else {
        score_add_kill(etype);
    }
    audio_play(SFX_ENEMY_DEATH);
}
```

The existing `enemies_set_flash(p->target)` in the non-killing branch is already correct — the boss flash tile is handled by the updated `enemies_set_flash` (subtask 2.3.7).

#### 2.3.12 Score module

`score.h` — add:
```c
void score_add_boss_kill(void);   /* 300 × diff_mult */
```

`score.c` — add:
```c
void score_add_boss_kill(void) {
    add_scaled(SCORE_KILL_BOSS);
}
```

#### 2.3.13 Boss sprite tiles (8 new tiles in gen_assets.py)

Sprite tile indices: SPR_BOSS_A=43, SPR_BOSS_B=44, SPR_BOSS_FLASH=45, SPR_BOSS_STUN=46, SPR_BOSS_BAR_1=47, SPR_BOSS_BAR_2=48, SPR_BOSS_BAR_3=49, SPR_BOSS_BAR_4=50.

SPRITE_TILE_COUNT: 43 → 51.

**SPR_BOSS_A** — Walk Frame A (3-point crowned mega-bug, 87.5% fill):
```
3.3333.3
33333333
32333323
33333333
33322333
33333333
.333333.
33....33
```

**SPR_BOSS_B** — Walk Frame B (legs shift inward):
```
3.3333.3
33333333
32333323
33333333
33322333
33333333
.333333.
.33..33.
```

**SPR_BOSS_FLASH** — Hit feedback (B→L, D→W per convention):
```
1.1111.1
11111111
1.1111.1
11111111
111..111
11111111
.111111.
11....11
```

**SPR_BOSS_STUN** — EMP frozen (spark gaps at rows 3,5):
```
3.3333.3
33333333
31333313
3.3333.3
33322333
3.3333.3
.333333.
33....33
```

**SPR_BOSS_BAR_1** — 25% filled:
```
........
........
........
33111111
33111111
........
........
........
```

**SPR_BOSS_BAR_2** — 50% filled:
```
........
........
........
33331111
33331111
........
........
........
```

**SPR_BOSS_BAR_3** — 75% filled:
```
........
........
........
33333311
33333311
........
........
........
```

**SPR_BOSS_BAR_4** — 100% filled:
```
........
........
........
33333333
33333333
........
........
........
```

HP bar design: 2-pixel-tall bar centered vertically. Shade 3 (black) for filled, shade 1 (light grey) for empty. Drains right-to-left (leftmost = remaining HP).

#### 2.3.14 Boss stat summary

| Stat | Wave 5 Boss | Wave 10 Boss |
|------|-------------|--------------|
| HP (Casual) | 20 | 40 |
| HP (Normal) | 30 | 60 |
| HP (Veteran) | 40 | 80 |
| Speed | 0x0060 (0.375 px/f) | 0x0060 (0.375 px/f) |
| Bounty | 30 energy | 50 energy |
| Score | 300 × diff_mult | 300 × diff_mult |
| Leak damage | 3 HP | 3 HP |
| Sprite | SPR_BOSS_A/B | SPR_BOSS_A/B |

**Done when**:
- Boss spawns as the final event of waves 5 and 10 with correct HP/speed/bounty per difficulty.
- Boss uses unique sprite with walk animation, flash, and stun states.
- HP bar (OAM 39) displays above boss and updates on damage (4 fill levels).
- Boss deals 3 HP damage on computer leak.
- Killing boss awards correct bounty and score.
- Boss HP bar hides on boss death or leak.
- `enemies_hide_all()` and `enemies_init()` properly manage OAM_BOSS_BAR.
- Win condition still requires boss to be dead (no change to game.c).
- Host tests pass for boss HP and difficulty helpers. Manual tests pass for boss encounter.

## Section 3 — Acceptance Scenarios

### Setup
1. `just build && just run` — ROM loads in mGBA.
2. Test all three difficulty modes (Casual, Normal, Veteran) on any map.

### Scenarios

| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Title spacing gap | Start game → observe title screen | Blank row between MAP selector (row 12) and PRESS START (row 14). Blank row between PRESS START and HI line (row 16). |
| 2 | Title blink at new row | Wait on title screen for 30 frames | PRESS START blinks on/off at row 14 (not row 13). |
| 3 | Title HI at new row | Cycle difficulty/map | HI score updates at row 16 (not row 15). |
| 4 | Wave 1 HP unchanged | Start Normal, wave 1 bugs | Bugs die in the same number of hits as before the patch (host test verifies HP=3). |
| 5 | Late-wave HP scaled | Play to wave 7+, Normal | Enemies visibly take more hits to kill than in wave 1. Robots survive noticeably longer. (Exact values verified by host tests: W5 Bug=4, W10 Robot=18.) |
| 6 | Veteran late waves punishing | Play Veteran to wave 7 | Robots survive many hits. Player must focus towers carefully or face leaks. Noticeably harder than wave 1–3. |
| 7 | Casual wave 10 beatable | Play Casual through wave 10 | Possible to win with reasonable tower setup. Late enemies are tougher but not overwhelming. |
| 8 | Boss spawns at wave 5 | Play to wave 5 (any difficulty) | After last regular enemy of wave 5 spawns + delay, a boss enemy with distinct sprite (3-point crown) appears at spawn point. |
| 9 | Boss spawns at wave 10 | Play to wave 10 | Boss appears as the final spawn of wave 10 with the same distinct sprite. |
| 10 | Boss HP bar visible | Observe boss after spawn | An 8×8 HP bar sprite (fully filled) hovers 8px above the boss sprite. Bar moves with boss. |
| 11 | Boss HP bar depletes | Hit boss with towers | Bar transitions through 4 fill levels (100% → 75% → 50% → 25%) as HP drops through thresholds. |
| 12 | Boss distinct sprite | Observe boss walk | Boss uses SPR_BOSS_A/B alternating walk animation. Visually distinct from bug/robot/armored. |
| 13 | Boss flash on hit | Projectile hits boss (non-killing) | 3-frame flash sprite (SPR_BOSS_FLASH). SFX_ENEMY_HIT plays. |
| 14 | Boss stun | EMP tower stuns boss | Boss shows SPR_BOSS_STUN. Movement freezes for stun duration. |
| 15 | Boss kill reward | Kill the boss | Energy award: 30 (W5) or 50 (W10). Score increases by 300 × difficulty multiplier. SFX_ENEMY_DEATH plays. |
| 16 | Boss leak damage | Let boss reach computer | Computer HP drops by 3 (not 1). Boss and HP bar sprites hide. |
| 17 | Boss HP bar hides on death | Kill the boss | Both boss sprite and HP bar sprite (OAM 39) hide simultaneously. |
| 18 | Boss HP bar hides during modal | Open pause while boss alive | Boss sprite and HP bar are hidden (enemies_hide_all). Resume restores them. |
| 19 | Wave 10 win requires boss dead | Kill all W10 regular enemies, boss still alive | Game does NOT transition to win. Only transitions when boss dies. |
| 20 | Boss HP per difficulty | Test W5 boss on each difficulty | Casual: boss depletes fastest (~20 HP). Veteran: much tankier (~40 HP). |
| 21 | No second boss overlap | W5 boss still alive during W6 | Wave 6 enemies spawn normally. Boss coexists in pool. No crash/glitch. |

## Section 4 — Test Strategy

### Host tests (extend existing)

**tests/test_difficulty.c** — add:
```
- difficulty_wave_enemy_hp: all 10 waves × 3 types × 3 difficulties (90 cases)
- Wave 1 values match pre-scaling (WAVE_HP_SCALE[0]=8 → identity)
- Wave 10 Normal Bug = 9, Robot = 18, Armored = 36
- Wave 10 Veteran Bug = 12, Robot = 21, Armored = 42
- Wave 10 Casual Bug = 3, Robot = 9, Armored = 18
- Clamp: Veteran Armored W10 = 42 (within u8)
- Floor: Casual Bug W1..W7 = 1 (not 0)
- Out-of-range wave_1based (0, 11, 255) → treated as wave 1
- difficulty_boss_hp: W5 × 3 diffs = {20, 30, 40}, W10 × 3 diffs = {40, 60, 80}
- Boss HP with garbage diff → Normal
```

**tests/test_score.c** — add:
```
- SCORE_KILL_BOSS = 300
- score_add_boss_kill: base 300 × Normal mult (12/8) = 450
- score_add_boss_kill: base 300 × Veteran mult (16/8) = 600
```

### Manual tests (extend tests/manual.md)

```
BOSS-1: Play Normal to W5. Count hits on boss (should be ~30 for AV L0 dmg 1).
BOSS-2: Let boss leak — HP drops by 3.
BOSS-3: Kill W10 boss. Score increases. Energy increases by 50.
BOSS-4: Open pause while boss alive. Resume. Boss + bar reappear at correct position.
BOSS-5: EMP stun boss. Boss freezes, shows stun sprite. Bar stays visible.
BOSS-6: Fast mode (SELECT) during boss fight. Boss moves/takes damage at 2× speed.
BOSS-7: Verify W5 boss can coexist with W6 regular enemies (no pool overflow crash).
```

## Section 5 — Risk Assessment

| Subtask | Risk | Rationale | Mitigation |
|---------|------|-----------|------------|
| 2.1 Title spacing | LOW | 2 constant changes + 1 tilemap row change. No logic changes. | Visual QA only. |
| 2.2 Difficulty rescale | MEDIUM | API change to `enemies_spawn` (1 production call site in waves.c + 8 test call sites in test_enemies.c). Balance may need post-QA tweaking. All math is in pure-helper constants. | Comprehensive host tests. All numbers in `difficulty_calc.h` — easy to adjust. Test calls use `wave_1based=1` (identity). |
| 2.3 Boss enemies | MEDIUM-HIGH | Touches enemy struct (WRAM growth), multiple render paths, OAM slot 39, wave script, projectile kill path, score module. | Boss flag is 1 byte per enemy. Module-level statics for 1-boss-at-a-time config. OAM 39 reserved-unused. HP bar is 4 simple comparisons. No pool expansion. No OAM rework. |

**High-risk items explicitly avoided**:
- No pool size expansion (stays at 14) → no OAM rework.
- No 16×16 composite sprites → no multi-OAM formation code.
- No new HUD layout for boss HP → bar is a sprite above the boss.
- Boss HP is u8 (max 80, well under 255).
- Per-wave HP scaling doesn't change the base difficulty table — wave 1 is identity.

## Section 6 — Out of Scope

- **Boss HP bar with more than 4 fill levels**: 4 levels (25% granularity) is sufficient for visual feedback.
- **Boss-specific SFX**: Boss uses existing SFX_ENEMY_DEATH and SFX_ENEMY_HIT. Unique boss SFX deferred.
- **Boss-specific death animation**: Boss dies instantly like regular enemies. Death animation deferred.
- **Per-wave spawn interval tightening**: HP scaling alone provides adequate difficulty progression. Spawn intervals unchanged.
- **Energy income reduction at high waves**: HP scaling implicitly reduces income rate (enemies take longer to kill → bounties arrive slower).
- **Mixed-archetype same-wave pressure**: Already present in waves 3–10. No additional mixing needed.
- **16×16 composite boss sprite**: Would require 4 OAM slots in formation, complex movement code. 8×8 with distinct design is sufficient.
- **Boss music/stinger**: No music changes. Deferred to future iteration.

## Constraints

- **ROM ≤ 64 KB**: Growth is ~430 bytes (10 array + 8 sprite tiles × 16 bytes + logic). Well within 4-bank headroom.
- **WRAM growth**: 21 bytes for boss statics. Acceptable.
- **OAM**: Slot 39 (reserved-unused) claimed for boss HP bar. No other OAM changes.
- **BG-write budget**: No BG writes from boss system (all sprite-based). Budget unchanged at ≤16/frame.
- **Pool size**: Stays at 14. Boss uses 1 slot. No OAM rework.
- **Sprite VRAM**: 43 → 51 tiles. 256 max. Ample headroom.
- **iter-6 idle rescan optimization**: Unaffected — boss is a regular enemy slot with `alive=1`, processed by the same targeting/scan code.
- **Pure-helper convention**: All new math in `difficulty_calc.h` and `score_calc.h` (host-testable, `<stdint.h>` only).
- **Bounty capture convention**: `enemies_is_boss()` captured BEFORE `enemies_apply_damage()` in projectiles.c.

## Decisions

| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| HP scaling curve | (A) Linear ×1.0–×2.0 (B) Quadratic ×1.0–×3.0 (C) Exponential | **(B) Quadratic-ish ×1.0–×3.0** | Gentle early (waves 1–3 unchanged), steep late (wave 10 = 3×). Creates the "difficulty wall" users reported missing without breaking early-game feel. |
| Scaling mechanism | (A) HP scaling only (B) HP + spawn interval (C) HP + bounty reduction | **(A) HP scaling only** | Single lever is simplest and sufficient. HP scaling implicitly reduces effective income (slower kills → slower bounties). Adding spawn interval changes risks over-correction. |
| Boss HP source | (A) Multiplier of normal enemy HP (B) Fixed per-wave×difficulty table | **(B) Fixed table** | Boss HP should be independently tunable. Coupling to the normal HP table creates fragile dependencies — changing base HP unintentionally changes boss HP. |
| Boss sprite | (A) 16×16 composite (4 OAM) (B) 8×8 single sprite | **(B) 8×8 single** | 16×16 requires formation movement code and 4 OAM slots. 8×8 with high fill (87.5%) and unique silhouette (3-point crown) is visually distinct and implementation-simple. |
| Boss HP bar | (A) HUD display (B) Sprite above boss (C) No bar | **(B) Sprite above boss** | No HUD layout change. OAM 39 is available. 4 fill levels with threshold comparison is cheap. Provides immediate spatial feedback. |
| HP bar fill levels | (A) 8 levels (B) 4 levels (C) Continuous pixel | **(B) 4 levels** | 4 tiles is minimal ROM cost (64 bytes). Threshold comparison is 3 `if` statements. 8 would need division; continuous needs per-pixel math. 4 is sufficient for "is this boss almost dead?" feedback. |
| Boss as new archetype vs flag | (A) EARCH_BOSS type (B) is_boss flag on existing slot | **(B) is_boss flag** | Reuses all spawn/move/render infrastructure. Module-level statics for speed/bounty (at most 1 boss). +1 byte per enemy (14 bytes WRAM) is trivial. New archetype would grow the stats table and require changes to every type-switch. |
| Boss spawn timing | (A) After wave enemies cleared (B) Last event of wave | **(B) Last event of wave** | Wave isn't "won" until boss is dead — creates narrative tension. As a spawn_event_t, it uses existing wave machinery with zero new state. |
| SPAWN_BOSS sentinel scope | (A) Global enum (B) waves.c local define | **(B) waves.c local** | Only waves.c reads spawn_event_t.type. No other module needs the sentinel. Keeps it encapsulated. |
| Boss score value | (A) Wave-dependent (200/500) (B) Fixed 300 | **(B) Fixed 300** | Avoids needing wave number in projectiles.c. Difficulty multiplier (×1.0/1.5/2.0) already provides score differentiation. 300 is 6× robot score — feels appropriately rewarding. |
| enemies_spawn API change | (A) wave_1based param (B) enemies reads waves_get_current internally | **(A) wave_1based param** | Avoids enemies.c depending on waves.h (would create circular include risk). Caller (waves.c) already knows the wave number. |

## Review

### Adversarial review (round 1)

| # | Severity | Finding | Resolution |
|---|----------|---------|------------|
| F1 | HIGH | `enemies_set_flash()` not updated for boss — would write `SPR_BUG_FLASH` (boss sets `type=ENEMY_BUG`) instead of `SPR_BOSS_FLASH` on every hit | Added subtask 2.3.7 with explicit boss branch in `enemies_set_flash()` |
| F2 | MEDIUM | `enemies_spawn_boss` referenced undefined `sx/sy` for `move_sprite(OAM_BOSS_BAR, ...)` — offset math only exists in `step_enemy` | Changed subtask 2.3.4: set tile only at spawn, rely on `step_enemy` (runs next frame) to position the bar. Avoids duplicating offset math. |
| F3 | MEDIUM | Wave script counts are hard-coded literals — implementer might add boss event to `_EV` array but forget to update `s_waves[n].count` | Added explicit callout in subtask 2.3.9 that counts are literals and both edits must be in lockstep. |
| F4 | MEDIUM | If W5 boss still alive during W10 spawn, `enemies_spawn_boss` returns false — unclear if wave advances or retries | Added explicit note in subtask 2.3.10 confirming existing retry path (`s_timer = 8`) handles this correctly. |
| F5 | MEDIUM | Projectile kill path showed `is_boss` capture but not `bounty` capture ordering — `enemies_apply_damage` clears `is_boss`, so `enemies_bounty` post-damage would read wrong value | Rewrote subtask 2.3.11 to show complete capture sequence: `bounty` + `etype` + `is_boss` all before `enemies_apply_damage`. |

### Cross-model validation (round 2, GPT-5.4)

| # | Severity | Finding | Resolution |
|---|----------|---------|------------|
| V1 | HIGH | `enemies_spawn` API change breaks `tests/test_enemies.c` (8 call sites) — spec only mentioned 1 call site in `waves.c` | Added explicit subtask to update all 8 calls in `test_enemies.c` to pass `wave_1based=1` (preserves ×1.0 identity). |
| V2 | HIGH | Boss walk animation snippet missing `e->anim++` — boss would be stuck on one frame | Fixed subtask 2.3.5 sprite selection: boss walk branch now includes `e->anim++` inside the else clause, matching the normal walk pattern. |
| V3 | HIGH | `enemies_spawn_boss` was underspecified — listed only boss-specific fields, not the full init block (`gen++`, `wp_idx=0`, etc.) | Rewrote subtask 2.3.4: now explicitly states "perform full regular-spawn init block first, then apply boss overrides." Lists all critical fields including `gen++`. |
| V4 | MEDIUM | Spec claimed `enemies_update` runs after `waves_update` (incorrect — `game.c::entity_tick` runs enemies first) | Corrected the frame-order reasoning in subtask 2.3.4. Bar position appears next frame in normal mode, same frame in fast mode. Both imperceptible. |
| V5 | MEDIUM | Acceptance scenarios 4–8 required QA to count exact hits — not reproducible without controlled setup | Changed scenarios to qualitative checks ("visibly takes more hits"). Exact HP values validated by host tests. |
