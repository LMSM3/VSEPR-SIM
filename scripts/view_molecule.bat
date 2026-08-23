@echo off
REM Auto-launch molecular viewer
REM Usage: view_molecule.bat molecule.xyz

if "%~1"=="" (
    echo Usage: view_molecule.bat ^<molecule.xyz^>
    echo.
    echo Example: view_molecule.bat water.xyz
    exit /b 1
)

set XYZ_FILE=%~1

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python not found. Please install Python 3.
    exit /b 1
)

REM Generate and auto-open viewer
echo Generating molecular viewer for %XYZ_FILE%...
python scripts\viewer_generator.py "%XYZ_FILE%" --open

if errorlevel 1 (
    echo.
    echo Build failed!
    exit /b 1
)

echo.
echo Done! Viewer opened in browser.
