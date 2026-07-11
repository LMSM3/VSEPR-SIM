@echo off
:: open_vsim_file.cmd - VSEPR-SIM universal file opener / GL router
:: Single handler for .xyz .xyza .xyzA .xyzc .xyzf .xyzF .xyzfull .vsxyz .vsim
::
:: Priority order:
::   1. live-xyza-viewer.exe "%1"          (OpenGL 3-D renderer - primary)
::   2. vsepr.exe open "%1"                (CLI fallback)
::   3. pythonw vsepr_xyz_popup.pyw "%1"   (Python popup - last resort)
::
:: The terminal / CLI path is a router only. The proper visualization system
:: (live-xyza-viewer) is always attempted first; other routes activate only
:: when that binary is absent from the install tree.
::
:: Registered as the shell\open\command for all VSIM/XYZ file types.
:: Lives at {app}\bin\ alongside the other binaries.
:: Usage (Windows shell): open_vsim_file.cmd "C:\path\to\file.xyz"

setlocal EnableDelayedExpansion
set "FILE=%~1"
set "DIR=%~dp0"

:: -- Priority 1: live-xyza-viewer.exe  (OpenGL 3-D renderer) ------------------
:: Canonical install location: {app}\bin\live-xyza-viewer.exe
:: Dev-tree fallback: installer\bin\ + ..\..\build\live-xyza-viewer.exe
set "GL_VIEWER="
if exist "%DIR%live-xyza-viewer.exe"                 set "GL_VIEWER=%DIR%live-xyza-viewer.exe"
if not defined GL_VIEWER (
    if exist "%DIR%..\..\build\live-xyza-viewer.exe" set "GL_VIEWER=%DIR%..\..\build\live-xyza-viewer.exe"
)
if defined GL_VIEWER (
    start "" "%GL_VIEWER%" "%FILE%"
    exit /b 0
)

:: -- Priority 2: vsepr.exe open  (CLI inspect - router fallback only) ----------
if exist "%DIR%vsepr.exe" (
    start "" "%DIR%vsepr.exe" open "%FILE%"
    exit /b 0
)

:: -- Priority 3: Python popup  (coordinate viewer - last resort) ---------------
:: Find pythonw.exe - try PATH first, then common install locations
where pythonw.exe >nul 2>&1
if %ERRORLEVEL% equ 0 ( set "PYWEXE=pythonw.exe" & goto :popup )

for %%P in (
    "%LOCALAPPDATA%\Programs\Python\Python313\pythonw.exe"
    "%LOCALAPPDATA%\Programs\Python\Python312\pythonw.exe"
    "%LOCALAPPDATA%\Programs\Python\Python311\pythonw.exe"
    "C:\Python313\pythonw.exe"
    "C:\Python312\pythonw.exe"
    "C:\Python311\pythonw.exe"
) do ( if exist %%P ( set "PYWEXE=%%~P" & goto :popup ) )

where python.exe >nul 2>&1
if %ERRORLEVEL% equ 0 ( set "PYWEXE=python.exe" & goto :popup )

:: -- No handler found ----------------------------------------------------------
powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('VSEPR-SIM: no viewer found.`n`nExpected: live-xyza-viewer.exe`nFallbacks: vsepr.exe, vsepr_xyz_popup.pyw`n`nRun the installer or rebuild.','VSEPR-SIM','OK','Error')"
exit /b 1

:popup
if exist "%DIR%vsepr_xyz_popup.pyw" (
    "%PYWEXE%" "%DIR%vsepr_xyz_popup.pyw" "%FILE%"
    exit /b 0
)
if exist "%DIR%..\..\tools\vsepr_xyz_popup.pyw" (
    "%PYWEXE%" "%DIR%..\..\tools\vsepr_xyz_popup.pyw" "%FILE%"
    exit /b 0
)
powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('VSEPR-SIM: vsepr_xyz_popup.pyw not found.`nRe-run the installer.','VSEPR-SIM','OK','Error')"
exit /b 1

endlocal