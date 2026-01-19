# FlowerAI PowerShell Launcher
$ErrorActionPreference = "Stop"

$bash = Get-Command bash -ErrorAction SilentlyContinue
if (-not $bash) {
    Write-Error "bash not found."
    exit 1
}

# Use forward slashes for the path so Bash understands it correctly
$scriptPath = "C:/Users/Liam/Desktop/Fls/FlowerAI/flowerai"

& $bash.Definition $scriptPath @args

