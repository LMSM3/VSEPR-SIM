@echo off
REM debug.bat
REM Universal debugging script for VSEPR-Sim (Windows)

setlocal enabledelayedexpansion

echo ╔════════════════════════════════════════════════════════════════╗
echo ║  VSEPR-Sim Debug ^& Diagnostic Tool                            ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

set MODE=%1
if "%MODE%"=="" set MODE=info

REM Check for WSL for bash-based debugging
if /i "%MODE%"=="wsl" (
    where wsl >nul 2>&1
    if %errorlevel%==0 (
        wsl bash -c "cd '%cd:\=/%' && chmod +x debug.sh && ./debug.sh %2"
        exit /b !errorlevel!
    ) else (
        echo [ERROR] WSL not available
        exit /b 1
    )
)

if /i "%MODE%"=="info" goto :info
if /i "%MODE%"=="-i" goto :info
if /i "%MODE%"=="build" goto :build
if /i "%MODE%"=="-b" goto :build
if /i "%MODE%"=="test" goto :test
if /i "%MODE%"=="-t" goto :test
if /i "%MODE%"=="clean" goto :clean
if /i "%MODE%"=="-c" goto :clean
if /i "%MODE%"=="rebuild" goto :rebuild
if /i "%MODE%"=="-r" goto :rebuild
if /i "%MODE%"=="thermal" goto :thermal
if /i "%MODE%"=="-th" goto :thermal
if /i "%MODE%"=="help" goto :help
if /i "%MODE%"=="-h" goto :help

echo [ERROR] Unknown mode: %MODE%
echo Run 'debug.bat help' for usage
exit /b 1

:info
echo ═══ System Information ═══
echo.

echo [OS]
ver
echo.

echo [CMake]
cmake --version 2>nul || echo   Not found
echo.

echo [Compiler]
where cl >nul 2>&1 && (
    cl 2>&1 | findstr /C:"Version"
) || echo   MSVC not found
where g++ >nul 2>&1 && (
    g++ --version | findstr /R "^g++"
) || echo   g++ not found
echo.

echo [Build Directory]
if exist build (
    echo   ✓ Exists
    if exist build\bin (
        dir /b build\bin 2>nul || echo   No binaries yet
    )
) else (
    echo   ✗ Not found (run build_universal.bat)
)
echo.

echo [Binary Status]
if exist build\bin\vsepr.exe (
    echo   ✓ vsepr.exe found
    dir build\bin\vsepr.exe | findstr /R "vsepr.exe"
) else (
    echo   ✗ vsepr.exe not found
)
echo.
goto :end

:build
echo ═══ Debug Build ═══
echo.
call build_universal.bat --debug --verbose
goto :end

:test
echo ═══ Running Tests ═══
echo.

if not exist build\bin\vsepr.exe (
    echo [BUILD] Binary not found, building first...
    call build_universal.bat --target vsepr
)

echo [TEST] Testing vsepr command:
build\bin\vsepr.exe --version
echo.

echo [TEST] Testing build command:
build\bin\vsepr.exe build H2O --output %TEMP%\test_water.xyz
echo.

echo [TEST] Testing therm command:
if exist %TEMP%\test_water.xyz (
    build\bin\vsepr.exe therm %TEMP%\test_water.xyz --temperature 298.15
) else (
    echo [ERROR] Test molecule not created
)
goto :end

:clean
echo ═══ Cleaning Build ═══
echo.

echo [CLEAN] Removing build directory...
if exist build rmdir /s /q build
echo [OK] Cleaned
echo.
goto :end

:rebuild
echo ═══ Clean Rebuild ═══
echo.
call build_universal.bat --clean --verbose
goto :end

:thermal
echo ═══ Thermal System Debug ═══
echo.
call test_thermal.bat
goto :end

:help
echo Usage: debug.bat [mode]
echo.
echo Modes:
echo   info, -i        Show system information (default)
echo   build, -b       Debug build with verbose output
echo   test, -t        Run quick functionality tests
echo   clean, -c       Clean build artifacts
echo   rebuild, -r     Clean rebuild
echo   thermal, -th    Test thermal properties system
echo   wsl [mode]      Use WSL bash debug script
echo   help, -h        Show this help
echo.
echo Examples:
echo   debug.bat info
echo   debug.bat build
echo   debug.bat thermal
echo   debug.bat wsl thermal
goto :end

:end
echo.
echo ═══ Debug session complete ═══
