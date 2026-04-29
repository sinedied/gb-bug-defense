/* Host-side stub of <gb/hardware.h> for tests/test_audio.c.
 *
 * The real GBDK header maps NRxx_REG to memory-mapped IO addresses on the
 * DMG. For host tests we route every audio register to a slot in a global
 * byte array so the test can assert exactly which writes audio.c made.
 *
 * IMPORTANT: keep this header in sync with the NRxx_REG names referenced
 * by src/audio.c (NR10..NR14, NR21..NR24, NR41..NR44, NR50, NR51, NR52).
 * If audio.c grows new register references, add slots here. */
#ifndef HOST_STUB_GB_HARDWARE_H
#define HOST_STUB_GB_HARDWARE_H

extern unsigned char g_audio_regs[32];

/* Slot layout: arbitrary but stable so tests can index by name too. */
enum {
    REG_NR10 = 0, REG_NR11, REG_NR12, REG_NR13, REG_NR14,
    REG_NR21,     REG_NR22, REG_NR23, REG_NR24,
    REG_NR30,     REG_NR31, REG_NR32, REG_NR33, REG_NR34,
    REG_NR41,     REG_NR42, REG_NR43, REG_NR44,
    REG_NR50,     REG_NR51, REG_NR52,
    REG__COUNT
};

/* Append-only log of audio register WRITES (one entry per `NRxx_REG = v`
 * lvalue assignment). Stores the register index only — final values live
 * in g_audio_regs[]. Tests use this to assert WRITE ORDER and WRITE COUNT,
 * not just final state. Without this, ordering bugs (e.g. NR50 before
 * NR52) and "second play got rejected" bugs are invisible because the
 * stub would record only the final byte. */
#define G_WRITE_LOG_CAP 256
extern unsigned char g_write_log[G_WRITE_LOG_CAP];
extern unsigned int  g_write_log_len;

/* Logging proxy: each NRxx_REG expands to a comma-expression that appends
 * the register index to the log and yields the lvalue slot. Bounds-checked
 * so a runaway test can't UB. The lvalue requirement of audio.c
 * (`NR52_REG = 0x80;`) is satisfied because the macro reduces to *ptr. */
#define _NR_LOG(idx) \
    (*((g_write_log_len < G_WRITE_LOG_CAP ? \
        (g_write_log[g_write_log_len++] = (unsigned char)(idx)) : 0), \
       &g_audio_regs[(idx)]))

#define NR10_REG _NR_LOG(REG_NR10)
#define NR11_REG _NR_LOG(REG_NR11)
#define NR12_REG _NR_LOG(REG_NR12)
#define NR13_REG _NR_LOG(REG_NR13)
#define NR14_REG _NR_LOG(REG_NR14)
#define NR21_REG _NR_LOG(REG_NR21)
#define NR22_REG _NR_LOG(REG_NR22)
#define NR23_REG _NR_LOG(REG_NR23)
#define NR24_REG _NR_LOG(REG_NR24)
#define NR30_REG _NR_LOG(REG_NR30)
#define NR31_REG _NR_LOG(REG_NR31)
#define NR32_REG _NR_LOG(REG_NR32)
#define NR33_REG _NR_LOG(REG_NR33)
#define NR34_REG _NR_LOG(REG_NR34)
#define NR41_REG _NR_LOG(REG_NR41)
#define NR42_REG _NR_LOG(REG_NR42)
#define NR43_REG _NR_LOG(REG_NR43)
#define NR44_REG _NR_LOG(REG_NR44)
#define NR50_REG _NR_LOG(REG_NR50)
#define NR51_REG _NR_LOG(REG_NR51)
#define NR52_REG _NR_LOG(REG_NR52)

/* Wave RAM stub — 16 bytes at 0xFF30..0xFF3F on real hardware. The music
 * engine writes WAVE0 here exactly once via _AUD3WAVERAM. Tests can read
 * back to assert the write sequence (DAC-off via NR30 BEFORE wave bytes). */
extern unsigned char g_wave_ram[16];
#define _AUD3WAVERAM g_wave_ram

/* Iter-3 #19: SRAM stub — 2 KB byte-addressable region at 0xA000 on real
 * hardware. Tests provide `g_sram[2048]`; save.c reads/writes via
 * `_SRAM[i]`. */
extern unsigned char g_sram[2048];
#define _SRAM g_sram

#endif
