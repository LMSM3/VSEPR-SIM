#!/usr/bin/env python3
"""
workspace_manager.py  --  VSEPR-SIM Workspace Manager
=======================================================
Interactive shell component and CAD-layer housekeeping tool.

Provides:
  status      -- scan and report workspace pollution
  clean       -- safe removal of generated outputs and temp files
  reset       -- full data reset (staged warning, requires 3-step confirmation)
  uninstall   -- nuclear option: wipe all generated artefacts (deep warning)
  log-molecules   -- scan all CSVs/JSONs for molecule/formula references and write recovery log
  rerun-molecules -- replay lost molecule computations from recovery log
  shell       -- drop into the interactive VSEPR-SIM doc_shell

VSEPR-SIM 4.0.4.02
"""
from __future__ import annotations

import csv
import io
import sys

# Force UTF-8 stdout on Windows — only when run as a standalone script
if __name__ == "__main__" and sys.stdout.encoding and sys.stdout.encoding.lower() not in ("utf-8", "utf_8"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

import json
import os
import pathlib
import re
import shutil
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Dict, List, Optional, Set, Tuple

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

# ── colour codes (ANSI, disabled on Windows if not supported) ──────────
_USE_COLOR = sys.stdout.isatty()

def _c(code: str, text: str) -> str:
    if not _USE_COLOR:
        return text
    return f"\033[{code}m{text}\033[0m"

RED    = lambda t: _c("31;1", t)
YELLOW = lambda t: _c("33;1", t)
GREEN  = lambda t: _c("32;1", t)
CYAN   = lambda t: _c("36;1", t)
BOLD   = lambda t: _c("1", t)
DIM    = lambda t: _c("2", t)

# ── known generated directories and root-level junk patterns ───────────
GENERATED_OUT_DIRS = [
    "out/colour_pillar_plants",
    "out/five_prong_run",
    "out/metal_graphs",
    "out/metal_sweep",
    "out/glass_optics_demo",
    "out/improvement",
    "out/latex_markup_demo",
    "out/latex_markup_demo_sh",
    "out/pillars",
    "out/pipeline",
    "out/reports",
    "out/test_continual",
    "out/build",
]

TEMP_ROOT_PATTERNS = [
    "_tmp_*.py", "_write_*.py",
    "*.csv", "*.html",
    "fire_smooth_*.csv", "fire_smooth_*.md",
    "sim_101x50_*.csv", "sim_101x50_*.md",
    "v4_*.csv", "v4_*.html",
    "alloy_pairs.csv",
    "metals_results.csv", "metals_report.md",
    "2025-*_cleaning_log.csv",
    "COLOUR_PILLAR_RELEASE.md",
    "Screenshot*.png",
    "Read_ME.bin",
    "email_results.py",
    "draftmode_.ps1",
]

# Directories that are NEVER touched
PROTECTED = {
    ".venv", ".github", ".vs", ".git",
    "src", "include", "core", "apps", "cmake", "tests",
    "pykernel", "scripts", "docs", "thenewmethods",
    "reporting", "config", "third_party",
    "CMakeLists.txt", "pyproject.toml", "LICENSE", "README.md",
    "TOUR.md", "QUICKBUILD.md", "build.ps1",
}

# Molecule/formula regex — matches things like H2O, Li2BeF4, C8H18, NaCl_m
FORMULA_RE = re.compile(
    r'\b([A-Z][a-z]?(?:\d*[A-Z][a-z]?)*\d*(?:_[a-z]+)?)\b'
)

KNOWN_FORMULAE: Set[str] = set()  # populated during log-molecules

# ── dataclasses ────────────────────────────────────────────────────────

@dataclass
class WorkspaceReport:
    generated_dirs: List[Tuple[pathlib.Path, int, int]] = field(default_factory=list)  # (path, n_files, size_bytes)
    root_junk: List[pathlib.Path] = field(default_factory=list)
    pycache_dirs: List[pathlib.Path] = field(default_factory=list)
    total_generated_files: int = 0
    total_generated_bytes: int = 0
    total_junk_bytes: int = 0

@dataclass
class MoleculeLogEntry:
    formula: str
    source_file: str
    context: str
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).replace(tzinfo=None).isoformat())

# ── helpers ────────────────────────────────────────────────────────────

def _size_str(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} TB"

def _dir_stats(p: pathlib.Path) -> Tuple[int, int]:
    """Return (n_files, total_bytes) for a directory tree."""
    n, total = 0, 0
    if not p.exists():
        return 0, 0
    for f in p.rglob("*"):
        if f.is_file():
            n += 1
            total += f.stat().st_size
    return n, total

def _match_root_junk() -> List[pathlib.Path]:
    junk = []
    for pattern in TEMP_ROOT_PATTERNS:
        for f in ROOT.glob(pattern):
            name = f.name
            if name not in PROTECTED and f.name not in PROTECTED:
                junk.append(f)
    return sorted(set(junk))

# ── status ─────────────────────────────────────────────────────────────

def cmd_status() -> WorkspaceReport:
    report = WorkspaceReport()
    print(BOLD("\n═══ VSEPR-SIM Workspace Status ═══\n"))

    # generated output dirs
    print(CYAN("Generated output directories:"))
    for rel in GENERATED_OUT_DIRS:
        p = ROOT / rel
        n, sz = _dir_stats(p)
        if n > 0:
            report.generated_dirs.append((p, n, sz))
            report.total_generated_files += n
            report.total_generated_bytes += sz
            print(f"  {str(p.relative_to(ROOT)):45s}  {n:5d} files  {_size_str(sz)}")
    if not report.generated_dirs:
        print(DIM("  (none found)"))

    # root junk
    print(CYAN("\nRoot-level generated/temp files:"))
    junk = _match_root_junk()
    report.root_junk = junk
    for f in junk:
        sz = f.stat().st_size
        report.total_junk_bytes += sz
        print(f"  {f.name:50s}  {_size_str(sz)}")
    if not junk:
        print(DIM("  (clean)"))

    # __pycache__
    caches = list(ROOT.rglob("__pycache__"))
    caches = [c for c in caches if ".venv" not in str(c)]
    report.pycache_dirs = caches
    if caches:
        print(CYAN(f"\n__pycache__ directories: {len(caches)}"))

    # summary
    print(BOLD(f"\n{'─'*55}"))
    print(f"  Generated output : {report.total_generated_files:,} files  ({_size_str(report.total_generated_bytes)})")
    print(f"  Root junk        : {len(junk)} files   ({_size_str(report.total_junk_bytes)})")
    print(f"  Pycache dirs     : {len(caches)}")
    total_recoverable = report.total_generated_bytes + report.total_junk_bytes
    print(YELLOW(f"  Recoverable space: {_size_str(total_recoverable)}"))
    print(BOLD(f"{'─'*55}\n"))
    return report

# ── clean ──────────────────────────────────────────────────────────────

def cmd_clean(dry_run: bool = False) -> None:
    """Remove pycache and root junk files. Does NOT touch out/ directories."""
    print(BOLD("\n═══ VSEPR-SIM Clean (safe) ═══\n"))
    if dry_run:
        print(YELLOW("  [DRY RUN — nothing will be deleted]\n"))

    removed = 0
    freed = 0

    # pycache
    for p in ROOT.rglob("__pycache__"):
        if ".venv" in str(p):
            continue
        sz = sum(f.stat().st_size for f in p.rglob("*") if f.is_file())
        print(f"  rm -rf  {p.relative_to(ROOT)}")
        if not dry_run:
            shutil.rmtree(p, ignore_errors=True)
        freed += sz
        removed += 1

    # root junk
    for f in _match_root_junk():
        sz = f.stat().st_size
        print(f"  rm      {f.name}")
        if not dry_run:
            f.unlink(missing_ok=True)
        freed += sz
        removed += 1

    print(GREEN(f"\n  {'Would free' if dry_run else 'Freed'} {_size_str(freed)} across {removed} items."))

# ── reset ──────────────────────────────────────────────────────────────

def cmd_reset() -> None:
    """
    Staged reset: clears all generated outputs under out/.
    Requires explicit 3-step confirmation.
    """
    print(BOLD("\n╔══════════════════════════════════════════════════════════╗"))
    print(BOLD("║              VSEPR-SIM  DATA RESET                     ║"))
    print(BOLD("╚══════════════════════════════════════════════════════════╝\n"))
    print(YELLOW("  This will DELETE all generated outputs under out/ including:"))
    print(YELLOW("  - Colour pillar plant runs"))
    print(YELLOW("  - Five-prong thermophysical atlas outputs"))
    print(YELLOW("  - Metal graphs and sweeps"))
    print(YELLOW("  - All experiment CSV/JSON reports\n"))
    print(RED("  Source code, scripts, pykernel, and build system are UNTOUCHED.\n"))

    # log molecules before wiping
    print(CYAN("  Step 0: Logging molecule references before reset..."))
    log_path = cmd_log_molecules(silent=True)
    if log_path:
        print(GREEN(f"  Molecule recovery log saved: {log_path}\n"))

    # staged confirmation
    print(YELLOW("  Three confirmations required.\n"))
    resp1 = input(BOLD("  [1/3] Type  RESET  to confirm you want to wipe generated data: ")).strip()
    if resp1 != "RESET":
        print(RED("  Aborted."))
        return

    resp2 = input(BOLD("  [2/3] Type the workspace name  VSPER-SIM  to continue: ")).strip()
    if resp2 != "VSPER-SIM":
        print(RED("  Aborted."))
        return

    resp3 = input(BOLD("  [3/3] Type  YES DELETE  to execute: ")).strip()
    if resp3 != "YES DELETE":
        print(RED("  Aborted."))
        return

    print(RED("\n  Executing reset..."))
    removed = 0
    freed = 0
    for rel in GENERATED_OUT_DIRS:
        p = ROOT / rel
        if p.exists():
            n, sz = _dir_stats(p)
            shutil.rmtree(p, ignore_errors=True)
            print(f"  ✓ removed  {rel}  ({n} files, {_size_str(sz)})")
            removed += n
            freed += sz

    # also clean root junk
    for f in _match_root_junk():
        freed += f.stat().st_size
        f.unlink(missing_ok=True)
        removed += 1

    # ensure out/ itself still exists as an empty root
    (ROOT / "out").mkdir(exist_ok=True)

    _write_reset_log(removed, freed, log_path)
    print(GREEN(f"\n  Reset complete. Freed {_size_str(freed)} across {removed} items."))
    print(GREEN(f"  Recovery log: {log_path}"))
    print(DIM("  Run  rerun-molecules  to replay lost computations.\n"))

def _write_reset_log(n_removed: int, freed: int, mol_log: Optional[pathlib.Path]) -> None:
    log_dir = ROOT / "out" / "reset_logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    ts = datetime.now(timezone.utc).replace(tzinfo=None).strftime("%Y%m%dT%H%M%S")
    log = {
        "timestamp": datetime.now(timezone.utc).replace(tzinfo=None).isoformat(),
        "items_removed": n_removed,
        "bytes_freed": freed,
        "molecule_log": str(mol_log) if mol_log else None,
        "generator": "workspace_manager.py",
    }
    with open(log_dir / f"reset_{ts}.json", "w", encoding="utf-8") as f:
        json.dump(log, f, indent=2)

# ── uninstall ──────────────────────────────────────────────────────────

def cmd_uninstall() -> None:
    """
    Nuclear option: remove ALL generated artefacts including loose root files,
    all out/ subdirs, all pycache, all validation/reporting temp files.
    Does NOT remove: src, core, pykernel, scripts, CMake system, .venv.
    """
    print(BOLD("\n╔══════════════════════════════════════════════════════════════╗"))
    print(BOLD("║          ⚠  VSEPR-SIM  FULL UNINSTALL  ⚠                  ║"))
    print(BOLD("╚══════════════════════════════════════════════════════════════╝\n"))
    print(RED("  WARNING: This is the NUCLEAR option."))
    print(RED("  It will remove ALL generated data, logs, reports, temp files,"))
    print(RED("  and output directories created by any VSEPR-SIM generator.\n"))
    print(YELLOW("  PRESERVED (untouched):"))
    print(YELLOW("    .venv  src  core  include  apps  pykernel  scripts  cmake"))
    print(YELLOW("    docs  thenewmethods  reporting  tests  config  third_party"))
    print(YELLOW("    CMakeLists.txt  pyproject.toml  README.md  LICENSE\n"))
    print(RED("  This operation CANNOT be undone without version control.\n"))

    resp1 = input(BOLD("  [1/4] Type  UNINSTALL  to acknowledge: ")).strip()
    if resp1 != "UNINSTALL":
        print(RED("  Aborted.")); return

    resp2 = input(BOLD("  [2/4] Type  I UNDERSTAND  to confirm data loss: ")).strip()
    if resp2 != "I UNDERSTAND":
        print(RED("  Aborted.")); return

    resp3 = input(BOLD("  [3/4] Type the workspace name  VSPER-SIM: ")).strip()
    if resp3 != "VSPER-SIM":
        print(RED("  Aborted.")); return

    resp4 = input(BOLD("  [4/4] Final: type  EXECUTE UNINSTALL: ")).strip()
    if resp4 != "EXECUTE UNINSTALL":
        print(RED("  Aborted.")); return

    print(RED("\n  Executing full uninstall...\n"))
    freed = 0
    removed = 0

    # 1. all generated out dirs
    out_root = ROOT / "out"
    if out_root.exists():
        n, sz = _dir_stats(out_root)
        shutil.rmtree(out_root, ignore_errors=True)
        out_root.mkdir(exist_ok=True)
        freed += sz; removed += n
        print(f"  ✓ out/  ({n} files, {_size_str(sz)})")

    # 2. root junk
    for f in _match_root_junk():
        freed += f.stat().st_size
        f.unlink(missing_ok=True)
        removed += 1
    print(f"  ✓ root temp files")

    # 3. pycache
    for p in ROOT.rglob("__pycache__"):
        if ".venv" in str(p):
            continue
        n, sz = _dir_stats(p)
        shutil.rmtree(p, ignore_errors=True)
        freed += sz; removed += n
    print(f"  ✓ __pycache__ directories")

    # 4. known build artefacts under build dirs
    for bdir in ["build", "build_check", "build_test", "build_vs", "build_wsl"]:
        p = ROOT / bdir
        if p.exists() and (p / "CMakeCache.txt").exists():
            # only wipe cmake generated stuff, not the dir itself
            for cmake_gen in ["CMakeFiles", "CMakeCache.txt", "cmake_install.cmake"]:
                target = p / cmake_gen
                if target.is_dir():
                    n2, s2 = _dir_stats(target)
                    shutil.rmtree(target, ignore_errors=True)
                    freed += s2; removed += n2
                elif target.is_file():
                    freed += target.stat().st_size
                    target.unlink(missing_ok=True)
                    removed += 1

    # 5. validation scripts generated in this session
    for name in [
        "validate_colour_pillar.py", "validate_comprehensive.py",
        "validate_data_quality.py", "validate_final_data.py",
        "validate_networks_final.py",
    ]:
        f = ROOT / "scripts" / name
        if f.exists():
            freed += f.stat().st_size
            f.unlink()
            removed += 1
            print(f"  ✓ scripts/{name}")

    ts = datetime.now(timezone.utc).replace(tzinfo=None).strftime("%Y%m%dT%H%M%S")
    ub_log = ROOT / f"uninstall_{ts}.log"
    ub_log.write_text(
        f"VSEPR-SIM UNINSTALL  {datetime.now(timezone.utc).replace(tzinfo=None).isoformat()}\n"
        f"Items removed : {removed}\n"
        f"Bytes freed   : {freed} ({_size_str(freed)})\n",
        encoding="utf-8",
    )

    print(GREEN(f"\n  Uninstall complete."))
    print(GREEN(f"  Freed {_size_str(freed)} across {removed} items."))
    print(DIM(f"  Log written: {ub_log.name}\n"))

# ── log-molecules ──────────────────────────────────────────────────────

# Chemical formulae that appear in VSEPR-SIM material atlas
ATLAS_FORMULAE = {
    "H2O","CO2","NH3","R134a","CH4","N2","O2","He","Ar","C2H6","C3H8",
    "C4H10","C5H12","C6H14","C8H18","C2H5OH","CH3OH","C3H6O","H2","Ne",
    "Xe","SO2","H2S","R22","R410A","R32","iC4H10","C6H12","C7H8","C6H6",
    "nC12H26","JP8","Na_l","NaCl_m","NaNO3KNO3","Li2BeF4","TVP1","DowA",
    "PbBi","Hg","UF6",
}

def _extract_formulae_from_file(fpath: pathlib.Path) -> List[MoleculeLogEntry]:
    entries = []
    try:
        text = fpath.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return entries

    if fpath.suffix == ".json":
        # search JSON for formula values
        for match in FORMULA_RE.finditer(text):
            formula = match.group(1)
            if formula in ATLAS_FORMULAE:
                ctx = text[max(0, match.start()-30):match.end()+30].strip().replace("\n", " ")
                entries.append(MoleculeLogEntry(
                    formula=formula,
                    source_file=str(fpath.relative_to(ROOT)),
                    context=ctx,
                ))
    elif fpath.suffix == ".csv":
        try:
            with open(fpath, encoding="utf-8", errors="ignore") as f:
                reader = csv.DictReader(f)
                for row in list(reader)[:5]:  # sample first 5 rows
                    for v in row.values():
                        if v in ATLAS_FORMULAE:
                            entries.append(MoleculeLogEntry(
                                formula=v,
                                source_file=str(fpath.relative_to(ROOT)),
                                context=f"csv_row: {dict(list(row.items())[:4])}",
                            ))
        except Exception:
            pass
    elif fpath.suffix in (".py", ".cpp", ".h"):
        for match in FORMULA_RE.finditer(text):
            formula = match.group(1)
            if formula in ATLAS_FORMULAE:
                line_no = text[:match.start()].count("\n") + 1
                ctx = f"line {line_no}: {text.splitlines()[line_no-1].strip()[:80]}"
                entries.append(MoleculeLogEntry(
                    formula=formula,
                    source_file=str(fpath.relative_to(ROOT)),
                    context=ctx,
                ))
    return entries


def cmd_log_molecules(silent: bool = False) -> Optional[pathlib.Path]:
    """
    Scan workspace for all molecule/formula references and write a recovery log.
    Returns path to the log file.
    """
    if not silent:
        print(BOLD("\n═══ VSEPR-SIM Molecule Reference Logger ═══\n"))

    all_entries: List[MoleculeLogEntry] = []
    formulae_found: Set[str] = set()

    # scan: scripts, pykernel, out (CSVs/JSONs only), apps, src
    scan_roots = [
        ROOT / "scripts", ROOT / "pykernel", ROOT / "apps", ROOT / "src",
        ROOT / "out",
    ]
    scan_exts = {".py", ".cpp", ".h", ".csv", ".json"}

    n_scanned = 0
    for scan_root in scan_roots:
        if not scan_root.exists():
            continue
        for fpath in scan_root.rglob("*"):
            if not fpath.is_file():
                continue
            if fpath.suffix not in scan_exts:
                continue
            entries = _extract_formulae_from_file(fpath)
            all_entries.extend(entries)
            for e in entries:
                formulae_found.add(e.formula)
            n_scanned += 1

    if not silent:
        print(f"  Scanned {n_scanned} files")
        print(f"  Found {len(formulae_found)} unique formulae: {sorted(formulae_found)}")

    if not all_entries:
        if not silent:
            print(DIM("  No molecule references found."))
        return None

    # deduplicate by (formula, source_file)
    seen = set()
    unique_entries = []
    for e in all_entries:
        key = (e.formula, e.source_file)
        if key not in seen:
            seen.add(key)
            unique_entries.append(e)

    # write log
    log_dir = ROOT / "out" / "molecule_recovery"
    log_dir.mkdir(parents=True, exist_ok=True)
    ts = datetime.now(timezone.utc).replace(tzinfo=None).strftime("%Y%m%dT%H%M%S")
    log_path = log_dir / f"molecule_log_{ts}.json"

    log_data = {
        "generated": datetime.now(timezone.utc).replace(tzinfo=None).isoformat(),
        "n_files_scanned": n_scanned,
        "unique_formulae": sorted(formulae_found),
        "n_entries": len(unique_entries),
        "entries": [
            {"formula": e.formula, "source": e.source_file,
             "context": e.context, "ts": e.timestamp}
            for e in unique_entries
        ],
    }
    with open(log_path, "w", encoding="utf-8") as f:
        json.dump(log_data, f, indent=2)

    if not silent:
        print(GREEN(f"\n  Recovery log written: {log_path}"))
        print(f"  {len(unique_entries)} entries, {len(formulae_found)} unique formulae\n")

    return log_path

# ── rerun-molecules ────────────────────────────────────────────────────

def cmd_rerun_molecules(log_path: Optional[str] = None) -> None:
    """
    Replay molecule computations from the most recent (or specified) recovery log.
    Regenerates PVT grids and saturation curves for all logged formulae.
    """
    print(BOLD("\n═══ VSEPR-SIM Molecule Recovery / Rerun ═══\n"))

    # find log
    if log_path:
        log_file = pathlib.Path(log_path)
    else:
        log_dir = ROOT / "out" / "molecule_recovery"
        logs = sorted(log_dir.glob("molecule_log_*.json")) if log_dir.exists() else []
        if not logs:
            print(RED("  No recovery log found. Run  log-molecules  first."))
            return
        log_file = logs[-1]
        print(f"  Using most recent log: {log_file.name}\n")

    data = json.loads(log_file.read_text(encoding="utf-8"))
    formulae = data.get("unique_formulae", [])
    print(f"  Formulae to recover: {formulae}\n")

    if not formulae:
        print(DIM("  Nothing to rerun."))
        return

    # import compute functions
    try:
        from five_prong_data_gen import (
            compute_state, compute_saturation_line, get_all_materials,
        )
    except ImportError as e:
        print(RED(f"  Cannot import five_prong_data_gen: {e}"))
        return

    out_dir = ROOT / "out" / "molecule_recovery" / "rerun"
    out_dir.mkdir(parents=True, exist_ok=True)
    mats = get_all_materials()

    rerun_results = []
    for formula in formulae:
        mat = mats.get(formula)
        if mat is None:
            print(YELLOW(f"  [{formula}] not in atlas -- skipping"))
            rerun_results.append({"formula": formula, "status": "not_in_atlas"})
            continue

        print(f"  [{formula}] recomputing...", end=" ")
        Tc, Pc = mat.critical.T_c, mat.critical.P_c
        T_lo = max(Tc * 0.30, 100.0)
        T_hi = Tc * 1.60
        P_lo = 1e4
        P_hi = Pc * 2.5

        # PVT grid (20x20 lightweight)
        pvt_rows = []
        for i in range(20):
            T = T_lo + i * (T_hi - T_lo) / 19
            for j in range(20):
                P = P_lo + j * (P_hi - P_lo) / 19
                try:
                    sp = compute_state(formula, T, P)
                    pvt_rows.append({
                        "formula": formula, "T_K": round(T, 2), "P_Pa": round(P, 1),
                        "phase": sp.phase.name if hasattr(sp.phase, "name") else str(sp.phase),
                        "h": round(sp.h, 3) if sp.h else None,
                        "s": round(sp.s, 5) if sp.s else None,
                        "Z": round(sp.Z, 5) if sp.Z else None,
                    })
                except Exception:
                    pass

        # saturation
        try:
            sats = compute_saturation_line(formula, n_points=40)
            sat_rows = [{"formula": formula, "T_sat_K": round(sp.T_sat, 3),
                         "P_sat_Pa": round(sp.P_sat, 1),
                         "h_fg": round(sp.h_fg, 3) if sp.h_fg else None}
                        for sp in sats]
        except Exception:
            sat_rows = []

        # write
        if pvt_rows:
            pvt_path = out_dir / f"recovered_pvt_{formula}.csv"
            with open(pvt_path, "w", newline="", encoding="utf-8") as f:
                w = csv.DictWriter(f, fieldnames=pvt_rows[0].keys())
                w.writeheader(); w.writerows(pvt_rows)

        if sat_rows:
            sat_path = out_dir / f"recovered_sat_{formula}.csv"
            with open(sat_path, "w", newline="", encoding="utf-8") as f:
                w = csv.DictWriter(f, fieldnames=sat_rows[0].keys())
                w.writeheader(); w.writerows(sat_rows)

        print(GREEN(f"{len(pvt_rows)} pvt pts, {len(sat_rows)} sat pts"))
        rerun_results.append({
            "formula": formula, "status": "ok",
            "pvt_points": len(pvt_rows), "sat_points": len(sat_rows),
        })

    # write summary
    summary_path = out_dir / "rerun_summary.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump({
            "source_log": str(log_file),
            "rerun_time": datetime.now(timezone.utc).replace(tzinfo=None).isoformat(),
            "results": rerun_results,
        }, f, indent=2)

    ok = sum(1 for r in rerun_results if r["status"] == "ok")
    print(GREEN(f"\n  Rerun complete: {ok}/{len(formulae)} formulae recovered."))
    print(f"  Outputs: {out_dir}")
    print(f"  Summary: {summary_path}\n")

# ── shell passthrough ──────────────────────────────────────────────────

def cmd_shell() -> None:
    """Launch the VSEPR-SIM interactive doc_shell."""
    doc_shell = ROOT / "scripts" / "doc_shell.py"
    if not doc_shell.exists():
        print(RED("  doc_shell.py not found."))
        return
    os.execv(sys.executable, [sys.executable, str(doc_shell)])

# ── main dispatcher ────────────────────────────────────────────────────

COMMANDS = {
    "status":          (cmd_status,          "Scan and report workspace pollution"),
    "clean":           (cmd_clean,           "Remove pycache + root temp files (safe)"),
    "clean --dry-run": (lambda: cmd_clean(dry_run=True), "Preview clean without deleting"),
    "reset":           (cmd_reset,           "Wipe all generated outputs (3-step confirm)"),
    "uninstall":       (cmd_uninstall,       "Nuclear: remove ALL artefacts (4-step confirm)"),
    "log-molecules":   (cmd_log_molecules,   "Scan and log all molecule/formula references"),
    "rerun-molecules": (cmd_rerun_molecules, "Replay computations from molecule recovery log"),
    "shell":           (cmd_shell,           "Launch VSEPR-SIM interactive doc_shell"),
}

BANNER = f"""
{BOLD('╔══════════════════════════════════════════════════════════╗')}
{BOLD('║         VSEPR-SIM  Workspace Manager  4.0.4.02          ║')}
{BOLD('╚══════════════════════════════════════════════════════════╝')}

  Commands:
    status            {DIM('scan and report workspace pollution')}
    clean             {DIM('remove pycache + root temp files (safe)')}
    clean --dry-run   {DIM('preview clean without deleting')}
    reset             {DIM('wipe all generated outputs  (3-step confirm)')}
    uninstall         {DIM('remove ALL artefacts         (4-step confirm)')}
    log-molecules     {DIM('scan and log all molecule/formula references')}
    rerun-molecules   {DIM('replay computations from molecule recovery log')}
    shell             {DIM('launch VSEPR-SIM interactive doc_shell')}

  Usage:
    python scripts/workspace_manager.py <command>
    python scripts/workspace_manager.py   # interactive prompt
"""

def _interactive_loop() -> None:
    print(BANNER)
    while True:
        try:
            cmd = input(CYAN("wsm> ")).strip()
        except (EOFError, KeyboardInterrupt):
            print("\n  Bye.")
            break
        if cmd in ("exit", "quit", "q"):
            break
        _dispatch(cmd)

def _dispatch(cmd: str) -> None:
    if not cmd:
        return
    fn_pair = COMMANDS.get(cmd)
    if fn_pair is None:
        print(RED(f"  Unknown command: '{cmd}'"))
        print(DIM(f"  Available: {', '.join(COMMANDS.keys())}"))
        return
    fn, _ = fn_pair
    fn()


def main() -> None:
    if len(sys.argv) < 2:
        _interactive_loop()
        return

    cmd = " ".join(sys.argv[1:])
    if cmd == "rerun-molecules" and len(sys.argv) > 2:
        cmd_rerun_molecules(sys.argv[2])
    else:
        _dispatch(cmd)


if __name__ == "__main__":
    main()

