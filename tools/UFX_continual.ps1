param(
    [string]$Root = "C:\R\VSPER-SIM",
    [string]$BuildDir = "build",
    [string]$Target = "uff_smoketest",
    [int]$Runs = 0,        # 0 = run indefinitely
    [int]$SleepSeconds = 5,

    [switch]$StopOnFail,
    [switch]$CleanOutputEachRun,
    [switch]$CleanBuildBeforeRun
)

$ErrorActionPreference = "Stop"

# ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
# Paths
# ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ

$BuildPath = Join-Path $Root $BuildDir
$Exe = Join-Path $BuildPath "$Target.exe"

$RunRoot = Join-Path $Root "runs\uff_continual"
$Summary = Join-Path $RunRoot "summary.csv"

# ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
# Helpers
# ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ

function Fail-Fast($msg) {
    Write-Host "[UFX-CONTINUAL] FATAL: $msg" -ForegroundColor Red
    exit 1
}

function Write-Step($msg) {
    Write-Host "[UFX-CONTINUAL] $msg" -ForegroundColor Cyan
}

function Write-Pass($msg) {
    Write-Host "[PASS] $msg" -ForegroundColor Green
}

function Write-Fail($msg) {
    Write-Host "[FAIL] $msg" -ForegroundColor Red
}

function Sanitize-CsvField($s) {
    if ($null -eq $s) {
        return ""
    }

    return ($s.ToString() `
        -replace "`r", " " `
        -replace "`n", " " `
        -replace ",", ";" `
        -replace '"', "'").Trim()
}

function New-RunId($index) {
    $stamp = (Get-Date).ToString("yyyyMMdd_HHmmss")
    return ("run_{0}_{1:D2}" -f $stamp, $index)
}

function Invoke-NativeToLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExePath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    # No Tee-Object. Native command writes directly to file.
    # $LASTEXITCODE now belongs to the native process, not the pipeline.
    & $ExePath @Arguments > $LogPath 2>&1
    return $LASTEXITCODE
}

function Scan-LogsForFailure {
    param(
        [string[]]$Paths
    )

    $patterns = @(
        '\bNaN\b',
        '\bInf\b',
        '\bfatal\b',
        '\bexception\b',
        '\bsegmentation fault\b',
        '\bassert(ion)?\b',
        '\bmissing=[1-9][0-9]*\b',
        '\bFAIL\b',
        '\[FAIL\]',
        '\bERROR\b'
    )

    foreach ($path in $Paths) {
        if (!(Test-Path $path)) {
            continue
        }

        foreach ($pattern in $patterns) {
            $hit = Select-String -Path $path -Pattern $pattern -CaseSensitive:$false -ErrorAction SilentlyContinue |
                Select-Object -First 1

            if ($hit) {
                return "failure pattern '$pattern' in $path"
            }
        }
    }

    return $null
}

# ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
# Fail-fast checks
# ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ

if (!(Test-Path $Root)) {
    Fail-Fast "Project root not found: $Root"
}

Set-Location $Root

$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if (!$cmakeCmd) {
    Fail-Fast "cmake not found in PATH"
}

if (!(Test-Path $BuildPath)) {
    Fail-Fast "Build directory not found: $BuildPath"
}

New-Item -ItemType Directory -Force -Path $RunRoot | Out-Null

if (!(Test-Path $Summary)) {
    "run_id,timestamp,status,build_status,test_status,elapsed_seconds,notes" |
        Out-File $Summary -Encoding utf8
}

# ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
# Main loop
# ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ

$i = 0
while (($Runs -le 0) -or ($i -lt $Runs)) {
    $i++
    $Start = Get-Date
    $RunId = New-RunId $i
    $RunDir = Join-Path $RunRoot $RunId
    $SmokeOutput = Join-Path $RunDir "output"

    New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
    New-Item -ItemType Directory -Force -Path $SmokeOutput | Out-Null

    $BuildLog = Join-Path $RunDir "build.log"
    $TestLog = Join-Path $RunDir "test.log"
    $InspectLog = Join-Path $RunDir "inspect.log"

    $BuildStatus = 0
    $TestStatus = 0
    $Status = "PASS"
    $Notes = "ok"

    $RunLabel = if ($Runs -le 0) { "Run $i / ?" } else { "Run $i / $Runs" }
    Write-Step "$RunLabel : $RunId"

    try {
        if ($CleanBuildBeforeRun) {
            Write-Step "Cleaning build target first"
            $CleanLog = Join-Path $RunDir "clean.log"

            $BuildStatus = Invoke-NativeToLog `
                -ExePath "cmake" `
                -Arguments @("--build", $BuildDir, "--target", "clean") `
                -LogPath $CleanLog

            if ($BuildStatus -ne 0) {
                throw "clean target failed with status $BuildStatus"
            }
        }

        if ($CleanOutputEachRun) {
            Write-Step "Cleaning smoketest output only"
            Remove-Item -Recurse -Force $SmokeOutput -ErrorAction SilentlyContinue
            New-Item -ItemType Directory -Force -Path $SmokeOutput | Out-Null
        }

        Write-Step "Building target: $Target"

        $BuildStatus = Invoke-NativeToLog `
            -ExePath "cmake" `
            -Arguments @("--build", $BuildDir, "--target", $Target) `
            -LogPath $BuildLog

        Get-Content $BuildLog | Select-Object -Last 30

        if ($BuildStatus -ne 0) {
            throw "build failed with status $BuildStatus"
        }

        if (!(Test-Path $Exe)) {
            throw "executable not found after build: $Exe"
        }

        Write-Step "Running smoketest"

        $TestStatus = Invoke-NativeToLog `
            -ExePath $Exe `
            -Arguments @($SmokeOutput) `
            -LogPath $TestLog

        Get-Content $TestLog | Select-Object -Last 40

        if ($TestStatus -ne 0) {
            throw "smoketest failed with status $TestStatus"
        }

        $Prov = Join-Path $SmokeOutput "uff_provenance_log.jsonl"
        $Auto = Join-Path $SmokeOutput "uff_autocreate_log.csv"
        $Val  = Join-Path $SmokeOutput "uff_validation_log.csv"

        "`n--- provenance first 3 ---" | Out-File $InspectLog -Append
        if (Test-Path $Prov) {
            Get-Content $Prov | Select-Object -First 3 | Out-File $InspectLog -Append
        } else {
            throw "missing provenance log: $Prov"
        }

        "`n--- autocreate first 5 ---" | Out-File $InspectLog -Append
        if (Test-Path $Auto) {
            Get-Content $Auto | Select-Object -First 5 | Out-File $InspectLog -Append
        } else {
            # autocreate log is only written when the auto-creator generates fallback entries.
            # A clean baseline run produces no auto-created entries — this is expected.
            "(no autocreate entries this run — all types resolved from reference)" |
                Out-File $InspectLog -Append
        }

        "`n--- validation first 10 ---" | Out-File $InspectLog -Append
        if (Test-Path $Val) {
            Get-Content $Val | Select-Object -First 10 | Out-File $InspectLog -Append
        } else {
            throw "missing validation log: $Val"
        }

        $failure = Scan-LogsForFailure @($BuildLog, $TestLog, $InspectLog)

        if ($failure) {
            throw $failure
        }

        Write-Pass "$RunId completed"
    }
    catch {
        $Status = "FAIL"
        $Notes = Sanitize-CsvField $_.Exception.Message

        Write-Fail "$RunId failed: $Notes"
    }

    $End = Get-Date
    $Elapsed = [int]($End - $Start).TotalSeconds

    $safeNotes = Sanitize-CsvField $Notes

    "$RunId,$($Start.ToString("yyyy-MM-dd HH:mm:ss")),$Status,$BuildStatus,$TestStatus,$Elapsed,$safeNotes" |
        Out-File $Summary -Append -Encoding utf8

    Write-Step "$Status in ${Elapsed}s"
    Write-Step "Logs: $RunDir"

    if (($Status -eq "FAIL") -and $StopOnFail) {
        Write-Step "Stopping after failure because -StopOnFail was set"
        break
    }

    if (($Runs -le 0) -or ($i -lt $Runs)) {
        Write-Step "Sleeping ${SleepSeconds}s"
        Start-Sleep -Seconds $SleepSeconds
    }
}

Write-Step "Full summary:"
Import-Csv $Summary | Format-Table -AutoSize
