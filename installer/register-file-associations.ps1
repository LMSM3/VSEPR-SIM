<#
.SYNOPSIS
    register-file-associations.ps1
    Windows shell integration for VSEPR-SIM file types.

.DESCRIPTION
    Registers file-type behaviors under HKCU (no admin required).
    All XYZ-family open commands route through open_vsim_file.cmd which
    applies priority order: vsepr-sim.exe -> vsepr.exe -> pythonw popup.

    .vsim          — Full takeover.  Double-click runs VSIM script.
    .xyzFull/.xyzfull  — Full takeover.  Double-click opens Replay Viewer.
    .vsxyz         — Full takeover.  Double-click opens VSIM coordinate viewer.
    .xyza/.xyzA    — Full takeover.  Enriched atomistic frame.
    .xyzc          — Full takeover.  Restartable checkpoint.
    .xyzf/.xyzF    — Full takeover.  Multi-frame trajectory.
    .xyz           — CONSERVATIVE.  Context-menu only; default handler unchanged.

    Opener priority (for all XYZ types):
      1. vsepr-sim.exe open (3-D viewer / replay)
      2. vsepr.exe open      (CLI inspect)
      3. pythonw vsepr_xyz_popup.pyw  (coordinate popup)
    This routing is implemented in open_vsim_file.cmd.

.PARAMETER BinaryPath
    Full path to the installed vsepr.exe.
    Default: %LOCALAPPDATA%\VSEPR-SIM\bin\vsepr.exe

.PARAMETER Unregister
    Remove all VSEPR-SIM file associations (uninstall mode).

v5.0.0 | v5 packaging
#>

[CmdletBinding()]
param(
    [string] $BinaryPath = "",
    [switch] $Unregister
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Resolve installed binary ───────────────────────────────────────────────────
if (-not $BinaryPath) {
    $BinaryPath = Join-Path $env:LOCALAPPDATA "VSEPR-SIM\bin\vsepr.exe"
}

if (-not $Unregister -and -not (Test-Path $BinaryPath)) {
    throw "vsepr.exe not found at: $BinaryPath`n  Install first, or pass -BinaryPath explicitly."
}

$ExeQ = "`"$BinaryPath`""   # quoted for registry command values

# Universal opener cmd (lives next to the binary)
# Priority: vsepr-sim.exe -> vsepr.exe -> pythonw popup
$BinDir  = Split-Path $BinaryPath
$OpenerCmd = Join-Path $BinDir "open_vsim_file.cmd"
$OpenerQ   = if (Test-Path $OpenerCmd) { "`"$OpenerCmd`"" } else { $ExeQ }

# ── Registry helpers ──────────────────────────────────────────────────────────
function Reg-Set([string]$path, [string]$name, [string]$value) {
    if (-not (Test-Path "Registry::$path")) {
        New-Item -Path "Registry::$path" -Force | Out-Null
    }
    Set-ItemProperty -Path "Registry::$path" -Name $name -Value $value -Force
}

function Reg-Del([string]$path) {
    if (Test-Path "Registry::$path") {
        Remove-Item -Path "Registry::$path" -Recurse -Force
        Write-Host ("  [del] {0}" -f $path) -ForegroundColor Yellow
    }
}

# ── Icon path (use exe itself as icon source if no .ico available) ────────────
$IconPath = $BinaryPath + ",0"
$iconDir   = Join-Path (Split-Path $BinaryPath) "..\icons"
foreach ($candidate in @("vsim.ico","vsepr.ico","vsepr-sim.ico")) {
    $ico = Join-Path $iconDir $candidate
    if (Test-Path $ico) { $IconPath = $ico + ",0"; break }
}

# ─────────────────────────────────────────────────────────────────────────────
# UNREGISTER path
# ─────────────────────────────────────────────────────────────────────────────
if ($Unregister) {
    Write-Host "Removing VSEPR-SIM file associations..." -ForegroundColor Cyan

    # .vsim — remove ProgID and extension mapping
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\VSIMFile"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.vsim"

    # .vsxyz — remove ProgID and extension mapping
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\VSIMVSXYZFile"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.vsxyz"

    # .xyzFull / .xyzfull — remove ProgID and extension mapping
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\XYZFullFile"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.xyzFull"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.xyzfull"

    # .xyz — remove only VSEPR's context-menu verb; never touch the default
    $xyzContextKey = "HKEY_CURRENT_USER\Software\Classes\.xyz\shell\OpenWithVSEPRSIM"
    Reg-Del $xyzContextKey

    # .xyza / .xyzA / .xyzc / .xyzf / .xyzF — full takeover ProgIDs
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\VSIMXYZAFile"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.xyza"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.xyzA"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\VSIMXYZCFile"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.xyzc"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\VSIMXYZFFile"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.xyzf"
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.xyzF"

    # Notify shell
    if ([System.Environment]::OSVersion.Platform -eq "Win32NT") {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class ShellNotify {
    [DllImport("shell32.dll")] public static extern void SHChangeNotify(
        int wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@ -ErrorAction SilentlyContinue
        try { [ShellNotify]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero) }
        catch { }
    }

    Write-Host "File associations removed." -ForegroundColor Green
    return
}

# ─────────────────────────────────────────────────────────────────────────────
# REGISTER path
# ─────────────────────────────────────────────────────────────────────────────
Write-Host "Registering VSEPR-SIM file associations..." -ForegroundColor Cyan
Write-Host ("  Binary: {0}" -f $BinaryPath)
Write-Host ""

# ── .vsim — full takeover ─────────────────────────────────────────────────────
Write-Host "[1/3] .vsim  (VSIM script — full default takeover)"

# ProgID
$vsimProg = "HKEY_CURRENT_USER\Software\Classes\VSIMFile"
Reg-Set $vsimProg                          "(default)"    "VSIM Script"
Reg-Set "$vsimProg\DefaultIcon"            "(default)"    $IconPath
Reg-Set "$vsimProg\shell"                  "(default)"    "Open"

# Open verb (run the script)
Reg-Set "$vsimProg\shell\Open"             "(default)"    "Open with VSEPR-SIM"
Reg-Set "$vsimProg\shell\Open\command"     "(default)"    "$ExeQ run `"%1`""

# Additional verbs
Reg-Set "$vsimProg\shell\Validate"         "(default)"    "Validate VSIM Script"
Reg-Set "$vsimProg\shell\Validate\command" "(default)"    "$ExeQ validate `"%1`""

Reg-Set "$vsimProg\shell\Inspect"          "(default)"    "Inspect with VSEPR-SIM"
Reg-Set "$vsimProg\shell\Inspect\command"  "(default)"    "$ExeQ inspect `"%1`""

# Extension → ProgID mapping (sets double-click default)
$vsimExt = "HKEY_CURRENT_USER\Software\Classes\.vsim"
Reg-Set $vsimExt "(default)" "VSIMFile"

Write-Host "  [ok] .vsim → VSIMFile (double-click runs script)" -ForegroundColor Green

# ── .xyzFull — full takeover, replay viewer ───────────────────────────────────
Write-Host "[2/3] .xyzFull  (VSEPR replay file — full default takeover)"

$fullProg = "HKEY_CURRENT_USER\Software\Classes\XYZFullFile"
Reg-Set $fullProg                           "(default)"   "VSEPR Replay File"
Reg-Set "$fullProg\DefaultIcon"             "(default)"   $IconPath
Reg-Set "$fullProg\shell"                   "(default)"   "Open"
Reg-Set "$fullProg\shell\Open"              "(default)"   "Open in VSEPR Replay Viewer"
Reg-Set "$fullProg\shell\Open\command"      "(default)"   "$OpenerQ `"%1`""
Reg-Set "$fullProg\shell\Inspect"           "(default)"   "Inspect with VSEPR-SIM"
Reg-Set "$fullProg\shell\Inspect\command"   "(default)"   "$ExeQ inspect `"%1`""

$fullExt = "HKEY_CURRENT_USER\Software\Classes\.xyzFull"
Reg-Set $fullExt "(default)" "XYZFullFile"
Reg-Set "HKEY_CURRENT_USER\Software\Classes\.xyzfull" "(default)" "XYZFullFile"

Write-Host "  [ok] .xyzFull/.xyzfull → XYZFullFile (double-click opens replay viewer)" -ForegroundColor Green

# ── .vsxyz — VSEPR native coordinate format, full takeover ────────────────────────────
Write-Host "[2b] .vsxyz  (VSEPR native XYZ — full default takeover)"

$vsxyzProg = "HKEY_CURRENT_USER\Software\Classes\VSIMVSXYZFile"
Reg-Set $vsxyzProg                           "(default)"   "VSEPR-SIM Coordinate File"
Reg-Set "$vsxyzProg\DefaultIcon"             "(default)"   $IconPath
Reg-Set "$vsxyzProg\shell"                   "(default)"   "Open"
Reg-Set "$vsxyzProg\shell\Open"              "(default)"   "Open with VSEPR-SIM"
Reg-Set "$vsxyzProg\shell\Open\command"      "(default)"   "$OpenerQ \"%1\""
Reg-Set "$vsxyzProg\shell\Inspect"           "(default)"   "Inspect with VSEPR-SIM"
Reg-Set "$vsxyzProg\shell\Inspect\command"   "(default)"   "$ExeQ inspect \"%1\""

Reg-Set "HKEY_CURRENT_USER\Software\Classes\.vsxyz" "(default)" "VSIMVSXYZFile"
Write-Host "  [ok] .vsxyz → VSIMVSXYZFile" -ForegroundColor Green

# ── .xyz — context-menu only, NO default takeover ────────────────────────────
Write-Host "[3/3] .xyz  (standard XYZ — context-menu 'Open with' only)"
#
# Strategy: write our verb ONLY under HKCU\Software\Classes\.xyz\shell\
# The .xyz extension already has a user or system default (e.g. Notepad,
# VESTA, OVITO).  We must not touch:
#   HKCU\Software\Classes\.xyz\(default)         ← leaves existing ProgID alone
#
# What we add:
#   HKCU\Software\Classes\.xyz\shell\OpenWithVSEPRSIM\
#       (default)  = "Open with VSEPR-SIM"
#       command\(default) = "vsepr-sim.exe" open "%1"
#
$xyzVerb = "HKEY_CURRENT_USER\Software\Classes\.xyz\shell\OpenWithVSEPRSIM"
Reg-Set $xyzVerb                    "(default)"  "Open with VSEPR-SIM"
Reg-Set "$xyzVerb\command"          "(default)"  "$OpenerQ `"%1`""

Write-Host "  [ok] .xyz — context-menu 'Open with VSEPR-SIM' added" -ForegroundColor Green
Write-Host "  [ok] .xyz — existing default handler is UNCHANGED" -ForegroundColor Green

# ── .xyza — enriched atomistic frame (full takeover) ─────────────────────────
Write-Host "[4/6] .xyza / .xyzA  (VSIM enriched atomistic frame — full takeover)"

$xyzaProg = "HKEY_CURRENT_USER\Software\Classes\VSIMXYZAFile"
Reg-Set $xyzaProg                          "(default)"    "VSIM Enriched Atomistic Frame"
Reg-Set "$xyzaProg\DefaultIcon"            "(default)"    $IconPath
Reg-Set "$xyzaProg\shell"                  "(default)"    "Open"
Reg-Set "$xyzaProg\shell\Open"             "(default)"    "Open with VSEPR-SIM"
Reg-Set "$xyzaProg\shell\Open\command"     "(default)"    "$OpenerQ `"%1`""
Reg-Set "$xyzaProg\shell\Inspect"          "(default)"    "Inspect with VSEPR-SIM"
Reg-Set "$xyzaProg\shell\Inspect\command"  "(default)"    "$ExeQ inspect `"%1`""

foreach ($ext in @(".xyza", ".xyzA")) {
    Reg-Set "HKEY_CURRENT_USER\Software\Classes\$ext" "(default)" "VSIMXYZAFile"
}
Write-Host "  [ok] .xyza/.xyzA → VSIMXYZAFile" -ForegroundColor Green

# ── .xyzc — restartable checkpoint (full takeover) ───────────────────────────
Write-Host "[5/6] .xyzc  (VSIM restartable checkpoint — full takeover)"

$xyzcProg = "HKEY_CURRENT_USER\Software\Classes\VSIMXYZCFile"
Reg-Set $xyzcProg                          "(default)"    "VSIM Checkpoint File"
Reg-Set "$xyzcProg\DefaultIcon"            "(default)"    $IconPath
Reg-Set "$xyzcProg\shell"                  "(default)"    "Open"
Reg-Set "$xyzcProg\shell\Open"             "(default)"    "Open with VSEPR-SIM"
Reg-Set "$xyzcProg\shell\Open\command"     "(default)"    "$OpenerQ `"%1`""
Reg-Set "$xyzcProg\shell\Inspect"          "(default)"    "Inspect with VSEPR-SIM"
Reg-Set "$xyzcProg\shell\Inspect\command"  "(default)"    "$ExeQ inspect `"%1`""

Reg-Set "HKEY_CURRENT_USER\Software\Classes\.xyzc" "(default)" "VSIMXYZCFile"
Write-Host "  [ok] .xyzc → VSIMXYZCFile" -ForegroundColor Green

# ── .xyzf — multi-frame trajectory (full takeover) ───────────────────────────
Write-Host "[6/6] .xyzf / .xyzF  (VSIM multi-frame trajectory — full takeover)"

$xyzfProg = "HKEY_CURRENT_USER\Software\Classes\VSIMXYZFFile"
Reg-Set $xyzfProg                          "(default)"    "VSIM Trajectory File"
Reg-Set "$xyzfProg\DefaultIcon"            "(default)"    $IconPath
Reg-Set "$xyzfProg\shell"                  "(default)"    "Open"
Reg-Set "$xyzfProg\shell\Open"             "(default)"    "Open with VSEPR-SIM"
Reg-Set "$xyzfProg\shell\Open\command"     "(default)"    "$OpenerQ `"%1`""
Reg-Set "$xyzfProg\shell\Inspect"          "(default)"    "Inspect with VSEPR-SIM"
Reg-Set "$xyzfProg\shell\Inspect\command"  "(default)"    "$ExeQ inspect `"%1`""

foreach ($ext in @(".xyzf", ".xyzF")) {
    Reg-Set "HKEY_CURRENT_USER\Software\Classes\$ext" "(default)" "VSIMXYZFFile"
}
Write-Host "  [ok] .xyzf/.xyzF → VSIMXYZFFile" -ForegroundColor Green

# ── Notify Windows Shell ──────────────────────────────────────────────────────
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class ShellNotify2 {
    [DllImport("shell32.dll")] public static extern void SHChangeNotify(
        int wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@ -ErrorAction SilentlyContinue
try { [ShellNotify2]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero) }
catch { }

Write-Host ""
Write-Host "File associations registered." -ForegroundColor Green
Write-Host "  .vsim               -> double-click runs VSIM script"
Write-Host "  .xyzFull/.xyzfull   -> double-click: priority opener (3-D viewer/CLI/popup)"
Write-Host "  .vsxyz              -> double-click: priority opener (3-D viewer/CLI/popup)"
Write-Host "  .xyza/.xyzA         -> double-click: priority opener"
Write-Host "  .xyzc               -> double-click: priority opener"
Write-Host "  .xyzf/.xyzF         -> double-click: priority opener"
Write-Host "  .xyz                -> right-click: 'Open with VSEPR-SIM' (default unchanged)"
Write-Host "  Opener priority: vsepr-sim.exe -> vsepr.exe -> pythonw popup" -ForegroundColor Gray
Write-Host ""
