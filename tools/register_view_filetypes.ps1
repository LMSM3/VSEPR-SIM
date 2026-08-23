# register_view_filetypes.ps1
# ===========================
# Associates XYZ-family artifacts with the supported live `vsepr-view`.

param(
    [string]$ViewerPath = "",
    [switch]$Unregister
)

$extensions = @(
    @{ Ext = ".xyz"; Desc = "XYZ Molecular Geometry" },
    @{ Ext = ".xyza"; Desc = "XYZA Extended Molecular Geometry" },
    @{ Ext = ".xyzf"; Desc = "XYZF Trajectory File" },
    @{ Ext = ".xyzFull"; Desc = "XYZFull Rich Trajectory" },
    @{ Ext = ".xyzc"; Desc = "XYZC Checkpoint File" },
    @{ Ext = ".dynx"; Desc = "DYNX Dynamic Session Archive" }
)

if (-not $ViewerPath) {
    $candidates = @(
        (Join-Path $PSScriptRoot "..\build\vsepr-view.exe"),
        (Join-Path $PSScriptRoot "..\build_vis\vsepr-view.exe"),
        "vsepr-view.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { $ViewerPath = (Resolve-Path $candidate).Path; break }
    }
}

if (-not $Unregister -and -not (Test-Path $ViewerPath)) {
    Write-Error "vsepr-view.exe not found. Specify -ViewerPath explicitly."
    exit 1
}

foreach ($entry in $extensions) {
    $extension = $entry.Ext
    $progId = "VSEPR-SIM.LiveViewer" + $extension.TrimStart(".")
    if ($Unregister) {
        Remove-Item -Path "HKCU:\Software\Classes\$extension" -Recurse -ErrorAction SilentlyContinue
        Remove-Item -Path "HKCU:\Software\Classes\$progId" -Recurse -ErrorAction SilentlyContinue
        continue
    }

    New-Item -Path "HKCU:\Software\Classes\$progId" -Force | Out-Null
    Set-ItemProperty "HKCU:\Software\Classes\$progId" -Name "(Default)" -Value $entry.Desc
    New-Item -Path "HKCU:\Software\Classes\$progId\DefaultIcon" -Force | Out-Null
    Set-ItemProperty "HKCU:\Software\Classes\$progId\DefaultIcon" -Name "(Default)" -Value "`"$ViewerPath`",0"
    New-Item -Path "HKCU:\Software\Classes\$progId\shell\open\command" -Force | Out-Null
    Set-ItemProperty "HKCU:\Software\Classes\$progId\shell\open\command" -Name "(Default)" -Value "`"$ViewerPath`" --artifact `"%1`""
    New-Item -Path "HKCU:\Software\Classes\$extension" -Force | Out-Null
    Set-ItemProperty "HKCU:\Software\Classes\$extension" -Name "(Default)" -Value $progId
}

Write-Host $(if ($Unregister) { "VSEPR-SIM live-viewer associations removed." } else { "VSEPR-SIM live-viewer associations registered." })
