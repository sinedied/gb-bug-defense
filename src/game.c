#include "game.h"
#include "gfx.h"
#include "input.h"
#include "title.h"
#include "gameover.h"
#include "map.h"
#include "hud.h"
#include "cursor.h"
#include "towers.h"
#include "enemies.h"
#include "projectiles.h"
#include "waves.h"
#include "economy.h"
#include <gb/gb.h>

enum { GS_TITLE, GS_PLAYING, GS_WIN, GS_LOSE };

static u8 s_state;

static void enter_title(void) {
    /* Hide every game-state sprite slot before redrawing the title. */
    cursor_init();
    towers_init();
    enemies_init();
    projectiles_init();
    title_enter();
    s_state = GS_TITLE;
}

static void enter_playing(void) {
    /* All BG writes for the gameplay state setup (map tiles + HUD labels)
     * happen during DISPLAY_OFF so they bypass active-scan VRAM blocking. */
    DISPLAY_OFF;
    gfx_hide_all_sprites();
    map_load();
    /* Init game state first so HUD reads correct values on its first draw. */
    economy_init();
    waves_init();
    hud_init();
    cursor_init();
    towers_init();
    enemies_init();
    projectiles_init();
    DISPLAY_ON;
    s_state = GS_PLAYING;
}

static void enter_gameover(bool win) {
    gameover_enter(win);
    s_state = win ? GS_WIN : GS_LOSE;
}

void game_init(void) {
    enter_title();
}

static void playing_update(void) {
    cursor_update();
    if (input_is_pressed(J_A)) {
        towers_try_place(cursor_tx(), cursor_ty());
    }
    towers_update();
    enemies_update();
    projectiles_update();
    waves_update();

    if (economy_get_hp() == 0) {
        enter_gameover(false);
        return;
    }
    if (waves_all_cleared() && enemies_count() == 0) {
        enter_gameover(true);
    }
}

static void playing_render(void) {
    /* All gameplay BG writes (HUD digits, computer-damaged swap, newly-
     * placed tower tiles) must land in the VBlank window. Worst case ~14
     * tile writes; comfortably fits in ~1080 cycles. */
    hud_update();
    map_render();
    towers_render();
}

void game_update(void) {
    switch (s_state) {
    case GS_TITLE:
        title_update();
        if (input_is_pressed(J_START)) enter_playing();
        break;
    case GS_PLAYING:
        playing_update();
        break;
    case GS_WIN:
    case GS_LOSE:
        gameover_update();
        if (input_is_pressed(J_START)) enter_title();
        break;
    }
}

void game_render(void) {
    switch (s_state) {
    case GS_TITLE:    title_render(); break;
    case GS_PLAYING:  playing_render(); break;
    case GS_WIN:
    case GS_LOSE:     gameover_render(); break;
    }
}
