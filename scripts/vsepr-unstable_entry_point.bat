@echo off
setlocal enabledelayedexpansion
REM ============================================================================
REM VSEPR-Sim Universal Launcher (Windows)
REM Supports: Native .exe, WSL builds, bash scripts, first-time init
REM Delegates to vseprW.sh for advanced logic
REM ============================================================================

set "SCRIPT_DIR=%~dp0"
set "VSEPR_EXE=%SCRIPT_DIR%build\bin\vsepr.exe"
set "VSEPR_LINUX=%SCRIPT_DIR%build\bin\vsepr"
set "VSEPR_WRAPPER=%SCRIPT_DIR%vseprW.sh"
set "INIT_FLAG=%SCRIPT_DIR%.vsepr_initialized"
set "USE_WSL=0"
set "USE_WRAPPER=0"

REM ============================================================================
REM First-Time Initialization
REM ============================================================================

if not exist "%INIT_FLAG%" (
    echo.
    echo ╔═══════════════════════════════════════════════════════════════╗
    echo ║       First-time setup detected - Initializing VSEPR-Sim     ║
    echo ╚═══════════════════════════════════════════════════════════════╝
    echo.
    
    REM Check for WSL
    wsl --status >nul 2>&1
    if errorlevel 1 (
        echo [INFO] WSL not available - Windows-only mode
    ) else (
        echo [OK] WSL detected
        set "USE_WSL=1"
    )
    
    REM Check for Git Bash
    where bash >nul 2>&1
    if errorlevel 1 (
        echo [INFO] Git Bash not found - limited bash support
    ) else (
        echo [OK] Bash available
    )
    
    REM Create initialization flag
    echo Initialized on %DATE% %TIME% > "%INIT_FLAG%"
    echo.
    echo [DONE] Initialization complete
    echo.
    timeout /t 2 >nul
)

REM ============================================================================
REM Special Command Handling
REM ============================================================================

REM Handle 'sudo' prefix (strip it, we're on Windows)
if "%~1"=="sudo" (
    shift
)

REM Handle 'test' command - delegate to bash test.sh
if "%~1"=="test" (
    shift
    goto :run_tests
)

REM Handle 'bash test.sh' pattern
if "%~1"=="bash" (
    if "%~2"=="test.sh" (
        shift
        shift
        goto :run_tests
    )
)

REM ============================================================================
REM Detect Execution Mode
REM ============================================================================

REM Check if wrapper script exists and should be used
if exist "%VSEPR_WRAPPER%" (
    REM Use wrapper for complex commands
    if "%~1"=="batch" set "USE_WRAPPER=1"
    if "%~1"=="optimize" set "USE_WRAPPER=1"
    if "%~1"=="md" set "USE_WRAPPER=1"
)

if "%USE_WRAPPER%"=="1" (
    goto :run_wrapper
)

REM Otherwise check for direct executable
if exist "%VSEPR_LINUX%" (
    set "USE_WSL=1"
    goto :run_vsepr
)

if exist "%VSEPR_EXE%" (
    set "USE_WSL=0"
    goto :run_vsepr
)

REM Nothing found
echo.
echo ERROR: VSEPR executable not found!
echo.
echo Build first:
 echo   Windows: build.bat --clean
echo   WSL:     wsl ./build.sh --clean
echo.
pause
exit /b 1

REM ============================================================================
REM Run Tests (bash test.sh)
REM ============================================================================

:run_tests
echo [Running tests...]
echo.

REM Try WSL first
wsl --status >nul 2>&1
if not errorlevel 1 (
    wsl bash test.sh %*
    exit /b %ERRORLEVEL%
)

REM Try Git Bash
where bash >nul 2>&1
if not errorlevel 1 (
    bash test.sh %*
    exit /b %ERRORLEVEL%
)

REM Fallback: Try PowerShell
echo [WARN] Bash not available, trying PowerShell test runner...
if exist "tests\run_tests.bat" (
    call tests\run_tests.bat %*
    exit /b %ERRORLEVEL%
)

echo [ERROR] Cannot run tests - no bash or test runner found
exit /b 1

REM ============================================================================
REM Run via Wrapper (vseprW.sh)
REM ============================================================================

:run_wrapper
echo [Using vseprW.sh wrapper...]

REM Try WSL
wsl --status >nul 2>&1
if not errorlevel 1 (
    wsl bash "%VSEPR_WRAPPER%" %*
    exit /b %ERRORLEVEL%
)

REM Try Git Bash
where bash >nul 2>&1
if not errorlevel 1 (
    bash "%VSEPR_WRAPPER%" %*
    exit /b %ERRORLEVEL%
)

REM Fallback to direct execution
echo [WARN] Bash not available, falling back to direct execution
goto :run_vsepr

REM ============================================================================
REM Run Direct Executable
REM ============================================================================

:run_vsepr

if "%USE_WSL%"=="1" (
    REM Convert Windows path to WSL path
    set "WSL_PATH=/mnt/c/Users/Liam/Desktop/vsepr-sim"
    wsl bash -c "cd '%WSL_PATH%' && ./build/bin/vsepr %*"
) else (
    "%VSEPR_EXE%" %*
)

exit /b %ERRORLEVEL%
