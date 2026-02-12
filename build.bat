@echo off
REM ============================================================================
REM VSEPR-Sim Build Script (Windows)
REM Direct build using CMake
REM ============================================================================

setlocal enabledelayedexpansion

echo ╔═══════════════════════════════════════════════════════════════╗
echo ║              VSEPR-Sim Windows Build Script                  ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

set "PROJECT_ROOT=%~dp0"
set "BUILD_DIR=%PROJECT_ROOT%build"

REM Parse arguments
set CLEAN=0
set BUILD_TYPE=Release

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--clean" set CLEAN=1
if /i "%~1"=="-c" set CLEAN=1
if /i "%~1"=="--debug" set BUILD_TYPE=Debug
if /i "%~1"=="--help" goto :show_help
shift
goto :parse_args
:args_done

REM Check for CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo ✗ ERROR: CMake not found in PATH
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

echo ✓ CMake found
echo.

REM Clean if requested
if %CLEAN%==1 (
    echo ▶ Cleaning build directory...
    if exist "%BUILD_DIR%" (
        rmdir /s /q "%BUILD_DIR%"
        echo ✓ Build directory cleaned
    )
    echo.
)

REM Check for WSL/Windows CMake cache conflict
if exist "%BUILD_DIR%\CMakeCache.txt" (
    findstr /C:"/mnt/c" "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 (
        echo ▶ Detected WSL CMake cache, cleaning...
        rmdir /s /q "%BUILD_DIR%"
        echo ✓ Cleaned WSL cache
        echo.
    )
)

REM Create build directory
if not exist "%BUILD_DIR%" (
    echo ▶ Creating build directory...
    mkdir "%BUILD_DIR%"
    echo ✓ Build directory created
    echo.
)

REM CMake Configuration
echo ▶ Configuring with CMake...
echo   Build Type: %BUILD_TYPE%
echo.

cd /d "%BUILD_DIR%"
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DBUILD_APPS=ON

if errorlevel 1 (
    echo.
    echo ✗ CMake configuration failed
    cd /d "%PROJECT_ROOT%"
    pause
    exit /b 1
)

echo.
echo ✓ CMake configuration completed
echo.

REM Build
echo ▶ Building project...
echo   Jobs: %NUMBER_OF_PROCESSORS%
echo.

cmake --build . --config %BUILD_TYPE% --parallel %NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo.
    echo ✗ Build failed
    cd /d "%PROJECT_ROOT%"
    pause
    exit /b 1
)

echo.
echo ✓ Build completed successfully!
echo.

cd /d "%PROJECT_ROOT%"

REM Show results
if exist "%BUILD_DIR%\bin\vsepr.exe" (
    echo ✓ Executable: build\bin\vsepr.exe
    echo.
    echo Quick Start:
    echo   build\bin\vsepr.exe --help
    echo   build\bin\vsepr.exe build H2O --output water.xyz
    echo   build\bin\vsepr.exe webgl water.xyz -o molecules.json
    echo.
) else (
    echo ✗ Build may have failed - executable not found
    echo.
)

pause
exit /b 0

:show_help
echo.
echo Usage: build.bat [OPTIONS]
echo.
echo Options:
echo   --clean, -c       Clean build directory before building
echo   --debug           Build in Debug mode (default: Release)
echo   --help            Show this help message
echo.
echo Examples:
echo   build.bat                 # Basic build
echo   build.bat --clean         # Clean build
echo   build.bat --debug         # Debug build
echo.
exit /b 0
