#ifndef GBTD_CURSOR_H
#define GBTD_CURSOR_H

#include "gtypes.h"

void cursor_init(void);
void cursor_update(void);

u8   cursor_tx(void);  /* play-field-local tile col */
u8   cursor_ty(void);  /* play-field-local tile row */
bool cursor_on_valid_tile(void);

#endif
