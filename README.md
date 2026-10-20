# Dino

Dino is a portable, native C++20 endless runner built with Qt 6. Jump over cacti, duck beneath birds, and keep going as the run gets faster. Its pixel-art stage and gameplay are adapted from [omarchy-dino](https://github.com/Saikomantisu/omarchy-dino). Its game engine is independent of Qt, and its palette can be customized or followed from a file.

## Build and test

Install a C++20 compiler, CMake, Ninja, and Qt 6.7 or newer with the Core, Gui, Widgets, and Test modules. From the project directory:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable target is `dino`. Run it from the build directory with `./build/dino`.

## Install

After building, preview the per-user installation and then apply it:

```bash
scripts/install-desktop.sh --dry-run --build-dir "$PWD/build"
scripts/install-desktop.sh --build-dir "$PWD/build"
```

The installer places `dino` under `~/.local/bin`, adds a desktop entry and icon, and registers one `io.github.dino_runner.desktop|ICON|Dino` row in `~/.config/apps.list`. Existing app order is kept, and duplicate Dino rows are reduced to one. Existing files are backed up before replacement. A new configuration follows the active palette when it is available and otherwise starts with the built-in palette. Existing theme configuration is preserved.

Use `--prefix DIR`, `--config-dir DIR`, `--apps-list FILE`, and `--build-dir DIR` to choose install paths. `--standalone` installs the executable and desktop entry without editing `apps.list` or following the active palette; its initial theme uses the built-in palette. `--dry-run` prints the selected paths without writing files. The same installer can be staged safely in a temporary directory with these options.

## Controls

- Space, Up, W, or K: start, jump, or retry. Hold to jump higher; holding a keyboard jump key keeps hopping.
- Down, S, or J: duck; press while airborne to drop quickly.
- Enter: start or retry.
- R: retry after a crash. A half-second delay prevents accidental restarts.
- Escape or Q: close.
- Left mouse button: start or jump; release to shorten the jump.

The stage fills the window, with a bottom-aligned ground line and pixel art scaled proportionally to the available width. It uses inline start/game-over prompts and a five-digit score with a saved high score. Runs begin with three clear seconds, then add small and large cactus groups as speed increases. Birds appear at three heights once the pace picks up. Every hundred points flashes the score; every seven hundred adds a twelve-second star field. All scene colors continue to use the selected theme, including during the night effect.

## Themes

Theme data is stored in `~/.config/dino/theme.json` by default. Override that location for any command with `--config-dir DIR`:

```bash
dino --config-dir DIR theme show
dino --config-dir DIR theme set accent='#ff8c42' background='#17191d'
dino --config-dir DIR theme import /path/to/palette.qml
dino --config-dir DIR theme follow /path/to/palette.qml
dino --config-dir DIR theme follow active
dino --config-dir DIR theme follow static NAME
dino --config-dir DIR theme sync
dino --config-dir DIR theme reset
```

`follow FILE` accepts QML color properties or JSON palettes. `follow active` reads the active palette file; `follow static NAME` reads a named palette from the user configuration's `themes` directory. `sync` reloads the followed source, while `set`, `import`, and `reset` use a saved palette without following a source. The running app reloads valid changes to its saved theme or followed source. If a change is invalid, it keeps displaying the last valid palette and reports the error; a later valid update restores live updates.

The seven palette keys are `background`, `surface`, `text`, `muted`, `accent`, `border`, and `accent_text`. Hex colors may use `#RRGGBB` or `#AARRGGBB`; in the eight-digit form, alpha comes first. For example, `#e00e1e36` has alpha `e0` and RGB `0e1e36`. QML palettes may also use `Qt.rgba(r, g, b, a)`, with each channel expressed as a fraction from 0 to 1; the same color can be written as `Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)`. Add `--dry-run` to a theme command to validate and print its resulting palette without saving it:

```bash
dino --config-dir DIR theme follow active --dry-run
```

## Attribution

The pixel-art sprites and gameplay design are adapted in C++ from [Saikomantisu's omarchy-dino](https://github.com/Saikomantisu/omarchy-dino), under its [MIT license](licenses/omarchy-dino-MIT.txt). This version uses native Qt Widgets and a fixed-step C++ engine; no QML or JavaScript runtime is required. Theme commands, palette values, active/static theme following, and live reload behavior remain unchanged.
