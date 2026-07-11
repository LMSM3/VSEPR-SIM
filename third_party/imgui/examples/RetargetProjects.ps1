# PowerShell Script to Retarget Visual Studio Projects
# This script updates WindowsTargetPlatformVersion and PlatformToolset in .vcxproj files

$projects = Get-ChildItem -Include *.vcxproj -Recurse
$updatedCount = 0

Write-Host "Found $($projects.Count) project files to process..." -ForegroundColor Cyan

foreach ($project in $projects) {
    Write-Host "`nProcessing: $($project.Name)" -ForegroundColor Yellow
    
    $content = Get-Content $project.FullName -Raw
    $modified = $false
    
    # Update WindowsTargetPlatformVersion
    if ($content -match '<WindowsTargetPlatformVersion>8\.1</WindowsTargetPlatformVersion>') {
        $content = $content -replace '<WindowsTargetPlatformVersion>8\.1</WindowsTargetPlatformVersion>', '<WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>'
        Write-Host "  - Updated WindowsTargetPlatformVersion: 8.1 -> 10.0" -ForegroundColor Green
        $modified = $true
    }
    
    if ($content -match '<WindowsTargetPlatformVersion>10\.0\.18362\.0</WindowsTargetPlatformVersion>') {
        $content = $content -replace '<WindowsTargetPlatformVersion>10\.0\.18362\.0</WindowsTargetPlatformVersion>', '<WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>'
        Write-Host "  - Updated WindowsTargetPlatformVersion: 10.0.18362.0 -> 10.0" -ForegroundColor Green
        $modified = $true
    }
    
    # Update PlatformToolset v140 -> v143
    if ($content -match '<PlatformToolset>v140</PlatformToolset>') {
        $content = $content -replace '<PlatformToolset>v140</PlatformToolset>', '<PlatformToolset>v143</PlatformToolset>'
        Write-Host "  - Updated PlatformToolset: v140 -> v143" -ForegroundColor Green
        $modified = $true
    }
    
    # Update PlatformToolset v141 -> v143 (if any)
    if ($content -match '<PlatformToolset>v141</PlatformToolset>') {
        $content = $content -replace '<PlatformToolset>v141</PlatformToolset>', '<PlatformToolset>v143</PlatformToolset>'
        Write-Host "  - Updated PlatformToolset: v141 -> v143" -ForegroundColor Green
        $modified = $true
    }
    
    # Update PlatformToolset v142 -> v143 (if any)
    if ($content -match '<PlatformToolset>v142</PlatformToolset>') {
        $content = $content -replace '<PlatformToolset>v142</PlatformToolset>', '<PlatformToolset>v143</PlatformToolset>'
        Write-Host "  - Updated PlatformToolset: v142 -> v143" -ForegroundColor Green
        $modified = $true
    }
    
    if ($modified) {
        Set-Content -Path $project.FullName -Value $content -NoNewline
        $updatedCount++
        Write-Host "  ✓ File saved" -ForegroundColor Green
    } else {
        Write-Host "  - No changes needed" -ForegroundColor Gray
    }
}

Write-Host "`n================================================" -ForegroundColor Cyan
Write-Host "Retargeting Complete!" -ForegroundColor Green
Write-Host "Updated $updatedCount out of $($projects.Count) projects" -ForegroundColor Cyan
Write-Host "================================================`n" -ForegroundColor Cyan
