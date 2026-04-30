# QA Log: Iter-7 Difficulty Scaling, Boss Enemies & Title Spacing

## Session 1 — 2025-01-XX

### Environment
- macOS (Darwin), Apple Silicon
- GBDK-2020 4.2.0 via justfile
- Host tests compiled with system cc (Apple Clang)
- ROM validated at 65536 bytes

### Dev Workflow Validation
| Command | Result |
|---------|--------|
| `just check` (build + test + ROM validate) | ✅ Pass |
| `just build` | ✅ Pass (65536 byte ROM) |
| `just test` | ✅ All 14 test suites pass |
| `just assets` | ✅ Regenerates res/assets.{c,h} |
| `just clean && just build` | ✅ Clean build succeeds |

### Static Review — Subtask 2.1 (Title Spacing)
- [x] `PROMPT_ROW=14` in `src/title.c` (line 35)
- [x] `HI_ROW=16` in `src/title.c` (line 52)
- [x] `tools/gen_assets.py` line 1481: `rows[14] = text_row("    PRESS START     ")`
- [x] Layout: row 10 (diff), 11 (blank), 12 (map), 13 (blank/gap), 14 (PRESS START), 15 (blank), 16 (HI)

### Static Review — Subtask 2.2 (Per-wave HP Scaling)
- [x] `WAVE_HP_SCALE[10] = { 8, 8, 9, 10, 11, 13, 15, 17, 20, 24 }` in `difficulty_calc.h` line 85
- [x] `difficulty_wave_enemy_hp()` inline function at line 87–94
- [x] `enemies_spawn(type, wave_1based)` signature updated in enemies.h/enemies.c
- [x] waves.c passes `s_cur + 1` as wave_1based to both enemies_spawn and enemies_spawn_boss
- [x] Test call sites in test_enemies.c all pass `1` as wave_1based (identity scale)
- [x] Host tests T10-T16: wave identity, wave 10 all diffs, mid-wave spot checks, OOB wave

### Static Review — Subtask 2.3 (Boss Enemies)
- [x] `enemies_spawn_boss(u8 wave_1based)` defined in enemies.c (line 100–133)
- [x] `is_boss` field in enemy_t struct (line 21)
- [x] OAM_BOSS_BAR = 39 in gtypes.h (line 46)
- [x] SPAWN_BOSS = 0xFF sentinel in waves.c (line 22)
- [x] W5_EV includes `{SPAWN_BOSS, BOSS_SPAWN_DELAY}` as final event (line 47)
- [x] W10_EV includes `{SPAWN_BOSS, BOSS_SPAWN_DELAY}` as final event (line 89)
- [x] s_waves counts: W5=13, W10=32 (correct for appended boss events)
- [x] Boss HP: difficulty_boss_hp() returns BOSS_HP[tier][diff] with W5={20,30,40}, W10={40,60,80}
- [x] Boss speed: BOSS_SPEED = 0x0060 (0.375 px/frame)
- [x] Boss bounty: 30 (W5), 50 (W10) via `s_boss_bounty`
- [x] Boss leak damage: BOSS_LEAK_DAMAGE = 3
- [x] Boss HP bar: 4 fill-level sprites (SPR_BOSS_BAR_1..4, tiles 47-50)
- [x] boss_update_bar() updates bar tile on non-killing damage
- [x] Bar hides on boss death (enemies_apply_damage) and boss leak (step_enemy)
- [x] enemies_init() and enemies_hide_all() clear OAM_BOSS_BAR
- [x] Boss sprite: SPR_BOSS_A=43, SPR_BOSS_B=44, FLASH=45, STUN=46
- [x] Boss walk animation: `(anim >> 4) & 1` toggle
- [x] Boss flash: enemies_set_flash checks is_boss first (line 165)
- [x] Projectiles: captures is_boss before damage, calls score_add_boss_kill on kill
- [x] Score: score_add_boss_kill() applies SCORE_KILL_BOSS=300 × difficulty multiplier
- [x] Host tests T17-T20: boss HP per wave/difficulty, junk diff, tier boundary

### Automated Test Coverage (Iter-7 specific)
- test_difficulty.c: T10 (wave HP identity), T11 (W10 Normal), T12 (W10 Veteran), T13 (W10 Casual), T14 (Casual bug floor), T15 (mid-wave), T16 (OOB wave), T17 (boss HP W5), T18 (boss HP W10), T19 (boss junk diff), T20 (boss tier)
- test_score.c: boss kill constant=300, boss kill Normal=450, boss kill Veteran=600, boss kill Casual=300
- test_enemies.c: all existing tests pass with wave_1based=1 parameter

### Acceptance Scenarios — Automated/Static Verification
| # | Scenario | Result | Notes |
|---|----------|--------|-------|
| 1 | Title spacing gap | ✅ | PROMPT_ROW=14, row 13 is blank gap |
| 2 | Title blink at new row | ✅ | Blink logic uses PROMPT_ROW=14 constant |
| 3 | Title HI at new row | ✅ | HI_ROW=16 constant verified |
| 4 | Wave 1 HP unchanged | ✅ | Host test T10 confirms identity at wave 1 |
| 5 | Late-wave HP scaled | ✅ | Host tests T11-T15 confirm exact values |
| 6 | Veteran late waves punishing | ✅ | T12: W10 Vet Robot=21 (3× base) |
| 7 | Casual wave 10 beatable | ✅ | T13: W10 Casual Bug=3 (same as old Normal W1) |
| 8 | Boss spawns at wave 5 | ✅ | W5_EV has SPAWN_BOSS sentinel, count=13 |
| 9 | Boss spawns at wave 10 | ✅ | W10_EV has SPAWN_BOSS sentinel, count=32 |
| 10 | Boss HP bar visible | ✅ | OAM_BOSS_BAR=39, SPR_BOSS_BAR_4 set on spawn |
| 11 | Boss HP bar depletes | ✅ | boss_update_bar() with 4 thresholds |
| 12 | Boss distinct sprite | ✅ | SPR_BOSS_A/B tiles 43/44, walk animation |
| 13 | Boss flash on hit | ✅ | SPR_BOSS_FLASH tile 45, enemies_set_flash handles is_boss |
| 14 | Boss stun | ✅ | SPR_BOSS_STUN tile 46, stun_timer stops movement |
| 15 | Boss kill reward | ✅ | Bounty (30/50) + score (300×mult) verified |
| 16 | Boss leak damage | ✅ | BOSS_LEAK_DAMAGE=3, bar hides |
| 17 | Boss HP bar hides on death | ✅ | move_sprite(OAM_BOSS_BAR,0,0) on kill |
| 18 | Boss HP bar hides during modal | ✅ | enemies_hide_all() clears OAM_BOSS_BAR |
| 19 | Wave 10 win requires boss dead | ✅ | waves_all_cleared() only after all events processed |
| 20 | Boss HP per difficulty | ✅ | T17-T20 test all combinations |
| 21 | No second boss overlap | ✅ | Guard in enemies_spawn_boss prevents >1 boss |

### Visual Testing
- mGBA requires GUI display; not available in this environment
- ROM structure and header validated (GB ROM image, MBC1+RAM+BATT, correct size)
- All logic paths verified through static analysis and host tests

### Issues Found
None.
