"""
Artifacts - File management (copy/export/zip/organize)
"""
import shutil
import zipfile
from pathlib import Path
from typing import List, Optional
from . import config

class ArtifactManager:
    """Manage build outputs, exports, and packaging"""
    
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        config.ensure_dirs()
    
    def copy(self, src: Path, dst: Path, overwrite: bool = True) -> bool:
        """Copy a file or directory"""
        try:
            if src.is_file():
                dst.parent.mkdir(parents=True, exist_ok=True)
                if overwrite or not dst.exists():
                    shutil.copy2(src, dst)
                    if self.verbose:
                        print(f"  {config.BULLET} Copied {src.name} → {dst}")
                    return True
            elif src.is_dir():
                if dst.exists() and overwrite:
                    shutil.rmtree(dst)
                shutil.copytree(src, dst)
                if self.verbose:
                    print(f"  {config.BULLET} Copied {src.name}/ → {dst}/")
                return True
            return False
        except Exception as e:
            print(f"{config.CROSSMARK} Copy failed: {e}")
            return False
    
    def export_xyz(self, molecule_name: str, src_xyz: Path) -> Path:
        """Export XYZ file to output directory"""
        dst = config.OUT_DIR / f"{molecule_name}.xyz"
        if self.copy(src_xyz, dst):
            return dst
        return None
    
    def export_html(self, molecule_name: str, src_html: Path) -> Path:
        """Export HTML viewer to output directory"""
        dst = config.OUT_DIR / f"{molecule_name}_viewer.html"
        if self.copy(src_html, dst):
            return dst
        return None
    
    def package_results(self, molecule_name: str, 
                       files: List[Path], output_zip: Optional[Path] = None) -> Path:
        """Package multiple files into a zip"""
        if output_zip is None:
            output_zip = config.OUT_DIR / f"{molecule_name}_results.zip"
        
        try:
            output_zip.parent.mkdir(parents=True, exist_ok=True)
            with zipfile.ZipFile(output_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
                for f in files:
                    if f.exists():
                        zf.write(f, f.name)
                        if self.verbose:
                            print(f"  {config.BULLET} Added {f.name}")
            
            print(f"{config.CHECKMARK} Package created: {output_zip.name}")
            return output_zip
        except Exception as e:
            print(f"{config.CROSSMARK} Package failed: {e}")
            return None
    
    def clean(self, target: str = "all") -> bool:
        """Clean build artifacts"""
        cleaned = []
        
        if target in ["all", "build"]:
            if config.BUILD_DIR.exists():
                shutil.rmtree(config.BUILD_DIR)
                cleaned.append("build/")
        
        if target in ["all", "out"]:
            if config.OUT_DIR.exists():
                shutil.rmtree(config.OUT_DIR)
                cleaned.append("out/")
        
        if target in ["all", "logs"]:
            if config.LOGS_DIR.exists():
                shutil.rmtree(config.LOGS_DIR)
                cleaned.append("logs/")
        
        if cleaned:
            print(f"{config.CHECKMARK} Cleaned: {', '.join(cleaned)}")
            return True
        else:
            print(f"{config.BULLET} Nothing to clean")
            return False
    
    def list_outputs(self) -> List[Path]:
        """List all files in output directory"""
        if not config.OUT_DIR.exists():
            return []
        return list(config.OUT_DIR.glob("*"))
