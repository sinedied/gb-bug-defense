# Difficulty Rebalance (Iter-4 Feature #23)

## Problem
The current difficulty tiers (Easy/Normal/Hard) need rebalancing and renaming to better communicate their intent. "Easy" becomes "Casual" (relaxed tutorial-like experience), Normal stays unchanged, and "Hard" becomes "Veteran" (punishing but fair from wave 1). HP tables, spawn scalers, and starting energy must be adjusted. Wave composition and tower stats are unchanged.

## Architecture
Pure data changes only — no new files, no new APIs, no structural changes to game logic. The enum integer values 0/1/2 remain unchanged (used as array indices into `DIFF_ENEMY_HP`, save slots, and score tables). Only symbolic names, comments, UI strings, and numeric constants change.

### Key numeric changes
| Parameter | Casual (was Easy) | Normal | Veteran (was Hard) |
|-----------|-------------------|--------|-------------------|
| Bug HP | 1 (was 2) | 3 (unchanged) | 4 (was 5) |
| Robot HP | 3 (was 4) | 6 (unchanged) | 7 (was 9) |
| Armored HP | 6 (was 8) | 12 (unchanged) | 14 (was 16) |
| Spawn num | 14 (was 12) | 8 (unchanged) | 6 (unchanged) |
| Start energy | 45 (unchanged) | 30 (unchanged) | 28 (was 24) |
| Score mult | ×1.0 (unchanged) | ×1.5 (unchanged) | ×2.0 (unchanged) |

### Selector width change
"VETERAN" is 7 characters. The current `draw_selector` renders exactly 6 label characters within a 10-tile layout. To accommodate "VETERAN" without abbreviation, the selector widens by 1 tile:

**Old layout (10 tiles):** `<` `·` `L₀L₁L₂L₃L₄L₅` `·` `>` (6-char label, `·` = space)
**New layout (11 tiles):** `<` `·` `L₀L₁L₂L₃L₄L₅L₆` `·` `>` (7-char label)

Exact on-screen rendering (all tiles shown):
```
CASUAL:  < C A S U A L · · >   (label "CASUAL ", trailing space is label char 6)
NORMAL:  < N O R M A L · · >   (label "NORMAL ", trailing space is label char 6)
VETERAN: < V E T E R A N · >   (label "VETERAN", no padding needed)
MAP 1:   < · M A P · 1 · · >   (label " MAP 1 ", centered)
MAP 2:   < · M A P · 2 · · >   (label " MAP 2 ", centered)
MAP 3:   < · M A P · 3 · · >   (label " MAP 3 ", centered)
```

The `·` between `<`/`>` and the label is the selector's own hardcoded space. The trailing `·` before `>` is also hardcoded. Label padding spaces are embedded in the label strings.

VBlank impact: 11 writes/frame for selector render (was 10), still under the 12-write worst-case (prompt blink = `PROMPT_LEN` = 12). Screen fit: col 5 + 11 = col 15, well within 20-col screen.

### Files affected

| File | Change type |
|------|-------------|
| `src/difficulty_calc.h` | Enum rename, HP values, spawn num, energy value, comments |
| `src/title.c` | Label strings, selector width (DIFF_W, MAP_W, draw_selector), comments |
| `tests/test_difficulty.c` | Symbol renames + value assertion updates |
| `tests/test_score.c` | Symbol renames (6 code references) |
| `src/game.h` | Comment update only |
| `src/score_calc.h` | Comment update only |
| `tests/test_save.c` | Comment update only |
| `tests/manual.md` | Terminology updates throughout |
| `memory/conventions.md` | Enum declaration + terminology in iter-3 #20 section |

## Subtasks

1. ✅ **Rename enum and macros in `src/difficulty_calc.h`** — Change:
   - `DIFF_EASY` → `DIFF_CASUAL` (value stays 0)
   - `DIFF_HARD` → `DIFF_VETERAN` (value stays 2)
   - `DIFF_SPAWN_NUM_EASY` → `DIFF_SPAWN_NUM_CASUAL`
   - `DIFF_SPAWN_NUM_HARD` → `DIFF_SPAWN_NUM_VETERAN`
   - `DIFF_START_ENERGY_EASY` → `DIFF_START_ENERGY_CASUAL`
   - `DIFF_START_ENERGY_HARD` → `DIFF_START_ENERGY_VETERAN`
   - All comments: "EASY" → "CASUAL", "HARD" → "VETERAN"
   - Top-of-file comment: update "EASY / NORMAL / HARD" → "CASUAL / NORMAL / VETERAN"
   
   The inline functions use numeric comparisons (`diff == 0`, `diff == 2`) not symbols, so they need only comment updates. Done when header compiles with `cc -std=c99 -Isrc -fsyntax-only`.

2. ✅ **Update numeric values in `src/difficulty_calc.h`** — Change:
   - `DIFF_ENEMY_HP[0]` (CASUAL): `{ 2, 4, 8 }` → `{ 1, 3, 6 }`
   - `DIFF_ENEMY_HP[2]` (VETERAN): `{ 5, 9, 16 }` → `{ 4, 7, 14 }`
   - `DIFF_SPAWN_NUM_CASUAL`: `12u` → `14u`, comment "x1.5" → "x1.75"
   - `DIFF_START_ENERGY_VETERAN`: `24u` → `28u`
   
   Row 1 (NORMAL) stays `{ BUG_HP, ROBOT_HP, ARMORED_HP }` (symbolic). Done when values match the table in this spec.
   - Depends on: subtask 1

3. ✅ **Widen selector and update labels in `src/title.c`** — Changes:
   - `#define DIFF_W 10` → `#define DIFF_W 11`
   - `#define MAP_W 10` → `#define MAP_W 11`
   - In `draw_selector`: loop bound `i < 6` → `i < 7`, trailing-space write `col + 8` → `col + 9`, `>` write `col + 9` → `col + 10`
   - `s_diff_labels[3]`: `{ " EASY ", "NORMAL", " HARD " }` → `{ "CASUAL ", "NORMAL ", "VETERAN" }`
   - `s_map_labels[3]`: `{ "MAP 1 ", "MAP 2 ", "MAP 3 " }` → `{ " MAP 1 ", " MAP 2 ", " MAP 3 " }`
   - Update the file-header comments: change "10 tiles" references for selectors to "11 tiles", update "6 chars" to "7 chars" in the `draw_selector` comment, and update "EASY"/"HARD" mentions to "CASUAL"/"VETERAN"
   
   Done when `just build` succeeds and visual inspection in mGBA shows correct selector rendering.
   - Depends on: none (independent of header changes)

4. ✅ **Update `tests/test_difficulty.c`** — Changes:
   - All `DIFF_EASY` → `DIFF_CASUAL`, `DIFF_HARD` → `DIFF_VETERAN` (symbol renames)
   - T2 function name: `test_t2_easy_hard_values` → `test_t2_casual_veteran_values`
   - T2 assertions for CASUAL: `2, 4, 8` → `1, 3, 6`
   - T2 assertions for VETERAN: `5, 9, 16` → `4, 7, 14`
   - T6 comment/name: references to "HARD" → "VETERAN"
   - T6 assertion `difficulty_scale_interval(50, DIFF_VETERAN)`: result is still `50*6/8 = 37` (num unchanged) ✓
   - T7 assertions: CASUAL stays 45, NORMAL stays 30, VETERAN changes from 24 → 28
   - T8: rename references in wrap/non-wrap comments
   
   Done when `just test` passes for test_difficulty.
   - Depends on: subtasks 1–2

5. ✅ **Update `tests/test_score.c`** — Rename 6 code references:
   - Line 80: `DIFF_EASY` → `DIFF_CASUAL`
   - Line 82: `DIFF_HARD` → `DIFF_VETERAN`
   - Line 106: `DIFF_EASY` → `DIFF_CASUAL`
   - Line 111: `DIFF_HARD` → `DIFF_VETERAN`
   - Line 129: `DIFF_HARD` → `DIFF_VETERAN`
   - Line 136: `DIFF_HARD` → `DIFF_VETERAN`
   - Update any "EASY"/"HARD" comments to "CASUAL"/"VETERAN"
   
   Done when `just test` passes for test_score.
   - Depends on: subtask 1

6. ✅ **Update comments in `src/game.h` and `src/score_calc.h`** — 
   - `src/game.h` line 23: "EASY / NORMAL / HARD" → "CASUAL / NORMAL / VETERAN"
   - `src/score_calc.h` line 35: "EASY=8, NORMAL=12, HARD=16" → "CASUAL=8, NORMAL=12, VETERAN=16"
   
   Done when no stale "EASY"/"HARD" references remain in these files.
   - Depends on: none

7. ✅ **Update `tests/test_save.c` comments** — Replace "EASY"→"CASUAL" and "HARD"→"VETERAN" in comments (lines 127-128 and any others). No code changes needed (file uses numeric indices, not symbols). Done when comments use new names.
   - Depends on: none

8. ✅ **Update `tests/manual.md`** — Global find-replace: "EASY" → "CASUAL", "HARD" → "VETERAN" throughout the file (section headers, test case descriptions, expected behaviors). Preserve unrelated uses of "hard" (e.g., "hard-coded") if any exist — review each hit. Done when terminology is consistent with new names.
   - Depends on: none

9. ✅ **Update `memory/conventions.md`** — In the "Iter-3 #20 conventions" section (lines ~89–120) and "Iter-3 #20 clarification" section (lines ~121–133):
   - Change `DIFF_EASY=0` → `DIFF_CASUAL=0`
   - Change `DIFF_HARD=2` → `DIFF_VETERAN=2`
   - Replace all "EASY"→"CASUAL" and "HARD"→"VETERAN" in those sections
   
   Done when conventions reflect new naming.
   - Depends on: none

10. ✅ **Verify all tests pass and ROM builds** — Run `just test` (all host test binaries) and `just build`. Verify no compiler warnings. Done when both commands exit 0.
    - Depends on: subtasks 1–9

## Acceptance Scenarios

### Setup
```bash
just test    # All host tests must pass
just build   # ROM builds without warnings
just run     # Launch in mGBA to visually verify title screen
```

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | Host tests pass | Run `just test` | All test binaries (test_difficulty, test_score, test_save, etc.) report OK; exit 0 |
| 2 | ROM builds clean | Run `just build` | No compiler warnings or errors; produces `build/gbtd.gb` |
| 3 | Title default label | Launch ROM in mGBA | Difficulty selector shows `< NORMAL  >` (with trailing space from 7-char label) |
| 4 | Title Casual label | Press LEFT from NORMAL | Selector changes to `< CASUAL  >` |
| 5 | Title Veteran label | Press RIGHT from NORMAL | Selector changes to `< VETERAN >` |
| 6 | Cycle wrap left | From CASUAL, press LEFT | Wraps to `< VETERAN >` |
| 7 | Cycle wrap right | From VETERAN, press RIGHT | Wraps to `< CASUAL  >` |
| 8 | Map selector width | Press DOWN to focus map selector, cycle through maps | All three map labels display correctly within the widened selector, no tile overflow or corruption |
| 9 | Casual HP=1 bug | Select CASUAL, MAP 1, start game. Place 1 AV tower on path. Wait for first bug to enter range | Bug is destroyed in exactly 1 AV shot (HP=1, AV damage=1) |
| 10 | Veteran starting energy | Select VETERAN, start game | HUD shows `E:028` on first frame |
| 11 | Veteran HP=4 bug | Select VETERAN, MAP 1, start game. Place 1 AV tower on path | First bug requires 4 AV hits to destroy (HP=4, AV damage=1) |

## Constraints
- Enum integer values `DIFF_CASUAL=0, DIFF_NORMAL=1, DIFF_VETERAN=2` MUST stay at 0/1/2 (array indices into `DIFF_ENEMY_HP`, SRAM save slots, score tables)
- `difficulty_calc.h` remains a pure helper header (`<stdint.h>` + `tuning.h` only — no GBDK includes)
- VBlank write budget: widened selector = 11 writes/frame, still under 16-write cap and under 12-write worst case (prompt)
- The NORMAL row of `DIFF_ENEMY_HP` must remain symbolic (`BUG_HP, ROBOT_HP, ARMORED_HP`) to catch `tuning.h` drift at compile time
- Score multiplier numeric values (CASUAL=8/8=×1.0, NORMAL=12/8=×1.5, VETERAN=16/8=×2.0) are UNCHANGED
- `DIFF_SPAWN_FLOOR` (30u) is UNCHANGED
- No changes to wave composition, tower stats, or any gameplay logic outside the difficulty-scaling system

## Decisions
| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| Label width | A) Abbreviate "VETERAN" to 6 chars (e.g., "VETER.", " VET "); B) Widen selector to 7-char labels (11 tiles) | B) Widen to 7 | Abbreviation is ugly/confusing. 11 writes still under VBlank budget (12 worst case = prompt). Fits in 20-col screen (ends at col 15). Only changes draw_selector + two #defines + label arrays. |
| Enum rename vs alias | A) Rename DIFF_EASY→DIFF_CASUAL throughout; B) Add `#define DIFF_CASUAL DIFF_EASY` aliases | A) Full rename | Old names should not persist in the codebase. Grep confirms no src/*.c files use the symbols directly (only header and tests). Clean break prevents confusion. |
| Spawn scaler for Veteran | A) Change num from 6 to something else; B) Keep at 6 | B) Keep num=6 | Roadmap explicitly says "identical numerator, change is HP-only". 6/8 = ×0.75 spawn speed unchanged. |
| Spawn scaler for Casual | Keep at 12 vs change to 14 | 14 (per roadmap) | Roadmap says num=14 (×1.75). Makes spawns even more spread out for a relaxed experience. Max interval: 90*14/8=157, fits uint8_t. |

## Review
1. **test_score.c has 6 code references to DIFF_EASY/DIFF_HARD** — Reviewer caught these would cause compile failure. Added dedicated subtask 5 with exact line numbers.
2. **Label width undecided** — Reviewer flagged the spec debated options but never concluded. Resolved: widen to 7 chars with exact code changes specified in subtask 3 and precise on-screen rendering documented in Architecture.
3. **memory/conventions.md omitted** — Reviewer noted this contains the authoritative enum declaration. Added subtask 9.
4. **Selector padding contract inconsistency** (cross-model review) — Clarified exact on-screen rendering for every label in Architecture section, showing which spaces come from the selector frame vs the label string.
5. **Manual gameplay scenarios imprecise** (cross-model review) — Tightened scenarios 9/11 to specify placing 1 AV tower and counting hits, making pass/fail objective.
6. **title.c comments stale** (cross-model review) — Added explicit instruction in subtask 3 to update file-header comments about tile counts and char widths.
