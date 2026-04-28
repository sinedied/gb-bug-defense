#include "enemies.h"
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
    u8 gen;        /* generation counter; bumped on spawn so projectiles can
                    * detect their target slot was reused by a different bug */
} enemy_t;

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
        move_sprite(OAM_ENEMIES_BASE + i, 0, 0);
    }
}

bool enemies_spawn(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (s_enemies[i].alive) continue;
        const waypoint_t *wp = map_waypoints();
        s_enemies[i].x = wp_x_fix(wp[0].tx);
        s_enemies[i].y = wp_y_fix(wp[0].ty);
        s_enemies[i].hp = BUG_HP;
        s_enemies[i].wp_idx = 0;
        s_enemies[i].anim = 0;
        s_enemies[i].alive = 1;
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
    fix8 step = BUG_SPEED;

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

    /* Animate every 16 frames. */
    e->anim++;
    u8 frame = (e->anim >> 4) & 1;
    set_sprite_tile(OAM_ENEMIES_BASE + i, frame ? SPR_BUG_B : SPR_BUG_A);
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
