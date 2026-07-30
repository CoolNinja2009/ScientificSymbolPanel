@echo off
setlocal enabledelayedexpansion
echo ========================================
echo  Scientific Symbol Panel - Release Build
echo ========================================
echo.

:: ============================================================================
:: Backend selection menu
:: ============================================================================
echo Select build target:
echo.
echo   [1] Windows Direct2D (Win32 native, no external deps)
echo   [2] Cross-platform (GLFW + ImGui, needs vcpkg or FetchContent)
echo.
set /p CHOICE="Enter choice (1 or 2): "

if "%CHOICE%"=="1" (
    set "BACKEND=Win32"
    set "ZIP_NAME=SSP-Win32-Release.zip"
    echo.
    echo --- Windows Direct2D backend selected ---
) else if "%CHOICE%"=="2" (
    set "BACKEND=GLFW"
    set "ZIP_NAME=SSP-CrossPlatform-Release.zip"
    echo.
    echo --- Cross-platform GLFW backend selected ---
) else (
    echo Invalid choice. Defaulting to Windows Direct2D.
    set "BACKEND=Win32"
    set "ZIP_NAME=SSP-Win32-Release.zip"
)

:: ============================================================================
:: Build
:: ============================================================================
set "BUILD_DIR=%~dp0build-release"

echo.
echo [1/3] Configuring CMake (Release, x64, %BACKEND% backend) ...

:: Wipe and reconfigure
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DSSP_BACKEND=%BACKEND% "%~dp0"
if errorlevel 1 (
    echo [FAIL] CMake configure failed.
    if "%BACKEND%"=="GLFW" (
        echo.
        echo NOTE: GLFW backend requires GLFW, ImGui, and OpenGL.
        echo Use vcpkg: vcpkg install glfw3 imgui --triplet x64-windows-static
        echo Or let FetchContent download them (needs internet + git).
    )
    pause
    exit /b 1
)

echo.
echo [2/3] Building Release ...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo [FAIL] Build failed
    pause
    exit /b 1
)

:: ============================================================================
:: Package
:: ============================================================================
echo.
echo [3/3] Packaging %ZIP_NAME% ...
powershell -Command ^
    "Set-Location '%BUILD_DIR%\Release'; ^
     $zip = Join-Path '%~dp0' '%ZIP_NAME%'; ^
     if (Test-Path $zip) { Remove-Item $zip }; ^
     Compress-Archive -Path SSP.exe,data -DestinationPath $zip -Force; ^
     Write-Host '%ZIP_NAME% created'"

echo.
echo ========================================
echo  Build complete!
echo  Backend:  %BACKEND%
echo  Output:   %BUILD_DIR%\Release\
echo  Package:  %ZIP_NAME%
echo ========================================
endlocal
