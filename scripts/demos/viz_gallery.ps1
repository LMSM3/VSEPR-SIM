#!/usr/bin/env pwsh
<#
.SYNOPSIS
    VSEPR-Sim CG Visualization Gallery — CLI-driven viewer orchestration

.DESCRIPTION
    Locates the vsepr executable and launches a curated set of lightweight
    bead visualization windows via the CLI --viz interface.

    Each demo window is a live GLFW/ImGui viewer showing a different scene
    configuration, scene geometry, or environment state overlay.

    Three launch modes:
      Sequential  (default) — open one viewer at a time; the next opens
                              when you close the current one.
      Parallel               — fire every window simultaneously in background.
      List                   — print the full demo catalog and exit.

.PARAMETER Mode
    Launch mode: Sequential (default), Parallel, or List.

.PARAMETER Category
    Limit launch to a category: Geometry, Overlay, Environment, or All (default).

.PARAMETER Exe
    Override the path to vsepr.exe. Auto-detected when not specified.

.PARAMETER EnvSteps
    Override the number of environment relaxation steps used for overlay demos.
    Default: 250.

.PARAMETER Help
    Print this help and exit.

.EXAMPLE
    .\scripts\demos\viz_gallery.ps1
    .\scripts\demos\viz_gallery.ps1 -Mode Parallel
    .\scripts\demos\viz_gallery.ps1 -Category Overlay
    .\scripts\demos\viz_gallery.ps1 -Category Environment -Mode Parallel
    .\scripts\demos\viz_gallery.ps1 -Mode List
#>

param(
    [ValidateSet('Sequential','Parallel','List')]
    [string]$Mode = 'Sequential',

    [ValidateSet('All','Geometry','Overlay','Environment')]
    [string]$Category = 'All',

    [string]$Exe = '',

    [int]$EnvSteps = 250,

    [Alias('h')]
    [switch]$Help
)

if ($Help) {
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 0
}

$ErrorActionPreference = 'Stop'

# ============================================================================
# Banner
# ============================================================================

Write-Host ''
Write-Host '╔═══════════════════════════════════════════════════════════════════════╗' -ForegroundColor Magenta
Write-Host '║              VSEPR-Sim  CG Visualization Gallery                     ║' -ForegroundColor Magenta
Write-Host '║              Lightweight Bead Viewer — CLI Orchestration             ║' -ForegroundColor Magenta
Write-Host '╚═══════════════════════════════════════════════════════════════════════╝' -ForegroundColor Magenta
Write-Host ''

# ============================================================================
# Locate vsepr executable
# ============================================================================

function Find-VseprExe {
    param([string]$Override)

    if ($Override -ne '' -and (Test-Path $Override)) {
        return $Override
    }

    $projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

    $candidates = @(
        (Join-Path $projectRoot 'build\Release\vsepr.exe'),
        (Join-Path $projectRoot 'build\Debug\vsepr.exe'),
        (Join-Path $projectRoot 'build\bin\vsepr.exe'),
        (Join-Path $projectRoot 'build\vsepr.exe'),
        (Join-Path $projectRoot 'out\build\x64-Release\vsepr.exe'),
        (Join-Path $projectRoot 'out\build\x64-Debug\vsepr.exe')
    )

    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }

    # Last resort: PATH
    $found = Get-Command 'vsepr.exe' -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }

    return $null
}

$vsepr = Find-VseprExe -Override $Exe

if (-not $vsepr) {
    Write-Host '✗ vsepr.exe not found.' -ForegroundColor Red
    Write-Host '  Build the project first:' -ForegroundColor Yellow
    Write-Host '    .\scripts\build\build.ps1 -BuildType Release' -ForegroundColor Yellow
    Write-Host '  or specify the path with -Exe:' -ForegroundColor Yellow
    Write-Host '    .\scripts\demos\viz_gallery.ps1 -Exe C:\path\to\vsepr.exe' -ForegroundColor Yellow
    Write-Host ''
    exit 1
}

Write-Host "✓ Executable : $vsepr" -ForegroundColor Green

# Quick sanity check — verify visualization is compiled in
$helpOutput = & $vsepr --help 2>&1
if ($helpOutput -notmatch '--viz') {
    Write-Host ''
    Write-Host '✗ This build does not expose --viz (BUILD_VIS may be OFF).' -ForegroundColor Red
    Write-Host '  Rebuild with visualization enabled:' -ForegroundColor Yellow
    Write-Host '    cmake .. -DBUILD_APPS=ON -DBUILD_VIS=ON' -ForegroundColor Yellow
    Write-Host ''
    exit 1
}

Write-Host '✓ Visualization : available' -ForegroundColor Green

# ============================================================================
# Demo Catalog
# ============================================================================
# Each entry is a hashtable:
#   Title      — short display name
#   Category   — Geometry | Overlay | Environment
#   Route      — 'cg' (vsepr cg viz ...) or 'top' (vsepr --viz ...)
#   Args       — array of CLI arguments after the route keyword
#   Desc       — one-line description shown in the gallery listing

$demos = @(

    # ------------------------------------------------------------------
    # GEOMETRY — pure structural scenes, no environment relaxation
    # ------------------------------------------------------------------

    [ordered]@{
        Title    = 'Isolated bead'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','isolated')
        Desc     = 'Single bead at origin. Verifies scene load and camera centering.'
    },

    [ordered]@{
        Title    = 'Pair — contact range'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','pair','--spacing','2.5')
        Desc     = 'Two beads at 2.5 Å. Near-contact geometry with visible axis glyphs.'
    },

    [ordered]@{
        Title    = 'Pair — equilibrium'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','pair','--spacing','4.0')
        Desc     = 'Two beads at 4.0 Å. Canonical pair reference geometry.'
    },

    [ordered]@{
        Title    = 'Pair — long range'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','pair','--spacing','9.0')
        Desc     = 'Two beads at 9.0 Å. Tests camera auto-distance for sparse systems.'
    },

    [ordered]@{
        Title    = 'Linear stack (5 beads)'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','stack','--beads','5','--spacing','3.5')
        Desc     = '5-bead chain along Z at 3.5 Å spacing. All axes aligned.'
    },

    [ordered]@{
        Title    = 'Linear stack (10 beads)'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','stack','--beads','10','--spacing','3.8')
        Desc     = '10-bead stack at 3.8 Å. Longer chain for axis-glyph density check.'
    },

    [ordered]@{
        Title    = 'T-shape'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','tshape','--spacing','4.0')
        Desc     = '3-bead T-configuration. Central bead at origin, mixed orientations.'
    },

    [ordered]@{
        Title    = 'Square (4 beads)'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','square','--spacing','5.0')
        Desc     = '4 beads in the XY plane, 5.0 Å side. Planar ring analogue.'
    },

    [ordered]@{
        Title    = 'Dense shell (8 neighbours)'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','8','--spacing','5.0')
        Desc     = 'Central bead + 8-member Fibonacci-distributed shell at 5.0 Å.'
    },

    [ordered]@{
        Title    = 'Dense shell (12 neighbours)'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0')
        Desc     = 'Central bead + 12-member shell. FCC-like first coordination shell.'
    },

    [ordered]@{
        Title    = 'Dense shell (20 neighbours)'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','20','--spacing','5.5')
        Desc     = 'Central bead + 20-member shell. High-coordination environment test.'
    },

    [ordered]@{
        Title    = 'Random cloud (15 beads)'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','cloud','--beads','15','--spacing','12.0','--seed','42')
        Desc     = '15 randomly placed beads in a 12 Å box, seed 42. Reproducible.'
    },

    [ordered]@{
        Title    = 'Random cloud (30 beads)'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','cloud','--beads','30','--spacing','18.0','--seed','7')
        Desc     = '30-bead random cluster, seed 7. Tests sparse-to-dense transitions.'
    },

    [ordered]@{
        Title    = 'Shell — no axes'
        Category = 'Geometry'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0','--no-axes')
        Desc     = '12-bead shell with orientation axes hidden. Clean bead-only view.'
    },

    # ------------------------------------------------------------------
    # OVERLAY — scalar state mapped to bead color, minimal env relaxation
    # ------------------------------------------------------------------

    [ordered]@{
        Title    = 'Shell — local density (rho)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0',
                     '--env-steps',$EnvSteps,'--overlay','rho')
        Desc     = "Shell + $EnvSteps env steps. Bead color encodes local density rho_B."
    },

    [ordered]@{
        Title    = 'Shell — coordination (C)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0',
                     '--env-steps',$EnvSteps,'--overlay','C')
        Desc     = "Shell + $EnvSteps steps. Bead color encodes coordination number C_B."
    },

    [ordered]@{
        Title    = 'Shell — orientational order (P2)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0',
                     '--env-steps',$EnvSteps,'--overlay','P2')
        Desc     = "Shell + $EnvSteps steps. Nematic-like P2 order parameter per bead."
    },

    [ordered]@{
        Title    = 'Shell — memory state (eta)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0',
                     '--env-steps',$EnvSteps,'--overlay','eta')
        Desc     = "Shell + $EnvSteps steps. Environment memory eta_B. Cool=low, warm=high."
    },

    [ordered]@{
        Title    = 'Shell — formation driving force (target_f)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0',
                     '--env-steps',$EnvSteps,'--overlay','target_f')
        Desc     = "Shell + $EnvSteps steps. Per-bead target formation factor."
    },

    [ordered]@{
        Title    = 'Stack — coordination (C)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','stack','--beads','8','--spacing','3.8',
                     '--env-steps',$EnvSteps,'--overlay','C')
        Desc     = "8-bead stack + $EnvSteps steps. End beads vs interior beads in C."
    },

    [ordered]@{
        Title    = 'Stack — memory state (eta)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','stack','--beads','8','--spacing','3.8',
                     '--env-steps',$EnvSteps,'--overlay','eta')
        Desc     = "8-bead stack + $EnvSteps steps. eta gradient along the chain axis."
    },

    [ordered]@{
        Title    = 'Cloud (20) — density (rho)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','cloud','--beads','20','--spacing','14.0',
                     '--seed','42','--env-steps',$EnvSteps,'--overlay','rho')
        Desc     = "20-bead cloud + $EnvSteps steps. Sparse vs dense pockets in rho."
    },

    [ordered]@{
        Title    = 'T-shape — memory state (eta)'
        Category = 'Overlay'
        Route    = 'cg'
        Args     = @('--preset','tshape','--spacing','4.0',
                     '--env-steps',$EnvSteps,'--overlay','eta')
        Desc     = "T-shape + $EnvSteps steps. Anisotropic env: axial vs lateral bead."
    },

    # ------------------------------------------------------------------
    # ENVIRONMENT — deep relaxation runs, large step counts
    # ------------------------------------------------------------------

    [ordered]@{
        Title    = 'Shell (20) — deep eta relaxation'
        Category = 'Environment'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','20','--spacing','5.0',
                     '--env-steps',600,'--tau','50','--overlay','eta')
        Desc     = '20-bead shell, 600 steps, tau=50 fs. Full eta convergence visible.'
    },

    [ordered]@{
        Title    = 'Shell (20) — deep P2 relaxation'
        Category = 'Environment'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','20','--spacing','5.0',
                     '--env-steps',600,'--tau','50','--overlay','P2')
        Desc     = '20-bead shell, 600 steps, tau=50 fs. Nematic P2 at convergence.'
    },

    [ordered]@{
        Title    = 'Cloud (30) — deep density relaxation'
        Category = 'Environment'
        Route    = 'cg'
        Args     = @('--preset','cloud','--beads','30','--spacing','16.0',
                     '--seed','99','--env-steps',800,'--tau','80','--overlay','rho')
        Desc     = '30-bead cloud, 800 steps, tau=80 fs. Heterogeneous rho landscape.'
    },

    [ordered]@{
        Title    = 'Stack (12) — long chain, C overlay'
        Category = 'Environment'
        Route    = 'cg'
        Args     = @('--preset','stack','--beads','12','--spacing','3.5',
                     '--env-steps',500,'--overlay','C')
        Desc     = '12-bead chain, 500 steps. Bulk interior vs terminal C distinction.'
    },

    [ordered]@{
        Title    = 'Shell (12) — fast tau, rho convergence'
        Category = 'Environment'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0',
                     '--env-steps',1000,'--tau','20','--overlay','rho')
        Desc     = '12-bead shell, 1000 steps, tau=20 fs. Rapid damping to equilibrium.'
    },

    [ordered]@{
        Title    = 'Shell (12) — slow tau, eta convergence'
        Category = 'Environment'
        Route    = 'cg'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0',
                     '--env-steps',1000,'--tau','500','--overlay','eta')
        Desc     = '12-bead shell, 1000 steps, tau=500 fs. Slow memory accumulation.'
    },

    # ------------------------------------------------------------------
    # Top-level --viz route (exercises the vsepr --viz entry point)
    # ------------------------------------------------------------------

    [ordered]@{
        Title    = '[top] Pair — default view'
        Category = 'Geometry'
        Route    = 'top'
        Args     = @('--preset','pair','--spacing','4.0')
        Desc     = 'Top-level vsepr --viz entry point. Pair scene, default config.'
    },

    [ordered]@{
        Title    = '[top] Shell — eta overlay'
        Category = 'Overlay'
        Route    = 'top'
        Args     = @('--preset','shell','--beads','12','--spacing','5.0',
                     '--env-steps',$EnvSteps,'--overlay','eta')
        Desc     = "Top-level vsepr --viz. Shell + $EnvSteps steps, eta overlay."
    }
)

# ============================================================================
# Filtering
# ============================================================================

$filtered = if ($Category -eq 'All') {
    $demos
} else {
    $demos | Where-Object { $_['Category'] -eq $Category }
}

# ============================================================================
# List mode
# ============================================================================

if ($Mode -eq 'List') {
    Write-Host "Demo catalog  ($($filtered.Count) entries, category: $Category)" -ForegroundColor Cyan
    Write-Host ''

    $pad = 40
    $idx = 1
    foreach ($d in $filtered) {
        $label = "[$idx] $($d['Title'])"
        Write-Host ('  {0,-45}  {1}' -f $label, $d['Desc']) -ForegroundColor White
        $idx++
    }
    Write-Host ''
    Write-Host "  Run with -Mode Sequential or -Mode Parallel to launch." -ForegroundColor Gray
    Write-Host ''
    exit 0
}

# ============================================================================
# Build CLI argument list for a demo entry
# ============================================================================

function Build-Args {
    param([hashtable]$Demo)

    if ($Demo['Route'] -eq 'top') {
        # vsepr --viz [args]
        return @('--viz') + $Demo['Args']
    } else {
        # vsepr cg viz [args]
        return @('cg', 'viz') + $Demo['Args']
    }
}

# ============================================================================
# Sequential mode — one window at a time, blocking
# ============================================================================

function Invoke-Sequential {
    param([array]$DemoList)

    $total = $DemoList.Count
    $idx   = 1

    foreach ($d in $DemoList) {
        $cliArgs = Build-Args -Demo $d

        Write-Host ''
        Write-Host "─────────────────────────────────────────────────────────────" -ForegroundColor DarkGray
        Write-Host "  Demo $idx / $total  :  $($d['Title'])" -ForegroundColor Cyan
        Write-Host "  Category    :  $($d['Category'])" -ForegroundColor DarkCyan
        Write-Host "  Description :  $($d['Desc'])" -ForegroundColor White
        Write-Host ''
        Write-Host "  Command     :  vsepr $($cliArgs -join ' ')" -ForegroundColor Yellow
        Write-Host ''
        Write-Host '  ▶ Launching viewer...  (close window to continue)' -ForegroundColor Green

        & $vsepr @cliArgs

        $idx++
    }

    Write-Host ''
    Write-Host '─────────────────────────────────────────────────────────────' -ForegroundColor DarkGray
    Write-Host "✓ Gallery complete — $total demos shown." -ForegroundColor Green
    Write-Host ''
}

# ============================================================================
# Parallel mode — all windows simultaneously, background processes
# ============================================================================

function Invoke-Parallel {
    param([array]$DemoList)

    Write-Host ''
    Write-Host "  Launching $($DemoList.Count) viewer windows in parallel..." -ForegroundColor Cyan
    Write-Host "  Close any window individually. Use the title bar to identify each." -ForegroundColor Gray
    Write-Host ''

    $procs = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
    $idx   = 1

    foreach ($d in $DemoList) {
        $cliArgs = Build-Args -Demo $d
        $label   = "[$idx] $($d['Title'])"

        Write-Host ("  {0,-50}  {1}" -f $label, ($cliArgs -join ' ')) -ForegroundColor Yellow

        $p = Start-Process -FilePath $vsepr `
                           -ArgumentList $cliArgs `
                           -PassThru `
                           -WindowStyle Normal
        $procs.Add($p)
        $idx++

        # Stagger launches slightly to avoid GLFW context race on init
        Start-Sleep -Milliseconds 120
    }

    Write-Host ''
    Write-Host "✓ $($procs.Count) viewers launched." -ForegroundColor Green
    Write-Host "  Waiting for all windows to close..." -ForegroundColor Gray
    Write-Host ''

    foreach ($p in $procs) {
        if (-not $p.HasExited) { $p.WaitForExit() }
    }

    Write-Host "✓ All viewers closed." -ForegroundColor Green
    Write-Host ''
}

# ============================================================================
# Launch
# ============================================================================

Write-Host "  Mode     : $Mode" -ForegroundColor White
Write-Host "  Category : $Category" -ForegroundColor White
Write-Host "  Demos    : $($filtered.Count)" -ForegroundColor White
Write-Host "  EnvSteps : $EnvSteps" -ForegroundColor White
Write-Host ''

if ($filtered.Count -eq 0) {
    Write-Host '⚠  No demos match the selected category.' -ForegroundColor Yellow
    Write-Host "   Run with -Mode List to see available entries." -ForegroundColor Gray
    exit 0
}

switch ($Mode) {
    'Sequential' { Invoke-Sequential -DemoList $filtered }
    'Parallel'   { Invoke-Parallel   -DemoList $filtered }
}

exit 0
