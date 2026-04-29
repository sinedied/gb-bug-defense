# QA Log: Difficulty Rebalance (Iter-4 #23)

## Session 1

### Date
2025-01-28

### Commands Tested
| Command | Result | Notes |
|---------|--------|-------|
| `just build` | ✅ PASS | Produces `build/bugdefender.gb` (65536 bytes). SDCC optimizer warnings in menu.c/towers.c are pre-existing and benign. |
| `just test` | ✅ PASS | All 12 test binaries pass: test_math, test_audio, test_music, test_pause, test_game_modal, test_anim, test_difficulty, test_score, test_enemies, test_towers, test_maps, test_save |
| `just check` | ✅ PASS | ROM validation: 65536 bytes, cart=0x03, rom=0x01, ram=0x01, hdr=0x33 |

### Value Verification in `src/difficulty_calc.h`
| Check | Expected | Actual | Result |
|-------|----------|--------|--------|
| Enum DIFF_CASUAL | 0 | 0 | ✅ |
| Enum DIFF_NORMAL | 1 | 1 | ✅ |
| Enum DIFF_VETERAN | 2 | 2 | ✅ |
| Casual HP | {1, 3, 6} | {1, 3, 6} | ✅ |
| Normal HP | {3, 6, 12} | {BUG_HP=3, ROBOT_HP=6, ARMORED_HP=12} | ✅ |
| Veteran HP | {4, 7, 14} | {4, 7, 14} | ✅ |
| Casual spawn num | 14 | 14u | ✅ |
| Veteran energy | 28 | 28u | ✅ |

### Spec Note
- Acceptance scenario 2 in `specs/iter4-23-difficulty.md` references `build/gbtd.gb` but the ROM is actually `build/bugdefender.gb` (renamed in iter4-29-rename). The justfile is correct; the spec text is stale.

### Visual / Gameplay Scenarios
- Scenarios 3–11 (title screen labels, gameplay HP/energy verification) require mGBA — not tested in this session (headless environment).
