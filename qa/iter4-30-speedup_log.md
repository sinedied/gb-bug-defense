# QA Log: Feature #30 — Speed-Up Toggle

## Session 1

**Date**: 2025-01-24
**Tester**: QA (automated)

### Dev Workflow Commands

| Command | Result | Notes |
|---------|--------|-------|
| `just build` | ✅ PASS | Produces `build/bugdefender.gb` (65536 bytes). Compiler warnings in menu.c and towers.c are pre-existing optimizer warnings, not related to this feature. |
| `just test` | ✅ PASS | All 13 host-side tests pass: test_math, test_audio, test_music, test_pause, test_game_modal, test_anim, test_difficulty, test_enemies, test_towers, test_maps, test_save, test_score, test_gate. |
| `just check` | ✅ PASS | ROM validation OK (65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr=0x33). |

### Code Verification

| Check | Result | Details |
|-------|--------|---------|
| `s_fast_mode` declaration | ✅ | `static bool s_fast_mode;` in game.c, reset to `false` in `enter_playing()` before `hud_init()` |
| `hud_set_fast_mode` API | ✅ | Declared in hud.h: `void hud_set_fast_mode(bool on);` |
| `entity_tick` extraction | ✅ | `static void entity_tick(void)` defined; called once normally, called again conditionally under `if (s_fast_mode)` |
| SELECT toggle wiring | ✅ | `s_fast_mode = !s_fast_mode; hud_set_fast_mode(s_fast_mode);` present in game.c |

### Acceptance Scenarios (manual-only, not executed)

Scenarios 1–9 from the spec require emulator interaction (visual speed verification, HUD indicator check, pause/menu modal gating, audio/economy single-tick validation). These cannot be automated in the current CI environment. Subtask 7 in the spec scopes these to manual QA.

Scenario 10 (wave-10 stress on real DMG hardware) is a pre-ship gate.

### Edge Cases Not Tested (require emulator)

- SELECT during pause/menu (should be no-op)
- SELECT + A on same frame (gate-lift edge case)
- New game reset after fast mode was on
- Tower-type cycle with fast mode off (col 18/19 display)
- Gate blink acceleration when fast mode on before first tower
