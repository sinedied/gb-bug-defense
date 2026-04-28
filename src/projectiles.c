#include "projectiles.h"
#include "enemies.h"
#include "economy.h"
#include "assets.h"
#include <gb/gb.h>

typedef struct {
    fix8 x, y;
    u8 target;
    u8 target_gen;   /* enemies_gen() at fire time; mismatch = slot reused */
    u8 alive;
} proj_t;

static proj_t s_proj[MAX_PROJECTILES];

void projectiles_init(void) {
    u8 i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        s_proj[i].alive = 0;
        move_sprite(OAM_PROJ_BASE + i, 0, 0);
    }
}

bool projectiles_fire(fix8 x, fix8 y, u8 target_idx) {
    u8 i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (!s_proj[i].alive) {
            s_proj[i].x = x;
            s_proj[i].y = y;
            s_proj[i].target = target_idx;
            s_proj[i].target_gen = enemies_gen(target_idx);
            s_proj[i].alive = 1;
            return true;
        }
    }
    return false;
}

static void step_proj(u8 i) {
    proj_t *p = &s_proj[i];
    /* Despawn if target is dead OR the slot was reused by a different bug
     * (gen mismatch). Otherwise an in-flight shot would silently retarget. */
    if (!enemies_alive(p->target) || enemies_gen(p->target) != p->target_gen) {
        p->alive = 0;
        move_sprite(OAM_PROJ_BASE + i, 0, 0);
        return;
    }
    fix8 tx = (fix8)((i16)enemies_x_px(p->target) << 8);
    fix8 ty = (fix8)((i16)enemies_y_px(p->target) << 8);
    i16 ddx = tx - p->x;
    i16 ddy = ty - p->y;

    /* Hit check (squared pixel distance). Unsigned to avoid i16 overflow:
     * worst-case 160² + 144² = 46336 > 32767. */
    i16 px = (i16)FIX8_INTU(p->x);
    i16 py = (i16)FIX8_INTU(p->y);
    i16 ex = (i16)enemies_x_px(p->target);
    i16 ey = (i16)enemies_y_px(p->target);
    i16 dxp = ex - px;
    i16 dyp = ey - py;
    u16 adxp = dxp < 0 ? (u16)-dxp : (u16)dxp;
    u16 adyp = dyp < 0 ? (u16)-dyp : (u16)dyp;
    u16 d2 = adxp * adxp + adyp * adyp;
    if (d2 <= (u16)PROJ_HIT_SQ) {
        bool killed = enemies_apply_damage(p->target, PROJ_DAMAGE);
        if (killed) economy_award(KILL_BOUNTY);
        p->alive = 0;
        move_sprite(OAM_PROJ_BASE + i, 0, 0);
        return;
    }

    /* Move toward target along dominant axis (cheap & correct enough). */
    fix8 step = PROJ_SPEED;
    /* Decompose into both axes so projectiles can travel diagonals.       */
    i16 abx = ddx < 0 ? -ddx : ddx;
    i16 aby = ddy < 0 ? -ddy : ddy;
    if (abx >= aby) {
        if (ddx > 0) p->x += (ddx < step ? ddx : step);
        else if (ddx < 0) p->x -= (-ddx < step ? -ddx : step);
        if (ddy > 0) p->y += step >> 1;
        else if (ddy < 0) p->y -= step >> 1;
    } else {
        if (ddy > 0) p->y += (ddy < step ? ddy : step);
        else if (ddy < 0) p->y -= (-ddy < step ? -ddy : step);
        if (ddx > 0) p->x += step >> 1;
        else if (ddx < 0) p->x -= step >> 1;
    }

    set_sprite_tile(OAM_PROJ_BASE + i, SPR_PROJ);
    u8 sx = FIX8_INTU(p->x) - 4 + 8;
    u8 sy = FIX8_INTU(p->y) - 4 + 16;
    move_sprite(OAM_PROJ_BASE + i, sx, sy);
}

void projectiles_update(void) {
    u8 i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (s_proj[i].alive) step_proj(i);
    }
}
