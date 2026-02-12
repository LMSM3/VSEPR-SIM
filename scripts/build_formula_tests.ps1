# Quick build script for formula parser tests
# This assumes you have a C++ compiler (g++, clang++, or MSVC) in your PATH

$ErrorActionPreference = "Continue"

Write-Host "=== Building Formula Parser Tests ===" -ForegroundColor Cyan
Write-Host ""

# Check for compiler
$compiler = $null
$compilerArgs = @()

if (Get-Command g++ -ErrorAction SilentlyContinue) {
    $compiler = "g++"
    $compilerArgs = @("-std=c++17", "-Wall", "-O2")
    Write-Host "✓ Found g++" -ForegroundColor Green
} elseif (Get-Command clang++ -ErrorAction SilentlyContinue) {
    $compiler = "clang++"
    $compilerArgs = @("-std=c++17", "-Wall", "-O2")
    Write-Host "✓ Found clang++" -ForegroundColor Green
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    $compiler = "cl"
    $compilerArgs = @("/std:c++17", "/EHsc", "/O2", "/W3")
    Write-Host "✓ Found MSVC (cl)" -ForegroundColor Green
} else {
    Write-Host "✗ No C++ compiler found in PATH" -ForegroundColor Red
    Write-Host "  Please install:" -ForegroundColor Yellow
    Write-Host "    - MinGW-w64 (g++)" -ForegroundColor Yellow
    Write-Host "    - LLVM (clang++)" -ForegroundColor Yellow
    Write-Host "    - Visual Studio (cl)" -ForegroundColor Yellow
    exit 1
}

# Include directories
$includes = @(
    "-I$PSScriptRoot\include",
    "-I$PSScriptRoot\src"
)

# Create output directory
$outDir = "$PSScriptRoot\build\bin"
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

Write-Host ""
Write-Host "Building test_formula_parser..." -ForegroundColor White

try {
    $testSource = "$PSScriptRoot\tests\test_formula_parser.cpp"
    $testOutput = "$outDir\test_formula_parser.exe"
    
    $buildCmd = @($compiler) + $compilerArgs + $includes + @($testSource, "-o", $testOutput)
    
    Write-Host "  Command: $($buildCmd -join ' ')" -ForegroundColor Gray
    & $buildCmd[0] $buildCmd[1..($buildCmd.Length-1)]
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ Built: $testOutput" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
    }
} catch {
    Write-Host "  ✗ Build error: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "Building formula_fuzz_tester..." -ForegroundColor White

try {
    $fuzzSource = "$PSScriptRoot\tests\formula_fuzz_tester.cpp"
    $fuzzOutput = "$outDir\formula_fuzz_tester.exe"
    
    $buildCmd = @($compiler) + $compilerArgs + $includes + @($fuzzSource, "-o", $fuzzOutput)
    
    Write-Host "  Command: $($buildCmd -join ' ')" -ForegroundColor Gray
    & $buildCmd[0] $buildCmd[1..($buildCmd.Length-1)]
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ Built: $fuzzOutput" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
    }
} catch {
    Write-Host "  ✗ Build error: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Build Summary ===" -ForegroundColor Cyan

if (Test-Path "$outDir\test_formula_parser.exe") {
    Write-Host "✓ test_formula_parser.exe ready" -ForegroundColor Green
    Write-Host "  Run: .\build\bin\test_formula_parser.exe" -ForegroundColor Gray
} else {
    Write-Host "✗ test_formula_parser.exe not built" -ForegroundColor Red
}

if (Test-Path "$outDir\formula_fuzz_tester.exe") {
    Write-Host "✓ formula_fuzz_tester.exe ready" -ForegroundColor Green
    Write-Host "  Run: .\build\bin\formula_fuzz_tester.exe [--iterations N]" -ForegroundColor Gray
} else {
    Write-Host "✗ formula_fuzz_tester.exe not built" -ForegroundColor Red
}

Write-Host ""
