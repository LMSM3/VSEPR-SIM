# run_all.ps1
# Run all supported .vsim scripts in examples/ via vsper.
# Usage: cd to examples/ directory, then: .\run_all.ps1
# Or from any location: .\run_all.ps1 -ExamplesDir "C:\path\to\examples"

param(
    [string]$ExamplesDir = $PSScriptRoot,
    [string]$VsperExe    = "vsper",      # assumes vsper is on PATH; override if needed
    [switch]$DryRun,
    [switch]$StopOnError
)

$scripts = @(
    # IKK Series
    "ikk1_schrodinger_alternative_ch4.vsim",
    "ikk2_projection_operator_nacl.vsim",
    "ikk3_matrix_wave_bridge_si.vsim",
    "ikk4_scale_interval_batch.vsim",
    "ikk5_field_identity_fe_large.vsim",
    # MIR Series
    "mir2_frozen_state_recovery_nacl.vsim",
    "mir3_recovery_efficiency_si.vsim",
    # WIC Series
    "wic1_identity_vector_trajectories_graphene.vsim",
    "wic2_interaction_history_graphite.vsim",
    "wic3_entangleon_polymer.vsim",
    # EVM Series
    "evm3_physical_compatibility_batch.vsim",
    "evm4_md_benchmarks_batch.vsim",
    # Scale demo
    "scale_ladder_n2_to_km_fe.vsim",
    # Day 75 baseline (flat, supported)
    "day75_ch4_flat_supported.vsim"
    # day75_modular_ikk1_formation.vsim-pre is excluded — requires vsim-precompile first
)

$passed  = @()
$failed  = @()
$skipped = @()

Write-Host "`nVSEPR-SIM Example Runner" -ForegroundColor Cyan
Write-Host "Examples dir : $ExamplesDir"
Write-Host "vsper        : $VsperExe"
Write-Host "Dry run      : $DryRun"
Write-Host "Scripts      : $($scripts.Count)`n"

foreach ($script in $scripts) {
    $path = Join-Path $ExamplesDir $script

    if (-not (Test-Path $path)) {
        Write-Host "  [SKIP] $script — file not found" -ForegroundColor Yellow
        $skipped += $script
        continue
    }

    Write-Host "  [RUN ] $script" -ForegroundColor White

    if ($DryRun) {
        Write-Host "         (dry run — skipping execution)" -ForegroundColor DarkGray
        continue
    }

    try {
        $result = & $VsperExe run $path 2>&1
        $exit   = $LASTEXITCODE

        if ($exit -eq 0) {
            Write-Host "         [PASS] exit $exit" -ForegroundColor Green
            $passed += $script
        } else {
            Write-Host "         [FAIL] exit $exit" -ForegroundColor Red
            $failed += $script
            if ($StopOnError) {
                Write-Host "`nStopping on first error (-StopOnError)." -ForegroundColor Red
                break
            }
        }
    } catch {
        Write-Host "         [ERR ] $_" -ForegroundColor Red
        $failed += $script
        if ($StopOnError) { break }
    }
}

Write-Host "`n--- Summary ---"
Write-Host "  Passed  : $($passed.Count)" -ForegroundColor Green
Write-Host "  Failed  : $($failed.Count)" -ForegroundColor $(if ($failed.Count -gt 0) {"Red"} else {"Green"})
Write-Host "  Skipped : $($skipped.Count)" -ForegroundColor Yellow

if ($failed.Count -gt 0) {
    Write-Host "`nFailed scripts:"
    $failed | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}
exit 0
