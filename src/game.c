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
#include "music.h"
#include "menu.h"
#include "pause.h"
#include "save.h"
#include "score.h"
#include "game_modal.h"
#include "tower_select.h"
#include "gate_calc.h"
#include <gb/gb.h>

enum { GS_TITLE, GS_PLAYING, GS_WIN, GS_LOSE };

static u8 s_state;
static u8 s_selected_type;     /* iter-2: TOWER_AV | TOWER_FW */
static u8 s_anim_frame;        /* iter-3 #21: global animation phase */
/* Iter-3 #20: file-scope so it survives enter_title()/enter_playing()
 * cycles within a power-on session. NOT zero-init — relies on crt0's
 * .data copy from ROM (acceptance scenario #1 is the regression guard). */
static u8 s_difficulty = DIFF_NORMAL;
/* Iter-3 #17: active map id (0..MAP_COUNT-1). Same persistence model
 * as s_difficulty — survives enter_title/enter_playing within one
 * power-on session, resets to MAP_1 on cold-boot via .data zero-init. */
static u8 s_active_map = 0;

/* Iter-4 #24: first-tower gate state */
static u8 s_gate_active;   /* 1 while waiting for first tower placement */
static u8 s_gate_blink;    /* frame counter for blink timing (wrapping u8) */
static u8 s_gate_vis;      /* last-painted blink phase: 1=text, 0=map tiles */
static u8 s_gate_dirty;    /* 1 = need BG restore on gate-lift frame */
static bool s_fast_mode;   /* iter-4 #30: 2× speed toggle */

#define GATE_COL    7
#define GATE_ROW1   8   /* screen row = PF row 7 + HUD_ROWS */
#define GATE_ROW2   9   /* screen row = PF row 8 + HUD_ROWS */
#define GATE_W      6

u8 game_get_selected_tower_type(void) { return s_selected_type; }

u8 game_anim_frame(void) { return s_anim_frame; }

u8 game_difficulty(void) { return s_difficulty; }
void game_set_difficulty(u8 d) {
    if (d < DIFF_COUNT) s_difficulty = d;
}

u8 game_active_map(void) { return s_active_map; }
void game_set_active_map(u8 m) {
    if (m < MAP_COUNT) s_active_map = m;
}

bool game_is_modal_open(void) {
    /* Single source of truth — keep aligned with playing_update's
     * modal-precedence branch order. */
    return menu_is_open() || pause_is_open();
}

/* Iter-4 #24: gate BG helpers ---------------------------------------- */
static void gate_paint_text(void) {
    /* "PLACEA" at (GATE_COL, GATE_ROW1) */
    set_bkg_tile_xy(GATE_COL + 0, GATE_ROW1, (u8)'P');
    set_bkg_tile_xy(GATE_COL + 1, GATE_ROW1, (u8)'L');
    set_bkg_tile_xy(GATE_COL + 2, GATE_ROW1, (u8)'A');
    set_bkg_tile_xy(GATE_COL + 3, GATE_ROW1, (u8)'C');
    set_bkg_tile_xy(GATE_COL + 4, GATE_ROW1, (u8)'E');
    set_bkg_tile_xy(GATE_COL + 5, GATE_ROW1, (u8)'A');
    /* "TOWER!" at (GATE_COL, GATE_ROW2) */
    set_bkg_tile_xy(GATE_COL + 0, GATE_ROW2, (u8)'T');
    set_bkg_tile_xy(GATE_COL + 1, GATE_ROW2, (u8)'O');
    set_bkg_tile_xy(GATE_COL + 2, GATE_ROW2, (u8)'W');
    set_bkg_tile_xy(GATE_COL + 3, GATE_ROW2, (u8)'E');
    set_bkg_tile_xy(GATE_COL + 4, GATE_ROW2, (u8)'R');
    set_bkg_tile_xy(GATE_COL + 5, GATE_ROW2, (u8)'!');
}

static void gate_restore_tiles(void) {
    u8 c;
    for (c = 0; c < GATE_W; c++) {
        set_bkg_tile_xy(GATE_COL + c, GATE_ROW1,
                        map_tile_at(GATE_COL + c, GATE_ROW1 - HUD_ROWS));
        set_bkg_tile_xy(GATE_COL + c, GATE_ROW2,
                        map_tile_at(GATE_COL + c, GATE_ROW2 - HUD_ROWS));
    }
}

static void gate_render(void) {
    u8 want;
    if (s_gate_dirty) {
        gate_restore_tiles();
        s_gate_dirty = 0;
        return;
    }
    if (!s_gate_active) return;
    if (game_is_modal_open()) return;
    want = gate_blink_visible(s_gate_blink);
    if (want == s_gate_vis) return;
    s_gate_vis = want;
    if (want) {
        gate_paint_text();
    } else {
        gate_restore_tiles();
    }
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
    /* Iter-3 #16: title theme. music_play is idempotent for the same
     * song, so re-entries (gameover -> title) don't restart the loop. */
    music_play(MUS_TITLE);
    s_state = GS_TITLE;
}

static void enter_playing(void) {
    /* All BG writes for the gameplay state setup (map tiles + HUD labels)
     * happen during DISPLAY_OFF so they bypass active-scan VRAM blocking. */
    DISPLAY_OFF;
    gfx_hide_all_sprites();
    map_load(game_active_map());
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
    s_fast_mode = false;   /* iter-4 #30: always start with normal speed */
    hud_init();
    /* Iter-4 #24: first-tower gate — paint text while display is off. */
    s_gate_active = 1;
    s_gate_blink  = 0;
    s_gate_vis    = 1;
    s_gate_dirty  = 0;
    gate_paint_text();
    /* Clear any stale audio state from the previous session — otherwise the
     * win/lose jingle on ch1 (priority 3) leaks across sessions and blocks
     * every subsequent ch1 SFX (e.g. tower-place) for the rest of the run. */
    audio_reset();
    DISPLAY_ON;
    s_anim_frame = 0;     /* iter-3 #21: deterministic phase per session */
    score_reset();        /* iter-3 #19: per-run score accumulator */
    /* Iter-3 #16: in-game music starts after audio_reset() (which calls
     * music_reset()), so no overlap with the title loop. */
    music_play(MUS_PLAYING);
    s_state = GS_PLAYING;
}

static void enter_gameover(bool win) {
    /* Iter-3 #19: finalise score (win bonus first, then commit to SRAM
     * if it's a record). Save writes happen ONLY here — never per-frame,
     * never mid-game. The same record check also drives the gameover
     * banner. */
    if (win) score_add_win_bonus();
    {
        u16 sc  = score_get();
        u8  m   = game_active_map();
        u8  d   = game_difficulty();
        bool rec = sc > save_get_hi(m, d);
        if (rec) save_write_hi(m, d, sc);
        /* Iter-3 #16: WIN/LOSE music stinger. music_play is synchronous-
         * arm so the stinger's first note plays even though
         * gameover_enter() blocks the main loop for several frames. */
        music_play(win ? MUS_WIN : MUS_LOSE);
        gameover_enter(win, sc, rec);
    }
    s_state = win ? GS_WIN : GS_LOSE;
}

void game_init(void) {
    enter_title();
}

static void cycle_tower_type(void) {
    s_selected_type = tower_select_next(s_selected_type, TOWER_TYPE_COUNT);
    hud_mark_t_dirty();
}

/* Iter-4 #30: entity-tick helper — runs towers, enemies, projectiles,
 * and wave/gate-blink logic as a single atomic step.  Called once per
 * frame at normal speed, twice when s_fast_mode is active.
 * BG-write note: gate-lift frame + simultaneous SELECT can reach 18
 * tiles (2 over the 16-tile VBlank cap) — accepted as a single-frame,
 * once-per-session cosmetic edge case. */
static void entity_tick(void) {
    towers_update();
    enemies_update();
    projectiles_update();
    if (s_gate_active) {
        s_gate_blink++;
        if (s_gate_blink >= 60) s_gate_blink = 0;
    } else {
        waves_update();
    }
    /* Consume wave-clear edge after each tick so a second entity_tick
     * call cannot overwrite the one-shot flag. */
    {
        u8 cleared = waves_just_cleared_wave();
        if (cleared) score_add_wave_clear(cleared);
    }
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
             * the boot-time NR52-first ordering invariant intact.
             * Iter-3 #16: explicitly restore NR50 to 0x77 (un-duck)
             * AFTER audio_reset (D-MUS-4) — audio_reset deliberately
             * doesn't touch NR50/51/52, so we restore it here. */
            pause_close();
            audio_reset();
            music_duck(0);
            enter_title();
            return;
        }
        return;
    }
    if (menu_is_open()) {
        menu_update();
    } else {
        cursor_update();
        /* Iter-4 #30: Speed toggle — SELECT edge.  Implicit modal gate:
         * this code is unreachable when pause or menu is open. */
        if (input_is_pressed(J_SELECT)) {
            s_fast_mode = !s_fast_mode;
            hud_set_fast_mode(s_fast_mode);
        }
        if (!s_gate_active && input_is_pressed(J_B)) {
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
            if (towers_try_place(cursor_tx(), cursor_ty(), s_selected_type)) {
                if (s_gate_active) {
                    s_gate_active = 0;
                    s_gate_dirty  = 1;
                }
            }
        }
        entity_tick();
        if (s_fast_mode) entity_tick();
    }
    if (!s_gate_active) economy_tick();   /* runs even with menu open — D19 */
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
    gate_render();      /* iter-4 #24: before towers so tower tile wins on overlap */
    menu_render();      /* iter-4 #25: BG save/restore + sprite OAM */
    towers_render();
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
