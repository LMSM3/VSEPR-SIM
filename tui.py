#!/usr/bin/env python3
"""
VSEPR Control Panel - Terminal TUI
Runs alongside active window; maps every internal command to a numbered action.
"""

import os
import sys
import json
import subprocess
import shutil
import datetime
from pathlib import Path

# ─── Paths ────────────────────────────────────────────────────────────────────
VSEPR_ROOT = Path(__file__).resolve().parent
TOOLS_DIR  = VSEPR_ROOT / "tools"
BUILD_DIR  = VSEPR_ROOT / "build"

sys.path.insert(0, str(TOOLS_DIR))

# ─── ANSI helpers ─────────────────────────────────────────────────────────────
RESET  = "\033[0m"
BOLD   = "\033[1m"
DIM    = "\033[2m"
RED    = "\033[31m"
GREEN  = "\033[32m"
YELLOW = "\033[33m"
CYAN   = "\033[36m"
MAGENTA= "\033[35m"
WHITE  = "\033[97m"
BG_DK  = "\033[48;5;235m"

def cls():
    os.system("cls" if os.name == "nt" else "clear")

def hline(ch="─", width=72):
    print(f"{DIM}{ch * width}{RESET}")

def header(text, width=72):
    pad = (width - len(text) - 4) // 2
    print(f"{CYAN}{BOLD}{'═' * pad}  {text}  {'═' * pad}{RESET}")

def ok(msg):   print(f"  {GREEN}✓{RESET} {msg}")
def warn(msg): print(f"  {YELLOW}!{RESET} {msg}")
def err(msg):  print(f"  {RED}✗{RESET} {msg}")
def dim(msg):  print(f"  {DIM}{msg}{RESET}")

# ─── Environment probe ───────────────────────────────────────────────────────
def find_python():
    for cmd in ("python3", "python"):
        if shutil.which(cmd):
            return cmd
    return None

def probe_build():
    """Return dict of target -> exists bool."""
    targets = {}
    if not BUILD_DIR.exists():
        return targets
    for f in BUILD_DIR.iterdir():
        if f.is_file() and f.stat().st_mode & 0o111 and not f.suffix:
            targets[f.name] = True
    return targets

def probe_python_tools():
    """Check which Python tools are importable."""
    status = {}
    for mod in ("scoring", "classification", "uniform_scoring",
                "chemical_validator", "discovery_pipeline"):
        try:
            __import__(mod)
            status[mod] = True
        except Exception as e:
            status[mod] = str(e)
    return status

# ─── Command registry ────────────────────────────────────────────────────────

class Command:
    def __init__(self, key, label, group, action, available=True, note=""):
        self.key = key
        self.label = label
        self.group = group
        self.action = action
        self.available = available
        self.note = note

COMMANDS = []

def register(key, label, group, action, available=True, note=""):
    COMMANDS.append(Command(key, label, group, action, available, note))

# ─── Actions ──────────────────────────────────────────────────────────────────

def action_build_all():
    """Full build (VIS=OFF)."""
    print()
    header("BUILDING ALL TARGETS")
    hline()
    os.chdir(str(BUILD_DIR))
    proc = subprocess.run(
        ["cmake", "..", "-DBUILD_VIS=OFF", "-DBUILD_GUI=OFF",
         "-DBUILD_TESTS=ON", "-DBUILD_APPS=ON"],
        capture_output=False
    )
    if proc.returncode != 0:
        err("cmake configure failed"); return
    proc = subprocess.run(["make", "-j4", "-k"], capture_output=False)
    if proc.returncode != 0:
        warn("Some targets failed (see output above)")
    else:
        ok("Build complete")
    os.chdir(str(VSEPR_ROOT))
    input(f"\n{DIM}Press Enter to continue...{RESET}")

def action_run_problem1():
    exe = BUILD_DIR / "problem1_two_body_lj"
    if not exe.exists():
        err("Not built"); return
    subprocess.run([str(exe)])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_run_problem2():
    exe = BUILD_DIR / "problem2_three_body_cluster"
    if not exe.exists():
        err("Not built"); return
    subprocess.run([str(exe)])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_run_qa():
    exe = BUILD_DIR / "qa_golden_tests"
    if not exe.exists():
        err("Not built"); return
    subprocess.run([str(exe)])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_run_meso_sim():
    exe = BUILD_DIR / "meso-sim"
    if not exe.exists():
        err("Not built"); return
    args = input(f"  {CYAN}args>{RESET} ").strip().split()
    subprocess.run([str(exe)] + args)
    input(f"\n{DIM}Press Enter...{RESET}")

def action_run_meso_discover():
    exe = BUILD_DIR / "meso-discover"
    if not exe.exists():
        err("Not built"); return
    args = input(f"  {CYAN}args>{RESET} ").strip().split()
    subprocess.run([str(exe)] + args)
    input(f"\n{DIM}Press Enter...{RESET}")

def action_run_meso_build():
    exe = BUILD_DIR / "meso-build"
    if not exe.exists():
        err("Not built"); return
    args = input(f"  {CYAN}args>{RESET} ").strip().split()
    subprocess.run([str(exe)] + args)
    input(f"\n{DIM}Press Enter...{RESET}")

def action_run_vsepr_cli():
    exe = BUILD_DIR / "vsepr"
    if not exe.exists():
        err("Not built"); return
    args = input(f"  {CYAN}args>{RESET} ").strip().split()
    subprocess.run([str(exe)] + args)
    input(f"\n{DIM}Press Enter...{RESET}")

def action_scoring_test():
    py = find_python()
    subprocess.run([py, str(TOOLS_DIR / "scoring.py")])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_classification_test():
    py = find_python()
    subprocess.run([py, str(TOOLS_DIR / "classification.py")])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_collect_formation():
    py = find_python()
    count = input(f"  Molecules/run [{CYAN}250{RESET}]: ").strip() or "250"
    runs  = input(f"  Runs [{CYAN}10{RESET}]: ").strip() or "10"
    seed  = input(f"  Seed [{CYAN}42{RESET}]: ").strip() or "42"
    out_default = str(Path.cwd() / "formation_output")
    out   = input(f"  Output [{CYAN}{out_default}{RESET}]: ").strip() or out_default
    subprocess.run([
        py, str(VSEPR_ROOT / "collect_formation_frequency.py"),
        "--per-run", count, "--runs", runs, "--seed", seed, "--output", out
    ])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_generate_baseline():
    py = find_python()
    count = input(f"  Count [{CYAN}100{RESET}]: ").strip() or "100"
    out_default = str(Path.cwd() / "out" / "baseline")
    out   = input(f"  Output [{CYAN}{out_default}{RESET}]: ").strip() or out_default
    subprocess.run([
        py, str(VSEPR_ROOT / "generate_baseline.py"),
        "--count", count, "--output", out
    ])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_view_formation():
    py = find_python()
    path = input(f"  Path [{CYAN}formation_output{RESET}]: ").strip() or "formation_output"
    # Find latest freq_ dir
    base = Path(path)
    freq_dirs = sorted(base.glob("freq_*"), reverse=True)
    if not freq_dirs:
        err(f"No freq_* directories in {path}"); return
    latest = freq_dirs[0]
    analysis = latest / "formation_frequency_analysis.json"
    if analysis.exists():
        data = json.loads(analysis.read_text())
        print(f"\n  {BOLD}Total molecules:{RESET} {data['total_molecules']}")
        print(f"  {BOLD}Unique formulas:{RESET} {data['unique_formulas']}")
        print(f"\n  {BOLD}Top formulas:{RESET}")
        for f, c in data["top_formulas"][:10]:
            pct = c / data["total_molecules"] * 100
            print(f"    {f:<20} {c:>5}  ({pct:.1f}%)")
    else:
        warn("No analysis file found")
    input(f"\n{DIM}Press Enter...{RESET}")

def action_compare_baselines():
    py = find_python()
    f1 = input(f"  Formation JSON: ").strip()
    f2 = input(f"  Baseline JSON:  ").strip()
    if not f1 or not f2:
        err("Need both paths"); return
    subprocess.run([py, str(VSEPR_ROOT / "compare_baselines.py"), f1, f2])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_ctest():
    os.chdir(str(BUILD_DIR))
    subprocess.run(["ctest", "--output-on-failure", "-j4"])
    os.chdir(str(VSEPR_ROOT))
    input(f"\n{DIM}Press Enter...{RESET}")

def action_build_status():
    targets = probe_build()
    py_mods = probe_python_tools()
    print()
    header("BUILD STATUS")
    hline()

    expected_exe = [
        "vsepr", "vsepr-cli", "meso-sim", "meso-discover",
        "meso-build", "meso-align", "meso-relax", "vsepr_batch",
        "problem1_two_body_lj", "problem2_three_body_cluster",
        "qa_golden_tests", "demo",
    ]
    for name in expected_exe:
        if name in targets:
            ok(name)
        else:
            err(f"{name} — NOT BUILT")

    failed = [
        ("test_application_validation", "missing atomistic/analysis/rdf.hpp"),
        ("test_safety_rails",           "IModel API mismatch"),
        ("test_batch_processing",       "needs BUILD_VIS=ON"),
        ("test_continuous_generation",  "needs BUILD_VIS=ON"),
        ("simple_nacl_test",            "API mismatch"),
    ]
    print(f"\n  {YELLOW}Known failures:{RESET}")
    for name, reason in failed:
        print(f"    {RED}✗{RESET} {name}: {DIM}{reason}{RESET}")

    print(f"\n  {CYAN}Python tools:{RESET}")
    for mod, st in py_mods.items():
        if st is True:
            ok(mod)
        else:
            err(f"{mod}: {st}")

    input(f"\n{DIM}Press Enter...{RESET}")

def action_shell():
    """Drop into a shell at VSEPR_ROOT."""
    shell = os.environ.get("SHELL", "/bin/bash")
    print(f"\n  {DIM}Spawning {shell} — type 'exit' to return{RESET}\n")
    subprocess.run([shell], cwd=str(VSEPR_ROOT))

def action_quit():
    print(f"\n{DIM}Goodbye.{RESET}\n")
    sys.exit(0)

# ─── Register commands ────────────────────────────────────────────────────────

builds   = probe_build()
py_ok    = find_python() is not None

register("1",  "Build all (VIS=OFF)",      "Build",       action_build_all)
register("2",  "Build status / diagnostics","Build",       action_build_status)
register("3",  "Run ctest suite",           "Build",       action_ctest, BUILD_DIR.exists())

register("4",  "problem1: two-body LJ",    "Validation",  action_run_problem1,  "problem1_two_body_lj" in builds)
register("5",  "problem2: three-body",      "Validation",  action_run_problem2,  "problem2_three_body_cluster" in builds)
register("6",  "QA golden tests",           "Validation",  action_run_qa,        "qa_golden_tests" in builds)

register("7",  "vsepr (unified CLI)",       "CLI Tools",   action_run_vsepr_cli, "vsepr" in builds)
register("8",  "meso-sim",                  "CLI Tools",   action_run_meso_sim,  "meso-sim" in builds)
register("9",  "meso-discover",             "CLI Tools",   action_run_meso_discover, "meso-discover" in builds)
register("10", "meso-build",               "CLI Tools",   action_run_meso_build,"meso-build" in builds)

register("11", "Scoring test",             "Python Tools", action_scoring_test,  py_ok)
register("12", "Classification test",      "Python Tools", action_classification_test, py_ok)

register("13", "Collect formation freq",   "Data Collection", action_collect_formation, py_ok)
register("14", "Generate baseline (--gen)","Data Collection", action_generate_baseline, py_ok)
register("15", "View formation results",   "Data Collection", action_view_formation,    py_ok)
register("16", "Compare baselines",        "Data Collection", action_compare_baselines, py_ok)

# New: Force computation commands
def action_compute_forces():
    exe = BUILD_DIR / "compute_forces"
    if not exe.exists():
        err("compute_forces not built (run command [1] first)"); return
    xyz = input(f"  {CYAN}XYZ file:{RESET} ").strip()
    if not xyz:
        err("Need input file"); return
    model = input(f"  {CYAN}Model [LJ+Coulomb]:{RESET} ").strip() or "LJ+Coulomb"
    xyzF = xyz.replace(".xyz", ".xyzF")
    subprocess.run([str(exe), "--input", xyz, "--model", model, "--output", xyzF, "--verbose"])
    input(f"\n{DIM}Press Enter...{RESET}")

def action_view_card_catalog():
    exe = BUILD_DIR / "card-viewer"
    if not exe.exists():
        err("card-viewer not built (needs BUILD_VIS=ON)"); return
    subprocess.run([str(exe)])
    input(f"\n{DIM}Press Enter...{RESET}")

compute_forces_ok = (BUILD_DIR / "compute_forces").exists()
card_viewer_ok = (BUILD_DIR / "card-viewer").exists()

register("17", "Compute forces",           "Analysis",        action_compute_forces,    compute_forces_ok)
register("18", "View card catalog",        "Visualization",   action_view_card_catalog, card_viewer_ok)

register("s",  "Shell (interactive)",      "Utility",     action_shell)
register("q",  "Quit",                    "Utility",     action_quit)


# ─── Main loop ────────────────────────────────────────────────────────────────

def draw_menu():
    cls()
    print()
    header("VSEPR CONTROL PANEL", 72)
    print(f"  {DIM}Root: {VSEPR_ROOT}{RESET}")
    print(f"  {DIM}CWD:  {Path.cwd()}{RESET}")
    hline()

    current_group = None
    for cmd in COMMANDS:
        if cmd.group != current_group:
            current_group = cmd.group
            print(f"\n  {BOLD}{MAGENTA}{current_group}{RESET}")

        avail_tag = f"{GREEN}●{RESET}" if cmd.available else f"{RED}○{RESET}"
        key_tag   = f"{CYAN}{cmd.key:>3}{RESET}"
        label     = cmd.label if cmd.available else f"{DIM}{cmd.label}{RESET}"
        note      = f"  {DIM}({cmd.note}){RESET}" if cmd.note else ""
        print(f"   {avail_tag} [{key_tag}]  {label}{note}")

    hline()

def main():
    while True:
        draw_menu()
        choice = input(f"\n  {WHITE}{BOLD}>{RESET} ").strip().lower()

        matched = None
        for cmd in COMMANDS:
            if cmd.key == choice:
                matched = cmd
                break

        if not matched:
            warn(f"Unknown command: '{choice}'")
            input(f"  {DIM}Press Enter...{RESET}")
            continue

        if not matched.available:
            err(f"'{matched.label}' not available (target not built or dependency missing)")
            input(f"  {DIM}Press Enter...{RESET}")
            continue

        print()
        try:
            matched.action()
        except KeyboardInterrupt:
            warn("Interrupted")
            input(f"\n  {DIM}Press Enter...{RESET}")
        except Exception as e:
            err(f"Error: {e}")
            input(f"\n  {DIM}Press Enter...{RESET}")

if __name__ == "__main__":
    main()
