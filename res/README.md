# res/

Generated asset C arrays for Bug Defender.

`assets.c` and `assets.h` are produced by `tools/gen_assets.py` and committed
to the repo so the standard build does not depend on Python at run time.
Regenerate with `just assets` after editing the generator.

Contents:

| Symbol             | Description                                 |
|--------------------|---------------------------------------------|
| `font_tiles`       | 128 BG tiles (ASCII direct-indexed).        |
| `map_tile_data`    | 10 BG tiles: ground, path, computer ×4 ×2.  |
| `sprite_tile_data` | 8 sprite tiles: cursor ×4, tower, bug ×2, projectile. |
| `title_tilemap`    | Full 20×18 BG map for the title screen.     |
| `win_tilemap`      | Full 20×18 BG map for the win screen.       |
| `lose_tilemap`     | Full 20×18 BG map for the lose screen.      |
| `gameplayN_tilemap`  | 20×17 play-field tile indices for map N (N∈{1,2,3}).   |
| `gameplayN_classmap` | 20×17 tile classification (TC_GROUND / TC_PATH / TC_COMPUTER) for map N. |
| `gameplayN_waypoints`| Waypoint list for map N (Iter-3 #17).                  |

DMG palette (BGP `0xE4`): 0=white, 1=light grey, 2=dark grey, 3=black.
