#ifndef GBTD_RANGE_PREVIEW_H
#define GBTD_RANGE_PREVIEW_H

#include "gtypes.h"

/* Iter-4 #31: Tower range preview — shows 8 dot sprites in a circle
 * at the tower's attack radius after a dwell period on the cursor. */

void range_preview_init(void);           /* hides OAM 1..8, resets state */
void range_preview_update(u8 tx, u8 ty); /* per-frame dwell + show/hide */
void range_preview_hide(void);           /* immediate hide + state reset */

#endif /* GBTD_RANGE_PREVIEW_H */
