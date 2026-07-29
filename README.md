# Scientific Symbol Panel

A cross-platform utility for quickly inserting scientific and mathematical symbols — the scientific equivalent of the Windows Emoji Panel (`Win + .`). Now runs on Windows, Linux, and macOS.

## Features

- **400+ symbols** across 18 categories (Math, Greek, Physics, Chemistry, Electronics, SI Units, Logic, Programming, Arrows, Fractions, Superscripts, Subscripts, Statistics, Geometry, Calculus, Astronomy, and more)
- **Instant search** — type to filter, results in <1ms
- **Hotkey toggle** — `Alt + A` by default, configurable
- **Recent symbols** — last 100 inserted, LRU order
- **Favorites** — pin symbols for quick access
- **Snippets** — insert full equations (Ohm's Law, Quadratic Formula, Euler's Identity, etc.)
- **Scientific notation converter** — type `6.02e23` → `6.02 × 10²³`
- **Superscript/Subscript builder** — `x^2` → `x²`, `H2O` → `H₂O`
- **LaTeX mode** — type `\pi` → `π`, `\sum` → `∑`
- **Command palette** — type commands like `/theme dark`, `/clear recent`
- **Borderless, lightweight** — <10 MB RAM, 0% CPU idle
- **Zero telemetry, zero internet**
- **High DPI support** (125%, 150%, 200%)

## Requirements

- **Windows 10+**, **Linux** (X11/Wayland), or **macOS 12+**
- CMake 3.28+
- C++23 compiler (MSVC 2022, GCC 13+, Clang 17+)
- OpenGL 3.3+

### Dependency Options

**Option A — vcpkg (recommended):**
```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh  # or bootstrap-vcpkg.bat on Windows
./vcpkg install glfw3 imgui
```

Then build with `-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake`.

**Option B — FetchContent (automatic):**
Dependencies (GLFW, ImGui) are fetched automatically at configure time. No pre-installation needed. OpenGL must still be available on your system.

## Build

```bash
# Configure (vcpkg)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Configure (FetchContent — no vcpkg)
cmake -B build

# Build
cmake --build build --config Release
```

### Platform Notes

| Platform | Notes |
|----------|-------|
| **Windows** | Visual Studio 2022 or MSVC Build Tools. vcpkg triplet: `x64-windows`. |
| **Linux** | Install OpenGL dev packages: `sudo apt install libgl1-mesa-dev xorg-dev` (Debian/Ubuntu) or equivalent. |
| **macOS** | OpenGL is deprecated but still functional. Framework linking is automatic. |

## Usage

| Key | Action |
|-----|--------|
| `Alt + A` | Toggle panel |
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

## Architecture

```
ScientificSymbolPanel/
├── CMakeLists.txt             # CMake build (vcpkg or FetchContent)
├── vcpkg.json                 # vcpkg manifest
├── src/
│   ├── main.cpp               # GLFW entry point, ImGui init
│   ├── UI/                    # ImGui rendering, themes, layout, panel
│   ├── Core/                  # Shared types, config, logging
│   ├── Platform/              # Platform abstraction (clipboard, hotkey, DPI)
│   ├── Storage/               # JSON persistence (settings, recent, favorites)
│   ├── Search/                # Trie + hash map search engine
│   └── Symbols/               # Database, converters, snippets
├── data/                      # Symbol database, snippets
└── deps/                      # Extra vendored dependencies (optional)
```

## Technology

- C++23
- **GLFW** — cross-platform windowing and input
- **OpenGL 3.3** — rendering backend
- **Dear ImGui** — immediate-mode UI toolkit
- Zero runtime dependencies beyond system OpenGL

### Windows-Specific Version

A native Windows build using Win32 + Direct2D + DirectWrite is maintained on the [`windows`](https://github.com/CoolNinja2009/ScientificSymbolPanel/tree/windows) branch. It has zero external dependencies beyond the Windows SDK.

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE)
