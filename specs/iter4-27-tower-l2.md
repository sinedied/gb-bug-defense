# Deeper Tower Upgrades (L2)

## Problem
Towers currently support only one upgrade level (L0→L1). Players who invest in a tower have no further progression path. Feature #27 adds a second upgrade level (L0→L1→L2) for all three tower types (AV, FW, EMP), deepening strategic choice and economy investment.

## Architecture

### Struct extension
`tower_stats_t` grows from 15 to 21 bytes (6 new fields). New fields placed adjacent to their L1 counterparts:
- `upgrade_cost_l2` after `upgrade_cost`
- `cooldown_l2` after `cooldown_l1`
- `damage_l2` after `damage_l1`
- `bg_tile_l2` + `bg_tile_alt_l2` after `bg_tile_alt_l1`
- `stun_frames_l2` after `stun_frames_l1`

The struct is `static const` ROM-resident. No WRAM growth.

### Level dispatch pattern
All existing `level ? L1 : L0` binary ternaries become 3-way:
```c
u8 lvl = s_towers[i].level;
u8 val = lvl >= 2 ? field_l2 : lvl ? field_l1 : field_l0;
```

### Upgrade logic
`towers_can_upgrade()` accepts level 0 or 1 (max level = 2). Returns false for level ≥ 2.
`towers_upgrade()` increments level (not hardcoded to 1), picks cost from `level == 0 ? upgrade_cost : upgrade_cost_l2`. After `level++`, cooldown assignment for TKIND_DAMAGE uses the new level value: `level == 1 ? cooldown_l1 : cooldown_l2`.

### Menu integration
- Upgrade button guard: `level >= 2` (was `level != 0`)
- Cost display: shows next upgrade cost for L0/L1 towers, dashes for L2
- Cost source: `level == 0 ? upgrade_cost : upgrade_cost_l2`

### Assets
6 new BG tiles (L2 main + L2 idle-alt × 3 types). MAP_TILE_COUNT 29→35.

### BG write budget
Unchanged from #26. The L2 upgrade-close frame produces the same 16 writes (HUD 3 + BG restore 12 + Phase 2 dirty 1). Tower render phases still contribute ≤1 write/frame.

## Exact Struct (`tower_stats_t`)

```c
typedef struct {
    u8  cost;
    u8  upgrade_cost;
    u8  upgrade_cost_l2;    /* L1→L2 cost (iter-4 #27) */
    u8  cooldown;           /* level 0 */
    u8  cooldown_l1;
    u8  cooldown_l2;        /* level 2 (iter-4 #27) */
    u8  damage;             /* only for TKIND_DAMAGE */
    u8  damage_l1;          /* only for TKIND_DAMAGE */
    u8  damage_l2;          /* only for TKIND_DAMAGE (iter-4 #27) */
    u8  range_px;           /* level-independent */
    u8  bg_tile;
    u8  bg_tile_alt;
    u8  bg_tile_l1;
    u8  bg_tile_alt_l1;
    u8  bg_tile_l2;         /* L2 BG tile (iter-4 #27) */
    u8  bg_tile_alt_l2;     /* L2 idle-blink tile (iter-4 #27) */
    u8  hud_letter;
    u8  kind;               /* TKIND_DAMAGE or TKIND_STUN */
    u8  stun_frames;        /* only for TKIND_STUN — level 0 */
    u8  stun_frames_l1;     /* only for TKIND_STUN — level 1 */
    u8  stun_frames_l2;     /* only for TKIND_STUN — level 2 (iter-4 #27) */
} tower_stats_t;
```

## tuning.h Constants

Append after the EMP L1 constants:

```c
/* Iter-4 #27: L2 tower stats */
#define TOWER_AV_UPG_COST_L2   25
#define TOWER_AV_COOLDOWN_L2   30
#define TOWER_AV_DAMAGE_L2      3

#define TOWER_FW_UPG_COST_L2   30
#define TOWER_FW_COOLDOWN_L2   70
#define TOWER_FW_DAMAGE_L2      6

#define TOWER_EMP_UPG_COST_L2  20
#define TOWER_EMP_COOLDOWN_L2 100
#define TOWER_EMP_STUN_L2     120
```

## `s_tower_stats[]` Values

```c
static const tower_stats_t s_tower_stats[TOWER_TYPE_COUNT] = {
    /* AV */ {
        .cost = TOWER_AV_COST, .upgrade_cost = TOWER_AV_UPG_COST,
        .upgrade_cost_l2 = TOWER_AV_UPG_COST_L2,
        .cooldown = TOWER_AV_COOLDOWN, .cooldown_l1 = TOWER_AV_COOLDOWN_L1,
        .cooldown_l2 = TOWER_AV_COOLDOWN_L2,
        .damage = TOWER_AV_DAMAGE, .damage_l1 = TOWER_AV_DAMAGE_L1,
        .damage_l2 = TOWER_AV_DAMAGE_L2,
        .range_px = TOWER_AV_RANGE_PX,
        .bg_tile = TILE_TOWER, .bg_tile_alt = TILE_TOWER_B,
        .bg_tile_l1 = TILE_TOWER_L1, .bg_tile_alt_l1 = TILE_TOWER_L1_B,
        .bg_tile_l2 = TILE_TOWER_L2, .bg_tile_alt_l2 = TILE_TOWER_L2_B,
        .hud_letter = 'A', .kind = TKIND_DAMAGE,
        .stun_frames = 0, .stun_frames_l1 = 0, .stun_frames_l2 = 0,
    },
    /* FW */ {
        .cost = TOWER_FW_COST, .upgrade_cost = TOWER_FW_UPG_COST,
        .upgrade_cost_l2 = TOWER_FW_UPG_COST_L2,
        .cooldown = TOWER_FW_COOLDOWN, .cooldown_l1 = TOWER_FW_COOLDOWN_L1,
        .cooldown_l2 = TOWER_FW_COOLDOWN_L2,
        .damage = TOWER_FW_DAMAGE, .damage_l1 = TOWER_FW_DAMAGE_L1,
        .damage_l2 = TOWER_FW_DAMAGE_L2,
        .range_px = TOWER_FW_RANGE_PX,
        .bg_tile = TILE_TOWER_2, .bg_tile_alt = TILE_TOWER_2_B,
        .bg_tile_l1 = TILE_TOWER_2_L1, .bg_tile_alt_l1 = TILE_TOWER_2_L1_B,
        .bg_tile_l2 = TILE_TOWER_2_L2, .bg_tile_alt_l2 = TILE_TOWER_2_L2_B,
        .hud_letter = 'F', .kind = TKIND_DAMAGE,
        .stun_frames = 0, .stun_frames_l1 = 0, .stun_frames_l2 = 0,
    },
    /* EMP */ {
        .cost = TOWER_EMP_COST, .upgrade_cost = TOWER_EMP_UPG_COST,
        .upgrade_cost_l2 = TOWER_EMP_UPG_COST_L2,
        .cooldown = TOWER_EMP_COOLDOWN, .cooldown_l1 = TOWER_EMP_COOLDOWN_L1,
        .cooldown_l2 = TOWER_EMP_COOLDOWN_L2,
        .damage = 0, .damage_l1 = 0, .damage_l2 = 0,
        .range_px = TOWER_EMP_RANGE_PX,
        .bg_tile = TILE_TOWER_3, .bg_tile_alt = TILE_TOWER_3_B,
        .bg_tile_l1 = TILE_TOWER_3_L1, .bg_tile_alt_l1 = TILE_TOWER_3_L1_B,
        .bg_tile_l2 = TILE_TOWER_3_L2, .bg_tile_alt_l2 = TILE_TOWER_3_L2_B,
        .hud_letter = 'E', .kind = TKIND_STUN,
        .stun_frames = TOWER_EMP_STUN, .stun_frames_l1 = TOWER_EMP_STUN_L1,
        .stun_frames_l2 = TOWER_EMP_STUN_L2,
    },
};
```

## `towers_can_upgrade()` — New Implementation

```c
bool towers_can_upgrade(u8 idx) {
    if (!s_towers[idx].alive) return false;
    if (s_towers[idx].level >= 2) return false;
    const tower_stats_t *st = &s_tower_stats[s_towers[idx].type];
    u8 cost = s_towers[idx].level == 0 ? st->upgrade_cost : st->upgrade_cost_l2;
    return economy_get_energy() >= cost;
}
```

## `towers_upgrade()` — New Implementation

```c
bool towers_upgrade(u8 idx) {
    if (!towers_can_upgrade(idx)) return false;
    const tower_stats_t *st = s_tower_stats + s_towers[idx].type;
    u8 cost = s_towers[idx].level == 0 ? st->upgrade_cost : st->upgrade_cost_l2;
    if (!economy_try_spend(cost)) return false;
    s_towers[idx].level++;
    s_towers[idx].spent += cost;
    s_towers[idx].dirty = 1;         /* iter-4 #26/#27: schedule tile repaint */
    s_towers[idx].idle_phase = 0;    /* matches the about-to-be-painted base tile */
    /* Iter-3 #18 F2 + iter-4 #27: only TKIND_DAMAGE resets cooldown.
     * EMP (TKIND_STUN) preserves its current cooldown across all upgrades
     * (L0→L1 and L1→L2). Resetting on upgrade forces an idle EMP at
     * cooldown=0 into a full dead-window (up to 100 frames at L2),
     * violating D-IT3-18-7 (cooldown set only on successful fire).
     * The L2 cadence (100) differs from L1 (120); the invariant applies
     * regardless of cadence equality. */
    if (st->kind == TKIND_DAMAGE) {
        s_towers[idx].cooldown = s_towers[idx].level == 1
                                 ? st->cooldown_l1 : st->cooldown_l2;
    }
    audio_play(SFX_TOWER_PLACE);
    return true;
}
```

## `towers_update()` — Level-Dependent Reads

### TKIND_STUN block

Replace:
```c
u8 stun_dur = s_towers[i].level
              ? st->stun_frames_l1 : st->stun_frames;
```
With:
```c
u8 lvl = s_towers[i].level;
u8 stun_dur = lvl >= 2 ? st->stun_frames_l2
            : lvl      ? st->stun_frames_l1 : st->stun_frames;
```

Replace both cooldown assignments (`any_stunned` and `found_target` branches):
```c
s_towers[i].cooldown = s_towers[i].level
                       ? st->cooldown_l1 : st->cooldown;
```
With:
```c
s_towers[i].cooldown = lvl >= 2 ? st->cooldown_l2
                     : lvl      ? st->cooldown_l1 : st->cooldown;
```

### TKIND_DAMAGE block

Replace:
```c
u8 dmg = s_towers[i].level ? st->damage_l1 : st->damage;
```
With:
```c
u8 lvl = s_towers[i].level;
u8 dmg = lvl >= 2 ? st->damage_l2 : lvl ? st->damage_l1 : st->damage;
```

Replace cooldown assignment:
```c
s_towers[i].cooldown = s_towers[i].level
                       ? st->cooldown_l1 : st->cooldown;
```
With:
```c
s_towers[i].cooldown = lvl >= 2 ? st->cooldown_l2
                     : lvl      ? st->cooldown_l1 : st->cooldown;
```

## `towers_render()` — Level Tile Selection

### Phase 2 (placement/dirty)

Replace:
```c
const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
set_bkg_tile_xy(s_towers[i].tx,
                s_towers[i].ty + HUD_ROWS,
                s_towers[i].level ? st->bg_tile_l1 : st->bg_tile);
```
With:
```c
const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
u8 lvl = s_towers[i].level;
set_bkg_tile_xy(s_towers[i].tx,
                s_towers[i].ty + HUD_ROWS,
                lvl >= 2 ? st->bg_tile_l2 : lvl ? st->bg_tile_l1 : st->bg_tile);
```

### Phase 3 (idle LED)

Replace:
```c
const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
u8 base = s_towers[i].level ? st->bg_tile_l1     : st->bg_tile;
u8 alt  = s_towers[i].level ? st->bg_tile_alt_l1  : st->bg_tile_alt;
```
With:
```c
const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
u8 lvl = s_towers[i].level;
u8 base = lvl >= 2 ? st->bg_tile_l2     : lvl ? st->bg_tile_l1     : st->bg_tile;
u8 alt  = lvl >= 2 ? st->bg_tile_alt_l2 : lvl ? st->bg_tile_alt_l1 : st->bg_tile_alt;
```

## `menu.c` Changes

### `menu_update()` — upgrade guard (line 114)

Replace:
```c
if (towers_get_level(s_tower_idx) != 0) return;
```
With:
```c
if (towers_get_level(s_tower_idx) >= 2) return;
```

### `menu_render()` — cost source (line 194)

Replace:
```c
u8 upg_cost = st->upgrade_cost;
```
With:
```c
u8 upg_cost = level == 0 ? st->upgrade_cost : st->upgrade_cost_l2;
```

### `menu_render()` — display branch (line 207)

Replace:
```c
if (level == 0) {
```
With:
```c
if (level < 2) {
```

## `towers.h` Header Updates

Replace:
```c
bool towers_can_upgrade(u8 idx);         /* level==0 && energy >= upgrade_cost */
bool towers_upgrade(u8 idx);             /* spends, sets level=1 */
```
With:
```c
bool towers_can_upgrade(u8 idx);         /* level<2 && energy >= next upgrade cost */
bool towers_upgrade(u8 idx);             /* spends, increments level */
```

## `tower_t` Comment Update (towers.c, line 24)

Replace:
```c
    u8 level;       /* 0 | 1 */
```
With:
```c
    u8 level;       /* 0 | 1 | 2 */
```

## L2 Tile Designs

All designs use `design_tile()` encoding: `.`=White, `,`=Light grey, `o`=Dark grey, `#`=Black.

### Design principle
L2 = "maxed out" variant. Outer ring/edges darkened one shade beyond L1. Center fills completed. Tower identity motifs (spoke/brick/ring) preserved.

### AV L2 — "Overclocked Scanner"
Outer ring light-grey→dark-grey. Both center rows filled to light-grey (L1 had only one).

Main:
```
#oo##oo#
oo####oo
o##oo##o
#o#,,#o#
#o#,,#o#
o##oo##o
oo####oo
#oo##oo#
```
Alt (blink) — LED at [3][4]: `,` → `.`
```
#oo##oo#
oo####oo
o##oo##o
#o#,.#o#
#o#,,#o#
o##oo##o
oo####oo
#oo##oo#
```

### FW L2 — "Hardened Wall"
Triple ember (was double in L1). Staggered mortar joints on row 5 (`##o##o##`).

Main:
```
########
#o##o##o
########
o##,,,oo
########
##o##o##
########
########
```
Alt (blink) — LED at [3][3]: `,` → `.`
```
########
#o##o##o
########
o##.,,oo
########
##o##o##
########
########
```

### EMP L2 — "Overloaded Coil"
Corner pixels darkened (white→light-grey). Edge pixels darkened (white→light-grey). Inner ring and center unchanged from L1.

Main:
```
,oo##oo,
,o####o,
o######o
##o,,o##
##o,,o##
o######o
,o####o,
,oo##oo,
```
Alt (blink) — LED at [3][3]: `,` → `.`
```
,oo##oo,
,o####o,
o######o
##o.,o##
##o,,o##
o######o
,o####o,
,oo##oo,
```

## gen_assets.py Changes

### New tile definitions (after L1 tile definitions, ~line 586)

```python
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
```

### Append to map_tiles list (after L1 entries)

```python
    # Iter-4 #27: L2 tower tiles.
    ('TILE_TOWER_L2',      TOWER_L2_BG),
    ('TILE_TOWER_L2_B',    TOWER_L2_BG_B),
    ('TILE_TOWER_2_L2',    TOWER2_L2_BG),
    ('TILE_TOWER_2_L2_B',  TOWER2_L2_BG_B),
    ('TILE_TOWER_3_L2',    TOWER3_L2_BG),
    ('TILE_TOWER_3_L2_B',  TOWER3_L2_BG_B),
```

### Update assets_h string

After the L1 defines, replace `#define MAP_TILE_COUNT  29` with:
```c
#define TILE_TOWER_L2     (MAP_TILE_BASE + 29)
#define TILE_TOWER_L2_B   (MAP_TILE_BASE + 30)
#define TILE_TOWER_2_L2   (MAP_TILE_BASE + 31)
#define TILE_TOWER_2_L2_B (MAP_TILE_BASE + 32)
#define TILE_TOWER_3_L2   (MAP_TILE_BASE + 33)
#define TILE_TOWER_3_L2_B (MAP_TILE_BASE + 34)
#define MAP_TILE_COUNT  35
```

## New Tests (`tests/test_towers.c`)

### Stub extension — projectiles_fire recording

Replace the existing `projectiles_fire` stub:
```c
bool projectiles_fire(fix8 x, fix8 y, u8 target, u8 damage) {
    (void)x; (void)y; (void)target; (void)damage;
    return true;
}
```
With:
```c
static u8 s_last_proj_damage;
static int s_proj_fire_count;

bool projectiles_fire(fix8 x, fix8 y, u8 target, u8 damage) {
    (void)x; (void)y; (void)target;
    s_last_proj_damage = damage;
    s_proj_fire_count++;
    return true;
}
```

Also add `s_proj_fire_count = 0; s_last_proj_damage = 0;` to `reset_all()`.

### Test functions

```c
/* T_L2_1: L0→L1→L2 upgrade path, and can_upgrade gate at L2. */
static void test_l2_can_upgrade_levels(void) {
    reset_all();
    CHECK(towers_try_place(5, 5, TOWER_AV));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    CHECK(towers_can_upgrade((u8)idx));        /* L0: can */
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_level((u8)idx), 1);
    CHECK(towers_can_upgrade((u8)idx));        /* L1: can */
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_level((u8)idx), 2);
    CHECK(!towers_can_upgrade((u8)idx));       /* L2: cannot */
    CHECK(!towers_upgrade((u8)idx));           /* upgrade fails */
}

/* T_L2_2: spent accumulates all 3 costs; sell refund = spent/2. */
static void test_l2_spent_and_refund(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_FW));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    CHECK_EQ(towers_get_spent((u8)idx), TOWER_FW_COST);
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_spent((u8)idx), TOWER_FW_COST + TOWER_FW_UPG_COST);
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_spent((u8)idx),
             TOWER_FW_COST + TOWER_FW_UPG_COST + TOWER_FW_UPG_COST_L2);
    /* 15 + 20 + 30 = 65. Refund = 65/2 = 32. */
    u8 energy_before = economy_get_energy();
    towers_sell((u8)idx);
    CHECK_EQ((u8)(economy_get_energy() - energy_before), 32);
}

/* T_L2_3: L2 AV fires with damage_l2. */
static void test_l2_av_damage(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_AV));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    CHECK(towers_upgrade((u8)idx));  /* L1: cooldown set to 40 */
    CHECK(towers_upgrade((u8)idx));  /* L2: cooldown set to 30 */
    /* Place enemy in range: AV center = (44, 52), range = 24 px. */
    place_enemy(0, 44, 52);
    s_proj_fire_count = 0;
    /* L2 AV cooldown=30. 30 frames to decrement to 0, then fires on 31st. */
    for (int f = 0; f < 31; f++) towers_update();
    CHECK(s_proj_fire_count > 0);
    CHECK_EQ(s_last_proj_damage, TOWER_AV_DAMAGE_L2);
}

/* T_L2_4: L2 EMP stuns with stun_frames_l2. */
static void test_l2_emp_stun_duration(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_EMP));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    CHECK(towers_upgrade((u8)idx));  /* L1: cooldown preserved (F2) */
    CHECK(towers_upgrade((u8)idx));  /* L2: cooldown preserved (F2) */
    CHECK_EQ(towers_get_level((u8)idx), 2);
    /* Place enemy in range. */
    place_enemy(0, 44, 52);
    s_stun_call_count = 0;
    /* Fresh-place cooldown=1, preserved across both upgrades.
     * Frame 1: cooldown 1→0. Frame 2: cooldown=0 → fire. */
    towers_update();
    towers_update();
    CHECK(s_stun_call_count > 0);
    int found_l2 = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result && s_stun_calls[k].frames == TOWER_EMP_STUN_L2)
            found_l2 = 1;
    }
    CHECK(found_l2);
}

/* T_L2_5: L1→L2 EMP upgrade preserves cooldown (F2 regression). */
static void test_l2_emp_upgrade_preserves_cooldown(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_EMP));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    /* Drain F3 fresh-place cooldown (1→0). */
    towers_update();
    /* Upgrade L0→L1. F2: cooldown preserved (still 0). */
    CHECK(towers_upgrade((u8)idx));
    /* Upgrade L1→L2. F2: cooldown preserved (still 0). */
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_level((u8)idx), 2);
    /* Place enemy. Tower should fire immediately (cooldown=0). */
    place_enemy(0, 44, 52);
    s_stun_call_count = 0;
    towers_update();
    int succ = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result && s_stun_calls[k].frames == TOWER_EMP_STUN_L2)
            succ++;
    }
    CHECK_EQ(succ, 1);
}

/* T_L2_6: tower stats have correct L2 constants. */
static void test_l2_tower_stats(void) {
    const tower_stats_t *av = towers_stats(TOWER_AV);
    const tower_stats_t *fw = towers_stats(TOWER_FW);
    const tower_stats_t *emp = towers_stats(TOWER_EMP);
    CHECK_EQ(av->damage_l2, TOWER_AV_DAMAGE_L2);
    CHECK_EQ(av->cooldown_l2, TOWER_AV_COOLDOWN_L2);
    CHECK_EQ(av->upgrade_cost_l2, TOWER_AV_UPG_COST_L2);
    CHECK_EQ(fw->damage_l2, TOWER_FW_DAMAGE_L2);
    CHECK_EQ(fw->cooldown_l2, TOWER_FW_COOLDOWN_L2);
    CHECK_EQ(fw->upgrade_cost_l2, TOWER_FW_UPG_COST_L2);
    CHECK_EQ(emp->stun_frames_l2, TOWER_EMP_STUN_L2);
    CHECK_EQ(emp->cooldown_l2, TOWER_EMP_COOLDOWN_L2);
    CHECK_EQ(emp->upgrade_cost_l2, TOWER_EMP_UPG_COST_L2);
}

/* T_L2_7: L2 towers render with L2 BG tiles. */
static void test_l2_render_tiles(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_AV));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    /* Render the L0 placement (Phase 2). */
    towers_render();
    CHECK_EQ(s_bkg_tiles[5][5 + HUD_ROWS], TILE_TOWER);
    /* Upgrade to L1, render. */
    CHECK(towers_upgrade((u8)idx));
    towers_render();
    CHECK_EQ(s_bkg_tiles[5][5 + HUD_ROWS], TILE_TOWER_L1);
    /* Upgrade to L2, render. */
    CHECK(towers_upgrade((u8)idx));
    towers_render();
    CHECK_EQ(s_bkg_tiles[5][5 + HUD_ROWS], TILE_TOWER_L2);
}
```

### Wire into main()

Add all 7 test calls to `main()` after the existing tests:
```c
    test_l2_can_upgrade_levels();
    test_l2_spent_and_refund();
    test_l2_av_damage();
    test_l2_emp_stun_duration();
    test_l2_emp_upgrade_preserves_cooldown();
    test_l2_tower_stats();
    test_l2_render_tiles();
```

## Subtasks

1. ✅ **Add L2 tuning constants** — Add 9 constants to `src/tuning.h`. Done when file compiles cleanly.

2. ✅ **Extend tower_stats_t** — Add 6 fields to struct in `src/towers.h`. Update header comments for `towers_can_upgrade`/`towers_upgrade`. Done when `just build` compiles (with zeroed L2 values in stats array).
   - Depends on: subtask 1

3. ✅ **Add L2 tile designs to gen_assets.py** — Define 6 `design_tile()` variables, append to `map_tiles`, update `assets_h` string with 6 new defines and `MAP_TILE_COUNT 35`. Run `just assets`. Done when `res/assets.h` contains all 6 `TILE_TOWER_*_L2*` defines and `MAP_TILE_COUNT=35`.

4. ✅ **Update s_tower_stats[] initializers** — Fill all L2 fields in the 3 entries in `src/towers.c`. Update `tower_t` level comment to `/* 0 | 1 | 2 */`. Done when `just build` compiles with no warnings.
   - Depends on: subtasks 1, 2, 3

5. ✅ **Update towers_can_upgrade and towers_upgrade** — Implement level-aware cost selection, `level++`, and updated F2 comment. Done when L0→L1→L2 works and L2 is rejected.
   - Depends on: subtask 4

6. ✅ **Update towers_update level dispatches** — Replace 4 binary ternaries with 3-way `lvl >= 2` pattern. Done when L2 towers use L2 stats.
   - Depends on: subtask 5

7. ✅ **Update towers_render level branches** — Phase 2 and Phase 3 tile selection. Done when L2 towers display L2 tiles and blink L2 alt tiles.
   - Depends on: subtask 4

8. ✅ **Update menu.c** — Three changes: guard (`>=2`), cost source (ternary with `upgrade_cost_l2`), display branch (`level<2`). Done when L1 towers show L2 cost, L2 towers show dashes, and upgrade action works for L1 towers.
   - Depends on: subtask 5

9. ✅ **Add L2 tests to test_towers.c** — Extend projectiles_fire stub, add 7 test functions, wire into main(). Done when `just test` passes with all new assertions green.
   - Depends on: subtasks 5, 6, 7

## Acceptance Scenarios

### Setup
`just setup && just build && just run`. Start game on Map 1 (Casual difficulty for high energy). Earn enough energy to place and fully upgrade towers.

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | L0→L1→L2 upgrade path | Place AV tower. Open menu, upgrade. Open menu again, upgrade. | Tower reaches level 2. Tile visibly changes at each step. |
| 2 | L2 max level gate | Open menu on an L2 tower, press A on UPG. | Nothing happens. Tower stays L2. Menu stays open. |
| 3 | L1 menu shows L2 cost | Open menu on L1 AV tower. | "UPG:25" displayed (AV L2 cost). |
| 4 | L2 menu shows dashes | Open menu on L2 tower. | "UPG:--" displayed. |
| 5 | L2 AV stronger than L0 | Place L0 AV and L2 AV side by side on same path. Observe kills. | L2 AV visibly kills enemies faster than L0 AV. |
| 6 | L2 FW stronger than L0 | Place L0 FW and L2 FW. Observe kills. | L2 FW kills armored enemies noticeably faster. |
| 7 | L2 EMP stuns longer | Place L2 EMP near path. Observe enemy freeze duration. | Enemies remain frozen visibly longer than with L0 EMP. |
| 8 | Sell L2 tower refund | Note energy. Upgrade FW to L2 (spend 15+20+30=65). Sell. | Energy increases by 32 (half of 65). |
| 9 | L2 idle animation | Upgrade tower to L2. Wait 3+ seconds. | Tower blinks with L2 alt tile (1-pixel LED diff). |
| 10 | All 3 types distinct at L2 | Place and upgrade one of each type to L2. | Each L2 tower visually distinct from the others and from L0/L1. |
| 11 | Insufficient energy blocks L2 | Have L1 tower but less energy than upg_cost_l2. Open menu, press A. | Menu stays open, no upgrade, no energy deducted. |
| 12 | Automated tests pass | Run `just test`. | All tests (existing + 7 new L2 tests) pass. |

## Constraints
- Max level = 2. No L3 support.
- All costs/spent values fit in u8 (max total spent: FW 15+20+30=65, well under 255).
- BG write budget unchanged: 16 writes on upgrade-close frame, ≤1 write/frame from towers_render.
- MAP_TILE_COUNT grows 29→35. Last tile at VRAM index 162 (under 256 limit).
- ROM growth: ~150 bytes (96 tile data + 18 struct bytes + ~36 code bytes). Negligible.
- No WRAM growth: `tower_stats_t` is ROM-resident `static const`. `tower_t` unchanged.
- SDCC C99 compatible: designated initializers, nested ternaries.
- Single-threaded: no TOCTOU between `towers_can_upgrade()` and `towers_upgrade()`.

## Decisions
| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| L2 field placement in struct | (A) Interleave next to L1 counterparts; (B) Append at end | A — interleave | Groups related fields (cooldown/cooldown_l1/cooldown_l2). Follows #26 pattern. Natural reading order. |
| Level dispatch style | (A) Nested ternary `lvl>=2 ? L2 : lvl ? L1 : L0`; (B) Helper macro; (C) Array indexing | A — nested ternary | Clear at each usage site, SDCC-friendly, only 6 instances. No indirection penalty. |
| Upgrade increment | (A) `level++`; (B) `level = target_level` | A — increment | Simpler, can't skip levels, pairs naturally with `can_upgrade()`'s `level >= 2` check. |
| Menu cost source | (A) `level==0 ? upg_cost : upg_cost_l2`; (B) Cost array | A — ternary | Only 2 possible costs. Array overengineered for 2 elements. |
| EMP L1→L2 cooldown | (A) Reset to cooldown_l2; (B) Preserve (extend F2) | B — preserve | D-IT3-18-7 applies at all level hops. Resetting forces idle EMP into 100f dead window. |
| L2 tile design principle | (A) Completely new shapes; (B) Same identity + deeper fill | B — same identity + deeper fill | Consistent with L1 approach. Players can identify tower type at all levels. |

## Review
- **R1 (MEDIUM): Missing F2 regression test for L1→L2 EMP.** Resolved: Added `test_l2_emp_upgrade_preserves_cooldown` that drains cooldown to 0, double-upgrades, and verifies immediate fire with L2 stun duration.
- **R2 (MEDIUM): Stale F2 comment in towers_upgrade ("cadences identical").** Resolved: Comment rewritten to state D-IT3-18-7 invariant directly, noting L2 cadence (100) differs from L1 (120).
- **R3 (MEDIUM): Tests not wired into main().** Resolved: Subtask 9 and test section explicitly require adding all 7 calls to `main()`.
- **R4 (MEDIUM): No render verification in automated tests.** Resolved: Added `test_l2_render_tiles` that verifies BG tile written via the existing `s_bkg_tiles` stub.
- **R5 (MEDIUM): Acceptance scenarios 5-7 used exact frame counts not observable in gameplay.** Resolved: Rewritten as qualitative comparative observations (L2 visibly faster/stronger than L0).
