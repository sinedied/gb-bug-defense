#include "enemies.h"
#include "enemies_anim.h"
#include "map.h"
#include "economy.h"
#include "assets.h"
#include <gb/gb.h>

typedef struct {
    fix8 x, y;
    u8 hp;
    u8 wp_idx;
    u8 anim;
    u8 alive;
    u8 type;       /* iter-2: ENEMY_BUG | ENEMY_ROBOT */
    u8 gen;        /* generation counter; bumped on spawn so projectiles can
                    * detect their target slot was reused by a different bug */
    u8 flash_timer;/* iter-3 #21: frames remaining in hit-flash override */
} enemy_t;

typedef struct {
    u8   hp;
    fix8 speed;
    u8   bounty;
    u8   spr_a;
    u8   spr_b;
} enemy_stats_t;

static const enemy_stats_t s_enemy_stats[ENEMY_TYPE_COUNT] = {
    { BUG_HP,   BUG_SPEED,   BUG_BOUNTY,   SPR_BUG_A,   SPR_BUG_B   },
    { ROBOT_HP, ROBOT_SPEED, ROBOT_BOUNTY, SPR_ROBOT_A, SPR_ROBOT_B },
};

static enemy_t s_enemies[MAX_ENEMIES];

static fix8 wp_x_fix(i8 tx) {
    /* tx may be negative (-1 = off-screen spawn). */
    i16 px = (i16)tx * 8 + 4;
    return (fix8)(px << 8);
}
static fix8 wp_y_fix(i8 ty) {
    i16 py = ((i16)ty + HUD_ROWS) * 8 + 4;
    return (fix8)(py << 8);
}

void enemies_init(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        s_enemies[i].alive = 0;
        s_enemies[i].gen = 0;
        s_enemies[i].type = ENEMY_BUG;
        s_enemies[i].flash_timer = 0;
        move_sprite(OAM_ENEMIES_BASE + i, 0, 0);
    }
}

void enemies_hide_all(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        move_sprite(OAM_ENEMIES_BASE + i, 0, 0);
    }
}

bool enemies_spawn(u8 type) {
    u8 i;
    if (type >= ENEMY_TYPE_COUNT) return false;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (s_enemies[i].alive) continue;
        const waypoint_t *wp = map_waypoints();
        s_enemies[i].x = wp_x_fix(wp[0].tx);
        s_enemies[i].y = wp_y_fix(wp[0].ty);
        s_enemies[i].hp = s_enemy_stats[type].hp;
        s_enemies[i].wp_idx = 0;
        s_enemies[i].anim = 0;
        s_enemies[i].alive = 1;
        s_enemies[i].type = type;
        s_enemies[i].flash_timer = 0;
        s_enemies[i].gen++;   /* wraps at 255; collision is astronomical */
        return true;
    }
    return false;
}

u8 enemies_count(void) {
    u8 n = 0, i;
    for (i = 0; i < MAX_ENEMIES; i++) if (s_enemies[i].alive) n++;
    return n;
}

bool enemies_alive(u8 idx)    { return s_enemies[idx].alive ? true : false; }
u8   enemies_x_px(u8 idx)     { return FIX8_INTU(s_enemies[idx].x); }
u8   enemies_y_px(u8 idx)     { return FIX8_INTU(s_enemies[idx].y); }
u8   enemies_wp_idx(u8 idx)   { return s_enemies[idx].wp_idx; }
u8   enemies_gen(u8 idx)      { return s_enemies[idx].gen; }
u8   enemies_bounty(u8 idx)   { return s_enemy_stats[s_enemies[idx].type].bounty; }

void enemies_set_flash(u8 idx) {
    if (idx >= MAX_ENEMIES) return;
    if (!s_enemies[idx].alive) return;
    s_enemies[idx].flash_timer = FLASH_FRAMES;
    /* Iter-3 #21 F1: apply flash tile immediately. enemies_update()
     * runs BEFORE projectiles_update() in playing_update(), so without
     * this, the flash sprite would only appear next frame — one frame
     * after the SFX. Writing the tile here syncs visual + audio on the
     * same frame the hit lands. */
    {
        u8 tile = (s_enemies[idx].type == ENEMY_BUG)
                  ? SPR_BUG_FLASH : SPR_ROBOT_FLASH;
        set_sprite_tile(OAM_ENEMIES_BASE + idx, tile);
    }
}

bool enemies_apply_damage(u8 idx, u8 dmg) {
    if (!s_enemies[idx].alive) return false;
    if (s_enemies[idx].hp <= dmg) {
        s_enemies[idx].hp = 0;
        s_enemies[idx].alive = 0;
        move_sprite(OAM_ENEMIES_BASE + idx, 0, 0);
        return true;
    }
    s_enemies[idx].hp -= dmg;
    return false;
}

static void step_enemy(u8 i) {
    enemy_t *e = &s_enemies[i];
    const waypoint_t *wps = map_waypoints();

    /* Target = next waypoint (wp_idx + 1). */
    u8 nxt = e->wp_idx + 1;
    if (nxt >= WAYPOINT_COUNT) {
        /* Reached computer: damage + despawn. */
        economy_damage(1);
        e->alive = 0;
        move_sprite(OAM_ENEMIES_BASE + i, 0, 0);
        return;
    }
    fix8 tx = wp_x_fix(wps[nxt].tx);
    fix8 ty = wp_y_fix(wps[nxt].ty);

    /* Path is axis-aligned; move along the dominant axis. */
    i16 ddx = tx - e->x;
    i16 ddy = ty - e->y;
    fix8 step = s_enemy_stats[e->type].speed;

    if (ddx > 0) {
        if (ddx <= step) { e->x = tx; }
        else             { e->x += step; }
    } else if (ddx < 0) {
        if (-ddx <= step) { e->x = tx; }
        else              { e->x -= step; }
    } else if (ddy > 0) {
        if (ddy <= step) { e->y = ty; }
        else             { e->y += step; }
    } else if (ddy < 0) {
        if (-ddy <= step) { e->y = ty; }
        else              { e->y -= step; }
    }

    if (e->x == tx && e->y == ty) {
        e->wp_idx = nxt;
    }

    /* Iter-3 #21: hit-flash sprite override takes precedence over the
     * walk anim. Timer ticks here (inside step_enemy / enemies_update),
     * which is already gated by playing_update behind
     * !game_is_modal_open() — so flash naturally freezes during pause
     * and the upgrade/sell menu (D16). */
    u8 tile;
    if (enemies_flash_step(&e->flash_timer)) {
        tile = (e->type == ENEMY_BUG) ? SPR_BUG_FLASH : SPR_ROBOT_FLASH;
    } else {
        e->anim++;
        u8 frame = (e->anim >> 4) & 1;
        tile = frame ? s_enemy_stats[e->type].spr_b : s_enemy_stats[e->type].spr_a;
    }
    set_sprite_tile(OAM_ENEMIES_BASE + i, tile);
    /* GB sprite origin: pixel center -> -4 to align top-left of 8x8 tile. */
    u8 sx = FIX8_INTU(e->x) - 4 + 8;
    u8 sy = FIX8_INTU(e->y) - 4 + 16;
    move_sprite(OAM_ENEMIES_BASE + i, sx, sy);
}

void enemies_update(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (s_enemies[i].alive) step_enemy(i);
    }
}
