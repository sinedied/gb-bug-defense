#ifndef GBTD_MENU_H
#define GBTD_MENU_H

#include "gtypes.h"

void menu_init(void);
void menu_open(u8 tower_idx);
void menu_close(void);
bool menu_is_open(void);

void menu_update(void);   /* input + state; called when open */
void menu_render(void);   /* sprite OAM placement; idempotent */

#endif
