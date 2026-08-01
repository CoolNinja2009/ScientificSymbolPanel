# Scientific Symbol Panel

A utility for quickly inserting scientific and mathematical symbols — the scientific equivalent of the Windows Emoji Panel (`Win + .`). Two rendering backends in one codebase: native Windows Direct2D (zero deps) and cross-platform GLFW + ImGui.

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
- **Inline text expansion** — type `delta`` ` in any textbox → `δ` (system-wide, no panel needed)
- **Runtime configurable** — trigger, case preference, custom keyword mappings via `settings.json`
- **Borderless, lightweight** — <10 MB RAM, 0% CPU idle
- **Zero telemetry, zero internet**
- **High DPI support** (125%, 150%, 200%)

## Requirements

### Windows Direct2D Backend (zero external deps)
- Windows 10+
- CMake 3.28+
- MSVC 2022 or Build Tools (C++23)

### Cross-Platform GLFW Backend
- Windows 10+, Linux (X11/Wayland), or macOS 12+
- CMake 3.28+
- C++23 compiler (MSVC 2022, GCC 13+, Clang 17+)
- OpenGL 3.3+

**Dependency options:**

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

### Windows (build_release.bat)
```batch
build_release.bat
```
Interactive menu:
- `[1]` Windows Direct2D — native Win32, statically linked, **zero external dependencies**
- `[2]` Cross-platform — GLFW + ImGui (vcpkg or FetchContent)

### Manual CMake
```bash
# Windows Direct2D backend
cmake -B build -DSSP_BACKEND=Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Cross-platform GLFW backend (vcpkg)
cmake -B build -DSSP_BACKEND=GLFW -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Cross-platform GLFW backend (FetchContent — no vcpkg)
cmake -B build -DSSP_BACKEND=GLFW -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release
```

### Platform Notes

| Platform | Notes |
|----------|-------|
| **Windows** | Visual Studio 2022 or MSVC Build Tools. Direct2D backend needs nothing else. |
| **Linux** | GLFW backend only. Install OpenGL dev packages: `sudo apt install libgl1-mesa-dev xorg-dev` (Debian/Ubuntu) or equivalent. |
| **macOS** | GLFW backend only. OpenGL is deprecated but still functional. Framework linking is automatic. |

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

### Inline Text Expansion

Type a symbol name followed by the trigger and a space in **any textbox** — the word and trigger are replaced with the best-matching Unicode symbol. No panel needed.

| Example | Result |
|---------|--------|
| `delta`` ` | δ |
| `alpha`` ` | α |
| `integral`` ` | ∫ |
| `ohm`` ` | Ω |

**Configurable** via `%LOCALAPPDATA%\ScientificSymbolPanel\settings.json`:

```json
{
  "expanderEnabled": true,
  "expanderTrigger": ";;",
  "expanderPreferLowercase": true,
  "expanderMappings": {
    "delta": 80,
    "ohm": 332
  }
}
```

| Setting | Default | Description |
|---------|---------|-------------|
| `expanderEnabled` | `true` | Master toggle |
| `expanderTrigger` | `"``"` | Trigger sequence (e.g. `";;"`, `"!!"`) |
| `expanderPreferLowercase` | `true` | Prefer lowercase Greek among equal-score matches |
| `expanderMappings` | `{}` | Keyword → symbol index overrides (see `data/symbol-index.txt` for indices) |

Restart the app after editing the config — no recompile needed.

### LaTeX Mode

Type `\` for LaTeX aliases: `\alpha` → `α`, `\beta` → `β`, `\sum` → `∑`, `\int` → `∫`, `\infty` → `∞`

## Architecture

```
ScientificSymbolPanel/
├── CMakeLists.txt             # Unified CMake (SSP_BACKEND=Win32|GLFW)
├── build_release.bat          # Windows one-click build with backend menu
├── vcpkg.json                 # vcpkg manifest (GLFW backend)
├── assets/                    # Icons, Windows resource file
├── src/
│   ├── main.cpp               # GLFW + ImGui entry point (cross-platform)
│   ├── main_glfw.cpp          # (legacy, excluded from build)
│   ├── main_win32.cpp         # Win32 Direct2D entry point
│   ├── App/                   # Win32 backend: application, input handler
│   ├── UI/                    # Panel (GLFW) + Renderer, Layout, Themes (Win32)
│   ├── Core/                  # Shared types, config, inline expander, logging
│   ├── Platform/              # Platform abstraction + Win32 helpers
│   ├── Storage/               # JSON persistence (settings, recent, favorites)
│   ├── Search/                # Trie + hash map search engine
│   └── Symbols/               # Database, converters, snippets
├── data/                      # Symbol database, snippets, symbol index
└── deps/                      # Extra vendored dependencies (optional)
```

## Technology

| Backend | Rendering | Windowing | Dependencies |
|---------|-----------|-----------|--------------|
| **Win32** | Direct2D + DirectWrite | Win32 API | None (Windows SDK only) |
| **GLFW** | OpenGL 3.3 + Dear ImGui | GLFW | GLFW, ImGui, OpenGL |

Shared core: C++23, custom JSON parser, binary symbol database, trie-based search engine.

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE)
