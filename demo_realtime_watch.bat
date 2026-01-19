@echo off
REM Demo: Generate 50 molecules with 1-second delay for real-time visualization
REM Usage: demo_realtime_watch.bat

setlocal enabledelayedexpansion

set "VSEPR_BIN=build\bin\vsepr-cli"
set "OUTPUT_DIR=xyz_output"
set "WATCH_FILE=%OUTPUT_DIR%\realtime_demo.xyz"

echo ╔══════════════════════════════════════════════════════════════╗
echo ║  Real-Time Molecule Generation Demo                         ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.
echo This demo generates 50 molecules with 1-second delays
echo Perfect for watching in real-time with Avogadro or VMD!
echo.

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if exist "%WATCH_FILE%" del "%WATCH_FILE%"

REM Check if CMake build exists
if not exist "build\bin\vsepr-cli.exe" if not exist "build\bin\vsepr-cli" (
    echo.
    echo ERROR: Project not built yet!
    echo.
    echo Please build the project first using ONE of:
    echo   1. build_universal.bat           ^(Recommended^)
    echo   2. cmake -B build ^&^& cmake --build build
    echo   3. .\build.ps1                   ^(PowerShell^)
    echo.
    echo After building, run this demo again.
    pause
    exit /b 1
)

REM Use vsepr-cli for molecule building
set "VSEPR_BIN=build\bin\vsepr-cli.exe"
if not exist "%VSEPR_BIN%" set "VSEPR_BIN=build\bin\vsepr-cli"

REM Array of molecular formulas (using counter-based selection)
set "formula[0]=H2O"
set "formula[1]=NH3"
set "formula[2]=CH4"
set "formula[3]=CO2"
set "formula[4]=H2O2"
set "formula[5]=N2O"
set "formula[6]=SO2"
set "formula[7]=H2S"
set "formula[8]=HCN"
set "formula[9]=HCl"
set "formula[10]=C2H6"
set "formula[11]=C2H4"
set "formula[12]=C2H2"
set "formula[13]=CH3OH"
set "formula[14]=CH2O"
set "formula[15]=C6H6"
set "formula[16]=C3H8"
set "formula[17]=C4H10"
set "formula[18]=CCl4"
set "formula[19]=SF6"
set "formula[20]=PCl5"
set "formula[21]=XeF4"
set "formula[22]=IF5"
set "formula[23]=BrF5"
set "formula[24]=ClF3"
set "formula[25]=NF3"
set "formula[26]=PF5"
set "formula[27]=AsF5"
set "formula[28]=SbCl5"
set "formula[29]=XeF2"
set "formula[30]=KrF2"
set "formula[31]=BF3"
set "formula[32]=AlCl3"
set "formula[33]=SiH4"
set "formula[34]=PH3"
set "formula[35]=H2Se"
set "formula[36]=H2Te"
set "formula[37]=CS2"
set "formula[38]=NO2"
set "formula[39]=N2O4"
set "formula[40]=O3"
set "formula[41]=SO3"
set "formula[42]=Cl2O"
set "formula[43]=F2O"
set "formula[44]=Br2"
set "formula[45]=I2"
set "formula[46]=HBr"
set "formula[47]=HI"
set "formula[48]=HF"
set "formula[49]=NaCl"

echo Starting generation...
echo.
echo In another terminal/window, run:
echo   avogadro %WATCH_FILE%
echo or
echo   vmd %WATCH_FILE%
echo.
echo Press Ctrl+C to stop early
echo.
timeout /t 3 /nobreak >nul

for /L %%i in (0,1,49) do (
    set /a "num=%%i+1"
    set /a "percent=!num! * 100 / 50"
    
    REM Get formula for this iteration
    set "current_formula=!formula[%%i]!"
    
    REM Progress indicator
    echo [!num!/50] !current_formula! - Progress: !percent!%%
    
    REM Build molecule and append to watch file
    "%VSEPR_BIN%" build "!current_formula!" --xyz temp_molecule.xyz >nul 2>&1
    
    if exist temp_molecule.xyz (
        type temp_molecule.xyz >> "%WATCH_FILE%"
        del temp_molecule.xyz
    ) else (
        REM Fallback: use dummy molecule if build fails
        echo 3 >> "%WATCH_FILE%"
        echo !current_formula! - Build Failed >> "%WATCH_FILE%"
        echo H 0.0 0.0 0.0 >> "%WATCH_FILE%"
        echo H 1.0 0.0 0.0 >> "%WATCH_FILE%"
        echo O 0.5 0.5 0.5 >> "%WATCH_FILE%"
    )
    
    REM Sleep 1 second between molecules
    timeout /t 1 /nobreak >nul
)

echo.
echo ╔══════════════════════════════════════════════════════════════╗
echo ║  ✓ Generation Complete!                                      ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.
echo Output file: %WATCH_FILE%
echo.

REM Count molecules and show stats
for /f %%A in ('find /c /v "" ^< "%WATCH_FILE%"') do set LINE_COUNT=%%A
for %%A in ("%WATCH_FILE%") do set FILE_SIZE=%%~zA

echo Statistics:
echo   File: %WATCH_FILE%
echo   Size: %FILE_SIZE% bytes
echo   Total time: ~50 seconds
echo.
echo View with:
echo   avogadro %WATCH_FILE%
echo   vmd %WATCH_FILE%
echo   pymol %WATCH_FILE%
echo.

endlocal
