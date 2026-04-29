
$files = Get-ChildItem *.tex
$spinner = "|", "/", "-", "\"
$step = 0

# Hide cursor for cleaner animation
[System.Console]::CursorVisible = $false

foreach ($file in $files) {
    Write-Host -NoNewline "Compiling $($file.Name)... "
    
    # Run pdflatex in background
    $process = Start-Process pdflatex -ArgumentList "-interaction=nonstopmode", "-draftmode", "`"$($file.Name)`"" -NoNewWindow -PassThru -ErrorAction SilentlyContinue
    
    # Spinner loop while process is running
    while (-not $process.HasExited) {
        $char = $spinner[$step % 4]
        Write-Host -NoNewline "`r[$char] $($file.Name)"
        $step++
        Start-Sleep -Milliseconds 150
    }

    # Final status check
    if ($process.ExitCode -eq 0) {
        Write-Host "`r[OK] $($file.Name)      " -ForegroundColor Green
    } else {
        Write-Host "`r[!!] $($file.Name)      " -ForegroundColor Red
    }
}

[System.Console]::CursorVisible = $true
Write-Host "Draft compilation cycle complete."
