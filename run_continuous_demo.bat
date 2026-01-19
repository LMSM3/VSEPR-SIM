@echo off
REM VSEPR-Sim Continuous Generation Demo for Windows
REM Demonstrates C++'s power for large-scale molecular discovery

echo ╔══════════════════════════════════════════════════════════════════╗
echo ║  VSEPR-Sim Continuous Generation - C++ Power Demonstration      ║
echo ╚══════════════════════════════════════════════════════════════════╝
echo.
echo This demonstrates how C++ enables continuous molecular discovery:
echo   • Generates N molecules (or unlimited)
echo   • Real-time statistics tracking
echo   • Checkpoint saving for resume capability
echo   • Streaming XYZ output for live visualization
echo   • Performance metrics (molecules/sec, molecules/hour)
echo.

echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
echo Step 1: Compiling vsepr_opengl_viewer.cpp
echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
echo.

g++ -std=c++17 -O2 examples\vsepr_opengl_viewer.cpp -o vsepr_opengl_viewer.exe -Iinclude -Ithird_party\glm -pthread

if %ERRORLEVEL% NEQ 0 (
    echo ✗ Compilation failed!
    exit /b 1
)

echo ✓ Compilation successful!
echo.

REM Clean up previous runs
if exist xyz_output rmdir /s /q xyz_output
if exist final_discovery_checkpoint.txt del final_discovery_checkpoint.txt

echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
echo Demo 1: Quick test (2,000 molecules, ~10 seconds)
echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
echo Command: vsepr_opengl_viewer.exe 2000 every-other --continue --watch molecules.xyz --checkpoint 500
echo.

echo y | vsepr_opengl_viewer.exe 2000 every-other --continue --watch molecules.xyz --checkpoint 500

echo.
echo.
echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
echo Results:
echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

if exist xyz_output\molecules.xyz (
    for %%A in (xyz_output\molecules.xyz) do set SIZE=%%~zA
    echo ✓ XYZ file created:
    echo     Path: xyz_output\molecules.xyz
    echo     Size: %SIZE% bytes
    echo.
    echo   First molecule:
    powershell -Command "Get-Content xyz_output\molecules.xyz -TotalCount 8"
    echo.
)

if exist final_discovery_checkpoint.txt (
    echo ✓ Checkpoint saved:
    echo     Path: final_discovery_checkpoint.txt
    echo.
    powershell -Command "Get-Content final_discovery_checkpoint.txt -TotalCount 6"
    echo.
)

echo.
echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
echo Performance Characteristics:
echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
echo.
echo Expected throughput:
echo   • ~200-300 molecules/sec (standard mode)
echo   • ~400-600 molecules/sec (every-other visualization)
echo   • ~720,000 - 1,080,000 molecules/hour
echo.
echo Scalability examples:
echo   100,000 molecules  → ~5-8 minutes
echo   1,000,000 molecules → ~50-80 minutes
echo   10,000,000 molecules → ~8-13 hours
echo.
echo Memory efficiency:
echo   • Streaming mode: Constant memory (~10-20 MB)
echo   • Statistics tracking: O(unique_formulas)
echo   • XYZ output: Appended to disk (not held in RAM)
echo.
echo.
echo ╔══════════════════════════════════════════════════════════════════╗
echo ║  Try These Commands:                                             ║
echo ╚══════════════════════════════════════════════════════════════════╝
echo.
echo # Generate 100,000 molecules with statistics every 5000:
echo   vsepr_opengl_viewer.exe 100000 every-other --continue --watch all.xyz --checkpoint 5000
echo.
echo # Generate 1 million molecules (takes ~1 hour):
echo   vsepr_opengl_viewer.exe 1000000 every-other --continue --watch million.xyz --checkpoint 10000
echo.
echo # View statistics:
echo   type final_discovery_checkpoint.txt
echo.
echo # Visualize XYZ file with Avogadro/VMD/PyMOL:
echo   avogadro xyz_output\molecules.xyz
echo   vmd xyz_output\molecules.xyz
echo   pymol xyz_output\molecules.xyz
echo.
