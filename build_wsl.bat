@echo off
REM build_wsl.bat
REM Build VSEPR-Sim using WSL and g++

echo ╔════════════════════════════════════════════════════════════════╗
echo ║  Building VSEPR-Sim with WSL/g++                              ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM Check for WSL
where wsl >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] WSL not found. Please install WSL with Ubuntu.
    exit /b 1
)

echo [INFO] Using WSL build environment
echo.

REM Convert Windows path to WSL path
set "WSL_PATH=/mnt/c/Users/Liam/Desktop/vsepr-sim"

REM Run build in WSL
wsl bash -c "cd '%WSL_PATH%' && mkdir -p build && cd build && cmake -G 'Unix Makefiles' .. && make -j4"

if %errorlevel% equ 0 (
    echo.
    echo ╔════════════════════════════════════════════════════════════════╗
    echo ║  ✓ Build successful!                                          ║
    echo ╚════════════════════════════════════════════════════════════════╝
    echo.
    echo Executables in: build/bin/
    exit /b 0
) else (
    echo.
    echo ╔════════════════════════════════════════════════════════════════╗
    echo ║  ✗ Build failed - see errors above                            ║
    echo ╚════════════════════════════════════════════════════════════════╝
    exit /b 1
)
