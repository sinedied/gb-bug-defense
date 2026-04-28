#ifndef GBTD_TOWERS_H
#define GBTD_TOWERS_H

#include "gtypes.h"

void towers_init(void);
void towers_update(void);
void towers_render(void);                /* VBlank-safe BG tile writes */

bool towers_try_place(u8 tx, u8 ty);     /* play-field-local tile coords */
bool towers_at(u8 tx, u8 ty);            /* occupancy check */

#endif
