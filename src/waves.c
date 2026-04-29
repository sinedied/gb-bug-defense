#include "waves.h"
#include "enemies.h"
#include "hud.h"
#include "difficulty_calc.h"
#include "game.h"

/* Iter-2 wave script: 10 waves, mixed bug+robot composition.
 * Each wave is a const list of (type, delay) spawn events. The first event's
 * delay is unused (spawn fires immediately on WS_DELAY -> WS_SPAWNING). All
 * subsequent delays are the spawn interval in frames. See specs/iter2.md §7. */

typedef struct { u8 type; u8 delay; } spawn_event_t;
typedef struct {
    const spawn_event_t *events;
    u8  count;
    u16 inter_delay;        /* delay AFTER this wave clears, before next starts */
} wave_t;

#define B ENEMY_BUG
#define R ENEMY_ROBOT
#define A ENEMY_ARMORED

/* W1: 5 bugs @ 90 f */
static const spawn_event_t W1_EV[] = {
    {B,90},{B,90},{B,90},{B,90},{B,90}
};
/* W2: 8 bugs @ 75 f */
static const spawn_event_t W2_EV[] = {
    {B,75},{B,75},{B,75},{B,75},{B,75},{B,75},{B,75},{B,75}
};
/* W3: B×3, R, B×2, R   (interval 75 f, count 7) */
static const spawn_event_t W3_EV[] = {
    {B,75},{B,75},{B,75},{R,75},{B,75},{B,75},{R,75}
};
/* W4: R, B×5, R, B×5   (interval 60 f, count 12) */
static const spawn_event_t W4_EV[] = {
    {R,60},{B,60},{B,60},{B,60},{B,60},{B,60},
    {R,60},{B,60},{B,60},{B,60},{B,60},{B,60}
};
/* W5: (B×2, R) × 4   (interval 60 f, count 12) */
static const spawn_event_t W5_EV[] = {
    {B,60},{B,60},{R,60},
    {B,60},{B,60},{R,60},
    {B,60},{B,60},{R,60},
    {B,60},{B,60},{R,60}
};
/* W6: R×2, B×6, R×2, B×6   (interval 50 f, count 16) */
static const spawn_event_t W6_EV[] = {
    {R,50},{R,50},{B,50},{B,50},{B,50},{B,50},{B,50},{B,50},
    {R,50},{R,50},{B,50},{B,50},{B,50},{B,50},{B,50},{B,50}
};
/* W7: (R,R,B) × 4, R, R + 1 armored   (count 15) */
static const spawn_event_t W7_EV[] = {
    {R,50},{R,50},{B,50},
    {R,50},{R,50},{B,50},
    {R,50},{R,50},{B,50},
    {R,50},{R,50},{B,50},
    {R,50},{R,50},
    {A,90}
};
/* W8: (B,B,R) × 6, B, B + 1 armored   (count 21) */
static const spawn_event_t W8_EV[] = {
    {B,50},{B,50},{R,50},
    {B,50},{B,50},{R,50},
    {B,50},{B,50},{R,50},
    {B,50},{B,50},{R,50},
    {B,50},{B,50},{R,50},
    {B,50},{B,50},{R,50},
    {B,50},{B,50},
    {A,90}
};
/* W9: strict alternating B,R × 10 + 2 armored   (count 22) */
static const spawn_event_t W9_EV[] = {
    {B,50},{R,50},{B,50},{R,50},{B,50},{R,50},{B,50},{R,50},{B,50},{R,50},
    {B,50},{R,50},{B,50},{R,50},{B,50},{R,50},{B,50},{R,50},{B,50},{R,50},
    {A,80},{A,80}
};
/* W10: R×4, B×6, R×4, B×6, R×4, B×4 + 3 armored   (count 31) */
static const spawn_event_t W10_EV[] = {
    {R,50},{R,50},{R,50},{R,50},
    {B,50},{B,50},{B,50},{B,50},{B,50},{B,50},
    {R,50},{R,50},{R,50},{R,50},
    {B,50},{B,50},{B,50},{B,50},{B,50},{B,50},
    {R,50},{R,50},{R,50},{R,50},
    {B,50},{B,50},{B,50},{B,50},
    {A,75},{A,75},{A,75}
};

static const wave_t s_waves[MAX_WAVES] = {
    { W1_EV,  5, 180 },
    { W2_EV,  8, 180 },
    { W3_EV,  7, 180 },
    { W4_EV, 12, 180 },
    { W5_EV, 12, 180 },
    { W6_EV, 16, 180 },
    { W7_EV, 15, 180 },
    { W8_EV, 21, 180 },
    { W9_EV, 22, 240 },
    { W10_EV,31, 0   },
};

#undef B
#undef R
#undef A

enum { WS_DELAY, WS_SPAWNING, WS_DONE };

static u8  s_state;
static u8  s_cur;       /* 0..MAX_WAVES; once == MAX_WAVES -> WS_DONE */
static u8  s_event_idx;
static u16 s_timer;
static u8  s_just_cleared;   /* iter-3 #19 one-shot edge */

void waves_init(void) {
    s_state     = WS_DELAY;
    s_cur       = 0;
    s_event_idx = 0;
    s_timer     = FIRST_GRACE;
    s_just_cleared = 0;
    hud_mark_w_dirty();
}

u8 waves_just_cleared_wave(void) {
    u8 v = s_just_cleared;
    s_just_cleared = 0;
    return v;
}

u8 waves_get_current(void) {
    if (s_cur >= MAX_WAVES) return MAX_WAVES;
    return s_cur + 1;
}

u8 waves_get_total(void) { return MAX_WAVES; }

bool waves_all_cleared(void) { return s_state == WS_DONE; }

void waves_update(void) {
    if (s_state == WS_DONE) return;

    if (s_state == WS_DELAY) {
        if (s_timer) s_timer--;
        if (s_timer == 0) {
            s_state = WS_SPAWNING;
            s_event_idx = 0;
            s_timer = 0;       /* spawn first event immediately */
        }
        return;
    }

    /* WS_SPAWNING */
    if (s_timer) { s_timer--; return; }

    const spawn_event_t *e = &s_waves[s_cur].events[s_event_idx];
    if (enemies_spawn(e->type)) {
        s_event_idx++;
        if (s_event_idx >= s_waves[s_cur].count) {
            u16 nxt_delay = s_waves[s_cur].inter_delay;
            /* Iter-3 #19: arm "wave N just cleared" edge. s_cur is
             * still 0-based here, so s_cur + 1 is the 1-based wave
             * number that just finished spawning. Cleared on read. */
            s_just_cleared = s_cur + 1u;
            s_cur++;
            hud_mark_w_dirty();
            if (s_cur >= MAX_WAVES) {
                s_state = WS_DONE;
            } else {
                s_state = WS_DELAY;
                s_timer = nxt_delay;
            }
        } else {
            s_timer = difficulty_scale_interval(s_waves[s_cur].events[s_event_idx].delay, game_difficulty());
        }
    } else {
        s_timer = 8;   /* pool full — retry shortly */
    }
}
