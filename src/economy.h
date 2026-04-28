#ifndef GBTD_ECONOMY_H
#define GBTD_ECONOMY_H

#include "gtypes.h"

void economy_init(void);
void economy_tick(void);          /* iter-2: passive income trickle */
u8   economy_get_hp(void);
u8   economy_get_energy(void);
void economy_damage(u8 dmg);
bool economy_try_spend(u8 cost);
void economy_award(u8 amount);

#endif
