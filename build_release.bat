@echo off
setlocal enabledelayedexpansion
echo ========================================
echo  Scientific Symbol Panel - Release Build
echo ========================================
echo.

set "BUILD_DIR=%~dp0build-release"
:: Wipe and reconfigure
echo [1/3] Configuring CMake (Release, x64) ...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release "%~dp0"
if errorlevel 1 (
    echo [FAIL] CMake configure failed
    pause
    exit /b 1
)

:: Build Release
echo.
echo [2/3] Building Release ...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo [FAIL] Build failed
    pause
    exit /b 1
)

:: Package
echo.
echo [3/3] Packaging SSP-Release.zip ...
powershell -Command ^
    "Set-Location '%BUILD_DIR%\Release'; ^
     $zip = Join-Path '%~dp0' 'SSP-Release.zip'; ^
     if (Test-Path $zip) { Remove-Item $zip }; ^
     Compress-Archive -Path SSP.exe,data,assets -DestinationPath $zip -Force; ^
     Write-Host 'SSP-Release.zip created'"

echo.
echo ========================================
echo  Build complete!
echo  Output: %BUILD_DIR%\Release\
echo  Release zip: SSP-Release.zip
echo  (No DLLs needed - statically linked)
echo ========================================
endlocal
