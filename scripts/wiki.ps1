################################################################################
# VSEPR-Sim Wiki Integration (PowerShell)
# Lookup chemical formulas and open Wikipedia pages
################################################################################

param(
    [string]$Command = "help",
    [string]$Formula = ""
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$WikiDB = Join-Path $ProjectRoot "data\wiki\common_chemicals.txt"
$WikiCache = Join-Path $ProjectRoot "data\wiki\cache"

# Create cache directory
New-Item -ItemType Directory -Path $WikiCache -Force | Out-Null

# ============================================================================
# Functions
# ============================================================================

function Normalize-Formula {
    param([string]$Formula)
    return $Formula -replace '\s', '' -replace '\([^)]*\)', ''
}

function Lookup-Wiki {
    param([string]$Formula)
    
    $normalized = Normalize-Formula $Formula
    $cacheFile = Join-Path $WikiCache "$normalized.url"
    
    # Check cache
    if (Test-Path $cacheFile) {
        $url = Get-Content $cacheFile -Raw
        return @{Found=$true; URL=$url.Trim()}
    }
    
    # Search database
    $content = Get-Content $WikiDB | Where-Object { $_ -notmatch '^#' -and $_ -ne '' }
    $match = $content | Where-Object { $_ -match "^$normalized\|" } | Select-Object -First 1
    
    if ($match) {
        $parts = $match -split '\|'
        $name = $parts[1]
        $url = $parts[2]
        
        # Cache result
        $url | Out-File -FilePath $cacheFile -Encoding utf8 -NoNewline
        
        return @{Found=$true; Name=$name; URL=$url}
    }
    
    return @{Found=$false}
}

function Open-Wiki {
    param([string]$URL)
    Start-Process $URL
}

# ============================================================================
# Commands
# ============================================================================

switch ($Command.ToLower()) {
    "lookup" {
        if ([string]::IsNullOrWhiteSpace($Formula)) {
            Write-Host "Usage: wiki.ps1 lookup <formula>"
            exit 1
        }
        
        $result = Lookup-Wiki $Formula
        if ($result.Found) {
            Write-Host "✓ Found: $($result.Name)" -ForegroundColor Green
            Write-Host "  URL: $($result.URL)" -ForegroundColor Cyan
        } else {
            Write-Host "⊘ No Wikipedia entry found for: $Formula" -ForegroundColor Yellow
            exit 1
        }
    }
    
    "open" {
        if ([string]::IsNullOrWhiteSpace($Formula)) {
            Write-Host "Usage: wiki.ps1 open <formula>"
            exit 1
        }
        
        $result = Lookup-Wiki $Formula
        if ($result.Found) {
            Write-Host "Opening Wikipedia: $($result.Name)" -ForegroundColor Cyan
            Open-Wiki $result.URL
        } else {
            Write-Host "⊘ No Wikipedia entry found for: $Formula" -ForegroundColor Yellow
            exit 1
        }
    }
    
    "auto" {
        if ([string]::IsNullOrWhiteSpace($Formula)) {
            exit 1
        }
        
        $result = Lookup-Wiki $Formula
        if ($result.Found) {
            Open-Wiki $result.URL
            exit 0
        }
        exit 1
    }
    
    "list" {
        Write-Host "Common chemicals in database:" -ForegroundColor Cyan
        Write-Host ""
        Get-Content $WikiDB | Where-Object { $_ -notmatch '^#' -and $_ -ne '' } | ForEach-Object {
            $parts = $_ -split '\|'
            Write-Host ("  {0,-15} {1}" -f $parts[0], $parts[1]) -ForegroundColor Gray
        }
    }
    
    "stats" {
        $total = (Get-Content $WikiDB | Where-Object { $_ -notmatch '^#' -and $_ -ne '' }).Count
        $cached = (Get-ChildItem $WikiCache -File -ErrorAction SilentlyContinue).Count
        Write-Host "Wiki Database Statistics:" -ForegroundColor Cyan
        Write-Host "  Total entries: $total" -ForegroundColor Gray
        Write-Host "  Cached lookups: $cached" -ForegroundColor Gray
    }
    
    default {
        Write-Host "VSEPR-Sim Wiki Integration" -ForegroundColor Magenta
        Write-Host ""
        Write-Host "Usage:" -ForegroundColor Cyan
        Write-Host "  wiki.ps1 lookup <formula>   - Look up chemical in database"
        Write-Host "  wiki.ps1 open <formula>     - Open Wikipedia page"
        Write-Host "  wiki.ps1 auto <formula>     - Auto-open if found (silent)"
        Write-Host "  wiki.ps1 list              - List all known chemicals"
        Write-Host "  wiki.ps1 stats             - Show database statistics"
        Write-Host ""
        Write-Host "Examples:" -ForegroundColor Yellow
        Write-Host "  .\scripts\wiki.ps1 lookup H2O"
        Write-Host "  .\scripts\wiki.ps1 open NH3"
        Write-Host "  .\scripts\wiki.ps1 list"
    }
}
