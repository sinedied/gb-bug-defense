#ifndef GBTD_GFX_H
#define GBTD_GFX_H

#include "gtypes.h"

void gfx_init(void);              /* palette + load common tiles into VRAM */
void gfx_hide_all_sprites(void);  /* hide every OAM slot */
void gfx_clear_bg(void);          /* fill BG with blank tile */

#endif
