"""
Targets - Build/test/run tasks (the verbs)
"""
from pathlib import Path
from typing import Optional, List
from .runner import Runner
from .artifacts import ArtifactManager
from . import config

def build(verbose: bool = False, clean: bool = False) -> bool:
    """Build C++ binaries with CMake"""
    runner = Runner("build", verbose=verbose)
    artifacts = ArtifactManager(verbose=verbose)
    
    if clean:
        artifacts.clean("build")
    
    # Configure
    print(f"{config.ARROW} Configuring CMake...")
    result = runner.bash(
        f"cd {config.ROOT} && cmake -B build -S . -DCMAKE_BUILD_TYPE=Release",
        cwd=config.ROOT
    )
    if not result:
        return False
    
    # Build
    print(f"{config.ARROW} Building C++ binaries...")
    result = runner.bash(
        f"cd {config.ROOT} && cmake --build build --config Release -j4",
        cwd=config.ROOT
    )
    
    return result.success

def test(pattern: str = "*", verbose: bool = False) -> bool:
    """Run C++ tests"""
    runner = Runner("test", verbose=verbose)
    
    print(f"{config.ARROW} Running tests: {pattern}")
    result = runner.bash(
        f"cd {config.BUILD_DIR} && ctest --output-on-failure -R '{pattern}'",
        cwd=config.BUILD_DIR
    )
    
    return result.success

def run_molecule(formula: str, optimize: bool = True, 
                xyz: bool = True, verbose: bool = False) -> Optional[Path]:
    """
    Run molecule builder (calls C++ CLI)
    
    Returns path to XYZ file if successful
    """
    runner = Runner(f"mol_{formula}", verbose=verbose)
    artifacts = ArtifactManager(verbose=verbose)
    
    # Check if binary exists
    if not config.VSEPR_CLI.exists():
        # Try WSL path
        wsl_cmd = f"wsl bash -c 'cd /mnt/c/Users/Liam/Desktop/vsepr-sim && ./build/bin/vsepr build {formula}"
        if optimize:
            wsl_cmd += " --optimize"
        output_file = config.OUT_DIR / f"{formula}.xyz"
        wsl_cmd += f" --output {output_file}'"
        
        print(f"{config.ARROW} Building {formula} (via WSL)...")
        result = runner.run(["powershell", "-c", wsl_cmd])
    else:
        # Build command
        cmd = [str(config.VSEPR_CLI), "build", formula]
        if optimize:
            cmd.append("--optimize")
        
        output_file = config.OUT_DIR / f"{formula}.xyz"
        cmd.extend(["--output", str(output_file)])
        
        print(f"{config.ARROW} Building {formula}...")
        result = runner.run(cmd, cwd=config.ROOT)
    
    if result.success and output_file.exists():
        print(f"{config.CHECKMARK} Created {output_file.name}")
        return output_file
    else:
        print(f"{config.CROSSMARK} Failed to create molecule")
        return None

def viz(molecule: str, mode: str = "default", 
        open_browser: bool = True, verbose: bool = False) -> Optional[Path]:
    """
    Generate HTML visualization
    
    Args:
        molecule: Name or XYZ file
        mode: Rendering mode (default, cartoon, etc.)
        open_browser: Auto-open in browser
    """
    runner = Runner(f"viz_{molecule}", verbose=verbose)
    
    # Find XYZ file
    if Path(molecule).suffix == ".xyz":
        xyz_file = Path(molecule)
    else:
        xyz_file = config.OUT_DIR / f"{molecule}.xyz"
    
    if not xyz_file.exists():
        print(f"{config.CROSSMARK} XYZ file not found: {xyz_file}")
        return None
    
    # Generate viewer
    print(f"{config.ARROW} Generating HTML viewer...")
    args = [str(xyz_file)]
    if open_browser:
        args.append("--open")
    
    result = runner.python(config.VIEWER_GEN, args=args)
    
    if result.success:
        html_file = xyz_file.parent / f"{xyz_file.stem}_viewer.html"
        if html_file.exists():
            print(f"{config.CHECKMARK} Viewer: {html_file.name}")
            return html_file
    
    return None

def export(molecule: str, formats: List[str] = None, verbose: bool = False) -> bool:
    """
    Export molecule in multiple formats
    
    Args:
        molecule: Molecule name
        formats: List of formats (xyz, html, json, png)
    """
    if formats is None:
        formats = ["xyz", "html"]
    
    artifacts = ArtifactManager(verbose=verbose)
    success = True
    
    print(f"{config.ARROW} Exporting {molecule}...")
    
    # XYZ
    if "xyz" in formats:
        src = config.OUT_DIR / f"{molecule}.xyz"
        if src.exists():
            artifacts.export_xyz(molecule, src)
        else:
            print(f"{config.CROSSMARK} XYZ not found")
            success = False
    
    # HTML
    if "html" in formats:
        src = config.OUT_DIR / f"{molecule}_viewer.html"
        if src.exists():
            artifacts.export_html(molecule, src)
        else:
            # Generate it
            xyz_path = viz(molecule, open_browser=False, verbose=verbose)
            if xyz_path:
                html_path = xyz_path.parent / f"{xyz_path.stem}_viewer.html"
                artifacts.export_html(molecule, html_path)
    
    return success

def report(molecule: str, format: str = "html", verbose: bool = False) -> Optional[Path]:
    """
    Generate analysis report
    
    Args:
        molecule: Molecule name
        format: Report format (html, pdf, markdown)
    """
    print(f"{config.ARROW} Generating {format} report for {molecule}...")
    
    # For now, just collect existing outputs
    artifacts = ArtifactManager(verbose=verbose)
    
    files = [
        config.OUT_DIR / f"{molecule}.xyz",
        config.OUT_DIR / f"{molecule}_viewer.html",
    ]
    
    existing = [f for f in files if f.exists()]
    
    if not existing:
        print(f"{config.CROSSMARK} No outputs found for {molecule}")
        return None
    
    # Package them
    output = artifacts.package_results(molecule, existing)
    return output

def clean(target: str = "all", verbose: bool = False) -> bool:
    """Clean build artifacts"""
    artifacts = ArtifactManager(verbose=verbose)
    return artifacts.clean(target)
