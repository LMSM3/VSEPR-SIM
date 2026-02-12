@echo off
setlocal enabledelayedexpansion
REM ============================================================================
REM Create Portable Distribution Package
REM ============================================================================

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║          VSEPR-Sim Portable Distribution Creator             ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

set "VERSION=2.0.0"
set "PROJECT_ROOT=%~dp0.."
set "DIST_DIR=%PROJECT_ROOT%\dist-portable"
set "OUTPUT_ZIP=%PROJECT_ROOT%\vsepr-sim-v%VERSION%-portable.zip"

REM ============================================================================
REM Check Prerequisites
REM ============================================================================

if not exist "%PROJECT_ROOT%\build\bin\vsepr.exe" (
    echo ✗ ERROR: vsepr.exe not found!
    echo Please build the project first:
    echo   build_windows.bat --clean
    echo.
    pause
    exit /b 1
)

echo ✓ Executable found
echo.

REM ============================================================================
REM Create Distribution Directory
REM ============================================================================

echo ▶ Creating distribution directory...

if exist "%DIST_DIR%" (
    rmdir /s /q "%DIST_DIR%"
)
mkdir "%DIST_DIR%"

REM Create subdirectories
mkdir "%DIST_DIR%\bin"
mkdir "%DIST_DIR%\data"
mkdir "%DIST_DIR%\docs"
mkdir "%DIST_DIR%\examples"

echo ✓ Directory structure created
echo.

REM ============================================================================
REM Copy Files
REM ============================================================================

echo ▶ Copying executables...
copy /Y "%PROJECT_ROOT%\build\bin\vsepr.exe" "%DIST_DIR%\bin\" >nul
copy /Y "%PROJECT_ROOT%\build\bin\vsepr_batch.exe" "%DIST_DIR%\bin\" >nul
copy /Y "%PROJECT_ROOT%\build\bin\md_demo.exe" "%DIST_DIR%\bin\" >nul
echo ✓ Executables copied

echo ▶ Copying data files...
xcopy /E /I /Y "%PROJECT_ROOT%\data\*" "%DIST_DIR%\data\" >nul
echo ✓ Data files copied

echo ▶ Copying documentation...
xcopy /E /I /Y "%PROJECT_ROOT%\docs\*" "%DIST_DIR%\docs\" >nul
copy /Y "%PROJECT_ROOT%\README.md" "%DIST_DIR%\" >nul
copy /Y "%PROJECT_ROOT%\LICENSE" "%DIST_DIR%\" 2>nul
copy /Y "%PROJECT_ROOT%\CHANGELOG.md" "%DIST_DIR%\" >nul
echo ✓ Documentation copied

echo ▶ Copying examples...
copy /Y "%PROJECT_ROOT%\*.xyz" "%DIST_DIR%\examples\" 2>nul
echo ✓ Examples copied

echo ▶ Copying launcher...
copy /Y "%PROJECT_ROOT%\vsepr.bat" "%DIST_DIR%\" >nul
echo ✓ Launcher copied

REM Create portable launcher
echo ▶ Creating portable launcher...
(
echo @echo off
echo REM VSEPR-Sim Portable Launcher
echo REM No installation required!
echo.
echo set "VSEPR_ROOT=%%~dp0"
echo set "PATH=%%VSEPR_ROOT%%bin;%%PATH%%"
echo.
echo "%%VSEPR_ROOT%%bin\vsepr.exe" %%*
) > "%DIST_DIR%\vsepr-portable.bat"
echo ✓ Portable launcher created

REM Create README for portable version
echo ▶ Creating portable README...
(
echo # VSEPR-Sim v%VERSION% - Portable Edition
echo.
echo ## Quick Start
echo.
echo 1. Extract this folder anywhere on your computer
echo 2. Double-click `vsepr-portable.bat` to run
echo 3. Or use from command line: `vsepr-portable.bat build H2O`
echo.
echo ## No Installation Required
echo.
echo This is a portable version - no installation needed!
echo All files are self-contained in this folder.
echo.
echo ## Usage
echo.
echo ```batch
echo # Build a molecule
echo vsepr-portable.bat build H2O --optimize --viz
echo.
echo # Show help
echo vsepr-portable.bat --help
echo.
echo # Run examples
echo cd examples
echo ..\vsepr-portable.bat view water.xyz
echo ```
echo.
echo ## Documentation
echo.
echo See `docs\QUICKSTART.md` for detailed instructions.
echo.
echo ## System Requirements
echo.
echo - Windows 10 or later
echo - No additional dependencies required
echo.
echo ## Support
echo.
echo For help and documentation, visit the docs folder.
) > "%DIST_DIR%\README_PORTABLE.txt"
echo ✓ Portable README created

echo.

REM ============================================================================
REM Create ZIP Archive
REM ============================================================================

echo ▶ Creating ZIP archive...

if exist "%OUTPUT_ZIP%" (
    del /f "%OUTPUT_ZIP%"
)

powershell -Command "Compress-Archive -Path '%DIST_DIR%\*' -DestinationPath '%OUTPUT_ZIP%' -CompressionLevel Optimal"

if errorlevel 1 (
    echo ✗ Failed to create ZIP archive
    pause
    exit /b 1
)

REM Get ZIP size
for %%F in ("%OUTPUT_ZIP%") do set ZIP_SIZE=%%~zF
set /a ZIP_SIZE_MB=!ZIP_SIZE! / 1048576

echo ✓ ZIP archive created
echo.

REM ============================================================================
REM Generate Checksum
REM ============================================================================

echo ▶ Generating checksum...
certutil -hashfile "%OUTPUT_ZIP%" SHA256 | findstr /v ":" | findstr /v "CertUtil" > "%PROJECT_ROOT%\vsepr-sim-v%VERSION%-portable.sha256"
echo ✓ Checksum generated
echo.

REM ============================================================================
REM Summary
REM ============================================================================

echo ╔═══════════════════════════════════════════════════════════════╗
echo ║                    Build Complete                             ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.
echo ✓ Portable distribution created:
echo   %OUTPUT_ZIP%
echo   Size: !ZIP_SIZE_MB! MB
echo.
echo ✓ SHA256 checksum:
type "%PROJECT_ROOT%\vsepr-sim-v%VERSION%-portable.sha256"
echo.
echo Distribution ready for:
echo   - Direct download
echo   - USB drive deployment
echo   - No-install usage
echo.
echo Users can extract and run immediately - no installation required!
echo.

pause
exit /b 0
