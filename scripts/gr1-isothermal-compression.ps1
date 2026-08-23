param([string]$Exe = "", [string]$OutputRoot = "out/gr1-demos")
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $Root "build/gr1-condensation.exe" }
if (-not (Test-Path $Exe)) { throw "GR-1 executable not found: $Exe. Build target gr1-condensation or pass -Exe." }
$Output = Join-Path $Root "$OutputRoot/GR-1B-isothermal-compression"
& $Exe --input (Join-Path $Root "examples/golden_runs/GR-1B-isothermal-compression.gr1") --output $Output
python (Join-Path $Root "tools/render_gr1_report.py") $Output
Write-Host "GR-1B isothermal-compression workbook and PNGs: $Output/post_render" -ForegroundColor Green
