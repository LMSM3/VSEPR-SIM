@echo off
:: Universal VSEPR-SIM artifact opener.
:: The supported interactive frontend is the fixed-timestep live viewer.

setlocal
set "FILE=%~1"
set "DIR=%~dp0"
set "VIEWER=%DIR%vsepr-view.exe"

if exist "%VIEWER%" (
    start "" "%VIEWER%" --artifact "%FILE%"
    exit /b 0
)

where vsepr-view.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    start "" vsepr-view.exe --artifact "%FILE%"
    exit /b 0
)

powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('VSEPR-SIM: vsepr-view.exe was not found. Rebuild with the vis preset or reinstall.','VSEPR-SIM','OK','Error')"
exit /b 1
