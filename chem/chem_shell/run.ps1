# chem_shell runner -- persistent controller, thin shell front end
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$pipeIn  = Join-Path $env:TEMP "chem_ctrl_in.txt"
$pipeOut = Join-Path $env:TEMP "chem_ctrl_out.txt"

if (!(Test-Path $pipeIn))  { New-Item -Path $pipeIn  -ItemType File | Out-Null }
if (!(Test-Path $pipeOut)) { New-Item -Path $pipeOut -ItemType File | Out-Null }

$controllerPath = Join-Path $root "controller.py"
$proc = Start-Process python -ArgumentList "`"$controllerPath`" `"$pipeIn`" `"$pipeOut`"" -PassThru -NoNewWindow

# Print MOTD from controller
$motdText = & python -c "import sys; sys.path.insert(0,'$($root -replace '\\','/')'); from controller import motd; print(motd())" 2>$null
if ($motdText) { Write-Host $motdText } else {
    Write-Host ""
    Write-Host "  chem_shell v0.1  --  type help for commands, Ctrl-C to quit"
    Write-Host ""
}

try {
    while ($true) {
        $cmd = Read-Host "chem"
        if ([string]::IsNullOrWhiteSpace($cmd)) { continue }
        Set-Content -Path $pipeIn -Value $cmd -NoNewline
        Start-Sleep -Milliseconds 200
        if (Test-Path $pipeOut) {
            $resp = Get-Content $pipeOut -Raw
            if ($resp) {
                Write-Host $resp
                Clear-Content $pipeOut
            }
        }
    }
}
finally {
    if ($proc -and !$proc.HasExited) { $proc.Kill() }
    Remove-Item $pipeIn  -ErrorAction SilentlyContinue
    Remove-Item $pipeOut -ErrorAction SilentlyContinue
}
