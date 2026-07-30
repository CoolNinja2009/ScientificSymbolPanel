# Build Instructions

## Prerequisites

1. **Visual Studio 2022** (17.4+) with:
   - Desktop development with C++
   - Windows 10 SDK (10.0.19041.0+)
2. **CMake 3.28+** (included with VS 2022)

## Quick Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\SSP.exe
```

## Visual Studio IDE

1. Open Visual Studio 2022
2. File → Open → Folder → select `ScientificSymbolPanel`
3. Select `x64-Release` from configuration dropdown
4. Build → Build All (`Ctrl+Shift+B`)

## Configurations

| Configuration | Description |
|---------------|-------------|
| `Debug` | Debug symbols, unoptimized, logging via OutputDebugString |
| `Release` | `/O2 /GL /LTCG /MT` — fully optimized, static CRT, no logging |

## Troubleshooting

### "Cannot open include file: 'd2d1.h'"
Install Windows 10 SDK via VS Installer → Individual Components.

### "CMake Error: No CMAKE_CXX_COMPILER"
Install "Desktop development with C++" workload in Visual Studio.

### Release binary size
The Release build statically links the C runtime (`/MT`) — no external DLL dependencies. Expected binary: ~500KB-1MB.
