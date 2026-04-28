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

# Build + launch emulator (one-command playtest)
run: build
    mgba -3 "{{ROM}}"

# Launch emulator on existing ROM (no rebuild)
emulator:
    mgba -3 "{{ROM}}"

clean:
    rm -rf "{{BUILD}}"

clean-all: clean
    rm -rf "{{GBDK_DIR}}"
