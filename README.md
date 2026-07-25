# Scientific Symbol Panel

A Windows utility for quickly inserting scientific and mathematical symbols — the scientific equivalent of the Windows Emoji Panel (`Win + .`).

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

- Windows 10 20H1+ or Windows 11
- Visual Studio 2022 with C++ Desktop Development workload
- CMake 3.28+

## Build

Open the folder in Visual Studio 2022 (File → Open → Folder), select `x64-Release`, and build.

Or from command line:
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

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
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # WinMain entry point
│   ├── App/                   # Application lifecycle, window management
│   ├── UI/                    # Direct2D/DirectWrite rendering, themes, layout, animation
│   ├── Core/                  # Shared types, config, logging
│   ├── Platform/              # Win32 helpers, hotkey, DPI, Mica
│   ├── Storage/               # JSON persistence (settings, recent, favorites)
│   ├── Search/                # Trie + hash map search engine
│   └── Symbols/               # Database, converters, snippets
├── data/                      # Symbol database, snippets
├── assets/                    # Icons, resources
└── packs/                     # Plugin symbol packs
```

## Technology

- C++23, MSVC
- Win32 API + Direct2D + DirectWrite
- Zero runtime dependencies beyond Windows SDK

## License

MIT License
