@echo off
REM Windows Command Line Build Script for ImGui Abstraction
REM This script builds the project without requiring Visual Studio IDE

echo ====================================
echo ImGui Abstraction - Windows CLI Build
echo ====================================
echo.

REM Check for Visual Studio installation
where cl.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Visual Studio compiler not found in PATH.
    echo Attempting to locate Visual Studio...
    
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    ) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    ) else (
        echo ERROR: Could not find Visual Studio installation!
        echo Please run this script from "x64 Native Tools Command Prompt for VS"
        exit /b 1
    )
)

echo Compiler found!
echo.

REM Create build directory
if not exist "build_cli" mkdir build_cli
cd build_cli

echo Compiling ImGui core files...
cl /c /EHsc /std:c++17 /I.. /I..\..\..\backends ^
   ..\..\..\imgui.cpp ^
   ..\..\..\imgui_demo.cpp ^
   ..\..\..\imgui_draw.cpp ^
   ..\..\..\imgui_tables.cpp ^
   ..\..\..\imgui_widgets.cpp

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile ImGui core
    cd ..
    exit /b 1
)

echo Compiling backend files...
cl /c /EHsc /std:c++17 /I..\..\..\  /I..\..\..\backends ^
   ..\..\..\backends\imgui_impl_win32.cpp ^
   ..\..\..\backends\imgui_impl_dx11.cpp

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile backends
    cd ..
    exit /b 1
)

echo Compiling abstraction layer...
cl /c /EHsc /std:c++17 /I..\..\..\  /I..\..\..\backends ^
   ..\ImGuiRenderer.cpp ^
   ..\example_main.cpp

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile abstraction layer
    cd ..
    exit /b 1
)

echo Linking executable...
link /OUT:imgui_abstraction_example.exe ^
     imgui.obj imgui_demo.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj ^
     imgui_impl_win32.obj imgui_impl_dx11.obj ^
     ImGuiRenderer.obj example_main.obj ^
     d3d11.lib dxgi.lib d3dcompiler.lib ^
     /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to link executable
    cd ..
    exit /b 1
)

cd ..
echo.
echo ====================================
echo Build successful!
echo Executable: build_cli\imgui_abstraction_example.exe
echo ====================================
echo.
echo To run the application:
echo   cd build_cli
echo   imgui_abstraction_example.exe
