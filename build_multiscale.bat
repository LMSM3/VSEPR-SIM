@echo off
setlocal enabledelayedexpansion
REM build_multiscale.bat
REM Build and test the multiscale integration module (Windows)

echo.
echo ╔═══════════════════════════════════════════════════════════╗
echo ║  MULTISCALE INTEGRATION BUILD                             ║
echo ║  GPU Resource Manager + MD↔FEA Bridge                    ║
echo ╚═══════════════════════════════════════════════════════════╝
echo.

set "BUILD_DIR=%~dp0build"

REM Check for WSL
where wsl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [INFO] WSL detected, using Linux build script...
    wsl bash build_multiscale.sh
    exit /b 0
)

REM Clean WSL cache if present
if exist "%BUILD_DIR%\CMakeCache.txt" (
    findstr /C:"/mnt/c" "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 (
        echo [CLEAN] Removing WSL CMake cache...
        rmdir /s /q "%BUILD_DIR%"
    )
)

REM Native Windows build
echo [1/4] Creating build directory...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo [2/4] Configuring CMake...
cmake .. -DCMAKE_BUILD_TYPE=Release

echo [3/4] Building multiscale_demo...
cmake --build . --target multiscale_demo --config Release -j 8

REM Check if build succeeded
if exist "Release\multiscale_demo.exe" (
    echo.
    echo ✓ Build successful!
    echo.
    echo ╔═══════════════════════════════════════════════════════════╗
    echo ║  READY TO RUN                                             ║
    echo ╠═══════════════════════════════════════════════════════════╣
    echo ║  Run all demos:                                           ║
    echo ║    .\Release\multiscale_demo.exe                          ║
    echo ║                                                           ║
    echo ║  Run specific demo:                                       ║
    echo ║    .\Release\multiscale_demo.exe 1  # GPU conflict        ║
    echo ║    .\Release\multiscale_demo.exe 2  # Property extraction ║
    echo ║    .\Release\multiscale_demo.exe 3  # Safe transition     ║
    echo ║    .\Release\multiscale_demo.exe 4  # Status monitoring   ║
    echo ║    .\Release\multiscale_demo.exe 5  # Automated workflow  ║
    echo ╚═══════════════════════════════════════════════════════════╝
    echo.
    
    echo [4/4] Running Demo 1: GPU Conflict Prevention...
    echo.
    .\Release\multiscale_demo.exe 1
    
) else (
    echo.
    echo ⚠ Build completed but multiscale_demo.exe not found
    exit /b 1
)

cd ..
