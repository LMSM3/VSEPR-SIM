# vseprw.ps1 - VSEPR-Sim Wrapper (Windows)
# "One command to build/run cleanly, every time"
# 
# Usage: .\vseprw.ps1 H2O relax --watch
#        .\vseprw.ps1 water.xyz sim -temp 300 -steps 1000

param(
    [Parameter(Position=0)]
    [string]$Spec,
    
    [Parameter(Position=1)]
    [string]$Action,
    
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Options,
    
    [switch]$ConfigureOnly,
    [switch]$BuildOnly,
    [switch]$Clean,
    [switch]$Help
)

# ============================================================================
# Configuration
# ============================================================================

$ErrorActionPreference = "Stop"
$REPO_ROOT = $PSScriptRoot
$BUILD_DIR = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $REPO_ROOT "build" }
$GENERATOR = if ($env:CMAKE_GENERATOR) { $env:CMAKE_GENERATOR } else { "Visual Studio 17 2022" }
$BUILD_TYPE = if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { "Release" }

# Tool mapping: ACTION → executable name
$TOOL_MAP = @{
    "relax"    = "meso-relax.exe"
    "sim"      = "meso-sim.exe"
    "discover" = "meso-discover.exe"
    "align"    = "meso-align.exe"
    "build"    = "meso-build.exe"
    "crystal"  = "crystal-viewer.exe"
    "qa"       = "qa_golden_tests.exe"
}

# ============================================================================
# Helper Functions
# ============================================================================

function Write-Info {
    param([string]$Message)
    Write-Host "[vseprw] " -ForegroundColor Blue -NoNewline
    Write-Host $Message
}

function Write-Success {
    param([string]$Message)
    Write-Host "[vseprw] " -ForegroundColor Green -NoNewline
    Write-Host $Message
}

function Write-Warning-Custom {
    param([string]$Message)
    Write-Host "[vseprw] " -ForegroundColor Yellow -NoNewline
    Write-Host $Message
}

function Write-Error-Custom {
    param([string]$Message)
    Write-Host "[vseprw ERROR] " -ForegroundColor Red -NoNewline
    Write-Host $Message
}

function Test-Toolchain {
    Write-Info "Checking toolchain..."
    
    # Check CMake
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        Write-Error-Custom "CMake not found. Install CMake 3.15+"
        exit 1
    }
    
    $cmakeVersion = (cmake --version | Select-Object -First 1).Split()[2]
    Write-Info "CMake: $cmakeVersion"
    
    # Check for Visual Studio or other compiler
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        if ($vsPath) {
            $vsVersion = & $vsWhere -latest -property displayName
            Write-Info "Compiler: $vsVersion"
        }
    } else {
        # Try to find cl.exe or g++
        $cl = Get-Command cl -ErrorAction SilentlyContinue
        $gxx = Get-Command g++ -ErrorAction SilentlyContinue
        
        if ($cl) {
            Write-Info "Compiler: MSVC (cl.exe)"
        } elseif ($gxx) {
            Write-Info "Compiler: GCC (g++.exe)"
        } else {
            Write-Warning-Custom "No compiler detected, CMake will try to find one"
        }
    }
    
    Write-Success "Toolchain ready"
}

function Invoke-Configure {
    if (Test-Path (Join-Path $BUILD_DIR "CMakeCache.txt")) {
        Write-Info "Build directory exists: $BUILD_DIR"
        return
    }
    
    Write-Info "Configuring project..."
    New-Item -ItemType Directory -Path $BUILD_DIR -Force | Out-Null
    
    Push-Location $BUILD_DIR
    try {
        cmake -G $GENERATOR `
              -DCMAKE_BUILD_TYPE=$BUILD_TYPE `
              -DBUILD_TESTS=ON `
              -DBUILD_APPS=ON `
              -DBUILD_VIS=ON `
              $REPO_ROOT
    } finally {
        Pop-Location
    }
    
    Write-Success "Configuration complete"
}

function Invoke-Build {
    if (-not (Test-Path (Join-Path $BUILD_DIR "CMakeCache.txt"))) {
        Write-Error-Custom "Build directory not configured. Run configure first."
        exit 1
    }
    
    Write-Info "Building project ($BUILD_TYPE)..."
    
    Push-Location $BUILD_DIR
    try {
        cmake --build . --config $BUILD_TYPE -j $env:NUMBER_OF_PROCESSORS
    } finally {
        Pop-Location
    }
    
    Write-Success "Build complete"
}

function Test-BuildFreshness {
    param([string]$BinaryPath)
    
    if (-not (Test-Path $BinaryPath)) {
        return $false
    }
    
    $binaryTime = (Get-Item $BinaryPath).LastWriteTime
    
    # Check if any source files are newer
    $sourceFiles = Get-ChildItem -Path (Join-Path $REPO_ROOT "meso"), (Join-Path $REPO_ROOT "apps") `
                                  -Include *.cpp,*.hpp -Recurse -ErrorAction SilentlyContinue
    
    foreach ($file in $sourceFiles) {
        if ($file.LastWriteTime -gt $binaryTime) {
            return $false
        }
    }
    
    return $true
}

function Show-Usage {
    @"
vseprw - VSEPR-Sim Wrapper (Windows)
=====================================

One command to build/run cleanly, every time.

USAGE:
    .\vseprw.ps1 <SPEC> <ACTION> [OPTIONS]

SPEC:
    - Molecule formula (H2O, CH4, CO2)
    - XYZ file path (water.xyz, crystal.xyz)
    - System name (NaCl, diamond)

ACTION:
    relax       Energy minimization (FIRE algorithm)
    sim         Molecular dynamics simulation
    discover    Reaction discovery analysis
    align       Kabsch alignment
    build       Interactive molecule builder
    crystal     Crystallographic viewer
    qa          Run QA golden tests

OPTIONS:
    Passed directly to the underlying tool.
    Use --help with each action for details.

EXAMPLES:
    .\vseprw.ps1 H2O relax
    .\vseprw.ps1 water.xyz sim -temp 300 -steps 1000
    .\vseprw.ps1 ethanol.xyz discover -threshold 2.5
    .\vseprw.ps1 NaCl crystal -pbc

ENVIRONMENT:
    `$env:BUILD_DIR         Build directory (default: .\build)
    `$env:CMAKE_GENERATOR   CMake generator (default: Visual Studio 17 2022)
    `$env:BUILD_TYPE        Build type (default: Release)

MODES:
    -ConfigureOnly   Configure CMake without building
    -BuildOnly       Build project without running tool
    -Clean           Clean build directory
    -Help            Show this message

"@
}

# ============================================================================
# Main Logic
# ============================================================================

if ($Help -or (-not $Spec -and -not $ConfigureOnly -and -not $BuildOnly -and -not $Clean)) {
    Show-Usage
    exit 0
}

if ($ConfigureOnly) {
    Test-Toolchain
    Invoke-Configure
    exit 0
}

if ($BuildOnly) {
    Test-Toolchain
    Invoke-Configure
    Invoke-Build
    exit 0
}

if ($Clean) {
    Write-Info "Cleaning build directory: $BUILD_DIR"
    if (Test-Path $BUILD_DIR) {
        Remove-Item -Path $BUILD_DIR -Recurse -Force
    }
    Write-Success "Clean complete"
    exit 0
}

# Parse SPEC and ACTION
if (-not $Spec -or -not $Action) {
    Write-Error-Custom "Missing SPEC or ACTION"
    Write-Host ""
    Show-Usage
    exit 1
}

# Map ACTION to tool
if (-not $TOOL_MAP.ContainsKey($Action)) {
    Write-Error-Custom "Unknown action: $Action"
    Write-Host ""
    Write-Host "Valid actions: $($TOOL_MAP.Keys -join ', ')"
    exit 1
}

$tool = $TOOL_MAP[$Action]
$binaryPath = Join-Path $BUILD_DIR $BUILD_TYPE $tool

# Ensure toolchain
Test-Toolchain

# Configure if needed
if (-not (Test-Path (Join-Path $BUILD_DIR "CMakeCache.txt"))) {
    Write-Info "First run: configuring project"
    Invoke-Configure
}

# Build if needed
if (-not (Test-BuildFreshness $binaryPath)) {
    Write-Info "Binary stale or missing, rebuilding..."
    Invoke-Build
} else {
    Write-Info "Binary up-to-date"
}

# Verify binary exists
if (-not (Test-Path $binaryPath)) {
    Write-Error-Custom "Tool not found after build: $tool"
    Write-Error-Custom "Expected: $binaryPath"
    exit 1
}

# Show execution summary
Write-Info "╔═══════════════════════════════════════════════════════"
Write-Info "║ SPEC:      $Spec"
Write-Info "║ ACTION:    $Action"
Write-Info "║ TOOL:      $tool"
Write-Info "║ BUILD:     $BUILD_DIR"
Write-Info "║ TYPE:      $BUILD_TYPE"
Write-Info "╚═══════════════════════════════════════════════════════"
Write-Host ""

# Execute the tool
Push-Location $REPO_ROOT
try {
    & $binaryPath $Spec $Options
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
