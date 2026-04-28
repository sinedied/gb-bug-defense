# justfile — GBTD build/run on macOS
# Prereqs: `brew install just mgba`

GBDK_VERSION := "4.2.0"
GBDK_URL     := "https://github.com/gbdk-2020/gbdk-2020/releases/download/" + GBDK_VERSION + "/gbdk-macos.tar.gz"
GBDK_DIR     := justfile_directory() + "/vendor/gbdk"
LCC          := GBDK_DIR + "/bin/lcc"
BUILD        := justfile_directory() + "/build"
OBJ          := BUILD + "/obj"
ROM          := BUILD + "/gbtd.gb"

default: run

# Install/verify GBDK + mGBA on macOS
setup:
    #!/usr/bin/env bash
    set -euo pipefail
    command -v mgba >/dev/null || { echo "mGBA missing — run: brew install mgba"; exit 1; }
    # Apple Silicon needs Rosetta to run the x86_64 GBDK binaries. The
    # `oahd` daemon used to be a reliable signal but isn't always running
    # even when Rosetta is installed; instead, attempt a translated exec
    # and treat success as proof Rosetta works.
    if [[ "$(uname -m)" == "arm64" ]] && ! arch -x86_64 /usr/bin/true 2>/dev/null; then
      echo "Apple Silicon detected without Rosetta. Install with:"
      echo "  softwareupdate --install-rosetta --agree-to-license"
      exit 1
    fi
    if [ ! -x "{{LCC}}" ]; then
      echo "Downloading GBDK-2020 {{GBDK_VERSION}}..."
      mkdir -p "{{GBDK_DIR}}/.."
      TMP="$(mktemp -d)"
      curl -fL "{{GBDK_URL}}" -o "$TMP/gbdk.tar.gz"
      tar -xzf "$TMP/gbdk.tar.gz" -C "{{GBDK_DIR}}/.."
      rm -rf "$TMP"
    fi
    test -x "{{LCC}}"
    echo "Toolchain OK: $({{LCC}} -v 2>&1 | head -1 || true)"

# (Re)generate res/assets.{c,h} from the Python source-of-truth.
assets:
    #!/usr/bin/env bash
    set -euo pipefail
    python3 tools/gen_assets.py res/

# Compile and link the ROM. GBDK defaults are correct for DMG: no MBC,
# 2 ROM banks (32 KB), DMG-only header, automatic header checksum + Nintendo
# logo. (Spec called for `-Wm-yC` / `-Wm-yo1` but those mean "CGB only" /
# "1 bank = 16 KB" in makebin — see memory/decisions.md.)
build: setup
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p "{{OBJ}}"
    "{{LCC}}" -Isrc -Ires \
      -o "{{ROM}}" \
      src/*.c res/assets.c
    SIZE=$(wc -c < "{{ROM}}" | tr -d ' ')
    echo "Built {{ROM}} ($SIZE bytes)"

# Validate ROM
check: build test
    #!/usr/bin/env bash
    set -euo pipefail
    test -f "{{ROM}}"
    SIZE=$(wc -c < "{{ROM}}" | tr -d ' ')
    if [ "$SIZE" -gt 32768 ]; then
      echo "ROM > 32KB: $SIZE"; exit 1
    fi
    CT=$(xxd -s 0x147 -l 1 -p "{{ROM}}")
    if [ "$CT" != "00" ]; then
      echo "Cart type != 0x00 (got $CT)"; exit 1
    fi
    echo "ROM check OK ($SIZE bytes, cart=0x$CT, ≤ 32 KB)"

# Host-side regression tests (no GBDK; uses system cc).
test:
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p "{{BUILD}}"
    cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_math.c -o "{{BUILD}}/test_math"
    "{{BUILD}}/test_math"
    # test_audio compiles src/audio.c (and now src/music.c — audio.c
    # calls into music.* for ch4 arbitration and master reset) against
    # host stubs of <gb/gb.h> and <gb/hardware.h> (tests/stubs/) so we
    # can assert exact NRxx_REG writes without an emulator. Stub dir
    # must come BEFORE -Isrc on the include path so it shadows GBDK's
    # real headers.
    cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc \
       tests/test_audio.c src/audio.c src/music.c -o "{{BUILD}}/test_audio"
    "{{BUILD}}/test_audio"
    # test_music exercises the iter-3 #16 music engine (src/music.c) in
    # isolation: synchronous-arm music_play, row advance, loop wrap,
    # stinger end, idempotency, song switch, ch4 arbitration, duck.
    cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc \
       tests/test_music.c src/music.c -o "{{BUILD}}/test_music"
    "{{BUILD}}/test_music"
    # test_pause compiles src/pause.c against the same host stubs.
    # Collaborator modules (input/menu/cursor/enemies/projectiles) are
    # stubbed inside test_pause.c so each test scripts button presses
    # deterministically without pulling GBDK-dependent sources.
    cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires \
       tests/test_pause.c src/pause.c -o "{{BUILD}}/test_pause"
    "{{BUILD}}/test_pause"
    # test_game_modal exercises the pure modal-precedence helper in
    # src/game_modal.h (header-only, no GBDK linkage). Catches F1
    # (same-frame menu-close + START leaks into pause_open).
    cc -std=c99 -Wall -Wextra -O2 -Isrc \
       tests/test_game_modal.c -o "{{BUILD}}/test_game_modal"
    "{{BUILD}}/test_game_modal"
    # test_anim exercises the iter-3 #21 pure animation helpers in
    # src/{map_anim,enemies_anim,towers_anim}.h (header-only, <stdint.h>
    # only — no GBDK linkage).
    cc -std=c99 -Wall -Wextra -O2 -Isrc \
       tests/test_anim.c -o "{{BUILD}}/test_anim"
    "{{BUILD}}/test_anim"
    # test_difficulty exercises the iter-3 #20 pure helper in
    # src/difficulty_calc.h (header-only, <stdint.h> only — no GBDK
    # linkage). Covers HP table, spawn scaler + 30-frame floor, energy
    # lookup, cycle wrap math, junk-input clamps.
    cc -std=c99 -Wall -Wextra -O2 -Isrc \
       tests/test_difficulty.c -o "{{BUILD}}/test_difficulty"
    "{{BUILD}}/test_difficulty"
    # test_enemies exercises iter-3 #18 stun API + flash>stun>walk priority.
    cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires \
       tests/test_enemies.c src/enemies.c -o "{{BUILD}}/test_enemies"
    "{{BUILD}}/test_enemies"
    # test_towers exercises iter-3 #18 EMP tower: kind discriminator,
    # AoE stun scan, cooldown-on-success, freshly-placed cooldown=0.
    cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires \
       tests/test_towers.c src/towers.c -o "{{BUILD}}/test_towers"
    "{{BUILD}}/test_towers"
    # test_maps (iter-3 #17) exercises map_select.h cycle wrap and
    # validates the three map classmaps + waypoint lists. Pulls
    # res/assets.c for the gameplayN_* symbols (mirrors test_enemies/
    # test_towers). map.h is read for typedef + constants only — the
    # test owns its own registry mirroring src/map.c's table.
    cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires \
       tests/test_maps.c res/assets.c -o "{{BUILD}}/test_maps"
    "{{BUILD}}/test_maps"

# Build + launch emulator (one-command playtest)
#
# Audio: macOS Homebrew only ships the Qt frontend (no SDL `mgba` binary —
# the lowercase `mgba` symlink resolves to the Qt app bundle on a
# case-insensitive APFS volume). Qt mGBA on macOS sometimes initializes
# with the in-app `Audio > Mute` toggle set OR with a low Qt Multimedia
# volume that produces no audible output. We force-override both via
# `-C mute=0 -C volume=0x100` (mGBA config keys, max=0x100) so audio is
# always on regardless of any prior persisted setting. If you still hear
# nothing, check the menu bar: Audio > Mute (must be unchecked) and
# macOS system volume.
run: build
    mgba -3 -C mute=0 -C volume=0x100 "{{ROM}}"

# Launch emulator on existing ROM (no rebuild)
emulator:
    mgba -3 -C mute=0 -C volume=0x100 "{{ROM}}"

clean:
    rm -rf "{{BUILD}}"

clean-all: clean
    rm -rf "{{GBDK_DIR}}"
