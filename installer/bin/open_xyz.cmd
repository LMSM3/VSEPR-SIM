@echo off
:: open_xyz.cmd — VSEPR-SIM XYZ/VSXYZ double-click launcher
:: Registered as the shell\open\command handler for .xyz and .vsxyz files.
:: Locates pythonw.exe, then invokes vsepr_xyz_popup.pyw with the clicked file.
::
:: Usage (by Windows shell): open_xyz.cmd "C:\path\to\file.xyz"

setlocal EnableDelayedExpansion

:: Prefer pythonw.exe from the same Python that owns pythonw on PATH
where pythonw.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
	set "PYWEXE=pythonw.exe"
	goto :run
)

:: Fallback: common Python install locations
for %%P in (
	"%LOCALAPPDATA%\Programs\Python\Python313\pythonw.exe"
	"%LOCALAPPDATA%\Programs\Python\Python312\pythonw.exe"
	"%LOCALAPPDATA%\Programs\Python\Python311\pythonw.exe"
	"C:\Python313\pythonw.exe"
	"C:\Python312\pythonw.exe"
	"C:\Python311\pythonw.exe"
) do (
	if exist %%P (
		set "PYWEXE=%%~P"
		goto :run
	)
)

:: No pythonw found — fall back to a console-mode python so the user sees an error
where python.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
	set "PYWEXE=python.exe"
	goto :run
)

:: Absolute last resort: show a message box via PowerShell
powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('Python not found.  Install Python 3.11+ from https://python.org and re-run the installer.','VSEPR-SIM','OK','Error')"
exit /b 1

:run
"%PYWEXE%" "%~dp0vsepr_xyz_popup.pyw" %*
endlocal
