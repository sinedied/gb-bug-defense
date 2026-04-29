# Rename to Bug Defender

## Problem
The game is currently named "GBTD" (an acronym for Game Boy Tower Defense) in all user-visible locations. User feedback suggests a real name: "Bug Defender" — thematic (the player defends against bugs), memorable, and exactly 11 characters when uppercased, fitting the Game Boy ROM header title field perfectly.

## Architecture

### ROM Header Title (bytes 0x134–0x13E)
GBDK-2020's `makebin` tool accepts the `-Wm-yn<NAME>` flag (no space between `-yn` and the name) to set the internal ROM title. The title field is 11 bytes of uppercase ASCII, padded with 0x00 if shorter. Currently **no** title flag is set (header bytes are null/uninitialized). Adding `-Wm-ynBUGDEFENDER` to the `lcc` invocation in the `build` recipe passes it through to makebin, which writes the title bytes and recomputes the header checksum (byte 0x14D) automatically.

### Header Checksum Interaction
The `just check` recipe validates the header checksum by computing it from bytes 0x134–0x14C of the produced ROM and comparing to the stored value at 0x14D. Since makebin always produces a self-consistent ROM (it computes and stores the correct checksum after writing the title), **no change to the check logic is needed**. The check will pass with any valid title.

### Scope of Changes

| Category | Files | What changes |
|----------|-------|--------------|
| Build system | `justfile` | ROM output path, lcc flags (add `-Wm-yn`), comment |
| Generated assets | `tools/gen_assets.py` → `res/assets.{c,h}` | Title screen tilemap text rows, docstring |
| Living documentation | `README.md`, `specs/roadmap.md`, `res/README.md`, `tests/manual.md` | Game name, ROM filename references |
| Persistent memory | `memory/decisions.md` | Append save-compat + namespace decisions |

### What does NOT change
- **`GBTD_` C macro/header guard prefixes** — internal developer-facing identifiers; mass rename is disproportionate refactor risk for zero user value.
- **SRAM magic bytes** `'G','B','T','D'` in `src/save_calc.h` — binary format marker. Changing would invalidate existing saves on flash carts.
- **Historical QA/spec files** (`qa/*.md`, `specs/mvp.md`, `specs/iter2.md`, `specs/iter3-*.md`, `specs/mvp-design.md`, etc.) — these document what was true at testing time. Updating them is revisionist.
- **`AGENTS.md`** — contains only shared-memory guidelines, no game name references.
- **Git repository directory name** — out of scope.
- **`res/assets.h` header guard** `GBTD_ASSETS_H` — falls under the "GBTD_ prefix retained" rule.

## Subtasks

1. ✅ **Update justfile** — (a) Change line 1 comment from `GBTD` to `Bug Defender`. (b) Change line 10 `ROM := BUILD + "/gbtd.gb"` to `ROM := BUILD + "/bugdefender.gb"`. (c) In the `build` recipe lcc invocation (line 58–59), add `-Wm-ynBUGDEFENDER` to the flags. Done when `just build` produces `build/bugdefender.gb` and `just check` passes.

   Exact changes:
   ```
   Line 1:  # justfile — Bug Defender build/run on macOS
   Line 10: ROM          := BUILD + "/bugdefender.gb"
   Line 59: -Wl-yt0x03 -Wm-yo4 -Wm-ya1 -Wm-yp0x149=0x01 -Wm-ynBUGDEFENDER \
   ```

2. ✅ **Update title screen tilemap in gen_assets.py** — (a) Change docstring (line 3) from `"GBTD asset generator."` to `"Bug Defender asset generator."`. (b) In `title_map()`, change row 2 from `"        GBTD        "` to `"    BUG DEFENDER    "` (4+12+4=20). (c) Replace row 4 `"   TOWER  DEFENSE   "` with a blank row (`blank_row()` call or omit the assignment — rows default to blank). (d) Change row 16 from `"  (C) 2025 GBTD MVP "` to `"    (C) 2025 GBTD   "` (keep GBTD as developer initialism / easter egg, drop "MVP"). Done when `python3 tools/gen_assets.py res/` succeeds without error.

3. ✅ **Regenerate res/assets.{c,h}** — Run `just assets`. Verify the `title_tilemap` array in `res/assets.c` reflects the new text (row 2 = "BUG DEFENDER" glyphs, row 4 = blanks). Done when `diff` shows only the expected tilemap byte changes (no other arrays affected).

4. ✅ **Update README.md** — (a) Change heading (line 1) from `# GBTD — Game Boy Tower Defense` to `# Bug Defender`. (b) Change the description paragraph (lines 3–4): replace "A monochrome Game Boy (DMG) tower-defense game where the player defends a desktop computer..." keeping the same meaning but with "Bug Defender is a monochrome..." (c) Change the table row that says `Compile sources → \`build/gbtd.gb\`` to `Compile sources → \`build/bugdefender.gb\``. Done when `grep -ic 'gbtd' README.md` returns 0 (case-insensitive, no matches).

5. ✅ **Update specs/roadmap.md** — (a) Change line 1 header from `# Roadmap: GBTD — Game Boy Tower Defense` to `# Roadmap: Bug Defender`. (b) Change line 16: `mgba -3 build/gbtd.gb` → `mgba -3 build/bugdefender.gb`. (c) Change line 25 table row: `build/gbtd.gb` → `build/bugdefender.gb`. (d) Change line 45 feature description: `build/gbtd.gb` → `build/bugdefender.gb`. Done when `grep -c 'gbtd\.gb' specs/roadmap.md` returns 0 and the header uses the new name.

6. ✅ **Update res/README.md** — Change line 3 from `"Generated asset C arrays for GBTD."` to `"Generated asset C arrays for Bug Defender."`. Done when the file references the new name.

7. ✅ **Update tests/manual.md** — Change line 8 `build/gbtd.gb` to `build/bugdefender.gb`. Done when `grep -c gbtd tests/manual.md` returns 0.

8. ✅ **Append clarification to memory/decisions.md** — The existing entry at line 538 (`### Iter-4: Game rename to Bug Defender`) mentions "AGENTS.md" as needing updates, but AGENTS.md contains no game name (only shared-memory guidelines). Per the append-only convention, do NOT edit the existing entry. Instead, append a new entry:
   ```
   ### Iter-4 #29 rename clarifications
   - **Date**: <today>
   - **Context**: Implementing the Bug Defender rename (iter-4 #29).
   - **Decision**: (1) AGENTS.md is unchanged — it contains no game-name text.
     (2) SRAM magic bytes 'G','B','T','D' (save_calc.h) are preserved for
     flash-cart save compatibility. (3) `GBTD_` C header-guard prefix is
     retained (internal, never user-visible). (4) Historical QA/spec docs
     are not updated — they are audit records of past state.
   - **Rationale**: Save-compat is non-negotiable for real hardware users.
     Namespace prefix rename would touch 27+ files for zero user value.
   - **Alternatives**: None — these are constraint clarifications, not design choices.
   ```
   Done when the new entry exists at the end of `memory/decisions.md`.

9. ✅ **Verify build end-to-end** — Run `just clean && just check`. Confirm: (a) `build/bugdefender.gb` produced at 65536 bytes, (b) header checksum passes, (c) cart type 0x03 / ROM size 0x01 / RAM size 0x01 correct, (d) `xxd -s 0x134 -l 11 build/bugdefender.gb` shows ASCII "BUGDEFENDER". Done when all checks pass and the ROM boots in mGBA showing the new title.

10. ⬜ **Single atomic commit** — Stage all changed files, commit with message `feat: rename game to Bug Defender (#29)`. Done when `git log --oneline -1` shows the commit and `git diff HEAD~1 --stat` lists only the expected files.

## Acceptance Scenarios

### Setup
```sh
just clean    # remove stale build/gbtd.gb
just check    # full build + validate + host tests
```

### Scenarios
| # | Scenario | Steps | Expected Result |
|---|----------|-------|-----------------|
| 1 | ROM produced with new filename | `just build` | `build/bugdefender.gb` exists, 65536 bytes |
| 2 | Old filename not produced | `ls build/gbtd.gb 2>&1` | "No such file or directory" |
| 3 | ROM header title correct | `xxd -s 0x134 -l 11 -p build/bugdefender.gb` | Hex output `4255474445 46454e4445 52` = ASCII "BUGDEFENDER" |
| 4 | Header checksum valid | `just check` | "ROM check OK" printed, exit 0 |
| 5 | Title screen shows new name | `just run`, observe title screen | Row 2 reads "BUG DEFENDER", row 4 is blank (no "TOWER DEFENSE"), bottom row shows "(C) 2025 GBTD" |
| 6 | Host tests still pass | `just test` | All test binaries exit 0 (tests don't reference the ROM filename) |
| 7 | No stale gbtd references in living docs | `grep -ric 'gbtd' README.md res/README.md tests/manual.md` + `grep -c 'gbtd\.gb' justfile specs/roadmap.md` | README/res/tests: 0 matches. justfile/roadmap: 0 matches for `.gb` filename. |
| 8 | Save compatibility | `just run`, let enemies reach computer until game over, note high score. Quit emulator. `just run` again — title HI line shows preserved score. | SRAM magic bytes unchanged; save round-trips correctly. |

## Constraints
- ROM header title: max 11 uppercase ASCII characters. "BUGDEFENDER" = exactly 11. No truncation needed.
- `-Wm-yn` flag format: `-Wm-ynBUGDEFENDER` (no space, no quotes around name).
- Title screen rows are 20 characters wide. `"    BUG DEFENDER    "` = 4 + 12 + 4 = 20. ✓
- Save format magic bytes (`GBTD` at SRAM 0xA000–0xA003) must not change — real flash cart users would lose their high scores.
- Feature #28 (title screen art) depends on this rename completing first. The title tilemap layout here is intentionally minimal — #28 will redesign it with art tiles.

## Decisions
| Decision | Options Considered | Choice | Rationale |
|----------|-------------------|--------|-----------|
| Row 4 subtitle | Keep "TOWER DEFENSE" / Remove / Replace with tagline | Remove (blank row) | "Bug Defender" is self-contained; subtitle is redundant. Feature #28 will redesign the title layout with art anyway. |
| Row 16 copyright | "(C) 2025 BUG DEF." / "(C) 2025 GBTD" / Remove entirely | `"    (C) 2025 GBTD   "` | Keep GBTD as dev initialism easter egg. Drop "MVP" (no longer accurate). 20-char pad preserved. |
| Historical QA/spec files | Update all / Update none / Update only "living" docs | Update only living docs (README, roadmap, tests/manual, res/README) | QA reports and completed iteration specs are historical records. Updating them erases the audit trail. |
| SRAM magic bytes | Change to "BUGD" / Keep "GBTD" | Keep "GBTD" | Save compatibility with existing flash cart users. Magic is binary-internal, never displayed to users. |
| Roadmap body references | Update header only / Update all `gbtd.gb` refs | Update all `gbtd.gb` occurrences | Roadmap is a living reference doc (unlike completed QA reports). Stale filename references would confuse developers. |

## Review
**Adversarial review findings (resolved):**

1. **memory/decisions.md not updated** — Added subtask 8 explicitly instructing the coder to append a new clarification entry (append-only convention). The existing entry at line 538 mentions "AGENTS.md" which is incorrect; the new entry corrects this and documents SRAM-magic + namespace rationales.

2. **specs/roadmap.md subtask too narrow** — Expanded subtask 5 to list all three `gbtd.gb` body references (lines 16, 25, 45) with a grep-based done-condition ensuring none remain. Consistent with how README is handled.

**Cross-model validation findings (resolved):**

3. **Subtask 8 edit-vs-append conflict** — Changed from "edit existing entry" to "append new clarification entry" per the project's append-only memory convention. Also corrected the stale AGENTS.md claim.

4. **Case-sensitive grep in acceptance checks** — Changed subtask 4 done-condition to case-insensitive grep (`grep -ic 'gbtd'`). Updated acceptance scenario 7 to use separate checks: case-insensitive for README/res/tests (which should have zero GBTD mentions), filename-only for justfile/roadmap (where `GBTD_` guards in code cross-references are acceptable but `.gb` filename refs are not).

5. **Save-compatibility scenario not reliably triggering SRAM write** — Updated scenario 8 steps: explicitly reach game-over (which triggers save write per conventions), verify HI persists across sessions. Backward compatibility with pre-rename saves is guaranteed by keeping the same magic bytes — no separate scenario needed since the binary format is byte-identical.
