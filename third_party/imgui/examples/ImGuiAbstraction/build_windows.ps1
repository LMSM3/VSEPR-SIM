# PowerShell Build Script for ImGui Abstraction
# Provides better error handling and cross-version support

Write-Host "====================================" -ForegroundColor Cyan
Write-Host "ImGui Abstraction - Windows Build" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""

# Find Visual Studio
$vsPath = $null
$vsPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
    "C:\Program Files\Microsoft Visual Studio\18\Community",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
)

foreach ($path in $vsPaths) {
    if (Test-Path "$path\VC\Auxiliary\Build\vcvarsall.bat") {
        $vsPath = $path
        Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green
        break
    }
}

if (-not $vsPath) {
    Write-Host "ERROR: Visual Studio not found!" -ForegroundColor Red
    Write-Host "Please install Visual Studio with C++ Desktop Development workload" -ForegroundColor Yellow
    exit 1
}

# Setup VS environment
$vcvarsall = "$vsPath\VC\Auxiliary\Build\vcvarsall.bat"
Write-Host "Setting up build environment..." -ForegroundColor Yellow

# Create build directory
$buildDir = "build_cli"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Source files
$imguiSources = @(
    "..\..\imgui.cpp",
    "..\..\imgui_demo.cpp",
    "..\..\imgui_draw.cpp",
    "..\..\imgui_tables.cpp",
    "..\..\imgui_widgets.cpp"
)

$backendSources = @(
    "..\..\backends\imgui_impl_win32.cpp",
    "..\..\backends\imgui_impl_dx11.cpp"
)

$abstractionSources = @(
    "..\ImGuiRenderer.cpp",
    "..\example_main.cpp"
)

$compilerFlags = "/c /EHsc /std:c++17 /O2 /W3 /I.. /I..\..\backends /DUNICODE /D_UNICODE"
$linkerFlags = "/SUBSYSTEM:CONSOLE d3d11.lib dxgi.lib d3dcompiler.lib"

# Build function
function Invoke-VSBuild {
    param($Sources, $Description)
    
    Write-Host "Compiling $Description..." -ForegroundColor Yellow
    
    foreach ($src in $Sources) {
        $filename = Split-Path $src -Leaf
        Write-Host "  - $filename" -ForegroundColor Gray
        
        $cmd = "cmd /c `"$vcvarsall`" x64 `&`& cd $buildDir `&`& cl $compilerFlags $src 2>&1"
        $result = Invoke-Expression $cmd
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Failed to compile $filename" -ForegroundColor Red
            Write-Host $result -ForegroundColor Red
            exit 1
        }
    }
}

# Compile
Invoke-VSBuild -Sources $imguiSources -Description "ImGui core files"
Invoke-VSBuild -Sources $backendSources -Description "Backend files"
Invoke-VSBuild -Sources $abstractionSources -Description "Abstraction layer"

# Link
Write-Host "Linking executable..." -ForegroundColor Yellow
$objFiles = @(
    "imgui.obj", "imgui_demo.obj", "imgui_draw.obj", "imgui_tables.obj", "imgui_widgets.obj",
    "imgui_impl_win32.obj", "imgui_impl_dx11.obj",
    "ImGuiRenderer.obj", "example_main.obj"
)

$linkCmd = "cmd /c `"$vcvarsall`" x64 `&`& cd $buildDir `&`& link /OUT:imgui_abstraction_example.exe $($objFiles -join ' ') $linkerFlags 2>&1"
$linkResult = Invoke-Expression $linkCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to link executable" -ForegroundColor Red
    Write-Host $linkResult -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "====================================" -ForegroundColor Green
Write-Host "Build successful!" -ForegroundColor Green
Write-Host "====================================" -ForegroundColor Green
Write-Host "Executable: $buildDir\imgui_abstraction_example.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "To run:" -ForegroundColor Yellow
Write-Host "  cd $buildDir" -ForegroundColor White
Write-Host "  .\imgui_abstraction_example.exe" -ForegroundColor White
