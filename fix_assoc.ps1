$path = "C:\R\VSPER-SIM\installer\register-file-associations.ps1"
$txt = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)

# Replace .xyz context-menu section with full-takeover
$s = $txt.IndexOf("# -- 5/9  .xyz  (context-menu ONLY)")
$e = $txt.IndexOf("# -- 6/9  .vsxyz")
$ns = @"
# -- 5/9  .xyz  (full takeover -- real 3D viewer available) ------------------
Write-Host "[5/9] .xyz  (standard XYZ -- full default takeover)"
Register-FullTakeover ``
    -ProgId      "VSIMXYZFile" ``
    -Description "XYZ Molecule File" ``
    -DefaultVerb "Open" ``
    -OpenLabel   "Open in VSEPR-SIM 3D Viewer" ``
    -OpenCommand "`$LaunchQ `"%1`"" ``
    -Extensions  @(".xyz") ``
    -ExtraVerbs  @{
        Inspect = @{ Label="Inspect with VSEPR-SIM"; Command="`$ExeQ inspect `"%1`"" }
    }
Write-Host "  [ok] .xyz -> VSIMXYZFile  (double-click opens 3D viewer)" -ForegroundColor Green


"@
$txt = $txt.Substring(0,$s) + $ns + $txt.Substring($e)

# Fix unregister prog list
$txt = $txt.Replace(
    'foreach ($prog in @("VSIMFile","VSIMDynxFile","VSIMXFile","XYZFullFile",',
    'foreach ($prog in @("VSIMFile","VSIMDynxFile","VSIMXFile","XYZFullFile","VSIMXYZFile",'
)
# Fix unregister ext list + remove old context-menu cleanup line
$txt = $txt.Replace(
    '".vsxyz",".xyza",".xyzA",".xyzc",".xyzf",".xyzF")) {',
    '".xyz",".vsxyz",".xyza",".xyzA",".xyzc",".xyzf",".xyzF")) {'
)
$txt = $txt.Replace(
    "    Reg-Del `"HKEY_CURRENT_USER\Software\Classes\.xyz\shell\OpenWithVSEPRSIM`"",
    ""
)
# Update summary line
$txt = $txt.Replace(
    'Write-Host "  .xyz          -> right-click: Open with VSEPR-SIM (default unchanged)"',
    'Write-Host "  .xyz          -> double-click opens 3D viewer (full default takeover)"'
)

[System.IO.File]::WriteAllText($path, $txt, [System.Text.Encoding]::UTF8)
Write-Host "Done."