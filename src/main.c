#include "gtypes.h"
#include "gfx.h"
#include "input.h"
#include "audio.h"
#include "game.h"
#include <gb/gb.h>

void main(void) {
    DISPLAY_OFF;
    gfx_init();
    gfx_hide_all_sprites();
    audio_init();
    input_init();
    game_init();

    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    /* Loop ordering matters for VRAM correctness on real DMG: BG writes
     * must land during VBlank (mode 1). game_render() makes only small
     * BG writes (worst case ~14 tiles in playing_render) which fit in the
     * ~1080-cycle VBlank window. game_update() runs game logic and
     * sprite shadow-OAM updates which are safe outside VBlank. Full-screen
     * BG redraws (title/gameover/map_load) bracket DISPLAY_OFF themselves
     * or via enter_playing(). */
    while (1) {
        wait_vbl_done();
        game_render();
        input_poll();
        game_update();
    }
}
