@echo off
setlocal enabledelayedexpansion
REM XYZ Suite Test Runner (Windows)
REM Runs comprehensive I/O validation with resource monitoring

echo ╔════════════════════════════════════════════════════════════════╗
echo ║           XYZ Suite Tester - Build ^& Execute                   ║
echo ╚════════════════════════════════════════════════════════════════╝

set "BUILD_DIR=%~dp0build"

REM Clean WSL cache if present
if exist "%BUILD_DIR%\CMakeCache.txt" (
    findstr /C:"/mnt/c" "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 (
        echo ▶ Cleaning WSL CMake cache...
        rmdir /s /q "%BUILD_DIR%"
    )
)

REM Build test
echo.
echo [1/3] Building test suite...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_APPS=ON >nul 2>&1
if errorlevel 1 (
    echo ❌ CMake configuration failed
    cd ..
    exit /b 1
)

cmake --build . --config Release --parallel 4 >nul 2>&1
if errorlevel 1 (
    echo ❌ Build failed
    cd ..
    exit /b 1
)

echo ✓ Build successful
cd ..

REM Check for test data
echo.
echo [2/3] Checking test data...

set TEST_FILES=
if exist benchmark_results\*.xyz (
    for /f %%A in ('dir /b benchmark_results\*.xyz 2^>nul ^| find /c /v ""') do set FILE_COUNT=%%A
    echo ✓ Found %FILE_COUNT% test molecules in benchmark_results\
) else if exist test_water.xyz (
    set TEST_FILES=test_water.xyz
    echo ⚠ Using local test file: test_water.xyz
) else (
    echo ⚠ No benchmark files found, creating sample molecule...
    (
        echo 3
        echo Water molecule - Test data
        echo O  0.000  0.000  0.117
        echo H  0.000  0.757 -0.467
        echo H  0.000 -0.757 -0.467
    ) > test_h2o.xyz
    set TEST_FILES=test_h2o.xyz
    echo ✓ Created test_h2o.xyz
)

REM Run test
echo.
echo [3/3] Running test suite...
echo ────────────────────────────────────────────────────────────────
echo.

REM Find the test executable
set "TEST_EXE="
if exist "build\bin\vsepr.exe" set "TEST_EXE=build\bin\vsepr.exe"
if exist "build\Release\vsepr.exe" set "TEST_EXE=build\Release\vsepr.exe"
if exist "build\bin\Release\vsepr.exe" set "TEST_EXE=build\bin\Release\vsepr.exe"

if "%TEST_EXE%"=="" (
    echo ❌ Test executable not found
    exit /b 1
)

echo ✓ Using: %TEST_EXE%
echo.

if "%TEST_FILES%"=="" (
    "%TEST_EXE%" --version
) else (
    "%TEST_EXE%" build H2O --output %TEST_FILES%
)

set EXIT_CODE=%errorlevel%

echo.
echo ────────────────────────────────────────────────────────────────

if %EXIT_CODE% equ 0 (
    echo ✓ All tests passed!
) else (
    echo ❌ Tests failed with exit code %EXIT_CODE%
)

echo.
echo System Resource Summary:
wmic cpu get loadpercentage /value 2>nul | find "LoadPercentage"
wmic OS get FreePhysicalMemory,TotalVisibleMemorySize /value 2>nul | find "Memory"

exit /b %EXIT_CODE%
