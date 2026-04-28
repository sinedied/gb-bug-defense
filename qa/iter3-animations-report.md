# QA Report: Iter-3 # Animated tiles & sprite polish21 

## Verdict: **SHIP** 

## Layer  Dev workflow1 
| Check | Result |
|---|---|
|  |  ROM = 32768 B (= 32 KB cap exactly) |
|  (5 host bins) |  test_math, test_audio, test_pause, test_game_modal, test_anim all pass |
|  |  ROM check OK (32768 B, cart= 32 KB) |0x00, 
|  |  mGBA boots cleanly, no crash; killed after 4 s |
| ROM headers Nintendo logo @0x104 (CE ED 66 ), cart type @0x147 = 0x00, DMG mode @0x143 = 0x00 |66  | 

## Layer  Code presence2 
| Check | Result |
|---|---|
| , ,  exist |  |
| All 3 use  only (no ) |  verified by grep |
|  API in  +  LUT |  map.c:22, map.c:61 |
|  calls  on HP changes |  economy.c:13, :32 |
|  has ;  writes tile immediately (F1) |  enemies.c:17, :110 (set_sprite_tile at line 108) |96
|  has , , 3-phase -after-write |  towers.c:26, :51, :173, :184, :210 |
|  has  ticked at top of , reset in  |  game.c:24, :70 (reset), :189 (tick before switch) |
|  calls  only on non-killing hit |  projectiles.c:91 (in `else` branch) |81
|  has  decoder + 7 corruption + 2 tower B + 2 sprite flash |  design_tile @362; COMP_TL_C1, TL_C2, TR_C2, TL_C3, TR_C3, BL_C3, BR_C3 (=7); TOWER_BG_B + TOWER2_BG_B (=2); SPR_BUG_FLASH + SPR_ROBOT_FLASH (=2) |
|  exists, F3 wrap test = 19216 exhaustive period-64 |  test_anim.c:127; existing 64-frame half-period check retained at :132 |128122

## Layer  Regression3 
| Check | Result |
|---|---|
| All previously-passing host tests still pass all 5 binaries OK | | 
| Boot smoke test in mGBA no crash within 4 s | | 
| Boot chime / audio_init untouched audio.c not modified by this iter (audio_play wired into projectiles) | | 
| Pause modal-gates idle scanner towers.c:194 `if (game_is_modal_open()) return;` | | 

## Layer  Manual scenarios4 
-  Iter-3 #21 section contains **17 scenarios** (16 initial + #17 F1 regression check), all explicitly MANUAL-REQUIRED (mGBA-driven, visual/audio observation).  Matches expected count.

## Build warnings (informational, non-blocking)
- SDCC peephole "EVELYN the modified DOG" warnings in `menu.c:140`, `menu.c:151`, `towers.c: pre-existing optimizer notices, not introduced by this iter.152` 

## Defects
None.

## Final Verdict: **SHIP**
