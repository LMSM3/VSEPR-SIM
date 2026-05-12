@echo off
:: open_vsim_file.cmd ? VSEPR-SIM universal file opener
:: Single handler for .xyz .xyza .xyzA .xyzc .xyzf .xyzF .xyzfull .vsxyz .vsim
::
:: Priority order:
::   1. vsepr-sim.exe open "%1"    (kernel ? 3-D replay viewer / inspect)
::   2. vsepr.exe open "%1"        (CLI fallback)
::   3. pythonw vsepr_xyz_popup.pyw "%1"   (Python popup ? last resort)
::
:: Registered as the shell\open\command for all VSIM/XYZ file types.
:: Lives at {app}\bin\ alongside the binaries.
:: Usage (Windows shell): open_vsim_file.cmd "C:\path\to\file.xyz"

setlocal EnableDelayedExpansion
set "FILE=%~1"
set "DIR=%~dp0"

:: ?? Priority 1: vsepr-sim.exe (3-D viewer / replay) ?????????????????????????
if exist "%DIR%vsepr-sim.exe" (
    start "" "%DIR%vsepr-sim.exe" open "%FILE%"
    exit /b 0
)

:: ?? Priority 2: vsepr.exe (CLI inspect) ??????????????????????????????????????
if exist "%DIR%vsepr.exe" (
    start "" "%DIR%vsepr.exe" open "%FILE%"
    exit /b 0
)

:: ?? Priority 3: Python popup (coordinate viewer) ?????????????????????????????
:: Find pythonw.exe ? try PATH first, then common install dirs
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

:: ?? No handler found ? show error ????????????????????????????????????????????
powershell -NoProfile -Command ^
  "Add-Type -AssemblyName PresentationFramework; ^
   [System.Windows.MessageBox]::Show('VSEPR-SIM binaries not found at %DIR%.^^nRun the installer or rebuild the project.','VSEPR-SIM','OK','Error')"
exit /b 1

:popup
if exist "%DIR%vsepr_xyz_popup.pyw" (
    "%PYWEXE%" "%DIR%vsepr_xyz_popup.pyw" "%FILE%"
    exit /b 0
)
powershell -NoProfile -Command ^
  "Add-Type -AssemblyName PresentationFramework; ^
   [System.Windows.MessageBox]::Show('vsepr_xyz_popup.pyw not found at %DIR%.^^nRe-run the installer.','VSEPR-SIM','OK','Error')"
exit /b 1

endlocal
