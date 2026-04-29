#ifndef GBTD_CURSOR_H
#define GBTD_CURSOR_H

#include "gtypes.h"

void cursor_init(void);
void cursor_update(void);
void cursor_blink_pause(bool paused);   /* iter-2: hold steady highlight */

u8   cursor_tx(void);  /* play-field-local tile col */
u8   cursor_ty(void);  /* play-field-local tile row */
bool cursor_on_valid_tile(void);
void cursor_invalidate_cache(void);  /* iter-5: force recompute after place/sell */

#endif
