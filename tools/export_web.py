#!/usr/bin/env python3
"""
Game Boy Web Exporter

Exports a .gb/.gbc ROM as a self-contained, playable static web page using
the binjgb WebAssembly emulator. Optionally wraps the game in a CSS-drawn
Game Boy / Game Boy Color device border.

Usage:
    export_web.py GAME_ROM [options]

Options:
    --output DIR        Output directory (default: web/)
    --title TEXT        Page title (default: ROM filename without extension)
    --no-border         Omit the device border (fullscreen game only)
    --target dmg|gbc    Force device skin (default: auto from .gb/.gbc extension)
    --color #RRGGBB     Override device body color
    --scale N           Pixel scale 2/3/4 (default: 2)
    --no-rewind         Disable rewind feature
    --no-fast-forward   Disable fast-forward feature
    --dmg-palette N     Default DMG palette index 0-83 (default: 79 = "GB Pocket")

Requirements: Python 3.6+ (stdlib only)
"""

import argparse
import hashlib
import os
import shutil
import sys
import urllib.request
from pathlib import Path

# ─── binjgb source files ────────────────────────────────────────────────────

BINJGB_BASE_URL = "https://raw.githubusercontent.com/binji/binjgb/main/docs/"
BINJGB_FILES = ["binjgb.js", "binjgb.wasm", "simple.css", "simple.js"]
CACHE_DIR = Path.home() / ".cache" / "gameboy-web-export"


def download_binjgb() -> Path:
    """Download binjgb files if not cached. Returns the cache directory."""
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    for filename in BINJGB_FILES:
        dest = CACHE_DIR / filename
        if not dest.exists():
            url = BINJGB_BASE_URL + filename
            print(f"  Downloading {filename}...", end=" ", flush=True)
            try:
                urllib.request.urlretrieve(url, dest)
                print(f"✓ ({dest.stat().st_size // 1024}KB)")
            except Exception as e:
                print(f"✗\n❌ Failed to download {filename}: {e}")
                sys.exit(1)
        else:
            print(f"  {filename}: cached ✓")
    return CACHE_DIR


# ─── simple.js generation ───────────────────────────────────────────────────

def generate_simple_js(
    upstream_path: Path,
    rom_filename: str,
    rewind: bool,
    fast_forward: bool,
    dmg_palette: int,
) -> str:
    """Patch the user-configurable constants at the top of upstream simple.js.

    binjgb's simple.js defines the Emulator class inline and ships its own
    bootstrap logic; we only need to override the handful of constants in
    the "User configurable" block at the top so the page loads our ROM.
    """
    import re

    js = upstream_path.read_text(encoding="utf-8")
    replacements = {
        "ROM_FILENAME": f"'{rom_filename}'",
        "ENABLE_FAST_FORWARD": "true" if fast_forward else "false",
        "ENABLE_REWIND": "true" if rewind else "false",
        "DEFAULT_PALETTE_IDX": str(dmg_palette),
    }
    for name, value in replacements.items():
        # Anchor at start-of-line (skipping leading whitespace) so we don't
        # accidentally match the same constant inside a `//` example comment.
        pattern = rf"(?m)^(\s*const\s+{re.escape(name)}\s*=\s*)[^;]+;"
        new_src, n = re.subn(pattern, lambda m: m.group(1) + value + ";", js, count=1)
        if n != 1:
            print(f"⚠️  Could not patch {name} in upstream simple.js — leaving as-is.")
        else:
            js = new_src
    return js


# ─── game.html (emulator page) ──────────────────────────────────────────────

GAME_HTML = """\
<!DOCTYPE html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <link rel="stylesheet" href="simple.css">
  <title>Game</title>
</head>
<body>
  <div id="game">
    <canvas id="mainCanvas" width="160" height="144">No Canvas Support</canvas>
  </div>
  <div id="controller">
    <div id="controller_dpad">
      <div id="controller_left"></div>
      <div id="controller_right"></div>
      <div id="controller_up"></div>
      <div id="controller_down"></div>
    </div>
    <div id="controller_select" class="capsuleBtn">Select</div>
    <div id="controller_start" class="capsuleBtn">Start</div>
    <div id="controller_b" class="roundBtn">B</div>
    <div id="controller_a" class="roundBtn">A</div>
  </div>
  <script src="binjgb.js"></script>
  <script src="simple.js"></script>
</body>
"""


# ─── CSS device designs ─────────────────────────────────────────────────────

def device_css(dmg_color: str, gbc_color: str) -> str:
    """Return the full responsive device CSS.

    Both DMG and GBC skins are emitted; runtime JS toggles `data-skin` on
    <body> to switch. Layout is fluid (uses CSS custom property `--u` as
    a single sizing unit + `clamp()` for the shell width) so it works on
    phones, tablets and desktop.
    """
    return f"""\
    /* ── Sizing variables (responsive). Everything inside the shell is
       expressed in multiples of --u so the device scales as a unit. ── */
    :root {{
      --shell-w: clamp(280px, 92vw, 460px);
      --u: calc(var(--shell-w) / 460);  /* 1u == 1px at the design width */
      --shell-bg: {dmg_color};
      --screen-housing: #4a4a50;
      --screen-off: #0e160e;
      --brand-color: rgba(255,255,255,0.7);
      --brand-color-text: rgba(255,255,255,0.7);
      --brand-extra-display: none;
    }}
    body[data-skin="gbc"] {{
      --shell-bg: {gbc_color};
      --screen-housing: #2c2640;
      --screen-off: #101018;
      --brand-color: rgba(255,255,255,0.85);
      --brand-color-text: #ff5870;
      --brand-extra-display: inline;
    }}

    .gb-shell {{
      position: relative;
      width: var(--shell-w);
      background: var(--shell-bg);
      /* TL TR BR BL — top mildly rounded, bottom-right large rounded
         curve, bottom-left lightly rounded. No diagonal cuts. */
      border-radius: calc(14 * var(--u)) calc(14 * var(--u))
                     calc(48 * var(--u)) calc(14 * var(--u));
      box-shadow:
        inset 0 calc(2 * var(--u)) 0 rgba(255,255,255,0.18),
        inset 0 calc(-3 * var(--u)) 0 rgba(0,0,0,0.5),
        0 calc(16 * var(--u)) calc(60 * var(--u)) rgba(0,0,0,0.7),
        0 calc(4 * var(--u))  calc(16 * var(--u)) rgba(0,0,0,0.4);
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 0 0 calc(28 * var(--u));
      font-family: Arial, sans-serif;
      box-sizing: border-box;
    }}

    /* ── Top brand row ── */
    .gb-brand-row {{
      width: 100%;
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: calc(18 * var(--u)) calc(28 * var(--u)) calc(8 * var(--u));
      box-sizing: border-box;
    }}
    .gb-power-area {{
      display: flex;
      align-items: center;
      gap: calc(6 * var(--u));
    }}
    .gb-power-led {{
      width: calc(8 * var(--u));
      height: calc(8 * var(--u));
      border-radius: 50%;
      background: #00e040;
      box-shadow: 0 0 calc(6 * var(--u)) calc(2 * var(--u)) #00e04088;
      animation: led-pulse 2.4s ease-in-out infinite;
    }}
    @keyframes led-pulse {{
      0%, 100% {{ opacity: 1; }}
      50% {{ opacity: 0.55; }}
    }}
    .gb-power-label,
    .gb-battery-label {{
      font-size: calc(8 * var(--u));
      color: rgba(0,0,0,0.45);
      letter-spacing: calc(1 * var(--u));
      text-transform: uppercase;
    }}
    .gb-nintendo-logo {{
      font-size: calc(13 * var(--u));
      font-weight: bold;
      letter-spacing: calc(2 * var(--u));
      color: rgba(0,0,0,0.5);
      text-transform: uppercase;
    }}

    /* ── Screen housing (dark plastic around the LCD) ── */
    .gb-screen-housing {{
      width: calc(100% - 32 * var(--u));
      background: var(--screen-housing);
      border-radius: calc(12 * var(--u)) calc(12 * var(--u))
                     calc(40 * var(--u)) calc(12 * var(--u));
      padding: calc(14 * var(--u)) calc(18 * var(--u))
               calc(18 * var(--u)) calc(18 * var(--u));
      box-shadow:
        inset 0 calc(4 * var(--u)) calc(14 * var(--u)) rgba(0,0,0,0.55),
        inset 0 1px 0 rgba(255,255,255,0.08);
      box-sizing: border-box;
    }}
    .gb-screen-top-label {{
      text-align: center;
      font-size: calc(7.5 * var(--u));
      letter-spacing: calc(1.5 * var(--u));
      color: rgba(255,255,255,0.35);
      margin-bottom: calc(10 * var(--u));
      text-transform: uppercase;
    }}
    .gb-screen-bezel {{
      background: var(--screen-off);
      border-radius: calc(4 * var(--u));
      padding: calc(10 * var(--u)) calc(10 * var(--u));
      box-shadow:
        inset 0 0 calc(20 * var(--u)) rgba(0,0,0,0.9),
        inset 0 calc(2 * var(--u)) calc(6 * var(--u)) rgba(0,0,0,0.6);
    }}
    /* Iframe is fluid: takes full bezel width and locks to the GB
       aspect ratio (160 / 144) — the canvas inside upscales via
       image-rendering: pixelated. */
    .gb-screen-bezel iframe {{
      display: block;
      width: 100%;
      aspect-ratio: 160 / 144;
      border: none;
      border-radius: calc(2 * var(--u));
      image-rendering: pixelated;
      background: var(--screen-off);
    }}
    .gb-screen-bottom-brand {{
      text-align: center;
      font-size: calc(15 * var(--u));
      font-weight: bold;
      letter-spacing: calc(3 * var(--u));
      color: var(--brand-color);
      margin-top: calc(10 * var(--u));
      text-shadow: 0 1px calc(2 * var(--u)) rgba(0,0,0,0.55);
    }}
    .gb-brand-extra {{
      display: var(--brand-extra-display);
      color: var(--brand-color-text);
      margin-left: calc(4 * var(--u));
      letter-spacing: calc(1 * var(--u));
    }}

    /* ── Controls row: D-pad ⟷ Select/Start ⟷ A/B ── */
    .gb-controls-row {{
      width: calc(100% - 28 * var(--u));
      display: grid;
      grid-template-columns: 1fr auto 1fr;
      align-items: center;
      gap: calc(12 * var(--u));
      margin-top: calc(22 * var(--u));
      padding: 0 calc(8 * var(--u));
      box-sizing: border-box;
    }}

    /* ── D-pad (left) ── */
    .gb-dpad {{
      position: relative;
      width: calc(96 * var(--u));
      height: calc(96 * var(--u));
      justify-self: start;
    }}
    .gb-dpad-h, .gb-dpad-v {{
      position: absolute;
      background: #2a2a2a;
      border-radius: calc(4 * var(--u));
      box-shadow:
        inset 0 calc(2 * var(--u)) calc(3 * var(--u)) rgba(0,0,0,0.4),
        0 calc(2 * var(--u)) calc(3 * var(--u)) rgba(0,0,0,0.3);
    }}
    .gb-dpad-h {{ left: 0; top: calc(32 * var(--u));
      width: 100%; height: calc(32 * var(--u)); }}
    .gb-dpad-v {{ left: calc(32 * var(--u)); top: 0;
      width: calc(32 * var(--u)); height: 100%; }}
    .gb-dpad-center {{
      position: absolute;
      left: calc(34 * var(--u)); top: calc(34 * var(--u));
      width: calc(28 * var(--u)); height: calc(28 * var(--u));
      background: #4a4a4a;
      border-radius: 50%;
      box-shadow: inset 0 calc(1 * var(--u)) calc(3 * var(--u)) rgba(0,0,0,0.5);
    }}

    /* ── Select / Start (center) ── pill above label, no overlap ── */
    .gb-menu-btns {{
      display: flex;
      flex-direction: row;
      align-items: center;
      gap: calc(14 * var(--u));
      transform: rotate(-25deg);
      transform-origin: center;
    }}
    .gb-menu-btn {{
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: calc(4 * var(--u));
    }}
    .gb-menu-pill {{
      width: calc(36 * var(--u));
      height: calc(11 * var(--u));
      background: #4a4a55;
      border-radius: calc(6 * var(--u));
      box-shadow:
        0 calc(2 * var(--u)) calc(3 * var(--u)) rgba(0,0,0,0.4),
        inset 0 1px 0 rgba(255,255,255,0.12);
    }}
    .gb-menu-label {{
      font-size: calc(7 * var(--u));
      letter-spacing: calc(1 * var(--u));
      color: rgba(0,0,0,0.5);
      text-transform: uppercase;
    }}

    /* ── A / B buttons (right) ── */
    .gb-ab-btns {{
      position: relative;
      width: calc(96 * var(--u));
      height: calc(96 * var(--u));
      justify-self: end;
    }}
    .gb-btn {{
      position: absolute;
      width: calc(38 * var(--u));
      height: calc(38 * var(--u));
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: calc(13 * var(--u));
      font-weight: bold;
      letter-spacing: calc(0.5 * var(--u));
      color: #4a0018;
      background: radial-gradient(ellipse at 35% 30%, #cc1144, #88002a);
      box-shadow:
        0 calc(3 * var(--u)) calc(6 * var(--u)) rgba(0,0,0,0.45),
        inset 0 1px 0 rgba(255,255,255,0.18);
    }}
    .gb-btn-a {{ right: calc(4 * var(--u));  top: calc(8 * var(--u)); }}
    .gb-btn-b {{ left:  calc(4 * var(--u));  bottom: calc(8 * var(--u)); }}

    /* ── Speaker grille (bottom row, right side) ── */
    .gb-bottom-row {{
      width: calc(100% - 28 * var(--u));
      display: flex;
      justify-content: flex-end;
      padding: 0 calc(16 * var(--u));
      margin-top: calc(18 * var(--u));
      box-sizing: border-box;
    }}
    .gb-speaker {{
      display: grid;
      grid-template-columns: repeat(6, calc(6 * var(--u)));
      grid-template-rows: repeat(4, calc(6 * var(--u)));
      gap: calc(4 * var(--u));
      transform: rotate(-25deg);
    }}
    .gb-speaker-hole {{
      width: calc(6 * var(--u));
      height: calc(6 * var(--u));
      border-radius: 50%;
      background: rgba(0,0,0,0.32);
      box-shadow: inset 0 1px calc(2 * var(--u)) rgba(0,0,0,0.5);
    }}
    .gb-speaker-hole:nth-child(1),
    .gb-speaker-hole:nth-child(8),
    .gb-speaker-hole:nth-child(9),
    .gb-speaker-hole:nth-child(16),
    .gb-speaker-hole:nth-child(17),
    .gb-speaker-hole:nth-child(24) {{ opacity: 0; }}

    /* ── DMG vs GBC text swap ── */
    body[data-skin="dmg"] .gb-screen-bottom-brand .gb-brand-extra {{ display: none; }}

    /* Hint line below the device. */
    .gb-hint {{
      margin-top: calc(16 * var(--u));
      color: rgba(255,255,255,0.32);
      font-size: clamp(10px, calc(11 * var(--u)), 13px);
      font-family: monospace;
      text-align: center;
      padding: 0 12px;
    }}
"""


# ─── index.html with border ──────────────────────────────────────────────────

def generate_index_html(
    title: str,
    rom_filename: str,
    is_gbc: bool,
    color: str,
    scale: int,  # kept for back-compat but no longer drives shell size
) -> str:
    css = device_css(
        dmg_color=color if not is_gbc else "#c8c6c0",
        gbc_color=color if is_gbc else "#6a0dad",
    )
    initial_skin = "gbc" if is_gbc else "dmg"

    speaker_holes = "\n".join(
        '          <div class="gb-speaker-hole"></div>' for _ in range(24)
    )

    return f"""\
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover"/>
  <meta name="theme-color" content="#1a1a2e"/>
  <title>{title}</title>
  <style>
    *, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
    html, body {{
      width: 100%;
      min-height: 100%;
      background: #1a1a2e;
      overflow-x: hidden;
    }}
    body {{
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      padding: clamp(8px, 3vw, 28px);
    }}
    .gb-page {{
      display: flex;
      flex-direction: column;
      align-items: center;
      width: 100%;
    }}
{css}
  </style>
</head>
<body data-skin="{initial_skin}">
  <div class="gb-page">
    <div class="gb-shell">

      <div class="gb-brand-row">
        <div class="gb-power-area">
          <div class="gb-power-led"></div>
          <span class="gb-power-label">Power</span>
        </div>
        <span class="gb-nintendo-logo">Nintendo</span>
        <span class="gb-battery-label">Battery</span>
      </div>

      <div class="gb-screen-housing">
        <div class="gb-screen-top-label">· Dot Matrix With Stereo Sound ·</div>
        <div class="gb-screen-bezel">
          <iframe
            id="game-frame"
            src="game.html"
            scrolling="no"
            title="{title}"
            allow="autoplay; gamepad"
          ></iframe>
        </div>
        <div class="gb-screen-bottom-brand">GAME&nbsp;BOY<span class="gb-brand-extra">&nbsp;COLOR</span>™</div>
      </div>

      <div class="gb-controls-row">
        <div class="gb-dpad">
          <div class="gb-dpad-h"></div>
          <div class="gb-dpad-v"></div>
          <div class="gb-dpad-center"></div>
        </div>
        <div class="gb-menu-btns">
          <div class="gb-menu-btn">
            <div class="gb-menu-pill"></div>
            <span class="gb-menu-label">Select</span>
          </div>
          <div class="gb-menu-btn">
            <div class="gb-menu-pill"></div>
            <span class="gb-menu-label">Start</span>
          </div>
        </div>
        <div class="gb-ab-btns">
          <div class="gb-btn gb-btn-a">A</div>
          <div class="gb-btn gb-btn-b">B</div>
        </div>
      </div>

      <div class="gb-bottom-row">
        <div class="gb-speaker">
{speaker_holes}
        </div>
      </div>

    </div><!-- .gb-shell -->

    <p class="gb-hint">
      Arrow keys · Z=A · X=B · Enter=Start · Backspace=Select · [ / ] = palette
    </p>
  </div>

  <script>
    // ── Iframe autofocus (sound + keyboard work without a manual click) ──
    const frame = document.getElementById('game-frame');
    function focusGame() {{
      try {{ frame.contentWindow.focus(); }} catch (_) {{}}
    }}
    frame.addEventListener('load', focusGame);
    // Refocus whenever the page is clicked/tapped outside the iframe.
    document.addEventListener('click', focusGame);
    document.addEventListener('touchend', focusGame);
    // Refocus on tab return.
    window.addEventListener('focus', focusGame);

    // ── Hidden DMG/GBC skin toggle ─────────────────────────────────────
    // Press 's' to cycle skins. URL hash #gbc / #dmg also works.
    // Persists in localStorage.
    const SKINS = ['dmg', 'gbc'];
    function setSkin(s) {{
      if (!SKINS.includes(s)) return;
      document.body.dataset.skin = s;
      try {{ localStorage.setItem('skin', s); }} catch (_) {{}}
    }}
    // Initial skin: URL hash > saved preference > server default.
    const hashSkin = (location.hash || '').replace('#','').toLowerCase();
    const savedSkin = (function() {{
      try {{ return localStorage.getItem('skin'); }} catch (_) {{ return null; }}
    }})();
    if (SKINS.includes(hashSkin)) setSkin(hashSkin);
    else if (SKINS.includes(savedSkin)) setSkin(savedSkin);
    // Listener on parent doc — works whenever the iframe doesn't have focus.
    document.addEventListener('keydown', (e) => {{
      if (e.key === 's' || e.key === 'S') {{
        const cur = document.body.dataset.skin || 'dmg';
        setSkin(cur === 'dmg' ? 'gbc' : 'dmg');
      }}
    }});
  </script>
</body>
</html>
"""


# ─── index.html without border (plain fullscreen) ───────────────────────────

def generate_plain_index_html(title: str, canvas_w: int, canvas_h: int) -> str:
    return f"""\
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>{title}</title>
  <style>
    html, body {{
      margin: 0; padding: 0;
      width: 100%; height: 100%;
      background: #000;
      display: flex;
      align-items: center;
      justify-content: center;
    }}
    iframe {{
      border: none;
      display: block;
    }}
  </style>
</head>
<body>
  <iframe src="game.html" width="{canvas_w}" height="{canvas_h}" scrolling="no" title="{title}"></iframe>
</body>
</html>
"""


# ─── main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Export a GB/GBC ROM as a static web page using binjgb"
    )
    parser.add_argument("rom", help="Path to .gb or .gbc ROM file")
    parser.add_argument("--output", default="web", help="Output directory (default: web/)")
    parser.add_argument("--title", help="Page title (default: ROM filename without extension)")
    parser.add_argument("--no-border", dest="border", action="store_false", default=True,
                        help="Omit the device border")
    parser.add_argument("--target", choices=["dmg", "gbc"], help="Force device skin")
    parser.add_argument("--color", help="Device body color hex (e.g. #c8c6c0)")
    parser.add_argument("--scale", type=int, choices=[2, 3, 4], default=2,
                        help="Pixel scale factor (default: 2 = 320×288)")
    parser.add_argument("--no-rewind", dest="rewind", action="store_false", default=True)
    parser.add_argument("--no-fast-forward", dest="fast_forward", action="store_false", default=True)
    parser.add_argument("--dmg-palette", type=int, default=62, metavar="N",
                        help="Default DMG palette index 0-83 (default: 62 = BGB classic peasoup green)")
    args = parser.parse_args()

    rom_path = Path(args.rom)
    if not rom_path.exists():
        print(f"❌ ROM not found: {rom_path}", file=sys.stderr)
        sys.exit(1)

    # Detect DMG vs GBC
    ext = rom_path.suffix.lower()
    if args.target:
        is_gbc = args.target == "gbc"
    else:
        is_gbc = ext == ".gbc"

    # Default colors
    if args.color:
        color = args.color
    else:
        color = "#800080" if is_gbc else "#c8c6c0"

    # Title
    title = args.title or rom_path.stem.replace("_", " ").replace("-", " ").title()

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"🎮 Exporting: {rom_path.name}")
    print(f"   Target: {'GBC' if is_gbc else 'DMG'} | Scale: {args.scale}× | Border: {args.border}")
    print(f"   Output: {output_dir.resolve()}")
    print()

    # Download binjgb
    print("📦 Checking binjgb files...")
    cache = download_binjgb()
    print()

    # Copy binjgb files (skip simple.js — we write a patched version below)
    for filename in BINJGB_FILES:
        if filename == "simple.js":
            continue
        shutil.copy2(cache / filename, output_dir / filename)

    # Copy ROM
    rom_dest = output_dir / rom_path.name
    shutil.copy2(rom_path, rom_dest)

    # Write simple.js (upstream binjgb code, with our config patched at top)
    simple_js = generate_simple_js(
        upstream_path=cache / "simple.js",
        rom_filename=rom_path.name,
        rewind=args.rewind,
        fast_forward=args.fast_forward,
        dmg_palette=args.dmg_palette,
    )
    (output_dir / "simple.js").write_text(simple_js)

    # Write game.html (emulator page)
    (output_dir / "game.html").write_text(GAME_HTML)

    # Write index.html
    canvas_w = 160 * args.scale
    canvas_h = 144 * args.scale
    if args.border:
        index_html = generate_index_html(
            title=title,
            rom_filename=rom_path.name,
            is_gbc=is_gbc,
            color=color,
            scale=args.scale,
        )
    else:
        index_html = generate_plain_index_html(title, canvas_w, canvas_h)
    (output_dir / "index.html").write_text(index_html)

    # Write Netlify headers for SharedArrayBuffer support
    headers_content = (
        "/*\n"
        "  Cross-Origin-Opener-Policy: same-origin\n"
        "  Cross-Origin-Embedder-Policy: require-corp\n"
    )
    (output_dir / "_headers").write_text(headers_content)

    print(f"✅ Web export complete!")
    print(f"   Files: {len(list(output_dir.iterdir()))} in {output_dir}/")
    print(f"\n▶  Preview:  cd {output_dir} && python3 -m http.server 8080")
    print(f"   Then open: http://localhost:8080")
    print()
    print(f"   Deploy to itch.io: zip -r game_web.zip {output_dir}/")
    print(f"   GitHub Pages: push {output_dir}/ and enable Pages")


if __name__ == "__main__":
    main()
