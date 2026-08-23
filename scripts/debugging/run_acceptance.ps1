param(
    [string]$Executable = "build/vsepr.exe",
    [switch]$IncludeVisualHandoff
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
Set-Location -LiteralPath $repoRoot

$exePath = (Resolve-Path -LiteralPath $Executable).Path
$failures = [System.Collections.Generic.List[string]]::new()

function Invoke-VsimCommand {
    param([string]$Verb, [string]$Script)

    $text = (& $exePath $Verb $Script 2>&1 | Out-String)
    $text = $text -replace "`e\[[0-9;]*[A-Za-z]", ""
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        $failures.Add("$Verb failed for $Script (exit $exitCode)")
    }
    return $text
}

function Require-Text {
    param([string]$Text, [string]$Needle, [string]$Message)
    if (-not $Text.Contains($Needle)) { $failures.Add($Message) }
}

function Reject-Text {
    param([string]$Text, [string]$Needle, [string]$Message)
    if ($Text.Contains($Needle)) { $failures.Add($Message) }
}

function Require-Png {
    param([string]$Path, [string]$Message)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        $failures.Add($Message)
        return
    }
    $signature = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
    $expected = [byte[]](0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a)
    if ($signature.Count -lt $expected.Count) {
        $failures.Add("$Message (truncated file)")
        return
    }
    for ($i = 0; $i -lt $expected.Count; $i++) {
        if ($signature[$i] -ne $expected[$i]) {
            $failures.Add("$Message (invalid PNG signature)")
            return
        }
    }
}

$headless = "scripts/debugging/bug_exposer_headless_viewer_suppression.vsim"
$exports = "scripts/debugging/bug_exposer_export_summary.vsim"
$dissolution = "scripts/debugging/bug_exposer_dissolution_bridge.vsim"
$sampling = "scripts/debugging/bug_exposer_material_sampling.vsim"
$builderPreview = "scripts/improvement_acceptance/builder_distribution_preview.vsim"

foreach ($script in @($headless, $exports, $dissolution, $sampling, $builderPreview)) {
    [void](Invoke-VsimCommand "validate" $script)
}

$headlessOutput = Invoke-VsimCommand "run" $headless
Reject-Text $headlessOutput "[view] Opening session-bound viewer" `
    "visual output 'none' still launched a viewer"

$exportOutput = Invoke-VsimCommand "run" $exports
Require-Text $exportOutput "[export:written] write_analysis_json" `
    "analysis export was not reported as written"
Require-Text $exportOutput "[export:written] write_metrics_tsv" `
    "metrics export was not reported as written"
Require-Text $exportOutput "[export:written] write_events_json" `
    "event export was not reported as written"

$dissolutionOutput = Invoke-VsimCommand "run" $dissolution
Require-Text $dissolutionOutput "dissolution           runtime-wired" `
    "dissolution was not present in the module execution plan"
Require-Text $dissolutionOutput "[dissolution] step=" `
    "dissolution pass did not emit step evidence"
Require-Text $dissolutionOutput "protonation=0" `
    "dissolution surface state did not persist across steps"
$eventPath = "out/debugging/dissolution_bridge/events.jsonl"
if (-not (Test-Path -LiteralPath $eventPath)) {
    $failures.Add("dissolution event log was not written")
} elseif (-not (Select-String -LiteralPath $eventPath -SimpleMatch "dissolution.surface_pass" -Quiet)) {
    $failures.Add("dissolution event log lacks dissolution-specific evidence")
}

$samplingValidation = Invoke-VsimCommand "validate" $sampling
Require-Text $samplingValidation "[analysis.sampling] raw-only keys:" `
    "sampling validation did not identify retained raw-only keys"
$samplingOutput = Invoke-VsimCommand "run" $sampling
Require-Text $samplingOutput "raw-only keys (no runtime effect):" `
    "sampling run did not disclose unsupported expansion intent"
Reject-Text $samplingOutput "[view] Opening session-bound viewer" `
    "sampling run launched a viewer despite visual output 'none'"

$builderOutput = Invoke-VsimCommand "expand" $builderPreview
Require-Text $builderOutput "canonical_parser: true" `
    "builder preview did not use the canonical VSIM parser"
Require-Text $builderOutput "distribution_count: 1" `
    "builder preview did not resolve the typed distribution card"
Require-Text $builderOutput "[distribution substitution_sites]" `
    "builder preview omitted the named distribution audit"
Require-Text $builderOutput "status: future_runnable" `
    "builder preview did not distinguish future-runnable intent"
Require-Text $builderOutput "executed_records: 0" `
    "builder preview claimed execution during a preview-only command"

if ($IncludeVisualHandoff) {
    $visual = "scripts/demos/bug_exposer_visual_xyz_handoff.vsim"
    $visualOutputDir = "out/bug_exposer_visual_xyz_handoff"
    $matplotlibPng = Join-Path $visualOutputDir "figures/bug_exposer_visual_xyz_handoff_matplotlib.png"
    if (Test-Path -LiteralPath $visualOutputDir) {
        Get-ChildItem -LiteralPath $visualOutputDir -Filter "viewer_ack_*.json" -File |
            Remove-Item -Force
    }
    if (Test-Path -LiteralPath $matplotlibPng) { Remove-Item -LiteralPath $matplotlibPng -Force }
    [void](Invoke-VsimCommand "validate" $visual)
    $visualOutput = Invoke-VsimCommand "run" $visual
    Require-Text $visualOutput "[view] Opening session-bound viewer" `
        "requested visual handoff did not launch the viewer"
    Require-Text $visualOutput "[view:ack]" `
        "requested visual handoff did not receive viewer acknowledgement"
    Require-Text $visualOutput "[matplotlib-png:written]" `
        "parallel Matplotlib PNG renderer did not report a written artifact"
    Require-Png $matplotlibPng "parallel Matplotlib PNG artifact was not written correctly"
    $ackFiles = @(Get-ChildItem -LiteralPath $visualOutputDir -Filter "viewer_ack_*.json" -File |
        Sort-Object LastWriteTime -Descending)
    if ($ackFiles.Count -eq 0) {
        $failures.Add("viewer acknowledgement JSON was not written")
    } else {
        try {
            $ack = Get-Content -LiteralPath $ackFiles[0].FullName -Raw | ConvertFrom-Json
            if (-not $ack.ok) { $failures.Add("viewer acknowledgement reported load failure") }
            if ($ack.backend -ne "qt-vtk") {
                $failures.Add("visual handoff reached '$($ack.backend)' instead of the required qt-vtk backend")
            }
            if ($ack.frame_count -lt 1) { $failures.Add("viewer acknowledgement did not report loaded frames") }
            if ($ack.max_particle_count -lt 3) { $failures.Add("viewer acknowledgement did not report the H2O payload") }
            if ($ack.run_label -ne "bug_exposer_visual_xyz_handoff") {
                $failures.Add("viewer acknowledgement run label did not match visual BUG_EXPOSER")
            }
        } catch {
            $failures.Add("viewer acknowledgement JSON could not be parsed: $($_.Exception.Message)")
        }
    }

    $pngOnly = "scripts/debugging/bug_exposer_matplotlib_png_only.vsim"
    $pngOnlyPath = "out/bug_exposer_matplotlib_png_only/figures/bug_exposer_matplotlib_png_only_matplotlib.png"
    if (Test-Path -LiteralPath $pngOnlyPath) { Remove-Item -LiteralPath $pngOnlyPath -Force }
    [void](Invoke-VsimCommand "validate" $pngOnly)
    $pngOnlyOutput = Invoke-VsimCommand "run" $pngOnly
    Reject-Text $pngOnlyOutput "[view] Opening session-bound viewer" `
        "PNG-only script launched an interactive viewer"
    Require-Text $pngOnlyOutput "[matplotlib-png:written]" `
        "PNG-only script did not report Matplotlib output"
    Require-Png $pngOnlyPath "PNG-only Matplotlib artifact was not written correctly"
}

if ($failures.Count -gt 0) {
    Write-Host "VSIM acceptance failed:" -ForegroundColor Red
    foreach ($failure in $failures) { Write-Host "  - $failure" }
    exit 1
}

Write-Host "VSIM acceptance passed." -ForegroundColor Green
Write-Host "Validated: headless visual intent, export accountability, dissolution dispatch, sampling capability truth, builder expansion audit."
if ($IncludeVisualHandoff) { Write-Host "Validated: Qt/VTK interactive handoff plus parallel Matplotlib PNG publication output." }
