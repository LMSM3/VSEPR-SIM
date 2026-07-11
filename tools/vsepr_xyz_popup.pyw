"""
vsepr_xyz_popup.pyw
-------------------
GUI-only popup launcher for .xyz and .vsxyz files.

Double-click a .xyz or .vsxyz file → shows a Tk inspection window with:
  - atom count, comment line, coordinate preview
  - element composition badges (CPK colours)
  - VSEPR metadata parsed from the comment line (if present)
  - Run Test → invokes qa_random_tests or qa_golden_tests
  - Open Folder → reveals file in Explorer
  - Save as .xyz → strips .vsxyz metadata prefix
  - Close

Invoked via installer/bin/open_xyz.cmd (installed) or directly via pythonw.exe.
No console window — uses pythonw.exe.

VSEPR-SIM v5 research platform — deterministic atomistic simulation.
"""

from __future__ import annotations

import os
import sys
import subprocess
from pathlib import Path
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext

# ---------------------------------------------------------------------------
# VSEPR-SIM project root detection
# ---------------------------------------------------------------------------

def find_project_root() -> Path:
    """Locate the VSEPR-SIM root in both dev-tree and installed layouts.

    Dev layout  : …/VSEPR-SIM/tools/vsepr_xyz_popup.pyw  (CMakeLists.txt sibling)
    Installed   : {app}/bin/vsepr_xyz_popup.pyw            (bin/ sibling of data/)
    """
    here = Path(__file__).resolve()

    # Installed layout: script lives in {app}/bin/ — project root is {app}
    installed_root = here.parent.parent
    if (installed_root / "data").exists() and (installed_root / "bin").exists():
        return installed_root

    # Dev layout: walk up looking for CMakeLists.txt + atomistic/
    p = here.parent
    for _ in range(6):
        if (p / "CMakeLists.txt").exists() and (p / "atomistic").exists():
            return p
        p = p.parent

    return here.parent.parent  # last-resort fallback


PROJECT_ROOT = find_project_root()
# Installed: executables live in {app}/bin; dev: build/
BUILD_DIR = (
    PROJECT_ROOT / "bin"
    if (PROJECT_ROOT / "bin" / "qa_random_tests.exe").exists()
    else PROJECT_ROOT / "build"
)


# ---------------------------------------------------------------------------
# Element symbol table (Z → symbol, for display)
# ---------------------------------------------------------------------------

Z_TO_SYMBOL = {
    1: "H", 2: "He", 3: "Li", 4: "Be", 5: "B", 6: "C", 7: "N", 8: "O",
    9: "F", 10: "Ne", 11: "Na", 12: "Mg", 13: "Al", 14: "Si", 15: "P",
    16: "S", 17: "Cl", 18: "Ar", 19: "K", 20: "Ca", 26: "Fe", 29: "Cu",
    30: "Zn", 35: "Br", 53: "I", 54: "Xe",
}

# CPK-ish colours for element badges
ELEMENT_COLORS = {
    "H": "#FFFFFF", "C": "#404040", "N": "#3050F8", "O": "#FF0D0D",
    "F": "#90E050", "P": "#FF8000", "S": "#FFFF30", "Cl": "#1FF01F",
    "Fe": "#E06633", "Cu": "#C88033", "Zn": "#7D80B0", "Si": "#F0C8A0",
    "Br": "#A62929", "I": "#940094", "Xe": "#429EB0",
}


# ---------------------------------------------------------------------------
# XYZ / VSXYZ parser
# ---------------------------------------------------------------------------

def parse_xyz(path: Path) -> dict:
    """Parse a .xyz or .vsxyz file and return summary info."""
    text = path.read_text(encoding="utf-8", errors="replace").splitlines()

    declared_atoms = None
    comment = ""
    atoms = []
    elements = {}

    if text:
        try:
            declared_atoms = int(text[0].strip())
        except ValueError:
            declared_atoms = None

    if len(text) > 1:
        comment = text[1].strip()

    for line in text[2:]:
        parts = line.split()
        if len(parts) >= 4:
            elem = parts[0]
            try:
                x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                atoms.append((elem, x, y, z))
                elements[elem] = elements.get(elem, 0) + 1
            except ValueError:
                continue

    # Parse VSEPR metadata from comment line
    metadata = {}
    if comment:
        for token in comment.split("|"):
            token = token.strip().lstrip("#").strip()
            if "=" in token:
                k, v = token.split("=", 1)
                metadata[k.strip()] = v.strip()

    return {
        "declared_atoms": declared_atoms,
        "actual_atoms": len(atoms),
        "comment": comment,
        "elements": elements,
        "atoms": atoms[:50],  # first 50 for preview
        "metadata": metadata,
        "total_lines": len(text),
    }


# ---------------------------------------------------------------------------
# Actions
# ---------------------------------------------------------------------------

def run_test(target: Path) -> None:
    """Launch qa_random_tests or qa_golden_tests for the given file."""
    exe = BUILD_DIR / "qa_random_tests.exe"
    if not exe.exists():
        exe = BUILD_DIR / "qa_golden_tests.exe"
    if not exe.exists():
        messagebox.showwarning(
            title="VSEPR-SIM",
            message="Test executable not found.",
            detail=f"Looked in: {BUILD_DIR}\n\nBuild with: cmake --build build --target qa_random_tests",
        )
        return

    try:
        subprocess.Popen(
            [str(exe), "--count", "5", "--seed", "42",
             "--output", str(target.parent / "vsxyz_test_run")],
            cwd=str(BUILD_DIR),
            creationflags=subprocess.CREATE_NEW_CONSOLE if sys.platform == "win32" else 0,
        )
        messagebox.showinfo(
            title="VSEPR-SIM",
            message="Test launched.",
            detail=f"Running: {exe.name}\nOutput: {target.parent / 'vsxyz_test_run'}",
        )
    except Exception as exc:
        messagebox.showerror(
            title="VSEPR-SIM",
            message="Failed to launch test.",
            detail=f"{type(exc).__name__}: {exc}",
        )


def open_folder(target: Path) -> None:
    """Reveal the file in Explorer / Finder."""
    if sys.platform == "win32":
        subprocess.Popen(["explorer", "/select,", str(target)])
    elif sys.platform == "darwin":
        subprocess.Popen(["open", "-R", str(target)])
    else:
        subprocess.Popen(["xdg-open", str(target.parent)])


def convert_to_xyz(target: Path) -> None:
    """Save a copy as plain .xyz (strip VSEPR metadata prefix)."""
    dest = target.with_suffix(".xyz")
    if dest.exists():
        if not messagebox.askyesno("VSEPR-SIM", f"Overwrite {dest.name}?"):
            return
    text = target.read_text(encoding="utf-8", errors="replace")
    dest.write_text(text, encoding="utf-8")
    messagebox.showinfo("VSEPR-SIM", f"Saved: {dest.name}")


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------

def show_popup(target: Path) -> None:
    info = parse_xyz(target)

    root = tk.Tk()
    root.title(f"VSEPR-SIM — {target.name}")
    root.geometry("640x500")
    root.resizable(True, True)
    root.configure(bg="#f0f0f0")

    # Try to set icon
    ico = PROJECT_ROOT / "assets" / "vsepr.ico"
    if ico.exists():
        try:
            root.iconbitmap(str(ico))
        except Exception:
            pass

    style = ttk.Style()
    style.configure("Header.TLabel", font=("Segoe UI", 13, "bold"))
    style.configure("Sub.TLabel", font=("Segoe UI", 9))

    frame = ttk.Frame(root, padding=14)
    frame.pack(fill="both", expand=True)

    # --- Header ---
    hdr = ttk.Frame(frame)
    hdr.pack(fill="x")
    ttk.Label(hdr, text="VSEPR-SIM File Probe", style="Header.TLabel").pack(side="left")

    ext = target.suffix.lower()
    ext_label = ".vsxyz" if ext == ".vsxyz" else ext
    ttk.Label(hdr, text=f"[{ext_label}]", foreground="#666").pack(side="left", padx=8)

    # --- File info ---
    info_frame = ttk.LabelFrame(frame, text="File", padding=8)
    info_frame.pack(fill="x", pady=(8, 0))

    ttk.Label(info_frame, text=f"Name: {target.name}").pack(anchor="w")
    ttk.Label(info_frame, text=f"Path: {target.parent}", style="Sub.TLabel",
              foreground="#555").pack(anchor="w")

    declared = info["declared_atoms"]
    declared_text = "?" if declared is None else str(declared)

    row = ttk.Frame(info_frame)
    row.pack(fill="x", pady=(4, 0))
    ttk.Label(row, text=f"Declared atoms: {declared_text}").pack(side="left")
    ttk.Label(row, text=f"   Coordinate rows: {info['actual_atoms']}").pack(side="left")
    ttk.Label(row, text=f"   Lines: {info['total_lines']}").pack(side="left")

    # --- Element composition ---
    if info["elements"]:
        elem_frame = ttk.LabelFrame(frame, text="Composition", padding=8)
        elem_frame.pack(fill="x", pady=(8, 0))

        elem_row = ttk.Frame(elem_frame)
        elem_row.pack(fill="x")
        for elem, count in sorted(info["elements"].items(), key=lambda x: -x[1]):
            color = ELEMENT_COLORS.get(elem, "#CCCCCC")
            badge = tk.Label(elem_row, text=f" {elem}:{count} ",
                             bg=color,
                             fg="white" if elem in ("C", "Fe", "Br", "I") else "black",
                             font=("Consolas", 9, "bold"),
                             relief="raised", bd=1)
            badge.pack(side="left", padx=2)

    # --- Metadata ---
    meta = info["metadata"]
    if meta:
        meta_frame = ttk.LabelFrame(frame, text="VSEPR Metadata", padding=8)
        meta_frame.pack(fill="x", pady=(8, 0))
        for k, v in meta.items():
            ttk.Label(meta_frame, text=f"{k}: {v}", style="Sub.TLabel").pack(anchor="w")

    # --- Comment / Preview ---
    preview_frame = ttk.LabelFrame(frame, text="Comment & Coordinate Preview", padding=8)
    preview_frame.pack(fill="both", expand=True, pady=(8, 0))

    comment_text = info["comment"] or "(no comment line)"
    ttk.Label(preview_frame, text=comment_text, wraplength=580,
              style="Sub.TLabel").pack(anchor="w")

    # Coordinate preview (scrollable)
    coord_box = scrolledtext.ScrolledText(preview_frame, height=8, width=70,
                                           font=("Consolas", 9), state="normal")
    coord_box.pack(fill="both", expand=True, pady=(4, 0))
    for elem, x, y, z in info["atoms"]:
        coord_box.insert("end", f"  {elem:<3s} {x:12.6f} {y:12.6f} {z:12.6f}\n")
    if info["actual_atoms"] > 50:
        coord_box.insert("end", f"\n  ... ({info['actual_atoms'] - 50} more atoms)\n")
    coord_box.configure(state="disabled")

    # --- Buttons ---
    btn_frame = ttk.Frame(frame)
    btn_frame.pack(fill="x", pady=(12, 0))

    ttk.Button(btn_frame, text="▶ Run Test",
               command=lambda: run_test(target)).pack(side="left")
    ttk.Button(btn_frame, text="📁 Open Folder",
               command=lambda: open_folder(target)).pack(side="left", padx=6)
    ttk.Button(btn_frame, text="💾 Save as .xyz",
               command=lambda: convert_to_xyz(target)).pack(side="left", padx=6)
    ttk.Button(btn_frame, text="Close",
               command=root.destroy).pack(side="right")

    # Center on screen
    root.update_idletasks()
    w, h = root.winfo_width(), root.winfo_height()
    x = (root.winfo_screenwidth() - w) // 2
    y = (root.winfo_screenheight() - h) // 2
    root.geometry(f"+{x}+{y}")

    root.mainloop()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> int:
    if len(sys.argv) < 2:
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror(
            title="VSEPR-SIM",
            message="No file was passed to the popup launcher.",
            detail="Double-click a .vsxyz file, or pass a path as argument.",
        )
        return 1

    target = Path(sys.argv[1]).expanduser().resolve()

    if not target.exists():
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror(
            title="VSEPR-SIM",
            message="Selected file does not exist.",
            detail=str(target),
        )
        return 1

    try:
        show_popup(target)
        return 0
    except Exception as exc:
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror(
            title="VSEPR-SIM crash",
            message="Launcher failed.",
            detail=f"{type(exc).__name__}: {exc}",
        )
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
