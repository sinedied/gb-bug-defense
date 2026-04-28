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
#include "audio.h"
#include "menu.h"
#include "pause.h"
#include "game_modal.h"
#include <gb/gb.h>

enum { GS_TITLE, GS_PLAYING, GS_WIN, GS_LOSE };

static u8 s_state;
static u8 s_selected_type;     /* iter-2: TOWER_AV | TOWER_FW */
static u8 s_anim_frame;        /* iter-3 #21: global animation phase */

u8 game_get_selected_tower_type(void) { return s_selected_type; }

u8 game_anim_frame(void) { return s_anim_frame; }

bool game_is_modal_open(void) {
    /* Single source of truth — keep aligned with playing_update's
     * modal-precedence branch order. */
    return menu_is_open() || pause_is_open();
}

static void enter_title(void) {
    /* Hide every game-state sprite slot before redrawing the title. */
    cursor_init();
    towers_init();
    enemies_init();
    projectiles_init();
    menu_init();
    pause_init();
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
    s_selected_type = TOWER_AV;
    economy_init();
    waves_init();
    cursor_init();
    towers_init();
    enemies_init();
    projectiles_init();
    menu_init();
    pause_init();
    hud_init();
    /* Clear any stale audio state from the previous session — otherwise the
     * win/lose jingle on ch1 (priority 3) leaks across sessions and blocks
     * every subsequent ch1 SFX (e.g. tower-place) for the rest of the run. */
    audio_reset();
    DISPLAY_ON;
    s_anim_frame = 0;     /* iter-3 #21: deterministic phase per session */
    s_state = GS_PLAYING;
}

static void enter_gameover(bool win) {
    /* Play the win/lose stinger BEFORE gameover_enter() — that call does
     * DISPLAY_OFF + a 360-tile redraw which blocks the main loop for several
     * frames. Triggering the SFX first lets the channel sequencer hold the
     * first note while audio_tick is paused. (Iter-2 spec §8 F9.) */
    audio_play(win ? SFX_WIN : SFX_LOSE);
    gameover_enter(win);
    s_state = win ? GS_WIN : GS_LOSE;
}

void game_init(void) {
    enter_title();
}

static void cycle_tower_type(void) {
    s_selected_type ^= 1;
    hud_mark_t_dirty();
}

static void playing_update(void) {
    /* Modal precedence: pause first, then upgrade/sell menu, then the
     * normal entity-tick path. game_is_modal_open() is the single
     * source of truth — keep it in sync with this branch order.
     *
     * Latch menu_is_open() BEFORE menu_update() so a same-frame
     * "A/B closes menu + START" cannot leak past the end-of-frame
     * START gate and unintentionally open pause. (F1 regression.) */
    bool menu_was_open = menu_is_open();
    if (pause_is_open()) {
        pause_update();
        economy_tick();
        audio_tick();
        if (pause_quit_requested()) {
            /* QUIT: silence in-flight SFX BEFORE leaving GS_PLAYING.
             * audio_reset() lives here (NOT in enter_title) to keep
             * the boot-time NR52-first ordering invariant intact. */
            pause_close();
            audio_reset();
            enter_title();
            return;
        }
        return;
    }
    if (menu_is_open()) {
        menu_update();
    } else {
        cursor_update();
        if (input_is_pressed(J_B)) {
            cycle_tower_type();
        }
        if (input_is_pressed(J_A)) {
            i8 idx = towers_index_at(cursor_tx(), cursor_ty());
            if (idx >= 0) {
                menu_open((u8)idx);
                /* Early return so the just-opened menu freezes gameplay
                 * for THIS frame too — otherwise enemies/projectiles/waves
                 * would tick once and re-emit OAM after menu_open hid them.
                 * Audio + economy still tick. */
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
    economy_tick();   /* runs even with menu open — D19 */
    audio_tick();     /* runs always so SFX continue during menu */

    /* Gameover wins over pause: same-frame HP→0 or last-enemy-killed
     * MUST transition before the START → pause_open() check below.
     * Both branches end with explicit `return;` (the WIN branch was
     * missing it pre-iter-3 — a same-frame START on the WIN frame
     * would otherwise paint pause glyphs over the WIN screen). */
    if (economy_get_hp() == 0) {
        enter_gameover(false);
        return;
    }
    if (waves_all_cleared() && enemies_count() == 0) {
        enter_gameover(true);
        return;
    }

    /* End-of-frame START → arm pause. Edge-detected (input_is_pressed),
     * so holding START across resume does NOT auto-re-pause. Gated by
     * !modal so START presses inside the upgrade/sell menu are ignored.
     * menu_was_open guards against same-frame menu-close + START leak
     * (see playing_modal_should_open_pause / src/game_modal.h). */
    if (playing_modal_should_open_pause(menu_was_open,
                                        input_is_pressed(J_START),
                                        menu_is_open(),
                                        pause_is_open())) {
        pause_open();
    }
}

static void playing_render(void) {
    /* All gameplay BG writes (HUD digits, computer-damaged swap, newly-
     * placed tower tiles) must land in the VBlank window. */
    hud_update();
    map_render();
    towers_render();
    menu_render();    /* sprite OAM only — no BG writes */
    pause_render();   /* sprite OAM only — no BG writes */
}

void game_update(void) {
    /* Iter-3 #21: tick the global animation phase once per game_update,
     * BEFORE the per-state switch, so every observer (towers idle
     * scanner, future anims) sees a consistent value within a frame.
     * 1-byte counter wraps at 256; all phase divisors used (32, 64)
     * divide it evenly. */
    s_anim_frame++;
    switch (s_state) {
    case GS_TITLE:
        title_update();
        audio_tick();   /* drain any in-flight win/lose jingle while idle */
        if (input_is_pressed(J_START)) enter_playing();
        break;
    case GS_PLAYING:
        playing_update();
        break;
    case GS_WIN:
    case GS_LOSE:
        gameover_update();
        audio_tick();   /* keep the win/lose stinger advancing */
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
