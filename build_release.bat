@echo off
setlocal
echo ========================================
echo  Scientific Symbol Panel - Release Build
echo ========================================
echo/

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build-release"

:: ============================================================================
:: Backend selection menu
:: ============================================================================
echo Select build target:
echo/
echo   [1] Windows Direct2D (Win32 native, no external deps^)
echo   [2] Cross-platform (GLFW + ImGui, needs vcpkg or FetchContent^)
echo/
set /p CHOICE="Enter choice (1 or 2): "

if "%CHOICE%"=="1" (
    set "BACKEND=Win32"
    set "ZIP_NAME=SSP-Win32-Release.zip"
    echo/
    echo --- Windows Direct2D backend selected ---
)
if "%CHOICE%"=="2" (
    set "BACKEND=GLFW"
    set "ZIP_NAME=SSP-CrossPlatform-Release.zip"
    echo/
    echo --- Cross-platform GLFW backend selected ---
)
if not "%CHOICE%"=="1" if not "%CHOICE%"=="2" (
    echo Invalid choice. Defaulting to Windows Direct2D.
    set "BACKEND=Win32"
    set "ZIP_NAME=SSP-Win32-Release.zip"
)

:: ============================================================================
:: Configure
:: ============================================================================
echo/
echo [1/3] Configuring CMake (Release, x64, %BACKEND% backend) ...

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

cmake -S "%ROOT%." -B "%BUILD_DIR%" -DSSP_BACKEND=%BACKEND%
if %ERRORLEVEL% NEQ 0 goto config_failed

echo/
echo [2/3] Building Release ...
cmake --build "%BUILD_DIR%" --config Release
if %ERRORLEVEL% NEQ 0 goto build_failed

:: ============================================================================
:: Package
:: ============================================================================
echo/
echo [3/3] Packaging %ZIP_NAME% ...
powershell -Command "Set-Location '%BUILD_DIR%\Release'; $zip = Join-Path '%ROOT%' '%ZIP_NAME%'; if (Test-Path $zip) { Remove-Item $zip }; Compress-Archive -Path SSP.exe,data -DestinationPath $zip -Force; Write-Host '%ZIP_NAME% created'"

echo/
echo ========================================
echo  Build complete!
echo  Backend:  %BACKEND%
echo  Output:   %BUILD_DIR%\Release\
echo  Package:  %ZIP_NAME%
echo ========================================
goto done

:config_failed
echo/
echo [FAIL] CMake configure failed.
if "%BACKEND%"=="GLFW" (
    echo/
    echo NOTE: GLFW backend requires GLFW, ImGui, and OpenGL.
    echo Use vcpkg: vcpkg install glfw3 imgui --triplet x64-windows-static
    echo Or let FetchContent download them (needs internet + git).
)
pause
exit /b 1

:build_failed
echo/
echo [FAIL] Build failed.
pause
exit /b 1

:done
endlocal
