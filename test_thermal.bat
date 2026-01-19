@echo off
REM test_thermal.bat
REM Test thermal properties system (Windows)

setlocal enabledelayedexpansion

echo ╔════════════════════════════════════════════════════════════════╗
echo ║  Thermal Properties System Test Suite                         ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM Check for WSL
where wsl >nul 2>&1
if %errorlevel%==0 (
    echo [INFO] WSL detected - using bash test script
    wsl bash -c "cd '%cd:\=/%' && chmod +x test_thermal.sh && ./test_thermal.sh"
    exit /b !errorlevel!
)

REM Native Windows testing
set VSEPR_BIN=build\bin\vsepr.exe
set TEST_DIR=outputs\thermal_tests

if not exist "%VSEPR_BIN%" (
    echo [ERROR] VSEPR binary not found: %VSEPR_BIN%
    echo [BUILD] Building...
    call build_universal.bat --target vsepr
    echo.
)

if not exist "%TEST_DIR%" mkdir "%TEST_DIR%"

echo ═══════════════════════════════════════════════════════════════
echo   Building Test Molecules
echo ═══════════════════════════════════════════════════════════════
echo.

REM Example molecules: But feel free to try and Valid set ! 
set "MOLECULES=H2O:water NH3:ammonia CH4:methane Br2:bromine"

for %%M in (%MOLECULES%) do (
    for /f "tokens=1,2 delims=:" %%A in ("%%M") do (
        set "FORMULA=%%A"
        set "NAME=%%B"
        set "OUTPUT_FILE=%TEST_DIR%\%%B.xyz"
        
        if not exist "%TEST_DIR%\%%B.xyz" (
            echo [BUILD] Building %%A -^> %%B.xyz
            "%VSEPR_BIN%" build "%%A" --output "%TEST_DIR%\%%B.xyz" >nul 2>&1
            
            if errorlevel 1 (
                echo [ERROR] Failed to create %%B.xyz
            ) else (
                echo [OK] Created %%B.xyz
            )
        ) else (
            echo [INFO] %%B.xyz already exists
        )
    )
)

echo.
echo ═══════════════════════════════════════════════════════════════
echo   Running Thermal Analysis
echo ═══════════════════════════════════════════════════════════════
echo.

set PASS=0
set FAIL=0

for %%M in (%MOLECULES%) do (
    for /f "tokens=1,2 delims=:" %%A in ("%%M") do (
        set "INPUT_FILE=%TEST_DIR%\%%B.xyz"
        
        if exist "%TEST_DIR%\%%B.xyz" (
            echo [TEST] Testing %%B at 298.15K
            "%VSEPR_BIN%" therm "%%INPUT_FILE%%" --temperature 298.15 >nul 2>&1
            
            if errorlevel 1 (
                echo [ERROR] Thermal analysis failed
                set /a FAIL+=1
            ) else (
                echo [OK] Thermal analysis passed
                set /a PASS+=1
            )
            echo.
        ) else (
            echo [SKIP] %%B (file not found)
            set /a FAIL+=1
            echo.
        )
    )
)

echo ═══════════════════════════════════════════════════════════════
echo   Test Summary
echo ═══════════════════════════════════════════════════════════════
echo.
echo   Tests passed: %PASS%
echo   Tests failed: %FAIL%
echo.

if %FAIL% equ 0 (
    echo ╔════════════════════════════════════════════════════════════════╗
    echo ║  ✓ All thermal tests passed!                                  ║
    echo ╚════════════════════════════════════════════════════════════════╝
    exit /b 0
) else (
    echo ╔════════════════════════════════════════════════════════════════╗
    echo ║  ⚠ Some tests failed - review output above                    ║
    echo ╚════════════════════════════════════════════════════════════════╝
    exit /b 1
)
