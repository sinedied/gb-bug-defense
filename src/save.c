#include "save.h"
#include "save_calc.h"
#include <gb/gb.h>
#include <gb/hardware.h>

/* Single-owner high-score cache. Lives ONLY here; no other module
 * caches the table. See memory/conventions.md "Single-owner save cache
 * (iter-3 #19)". */
static u16 s_hi[SAVE_HI_COUNT];

/* Read the 9-slot high-score table from SRAM into the cache. The caller
 * is expected to have already validated magic+version (via save_init). */
void save_load_highscores(void) {
    u8 i;
    u8 lo, hi;
    ENABLE_RAM;
    SWITCH_RAM(0);
    for (i = 0; i < SAVE_HI_COUNT; i++) {
        lo = _SRAM[SAVE_OFF_HI + (u16)(i * 2u)];
        hi = _SRAM[SAVE_OFF_HI + (u16)(i * 2u) + 1u];
        s_hi[i] = (u16)((u16)lo | ((u16)hi << 8));
    }
    DISABLE_RAM;
}

void save_init(void) {
    u8 hdr[6];
    u8 i;
    ENABLE_RAM;
    SWITCH_RAM(0);
    for (i = 0; i < 6u; i++) hdr[i] = _SRAM[i];
    DISABLE_RAM;

    if (save_magic_ok(hdr)) {
        save_load_highscores();
        return;
    }

    /* Fresh cart or magic mismatch: zero cache, stamp fresh header +
     * zeroed slot table. Single ENABLE/DISABLE pair keeps the SRAM
     * write window short.
     *
     * Failure-atomic ordering invariant (iter-3 #19 F1): the score
     * slots are zeroed FIRST, then the version byte, then the magic
     * bytes LAST. If power is lost mid-reset, the next boot sees an
     * incomplete magic stamp → save_magic_ok() returns false → reset
     * runs again cleanly. Reordering this block re-introduces the
     * "valid header, garbage scores" hazard. */
    for (i = 0; i < SAVE_HI_COUNT; i++) s_hi[i] = 0;
    ENABLE_RAM;
    SWITCH_RAM(0);
    for (i = 0; i < (u8)(SAVE_HI_COUNT * 2u); i++) {
        _SRAM[SAVE_OFF_HI + i] = 0;
    }
    _SRAM[SAVE_OFF_PAD]       = 0;
    _SRAM[SAVE_OFF_VERSION]   = SAVE_VERSION;
    _SRAM[SAVE_OFF_MAGIC + 0] = SAVE_MAGIC0;
    _SRAM[SAVE_OFF_MAGIC + 1] = SAVE_MAGIC1;
    _SRAM[SAVE_OFF_MAGIC + 2] = SAVE_MAGIC2;
    _SRAM[SAVE_OFF_MAGIC + 3] = SAVE_MAGIC3;
    DISABLE_RAM;
}

u16 save_get_hi(u8 map_id, u8 diff) {
    return s_hi[save_slot_index(map_id, diff)];
}

void save_write_hi(u8 map_id, u8 diff, u16 v) {
    u8 slot = save_slot_index(map_id, diff);
    u8 off  = save_slot_offset(map_id, diff);
    s_hi[slot] = v;
    ENABLE_RAM;
    SWITCH_RAM(0);
    _SRAM[off + 0] = (u8)(v & 0xFFu);
    _SRAM[off + 1] = (u8)((v >> 8) & 0xFFu);
    DISABLE_RAM;
}
