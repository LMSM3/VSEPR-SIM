@echo off
REM Build VSEPR OpenGL Viewer with Batch Processing
REM Demonstrates 10,000 random molecule generation with visualization updates

echo ╔════════════════════════════════════════════════════════════════╗
echo ║  VSEPR OpenGL Viewer - Batch Processing Demo                  ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM Check for g++ (MinGW or MSYS2)
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: g++ not found. Please install MinGW or MSYS2.
    echo.
    echo Installation options:
    echo   1. MinGW: https://www.mingw-w64.org/
    echo   2. MSYS2: https://www.msys2.org/
    echo   3. Visual Studio with C++ tools
    pause
    exit /b 1
)

echo [1/3] Compiling vsepr_opengl_viewer.cpp...
echo.

g++ -std=c++17 -O2 ^
    vsepr_opengl_viewer.cpp ^
    -o vsepr_opengl_viewer.exe ^
    -I../include ^
    -I../third_party/glm

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ✗ Compilation failed!
    pause
    exit /b 1
)

echo.
echo ✓ Compilation successful!
echo.
echo [2/3] Executable created: vsepr_opengl_viewer.exe
echo.
echo [3/3] Usage:
echo   vsepr_opengl_viewer.exe [batch_size] [visualization_mode]
echo.
echo Examples:
echo   vsepr_opengl_viewer.exe              (Demo mode with 10,000 molecules)
echo   vsepr_opengl_viewer.exe 5000         (5,000 molecules, every other visualized)
echo   vsepr_opengl_viewer.exe 10000 all    (10,000 molecules, all visualized)
echo.
echo ════════════════════════════════════════════════════════════════
echo.

REM Ask if user wants to run now
set /p RUN="Run viewer now? (y/n): "
if /i "%RUN%"=="y" (
    echo.
    echo Starting VSEPR OpenGL Viewer...
    echo.
    vsepr_opengl_viewer.exe
)

echo.
echo Done!
pause
