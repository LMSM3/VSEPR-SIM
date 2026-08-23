<#
.SYNOPSIS
    register-file-associations.ps1
    Windows shell integration for VSEPR-SIM file types.

.DESCRIPTION
    Registers file-type behaviours under HKCU (no admin required).

    .vsim          -- Full takeover.  Double-click: vsepr run "%1"
                     Verbs: Open, Validate, Inspect

    .dynx          -- Full takeover.  Double-click: vsepr view "%1"    (WO-72B)
                     Verbs: Open (session archive viewer), Validate, Inspect

    .X             -- Full takeover.  Double-click: vsepr x run "%1"   (WO-72A)
                     Verbs: Open (run suite), Inspect, Validate

    .xyzFull       -- Full takeover.  Double-click: vsepr view "%1"
                     Verbs: Open (replay viewer), Inspect

    .vsxyz         -- Full takeover.  vsepr open "%1"
    .xyza/.xyzA    -- Full takeover.  vsepr open "%1"
    .xyzc          -- Full takeover.  vsepr open "%1"
    .xyzf/.xyzF    -- Full takeover.  vsepr open "%1"

    .xyz           -- CONSERVATIVE.  Adds context-menu "Open with VSEPR-SIM" only.
                     The user's existing default handler is PRESERVED.

.PARAMETER BinaryPath
    Full path to vsepr.exe.
    Default: %LOCALAPPDATA%\VSEPR-SIM\bin\vsepr.exe

.PARAMETER Unregister
    Remove all VSEPR-SIM file associations.

.PARAMETER DryRun
    Print what would be registered without touching the registry.

    Also invocable as: vsepr install register-associations --dry-run

v5.1.4 | WO-72C
#>

[CmdletBinding()]
param(
    [string] $BinaryPath = "",
    [switch] $Unregister,
    [switch] $DryRun
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

function Shell-Notify {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class ShellNotify3 {
    [DllImport("shell32.dll")] public static extern void SHChangeNotify(
        int wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@ -ErrorAction SilentlyContinue
    try { [ShellNotify3]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero) } catch { }
}

# Registers a full-takeover association:
#   ProgID, DefaultIcon, default verb, Open command, optional extra verbs,
#   and maps all listed extensions to the ProgID.
function Register-FullTakeover {
    param(
        [string]   $ProgId,
        [string]   $Description,
        [string]   $DefaultVerb,
        [string]   $OpenLabel,
        [string]   $OpenCommand,
        [string[]] $Extensions,
        [hashtable]$ExtraVerbs = @{}
    )
    $base = "HKEY_CURRENT_USER\Software\Classes\$ProgId"
    if ($DryRun) {
        Write-Host ("  [dry] ProgID={0}  extensions={1}" -f $ProgId, ($Extensions -join ",")) -ForegroundColor DarkGray
        return
    }
    Reg-Set $base                          "(default)"  $Description
    Reg-Set "$base\DefaultIcon"            "(default)"  $IconPath
    Reg-Set "$base\shell"                  "(default)"  $DefaultVerb
    Reg-Set "$base\shell\Open"             "(default)"  $OpenLabel
    Reg-Set "$base\shell\Open\command"     "(default)"  $OpenCommand
    foreach ($verbName in $ExtraVerbs.Keys) {
        $vd = $ExtraVerbs[$verbName]
        Reg-Set "$base\shell\$verbName"             "(default)"  $vd.Label
        Reg-Set "$base\shell\$verbName\command"     "(default)"  $vd.Command
    }
    foreach ($ext in $Extensions) {
        Reg-Set "HKEY_CURRENT_USER\Software\Classes\$ext" "(default)" $ProgId
    }
}

# ── Icon path (use exe itself as icon source if no .ico available) ────────────
# -- Launcher: open_vsim_file.cmd routes double-clicks to mol_viewer.py -------
$LauncherCmd = Join-Path $PSScriptRoot "bin\open_vsim_file.cmd"
if (-not (Test-Path $LauncherCmd)) {
    $LauncherCmd = Join-Path (Split-Path $BinaryPath) "open_vsim_file.cmd"
}
$LaunchQ = "`"$LauncherCmd`""
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

    foreach ($prog in @("VSIMFile","VSIMDynxFile","VSIMXFile","XYZFullFile",
                        "VSIMVSXYZFile","VSIMXYZAFile","VSIMXYZCFile","VSIMXYZFFile")) {
        Reg-Del "HKEY_CURRENT_USER\Software\Classes\$prog"
    }
    foreach ($ext in @(".vsim",".dynx",".X",".xyzFull",".xyzfull",
                       ".vsxyz",".xyza",".xyzA",".xyzc",".xyzf",".xyzF")) {
        Reg-Del "HKEY_CURRENT_USER\Software\Classes\$ext"
    }
    Reg-Del "HKEY_CURRENT_USER\Software\Classes\.xyz\shell\OpenWithVSEPRSIM"

    if (-not $DryRun) { Shell-Notify }
    Write-Host "File associations removed." -ForegroundColor Green
    return
}

# ============================================================================
# REGISTER path
# ============================================================================
if ($DryRun) {
    Write-Host "DRY RUN -- no registry changes will be made." -ForegroundColor Yellow
    Write-Host ("  Binary would be: {0}" -f $BinaryPath)
    Write-Host ""
} else {
    Write-Host "Registering VSEPR-SIM file associations..." -ForegroundColor Cyan
    Write-Host ("  Binary: {0}" -f $BinaryPath)
    Write-Host ""
}

# -- 1/9  .vsim ---------------------------------------------------------------
Write-Host "[1/9] .vsim  (VSIM script -- full default takeover)"
Register-FullTakeover `
    -ProgId      "VSIMFile" `
    -Description "VSIM Script" `
    -DefaultVerb "Open" `
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" `
    -OpenCommand "$LaunchQ `"%1`"" `
    -Extensions  @(".vsim") `
    -ExtraVerbs  @{
        Validate = @{ Label="Validate VSIM Script";   Command="$ExeQ validate `"%1`"" }
        Inspect  = @{ Label="Inspect with VSEPR-SIM"; Command="$ExeQ inspect `"%1`"" }
    }
Write-Host "  [ok] .vsim -> VSIMFile  (double-click runs script)" -ForegroundColor Green

# -- 2/9  .dynx  (WO-72B) ----------------------------------------------------
Write-Host "[2/9] .dynx  (VSIM dynamic session archive -- full takeover)"
Register-FullTakeover `
    -ProgId      "VSIMDynxFile" `
    -Description "VSIM Session Archive" `
    -DefaultVerb "Open" `
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" `
    -OpenCommand "`$LaunchQ `"%1`"" `
    -Extensions  @(".dynx") `
    -ExtraVerbs  @{
        Validate = @{ Label="Validate Session Archive"; Command="$ExeQ dynx validate `"%1`"" }
        Inspect  = @{ Label="Inspect with VSEPR-SIM";   Command="$ExeQ dynx inspect `"%1`"" }
    }
Write-Host "  [ok] .dynx -> VSIMDynxFile  (double-click opens session archive viewer)" -ForegroundColor Green

# -- 3/9  .X  (WO-72A) -------------------------------------------------------
Write-Host "[3/9] .X  (VSIM suite bundle -- full takeover)"
Register-FullTakeover `
    -ProgId      "VSIMXFile" `
    -Description "VSIM Suite Bundle" `
    -DefaultVerb "Open" `
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" `
    -OpenCommand "`$LaunchQ `"%1`"" `
    -Extensions  @(".X") `
    -ExtraVerbs  @{
        Inspect  = @{ Label="Inspect Suite";  Command="$ExeQ x inspect `"%1`"" }
        Validate = @{ Label="Validate Suite"; Command="$ExeQ x validate `"%1`"" }
    }
Write-Host "  [ok] .X -> VSIMXFile  (double-click runs suite)" -ForegroundColor Green

# -- 4/9  .xyzFull -----------------------------------------------------------
Write-Host "[4/9] .xyzFull  (VSEPR replay file -- full takeover)"
Register-FullTakeover `
    -ProgId      "XYZFullFile" `
    -Description "VSEPR Replay File" `
    -DefaultVerb "Open" `
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" `
    -OpenCommand "`$LaunchQ `"%1`"" `
    -Extensions  @(".xyzFull", ".xyzfull") `
    -ExtraVerbs  @{
        Inspect = @{ Label="Inspect with VSEPR-SIM"; Command="$ExeQ inspect `"%1`"" }
    }
Write-Host "  [ok] .xyzFull/.xyzfull -> XYZFullFile  (double-click opens replay viewer)" -ForegroundColor Green

# -- 5/9  .xyz  (context-menu ONLY) ------------------------------------------
Write-Host "[5/9] .xyz  (standard XYZ -- context-menu only, no default hijack)"
$xyzVerb = "HKEY_CURRENT_USER\Software\Classes\.xyz\shell\OpenWithVSEPRSIM"
Reg-Set $xyzVerb            "(default)"  "Open with VSEPR-SIM"
Reg-Set "$xyzVerb\command"  "(default)"  "$ExeQ open `"%1`""
Write-Host "  [ok] .xyz -- context-menu added" -ForegroundColor Green
Write-Host "  [ok] .xyz -- existing default handler is UNCHANGED" -ForegroundColor Green

# -- 6/9  .vsxyz -------------------------------------------------------------
Write-Host "[6/9] .vsxyz  (VSEPR native coordinate file -- full takeover)"
Register-FullTakeover `
    -ProgId      "VSIMVSXYZFile" `
    -Description "VSEPR-SIM Coordinate File" `
    -DefaultVerb "Open" `
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" `
    -OpenCommand "`$LaunchQ `"%1`"" `
    -Extensions  @(".vsxyz") `
    -ExtraVerbs  @{
        Inspect = @{ Label="Inspect with VSEPR-SIM"; Command="$ExeQ inspect `"%1`"" }
    }
Write-Host "  [ok] .vsxyz -> VSIMVSXYZFile" -ForegroundColor Green

# -- 7/9  .xyza / .xyzA ------------------------------------------------------
Write-Host "[7/9] .xyza / .xyzA  (VSIM enriched atomistic frame -- full takeover)"
Register-FullTakeover `
    -ProgId      "VSIMXYZAFile" `
    -Description "VSIM Enriched Atomistic Frame" `
    -DefaultVerb "Open" `
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" `
    -OpenCommand "`$LaunchQ `"%1`"" `
    -Extensions  @(".xyza", ".xyzA") `
    -ExtraVerbs  @{
        Inspect = @{ Label="Inspect with VSEPR-SIM"; Command="$ExeQ inspect `"%1`"" }
    }
Write-Host "  [ok] .xyza/.xyzA -> VSIMXYZAFile" -ForegroundColor Green

# -- 8/9  .xyzc ---------------------------------------------------------------
Write-Host "[8/9] .xyzc  (VSIM checkpoint -- full takeover)"
Register-FullTakeover `
    -ProgId      "VSIMXYZCFile" `
    -Description "VSIM Checkpoint File" `
    -DefaultVerb "Open" `
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" `
    -OpenCommand "`$LaunchQ `"%1`"" `
    -Extensions  @(".xyzc") `
    -ExtraVerbs  @{
        Inspect = @{ Label="Inspect with VSEPR-SIM"; Command="$ExeQ inspect `"%1`"" }
    }
Write-Host "  [ok] .xyzc -> VSIMXYZCFile" -ForegroundColor Green

# -- 9/9  .xyzf / .xyzF ------------------------------------------------------
Write-Host "[9/9] .xyzf / .xyzF  (VSIM trajectory -- full takeover)"
Register-FullTakeover `
    -ProgId      "VSIMXYZFFile" `
    -Description "VSIM Trajectory File" `
    -DefaultVerb "Open" `
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" `
    -OpenCommand "`$LaunchQ `"%1`"" `
    -Extensions  @(".xyzf", ".xyzF") `
    -ExtraVerbs  @{
        Inspect = @{ Label="Inspect with VSEPR-SIM"; Command="$ExeQ inspect `"%1`"" }
    }
Write-Host "  [ok] .xyzf/.xyzF -> VSIMXYZFFile" -ForegroundColor Green

# -- Notify Windows Shell ----------------------------------------------------
if (-not $DryRun) { Shell-Notify }

Write-Host ""
if ($DryRun) {
    Write-Host "Dry run complete -- no changes made." -ForegroundColor Yellow
} else {
    Write-Host "File associations registered." -ForegroundColor Green
}
Write-Host "  .vsim         -> double-click runs VSIM script"
Write-Host "  .dynx         -> double-click opens session archive viewer  [WO-72B]"
Write-Host "  .X            -> double-click runs suite bundle              [WO-72A]"
Write-Host "  .xyzFull      -> double-click opens VSIM Replay Viewer"
Write-Host "  .vsxyz        -> double-click opens coordinate viewer"
Write-Host "  .xyza/.xyzA   -> double-click opens enriched atomistic preview"
Write-Host "  .xyzc         -> double-click opens checkpoint preview"
Write-Host "  .xyzf/.xyzF   -> double-click opens trajectory preview"
Write-Host "  .xyz          -> right-click: Open with VSEPR-SIM (default unchanged)"
Write-Host ""
