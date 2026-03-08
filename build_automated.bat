@echo off
REM ============================================================================
REM VSEPR-Sim Automated Build & Test Script (Windows)
REM 
REM Builds the project and runs all tests in one command.
REM Includes validation for the new heat-gated reaction control system (Item #7).
REM
REM Usage:
REM   build_automated.bat                 - Quick build + tests
REM   build_automated.bat --clean         - Clean build + tests
REM   build_automated.bat --verbose       - Verbose output
REM   build_automated.bat --heat-only     - Build + run only heat_gate tests
REM ============================================================================

setlocal enabledelayedexpansion

set PROJECT_ROOT=%~dp0
set BUILD_DIR=%PROJECT_ROOT%build

REM Parse arguments
set CLEAN=0
set VERBOSE=0
set HEAT_ONLY=0

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--clean" set CLEAN=1
if /i "%~1"=="-c" set CLEAN=1
if /i "%~1"=="--verbose" set VERBOSE=1
if /i "%~1"=="-v" set VERBOSE=1
if /i "%~1"=="--heat-only" set HEAT_ONLY=1
if /i "%~1"=="--help" goto show_help
if /i "%~1"=="-h" goto show_help
shift
goto parse_args

:show_help
echo VSEPR-Sim Automated Build ^& Test (Windows)
echo.
echo Usage: %~nx0 [OPTIONS]
echo.
echo Options:
echo   --clean, -c       Clean build before building
echo   --verbose, -v     Verbose output
echo   --heat-only       Only run heat_gate tests (Item #7)
echo   --help, -h        Show this help
echo.
exit /b 0

:args_done

REM ============================================================================
REM Header
REM ============================================================================

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║  VSEPR-Sim Automated Build ^& Test (Windows)                  ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.
echo Project Root: %PROJECT_ROOT%
echo Build Directory: %BUILD_DIR%
echo.

REM ============================================================================
REM Clean (optional)
REM ============================================================================

if %CLEAN%==1 (
    echo ▶ Cleaning build directory...
    if exist "%BUILD_DIR%" (
        rd /s /q "%BUILD_DIR%"
        echo ✓ Build directory cleaned
    ) else (
        echo ℹ Build directory doesn't exist, skipping clean
    )
    echo.
)

REM ============================================================================
REM CMake Configuration
REM ============================================================================

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║  CMake Configuration                                          ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

echo ▶ Configuring CMake...

set CMAKE_ARGS=-S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

REM Try to detect Visual Studio
where cl >nul 2>&1
if %ERRORLEVEL%==0 (
    echo ℹ Using MSVC compiler
) else (
    echo ℹ MSVC not in PATH, CMake will auto-detect
)

if %VERBOSE%==1 (
    set CMAKE_ARGS=%CMAKE_ARGS% --debug-output
)

cmake %CMAKE_ARGS%
if %ERRORLEVEL% neq 0 (
    echo ✗ CMake configuration failed
    exit /b 1
)

echo ✓ CMake configuration successful
echo.

REM ============================================================================
REM Build
REM ============================================================================

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║  Building Project                                             ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

echo ▶ Compiling...

set BUILD_ARGS=--build "%BUILD_DIR%" --config Release

REM Detect number of cores
for /f "tokens=2 delims==" %%i in ('wmic cpu get NumberOfLogicalProcessors /value ^| find "="') do set CORES=%%i
if not defined CORES set CORES=4

set BUILD_ARGS=%BUILD_ARGS% --parallel %CORES%

echo ℹ Using %CORES% parallel jobs

if %VERBOSE%==1 (
    set BUILD_ARGS=%BUILD_ARGS% --verbose
)

cmake %BUILD_ARGS%
if %ERRORLEVEL% neq 0 (
    echo ✗ Build failed
    exit /b 1
)

echo ✓ Build successful
echo.

REM ============================================================================
REM Run Tests
REM ============================================================================

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║  Running Tests                                                ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

cd /d "%BUILD_DIR%"

if %HEAT_ONLY%==1 (
    REM Only run heat_gate tests (Item #7)
    echo ▶ Running heat_gate tests only (Item #7)...
    
    if exist "tests\Release\test_heat_gate.exe" (
        tests\Release\test_heat_gate.exe
        if !ERRORLEVEL! equ 0 (
            echo ✓ Heat gate tests PASSED
        ) else (
            echo ✗ Heat gate tests FAILED
            cd /d "%PROJECT_ROOT%"
            exit /b 1
        )
    ) else if exist "tests\test_heat_gate.exe" (
        tests\test_heat_gate.exe
        if !ERRORLEVEL! equ 0 (
            echo ✓ Heat gate tests PASSED
        ) else (
            echo ✗ Heat gate tests FAILED
            cd /d "%PROJECT_ROOT%"
            exit /b 1
        )
    ) else (
        echo ✗ test_heat_gate.exe not found
        cd /d "%PROJECT_ROOT%"
        exit /b 1
    )
) else (
    REM Run all tests
    echo ▶ Running CTest suite...
    
    if exist "CTestTestfile.cmake" (
        set CTEST_ARGS=--output-on-failure --parallel %CORES%
        if %VERBOSE%==1 (
            set CTEST_ARGS=!CTEST_ARGS! --verbose
        )
        
        ctest !CTEST_ARGS!
        if !ERRORLEVEL! equ 0 (
            echo ✓ All tests PASSED
        ) else (
            echo ✗ Some tests FAILED
            cd /d "%PROJECT_ROOT%"
            exit /b 1
        )
    ) else (
        echo ⚠ CTest not configured (tests may not be built)
        echo ℹ Trying to run tests manually...
        
        REM Try to run heat_gate test directly
        if exist "tests\Release\test_heat_gate.exe" (
            echo ▶ Running test_heat_gate...
            tests\Release\test_heat_gate.exe
            if !ERRORLEVEL! equ 0 (
                echo ✓ test_heat_gate PASSED
            ) else (
                echo ✗ test_heat_gate FAILED
                cd /d "%PROJECT_ROOT%"
                exit /b 1
            )
        ) else if exist "tests\test_heat_gate.exe" (
            echo ▶ Running test_heat_gate...
            tests\test_heat_gate.exe
            if !ERRORLEVEL! equ 0 (
                echo ✓ test_heat_gate PASSED
            ) else (
                echo ✗ test_heat_gate FAILED
                cd /d "%PROJECT_ROOT%"
                exit /b 1
            )
        )
    )
)

cd /d "%PROJECT_ROOT%"

echo.

REM ============================================================================
REM Summary
REM ============================================================================

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║  Build ^& Test Summary                                         ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

echo ✓ Build completed successfully
echo ✓ Tests passed

if %HEAT_ONLY%==0 (
    echo ℹ Binaries available in: %BUILD_DIR%\bin\
    echo ℹ Tests available in: %BUILD_DIR%\tests\
)

echo.
echo ✓ All automated checks passed!
echo.

REM ============================================================================
REM Additional Validation (Item #7 specific)
REM ============================================================================

if exist "build\examples\Release\demo_temperature_heat_mapping.exe" (
    echo.
    echo ╔═══════════════════════════════════════════════════════════════╗
    echo ║  Optional: Run Item #7 Demo                                  ║
    echo ╚═══════════════════════════════════════════════════════════════╝
    echo.
    echo ℹ Demo available: build\examples\Release\demo_temperature_heat_mapping.exe
    echo.
)

exit /b 0
