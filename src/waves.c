#include "waves.h"
#include "enemies.h"
#include "hud.h"

typedef struct { u8 count; u8 spawn_interval; } wave_t;

static const wave_t s_waves[3] = {
    { 5,  90 },
    { 8,  75 },
    { 12, 60 },
};

enum { WS_DELAY, WS_SPAWNING, WS_DONE };

static u8  s_state;
static u8  s_cur;       /* 0..2; once 3 -> WS_DONE */
static u8  s_spawned;
static u16 s_timer;

void waves_init(void) {
    s_state   = WS_DELAY;
    s_cur     = 0;
    s_spawned = 0;
    s_timer   = FIRST_GRACE;
    hud_mark_w_dirty();
}

u8 waves_get_current(void) {
    if (s_cur >= 3) return 3;
    return s_cur + 1;
}

bool waves_all_cleared(void) { return s_state == WS_DONE; }

void waves_update(void) {
    if (s_state == WS_DONE) return;

    if (s_state == WS_DELAY) {
        if (s_timer) s_timer--;
        if (s_timer == 0) {
            s_state = WS_SPAWNING;
            s_spawned = 0;
            s_timer = 0;
        }
        return;
    }

    /* WS_SPAWNING */
    if (s_timer) s_timer--;
    if (s_timer == 0) {
        if (enemies_spawn()) {
            s_spawned++;
            s_timer = s_waves[s_cur].spawn_interval;
        } else {
            s_timer = 8; /* pool full, retry shortly */
        }
    }

    if (s_spawned >= s_waves[s_cur].count) {
        s_cur++;
        hud_mark_w_dirty();
        if (s_cur >= 3) {
            s_state = WS_DONE;
        } else {
            s_state = WS_DELAY;
            s_timer = INTER_WAVE_DELAY;
        }
    }
}
