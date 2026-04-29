/* Host-side stub of <gb/gb.h>.
 *
 * audio.c only needs the integer typedefs that gtypes.h pulls; the GBDK
 * BGB/cart/joypad APIs are not referenced from audio.c. pause.c does
 * reference J_* button masks and move_sprite/set_sprite_tile, so the
 * stub provides those symbols too. The test binary supplies the actual
 * function bodies. */
#ifndef HOST_STUB_GB_GB_H
#define HOST_STUB_GB_GB_H

/* DMG joypad masks (mirror real <gb/gb.h>). */
#define J_RIGHT  0x01U
#define J_LEFT   0x02U
#define J_UP     0x04U
#define J_DOWN   0x08U
#define J_A      0x10U
#define J_B      0x20U
#define J_SELECT 0x40U
#define J_START  0x80U

/* Forward decls — test binary stubs them with capture logic. */
void move_sprite(unsigned char nb, unsigned char x, unsigned char y);
void set_sprite_tile(unsigned char nb, unsigned char tile);
void set_bkg_tile_xy(unsigned char x, unsigned char y, unsigned char tile);

/* Iter-3 #19: SRAM access protocol macros. The test binary supplies
 * the backing storage (`g_rRAMG`, `g_rRAMB`). save.c uses these via
 * ENABLE_RAM/DISABLE_RAM/SWITCH_RAM, identical token form to the real
 * <gb/gb.h>. */
extern unsigned char g_rRAMG;
extern unsigned char g_rRAMB;
#define ENABLE_RAM   (g_rRAMG = 0x0A)
#define DISABLE_RAM  (g_rRAMG = 0x00)
#define SWITCH_RAM(b) (g_rRAMB = (unsigned char)(b))

#endif
