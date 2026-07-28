# Scientific Symbol Panel

A cross-platform utility for quickly inserting scientific and mathematical symbols — the scientific equivalent of the Windows Emoji Panel (`Win + .`).

Works on **Windows**, **macOS**, and **Linux** with the same keystroke.

## Features

- **400+ symbols** across 18 categories (Math, Greek, Physics, Chemistry, Electronics, SI Units, Logic, Programming, Arrows, Fractions, Superscripts, Subscripts, Statistics, Geometry, Calculus, Astronomy, and more)
- **Instant search** — type to filter, results in <1ms
- **Hotkey toggle** — `Alt + A` by default, configurable in `src/config.h`
- **Recent symbols** — last 100 inserted, LRU order
- **Favorites** — pin symbols for quick access
- **Snippets** — insert full equations (Ohm's Law, Quadratic Formula, Euler's Identity, etc.)
- **Scientific notation converter** — type `6.02e23` → `6.02 × 10²³`
- **Superscript/Subscript builder** — `x^2` → `x²`, `H2O` → `H₂O`
- **LaTeX mode** — type `\pi` → `π`, `\sum` → `∑`
- **Command palette** — type commands like `/theme dark`, `/clear recent`
- **Hot-reload symbols** — add symbols without rebuilding; just run `python tools/add_symbol.py` and toggle the panel
- **Borderless, lightweight** — <10 MB RAM, 0% CPU idle
- **Zero telemetry, zero internet**
- **Cross-platform** — Windows, macOS, Linux

## Requirements

- CMake 3.28+
- C++23 compiler (MSVC 2022, GCC 14+, Clang 18+)
- [vcpkg](https://github.com/microsoft/vcpkg) for dependencies
- OpenGL 3.3+

## Quick Start

```powershell
# Install vcpkg (one-time)
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat  # or .sh on Linux/macOS
set VCPKG_ROOT=C:\path\to\vcpkg  # or export on Linux/macOS

# Build
cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

On Linux/macOS:
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

## Configuration

Edit `src/config.h` to change settings — rebuild to apply:

```c
#define SSP_HOTKEY_MOD      GLFW_MOD_ALT    // Modifier: ALT, CTRL, SUPER, SHIFT
#define SSP_HOTKEY_KEY      GLFW_KEY_A      // Key: A-Z, F1-F12, etc.
#define SSP_WINDOW_WIDTH    360
#define SSP_WINDOW_HEIGHT   480
#define SSP_MAX_RECENT      100
#define SSP_ANIMATIONS      1               // 1 = on, 0 = off
#define SSP_FUZZY_SEARCH    1
```

## Adding Symbols

Add new symbols with a single command — no manual JSON editing or binary fiddling, and **no rebuild required** (the panel hot-reloads on next toggle):

```powershell
python tools/add_symbol.py "⌀" "Diameter" "Geometry" -a "dia,circle" -l "\\diameter"
```

**What it does:**
- Appends the symbol to `data/symbols.json`
- Auto-computes the Unicode codepoint from the character
- Regenerates `data/symbols.bin`
- Warns on duplicates (same codepoint or name)
- The running panel detects the change and reloads on next toggle — no rebuild needed

**Full options:**

| Flag | Description |
|------|-------------|
| `-a, --aliases` | Comma-separated search aliases |
| `-k, --keywords` | Comma-separated keywords for search ranking |
| `-l, --latex` | LaTeX command (e.g. `\\diameter`) |
| `--html` | HTML entity (e.g. `&amp;diam;`) |
| `-d, --description` | Human-readable description |

After editing `data/symbols.json` by hand, regenerate the binary:

```powershell
python tools/add_symbol.py --rebuild
```

## Usage

| Key | Action |
|-----|--------|
| `Alt + A` | Toggle panel (configurable in `config.h`) |
| Type | Search symbols |
| `↑` `↓` `←` `→` | Navigate grid |
| `Enter` | Insert selected symbol |
| `Escape` | Close panel |
| `Ctrl + F` | Focus search bar |
| `Tab` / `Shift + Tab` | Cycle UI zones |
| `Ctrl + 1-9` | Jump to category |
| `Page Up/Down` | Scroll results |
| `/` | Command palette mode |

### LaTeX Mode

Type `\` for LaTeX aliases: `\alpha` → `α`, `\beta` → `β`, `\sum` → `∑`, `\int` → `∫`, `\infty` → `∞`

## Launch Speed

Measured on an AMD Ryzen 7 7700X (Windows 11, Release build):

| Metric | Time |
|--------|------|
| App construction → ready | **~120 ms** |
| Panel toggle (show) | **<16 ms** (one frame) |
| Search response | **<1 ms** |

The panel uses a cold-start timer in `main.cpp` that prints startup time to stdout. The app is designed to stay resident after first launch — subsequent toggles are instant.

## Architecture

```
ScientificSymbolPanel/
├── CMakeLists.txt
├── vcpkg.json                 # Dependency manifest
├── src/
│   ├── config.h               # User-editable settings (hotkey, window, etc.)
│   ├── main.cpp               # Entry point, benchmark timer
│   ├── App/                   # Application lifecycle, GLFW callbacks
│   ├── UI/                    # NanoVG rendering, themes, layout, animation
│   ├── Core/                  # Shared types, config, logging
│   ├── Platform/              # Cross-platform OS layer (GLFW, paths, hotkey, dark mode)
│   ├── Storage/               # JSON persistence (settings, recent, favorites)
│   ├── Search/                # Trie + hash map search engine
│   └── Symbols/               # Database, converters, snippets
├── data/                      # Symbol database (JSON + binary), snippets
├── tools/                     # add_symbol.py, rebuild helpers
├── assets/                    # Icons, fonts (DejaVu Sans bundled)
└── packs/                     # Plugin symbol packs
```

## Technology

- C++23
- **GLFW** — cross-platform windowing and input
- **NanoVG** — GPU-accelerated 2D vector rendering (OpenGL 3.3)
- **stb_truetype** — font rasterization
- Zero runtime dependencies beyond GLFW and OpenGL
- Bundled DejaVu Sans font (757 KB, full Unicode coverage)

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE)
