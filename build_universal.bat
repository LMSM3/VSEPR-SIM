@echo off
REM build_universal.bat
REM Universal build script for VSEPR-Sim (Windows)

setlocal enabledelayedexpansion

echo ╔════════════════════════════════════════════════════════════════╗
echo ║  VSEPR-Sim Universal Build System (Batch)                     ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM Parse arguments
set BUILD_TYPE=Release
set CLEAN=false
set JOBS=8
set TARGET=all
set VERBOSE=false

:parse_args
if "%1"=="" goto :end_parse
if /i "%1"=="--debug" set BUILD_TYPE=Debug& shift& goto :parse_args
if /i "%1"=="-d" set BUILD_TYPE=Debug& shift& goto :parse_args
if /i "%1"=="--clean" set CLEAN=true& shift& goto :parse_args
if /i "%1"=="-c" set CLEAN=true& shift& goto :parse_args
if /i "%1"=="--jobs" set JOBS=%2& shift& shift& goto :parse_args
if /i "%1"=="-j" set JOBS=%2& shift& shift& goto :parse_args
if /i "%1"=="--target" set TARGET=%2& shift& shift& goto :parse_args
if /i "%1"=="-t" set TARGET=%2& shift& shift& goto :parse_args
if /i "%1"=="--verbose" set VERBOSE=true& shift& goto :parse_args
if /i "%1"=="-v" set VERBOSE=true& shift& goto :parse_args
if /i "%1"=="--help" goto :show_help
if /i "%1"=="-h" goto :show_help
echo [ERROR] Unknown option: %1
echo Run with --help for usage information
exit /b 1

:show_help
echo Usage: build_universal.bat [options]
echo.
echo Options:
echo   --debug, -d         Build in Debug mode (default: Release)
echo   --clean, -c         Clean build directory before building
echo   --jobs, -j ^<N^>      Number of parallel jobs (default: 8)
echo   --target, -t ^<T^>    Specific target to build (default: all)
echo   --verbose, -v       Verbose build output
echo   --help, -h          Show this help message
echo.
echo Examples:
echo   build_universal.bat                    # Standard release build
echo   build_universal.bat --debug            # Debug build
echo   build_universal.bat --clean --jobs 16  # Clean rebuild with 16 cores
echo   build_universal.bat --target vsepr     # Build only vsepr binary
exit /b 0

:end_parse

REM Clean if requested
if "%CLEAN%"=="true" (
    echo [CLEAN] Cleaning build directory...
    if exist build rmdir /s /q build
    echo [OK] Build directory cleaned
    echo.
)

REM Check for WSL
where wsl >nul 2>&1
if %errorlevel%==0 (
    echo [INFO] WSL detected - using bash build script
    wsl bash -c "cd '%cd:\=/%' && ./build_universal.sh --jobs %JOBS% %*"
    exit /b !errorlevel!
)

REM Create build directory
if not exist build (
    echo [BUILD] Creating build directory...
    mkdir build
    echo [OK] Build directory created
    echo.
)

REM Configure with CMake
echo [CMAKE] Configuring CMake (%BUILD_TYPE%)...
cd build
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)
echo [OK] CMake configuration complete
echo.

REM Build
echo [BUILD] Building target: %TARGET% (%JOBS% parallel jobs)...
cmake --build . --config %BUILD_TYPE% --target %TARGET% -j %JOBS%

if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║  ✓ Build Complete!                                            ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM Show binary locations
if exist bin\vsepr.exe (
    echo [INFO] Binary location: %cd%\bin\vsepr.exe
    echo [INFO] Quick test:      .\build\bin\vsepr.exe --help
    echo [INFO] Therm test:      .\build\bin\vsepr.exe therm ^<file.xyz^>
)

echo.
cd ..
