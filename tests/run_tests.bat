@echo off
REM ============================================================================
REM Run BATS Integration Tests on Windows
REM ============================================================================

setlocal enabledelayedexpansion

echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║        VSEPR-Sim Integration Tests (Windows)                  ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM ============================================================================
REM Check for WSL
REM ============================================================================

where wsl >nul 2>&1
if errorlevel 1 (
    echo ✗ ERROR: WSL not found
    echo.
    echo Integration tests require WSL with BATS installed.
    echo.
    echo Install WSL:
    echo   wsl --install
    echo.
    echo Install BATS in WSL:
    echo   wsl sudo apt-get update
    echo   wsl sudo apt-get install -y bats
    echo.
    pause
    exit /b 1
)

echo ✓ WSL detected
echo.

REM ============================================================================
REM Check for BATS in WSL
REM ============================================================================

echo ▶ Checking for BATS...
wsl which bats >nul 2>&1
if errorlevel 1 (
    echo ✗ BATS not found in WSL
    echo.
    echo Installing BATS...
    wsl sudo apt-get update
    wsl sudo apt-get install -y bats
    
    if errorlevel 1 (
        echo ✗ Failed to install BATS
        pause
        exit /b 1
    )
)

echo ✓ BATS available
echo.

REM ============================================================================
REM Check for Executable
REM ============================================================================

if not exist "build\bin\vsepr" (
    if not exist "build\bin\vsepr.exe" (
        echo ✗ ERROR: VSEPR executable not found
        echo.
        echo Please build first:
        echo   build_windows.bat --clean
        echo.
        pause
        exit /b 1
    )
)

echo ✓ Executable found
echo.

REM ============================================================================
REM Run Tests
REM ============================================================================

echo ▶ Running integration tests...
echo.

wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && bats tests/integration_tests.bats"

set TEST_EXIT_CODE=%ERRORLEVEL%

echo.

if %TEST_EXIT_CODE% equ 0 (
    echo ✓ All tests passed!
) else (
    echo ✗ Some tests failed
    echo Exit code: %TEST_EXIT_CODE%
)

echo.
pause
exit /b %TEST_EXIT_CODE%
