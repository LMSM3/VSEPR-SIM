#!/usr/bin/env pwsh
<#
.SYNOPSIS
Runs the isolated VSEPR-SIM dual-backend 3D demonstration pipeline.
#>

param(
	[string]$OutputDir = 'out\dual_backend_3d',
	[ValidateSet('None', 'OpenGL', 'BGFX', 'Native', 'Both', 'All')]
	[string]$Render = 'None',
	[int]$Scene = 0,
	[int]$StressParticles = 0,
	[int]$Steps = 0,
	[switch]$SkipBuild,
	[switch]$SkipPostprocess
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $projectRoot
$outputPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDir))
$dataDemo = Join-Path $projectRoot 'build\dual-backend-3d-data.exe'
$cgDemo = Join-Path $projectRoot 'build_vis\cg-anim-demo.exe'
$bgfxDemo = Join-Path $projectRoot 'build_bgfx_demo\bgfx_random_values_demo.exe'
$nativeDemo = Join-Path $projectRoot 'build\native-particle-demo.exe'
$postprocessor = Join-Path $PSScriptRoot 'dual_backend_postprocess.R'

if (-not $SkipBuild) {
	& 'C:\msys64\ucrt64\bin\cmake.exe' --build --preset release --target dual-backend-3d-data --parallel 2
	if ($LASTEXITCODE -ne 0) { throw 'Headless data-generator build failed.' }
	& 'C:\msys64\ucrt64\bin\cmake.exe' --build --preset release --target native-particle-demo --parallel 2
	if ($LASTEXITCODE -ne 0) { throw 'Native CPU/GDI renderer build failed.' }
	& 'C:\msys64\ucrt64\bin\cmake.exe' --build --preset bgfx-demo --target bgfx_random_values_demo --parallel 2
	if ($LASTEXITCODE -ne 0) { throw 'BGFX demo build failed.' }
}

if (-not (Test-Path $dataDemo)) { throw "Generator not found: $dataDemo" }
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

$generatorArgs = @('--headless', '--output', $outputPath)
if ($StressParticles -gt 0) { $generatorArgs += @('--stress-particles', $StressParticles) }
if ($Steps -gt 0) { $generatorArgs += @('--steps', $Steps) }
& $dataDemo @generatorArgs
if ($LASTEXITCODE -ne 0) { throw 'Artifact generation failed.' }

if (-not $SkipPostprocess) {
	$rscript = Get-Command Rscript.exe -ErrorAction SilentlyContinue
	if (-not $rscript) { throw 'Rscript.exe was not found on PATH.' }
	& $rscript.Source $postprocessor $outputPath
	if ($LASTEXITCODE -ne 0) { throw 'R visualization/XLSX postprocessing failed.' }
}

if ($Render -in @('OpenGL', 'Both', 'All')) {
	if (-not (Test-Path $cgDemo)) {
		throw "OpenGL demo not found: $cgDemo. Build it with: cmake --build --preset vis --target cg-anim-demo"
	}
	Start-Process -FilePath $cgDemo -ArgumentList @('--output', $outputPath)
}
if ($Render -in @('BGFX', 'Both', 'All')) {
	if (-not (Test-Path $bgfxDemo)) { throw "BGFX demo not found: $bgfxDemo" }
	Start-Process -FilePath $bgfxDemo -ArgumentList @('--particles', (Join-Path $outputPath 'particles.csv'), '--scene', $Scene)
}
if ($Render -in @('Native', 'All')) {
	if (-not (Test-Path $nativeDemo)) { throw "Native renderer not found: $nativeDemo" }
	Start-Process -FilePath $nativeDemo -ArgumentList @('--particles', (Join-Path $outputPath 'particles.csv'), '--scene', $Scene)
}

Write-Host "Dual-backend demonstration output: $outputPath" -ForegroundColor Green
Get-ChildItem $outputPath | Select-Object Name, Length, LastWriteTime
