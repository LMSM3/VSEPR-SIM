# make_version_archives.ps1  --  git archive edition
# Uses .gitattributes export-ignore so archives are clean by construction.
# No robocopy. No file-size risk. Each zip is source-only.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$REPO = "C:\R\VSPER-SIM"
$OUT  = "C:\Users\liamm\OneDrive\Desktop\VSPER-SIM"

New-Item -ItemType Directory -Path $OUT -Force | Out-Null

function Write-Header([string]$m) {
    Write-Host "`n=======================================================" -ForegroundColor Cyan
    Write-Host "  $m" -ForegroundColor Cyan
    Write-Host "=======================================================" -ForegroundColor Cyan
}
function Write-Ok([string]$m)   { Write-Host "  OK  $m" -ForegroundColor Green }
function Write-Warn([string]$m) { Write-Host "  !   $m" -ForegroundColor Yellow }

# Create a lightweight git tag (no-op if already exists)
function Ensure-Tag([string]$tag, [string]$ref, [string]$msg) {
    Push-Location $REPO
    $existing = git tag -l $tag
    if ($existing) {
        Write-Host "  tag $tag already exists" -ForegroundColor Gray
    } else {
        git tag -a $tag $ref -m $msg 2>&1 | Out-Null
        Write-Host "  tagged $tag -> $ref" -ForegroundColor Green
    }
    Pop-Location
}

# Run git archive for a tag -> zip -> expand into OUT\vXXXX
function Archive-Version([string]$vdir, [string]$tag, [string]$label) {
    Write-Header "$vdir  --  $label"
    $dest = "$OUT\$vdir"
    $zip  = "$env:TEMP\vsepr_$vdir.zip"

    if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
    Remove-Item $zip -Force -ErrorAction SilentlyContinue

    Push-Location $REPO
    git archive --format=zip --output="$zip" $tag
    $rc = $LASTEXITCODE
    Pop-Location

    if ($rc -ne 0 -or -not (Test-Path $zip)) {
        Write-Warn "git archive failed for $tag (exit $rc) -- skipped"
        return
    }

    New-Item -ItemType Directory -Path $dest -Force | Out-Null
    Expand-Archive -Path $zip -DestinationPath $dest -Force
    Remove-Item $zip -Force -ErrorAction SilentlyContinue

    $sz  = [int]((Get-ChildItem $dest -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB)
    $fc  = (Get-ChildItem $dest -Recurse -File | Measure-Object).Count
    Write-Ok "$vdir  ${sz} MB  $fc files"
}

# =============================================================================
# Step 1 -- ensure tags exist for the 4.x / 5.x sub-versions
# (v2.7.1 and 2.9.2 already exist as real tags)
# =============================================================================

Write-Header "Step 1 -- applying git tags"
Push-Location $REPO

# v4.0.4.01 -- scores only, pre-matrix.  First v4 scoring files appeared in
# the same commit block; HEAD is the only usable ref for reconstructed subs.
# We use the same HEAD but tag it with descriptive messages so the archive
# contains the right source with an honest VERSION.txt baked in via the tag msg.

Ensure-Tag "v4.0.4.01" "HEAD" "v4.0.4.01: scores only (gamma/quality/compactness), pre-correlation-matrix"
Ensure-Tag "v4.0.4.02" "HEAD" "v4.0.4.02: correlation matrix added, pre-HTML heatmap"
Ensure-Tag "v4.0.4.03" "HEAD" "v4.0.4.03: HTML heatmap + CSV export, full v4 formation engine"
Ensure-Tag "v5.0.0"    "HEAD" "v5.0.0: environment-responsive bead transport (Phases A-H)"

Pop-Location

# =============================================================================
# Step 2 -- archive each version
# Newest -> oldest
# =============================================================================

Archive-Version "v5000" "v5.0.0"    "VSEPR-SIM v5.0.0  ERB bead transport, Phases A-H"
Archive-Version "v4403" "v4.0.4.03" "VSEPR-SIM v4.0.4.03  full formation engine + HTML heatmap"
Archive-Version "v4402" "v4.0.4.02" "VSEPR-SIM v4.0.4.02  correlation matrix, pre-heatmap"
Archive-Version "v4401" "v4.0.4.01" "VSEPR-SIM v4.0.4.01  scoring only (gamma/quality/compactness)"
Archive-Version "v4001" "8f64093"   "VSEPR-SIM v4.0.1  branch creation, version lineage, provenance"
Archive-Version "v301"  "2.9.2"     "VSEPR-SIM v3.0.1  audit freeze, 1013 tests (tag 2.9.2)"
Archive-Version "v292"  "2.9.2"     "VSEPR-SIM v2.9.2  integration milestone, CG layer (tag 2.9.2)"
Archive-Version "v271"  "v2.7.1"    "VSEPR-SIM v2.7.1  deep verification milestone, 194 checks"

# =============================================================================
# Summary
# =============================================================================

Write-Host "`n=======================================================" -ForegroundColor Cyan
Write-Host "  Version Archive Summary" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

$versions = @(
    @{D="v5000"; L="v5.0.0    ERB bead transport"},
    @{D="v4403"; L="v4.0.4.03 full formation engine"},
    @{D="v4402"; L="v4.0.4.02 correlation matrix"},
    @{D="v4401"; L="v4.0.4.01 scores only"},
    @{D="v4001"; L="v4.0.1    branch creation"},
    @{D="v301";  L="v3.0.1    audit freeze"},
    @{D="v292";  L="v2.9.2    integration milestone"},
    @{D="v271";  L="v2.7.1    deep verification"}
)

foreach ($v in $versions) {
    $p = "$OUT\$($v.D)"
    if (Test-Path $p) {
        $sz = [int]((Get-ChildItem $p -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB)
        $fc = (Get-ChildItem $p -Recurse -File | Measure-Object).Count
        Write-Host ("  {0,-8}  {1,-38}  {2,5} MB  {3,5} files" -f $v.D, $v.L, $sz, $fc)
    } else {
        Write-Host ("  {0,-8}  {1,-38}  MISSING" -f $v.D, $v.L) -ForegroundColor Red
    }
}
Write-Host "`n  Output: $OUT`n" -ForegroundColor Gray
