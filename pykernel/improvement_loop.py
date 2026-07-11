"""
improvement_loop.py — Phase C: Closed slow-loop autonomous improvement.

Scans the codebase for files flagged for destruction or improvement,
generates report data from simulation runs, and queues improvement tasks.

The loop:
  1. Scan source tree for flagged files (TODO, FIXME, HACK, DEPRECATED,
     "flagged for destruction", disabled targets in CMake)
  2. Run simulations to collect fresh data
  3. Compare current results against golden baselines
  4. Generate improvement reports (what regressed, what improved)
  5. Write task queue for next iteration

This is a "slow loop" — it runs end-to-end in minutes or hours,
not milliseconds. The value is cumulative: each pass produces
data that informs the next pass.

Anti-black-box: every scan result, every comparison, every flagged
file is written to disk as structured data.
"""

import os
import re
import json
import csv
import hashlib
import subprocess
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass, field


@dataclass
class FlaggedFile:
    """A source file flagged for attention."""
    path: str
    flags: List[str]           # e.g. ["TODO", "FIXME", "DEPRECATED"]
    flag_count: int = 0
    line_numbers: List[int] = field(default_factory=list)
    snippets: List[str] = field(default_factory=list)
    priority: int = 0          # Higher = more urgent
    file_hash: str = ""

    def to_dict(self) -> dict:
        return {
            "path": self.path,
            "flags": self.flags,
            "flag_count": self.flag_count,
            "line_numbers": self.line_numbers,
            "snippets": self.snippets[:10],  # Limit for readability
            "priority": self.priority,
            "file_hash": self.file_hash,
        }


@dataclass
class SimulationResult:
    """Result from a single simulation run."""
    spec: str
    energy: Optional[float] = None
    force_norm: Optional[float] = None
    n_steps: int = 0
    converged: bool = False
    wall_time: float = 0.0
    returncode: int = -1
    error: str = ""

    def to_dict(self) -> dict:
        return vars(self)


@dataclass
class ImprovementReport:
    """Report from one improvement loop iteration."""
    iteration: int
    timestamp: str
    flagged_files: List[FlaggedFile]
    simulation_results: List[SimulationResult]
    regressions: List[Dict]
    improvements: List[Dict]
    tasks: List[Dict]
    summary: Dict

    def to_dict(self) -> dict:
        return {
            "iteration": self.iteration,
            "timestamp": self.timestamp,
            "flagged_files_count": len(self.flagged_files),
            "flagged_files": [f.to_dict() for f in self.flagged_files],
            "simulation_results": [s.to_dict() for s in self.simulation_results],
            "regressions": self.regressions,
            "improvements": self.improvements,
            "tasks": self.tasks,
            "summary": self.summary,
        }


# Patterns that flag files for attention
FLAG_PATTERNS = {
    "TODO": (re.compile(r'\bTODO\b', re.IGNORECASE), 1),
    "FIXME": (re.compile(r'\bFIXME\b', re.IGNORECASE), 3),
    "HACK": (re.compile(r'\bHACK\b', re.IGNORECASE), 2),
    "DEPRECATED": (re.compile(r'\bDEPRECATED\b', re.IGNORECASE), 2),
    "DISABLED": (re.compile(r'(?://|#)\s*(?:DISABLED|disabled)', re.IGNORECASE), 2),
    "DESTRUCTION": (re.compile(r'flag(?:ged)?\s+for\s+destruction', re.IGNORECASE), 5),
    "QUARANTINED": (re.compile(r'\bquarantined\b', re.IGNORECASE), 4),
    "BROKEN": (re.compile(r'\.broken\b|\.old\b|\bbroken\b', re.IGNORECASE), 3),
    "REMOVE": (re.compile(r'\bwill\s+be\s+removed\b', re.IGNORECASE), 4),
    "TEMPORARY": (re.compile(r'\btemporary\b|\btemp\s+fix\b', re.IGNORECASE), 1),
}

# File extensions to scan
SCAN_EXTENSIONS = {
    ".cpp", ".hpp", ".h", ".c", ".cu",
    ".py", ".sh", ".ps1", ".bat",
    ".cmake", ".txt",  # CMakeLists.txt
    ".md", ".tex",
}

# Directories to skip
SKIP_DIRS = {
    "third_party", "build", "cmake-build-debug", "cmake-build-release",
    ".git", "__pycache__", "node_modules", ".vs", "out",
    ".venv", "venv", "env", ".env", ".pytest_cache",
}


class ImprovementLoop:
    """
    Phase C: Closed slow-loop for autonomous codebase improvement.

    Scans for flagged files, runs simulations, compares against baselines,
    generates improvement reports, and queues tasks.

    Usage:
        loop = ImprovementLoop()
        report = loop.run_iteration()
        loop.save_report(report)

        # Or run continuously:
        loop.run_loop(max_iterations=10)
    """

    def __init__(self, project_root: Optional[str] = None,
                 output_dir: str = "out/improvement",
                 baseline_path: Optional[str] = None):
        self._root = Path(project_root) if project_root else Path(__file__).parent.parent.absolute()
        self._output_dir = Path(output_dir)
        self._output_dir.mkdir(parents=True, exist_ok=True)

        self._baseline_path = Path(baseline_path) if baseline_path else self._output_dir / "baseline.json"
        self._baseline = self._load_baseline()
        self._iteration = self._get_last_iteration() + 1

        # Standard simulation specs for regression testing
        self._test_specs = [
            "H2O",
            "NaCl@crystal",
            "Fe@crystal",
            "C6H6",
            "Ar",
            "CO2",
        ]

    def _load_baseline(self) -> Dict:
        """Load baseline results for comparison."""
        if self._baseline_path.exists():
            try:
                with open(self._baseline_path, "r") as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                pass
        return {}

    def _save_baseline(self, results: List[SimulationResult]):
        """Save current results as new baseline."""
        baseline = {}
        for r in results:
            if r.energy is not None:
                baseline[r.spec] = {
                    "energy": r.energy,
                    "force_norm": r.force_norm,
                    "n_steps": r.n_steps,
                    "converged": r.converged,
                    "timestamp": datetime.now().isoformat(),
                }
        with open(self._baseline_path, "w") as f:
            json.dump(baseline, f, indent=2)

    def _get_last_iteration(self) -> int:
        """Find the last iteration number from saved reports."""
        max_iter = -1
        for p in self._output_dir.glob("report_*.json"):
            try:
                n = int(p.stem.split("_")[1])
                max_iter = max(max_iter, n)
            except (IndexError, ValueError):
                pass
        return max_iter

    # ========================================================================
    # Phase C.1: Scan for flagged files
    # ========================================================================

    def scan_flagged_files(self) -> List[FlaggedFile]:
        """Scan source tree for files flagged for destruction or improvement."""
        flagged = []

        for root, dirs, files in os.walk(self._root):
            # Skip excluded directories
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            rel_root = os.path.relpath(root, self._root)

            for fname in files:
                ext = os.path.splitext(fname)[1].lower()
                if ext not in SCAN_EXTENSIONS:
                    continue

                fpath = os.path.join(root, fname)
                rel_path = os.path.relpath(fpath, self._root)

                try:
                    with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                        lines = f.readlines()
                except (IOError, OSError):
                    continue

                file_flags = []
                file_lines = []
                file_snippets = []
                total_priority = 0

                for i, line in enumerate(lines, 1):
                    for flag_name, (pattern, priority) in FLAG_PATTERNS.items():
                        if pattern.search(line):
                            if flag_name not in file_flags:
                                file_flags.append(flag_name)
                            file_lines.append(i)
                            file_snippets.append(f"L{i}: {line.rstrip()[:120]}")
                            total_priority += priority

                if file_flags:
                    # File content hash for change detection
                    content = "".join(lines)
                    fhash = hashlib.sha256(content.encode("utf-8", errors="replace")).hexdigest()[:12]

                    flagged.append(FlaggedFile(
                        path=rel_path,
                        flags=file_flags,
                        flag_count=len(file_lines),
                        line_numbers=file_lines[:50],
                        snippets=file_snippets[:20],
                        priority=total_priority,
                        file_hash=fhash,
                    ))

        # Sort by priority descending
        flagged.sort(key=lambda f: f.priority, reverse=True)
        return flagged

    def scan_cmake_disabled(self) -> List[FlaggedFile]:
        """Scan CMakeLists.txt for commented-out or disabled targets."""
        cmake_files = list(self._root.rglob("CMakeLists.txt"))
        flagged = []

        for cmake_path in cmake_files:
            if any(skip in str(cmake_path) for skip in SKIP_DIRS):
                continue

            try:
                with open(cmake_path, "r", encoding="utf-8", errors="replace") as f:
                    lines = f.readlines()
            except (IOError, OSError):
                continue

            rel_path = os.path.relpath(str(cmake_path), str(self._root))
            disabled_lines = []
            snippets = []

            for i, line in enumerate(lines, 1):
                stripped = line.strip()
                # Commented-out add_executable or add_library
                if stripped.startswith("#") and (
                    "add_executable" in stripped.lower() or
                    "add_library" in stripped.lower()
                ):
                    disabled_lines.append(i)
                    snippets.append(f"L{i}: {stripped[:120]}")
                # TODO: Enable patterns
                if "TODO:" in stripped and "Enable" in stripped:
                    disabled_lines.append(i)
                    snippets.append(f"L{i}: {stripped[:120]}")

            if disabled_lines:
                flagged.append(FlaggedFile(
                    path=rel_path,
                    flags=["CMAKE_DISABLED"],
                    flag_count=len(disabled_lines),
                    line_numbers=disabled_lines,
                    snippets=snippets,
                    priority=len(disabled_lines) * 2,
                ))

        return flagged

    # ========================================================================
    # Phase C.2: Run simulations
    # ========================================================================

    def run_simulations(self, specs: Optional[List[str]] = None) -> List[SimulationResult]:
        """Run regression simulation suite."""
        from pykernel.gpu_bridge import GPUBridge
        bridge = GPUBridge()

        if not bridge.vsepr_binary:
            print("[ImprovementLoop] WARNING: vsepr binary not found, skipping simulations")
            return []

        if specs is None:
            specs = self._test_specs

        results = []
        for spec in specs:
            print(f"  Simulating: {spec}")
            import time
            t0 = time.time()

            sim = bridge.run_simulation(spec, "relax", timeout=120)
            wall = time.time() - t0

            sr = SimulationResult(
                spec=spec,
                returncode=sim.get("returncode", -1),
                wall_time=wall,
            )

            if sim["success"]:
                stdout = sim.get("stdout", "")
                # Parse energy
                for line in stdout.split("\n"):
                    ll = line.lower()
                    if "energy" in ll and ("=" in line or ":" in line):
                        parts = line.replace("=", ":").split(":")
                        if len(parts) >= 2:
                            try:
                                sr.energy = float(parts[-1].strip().split()[0])
                            except (ValueError, IndexError):
                                pass
                    if "converged" in ll:
                        sr.converged = True
                    if "step" in ll:
                        try:
                            for p in line.split():
                                if p.isdigit():
                                    sr.n_steps = max(sr.n_steps, int(p))
                        except ValueError:
                            pass
            else:
                sr.error = sim.get("stderr", "")[:200]

            results.append(sr)

        return results

    # ========================================================================
    # Phase C.3: Compare against baseline
    # ========================================================================

    def compare_results(self, results: List[SimulationResult]) -> Tuple[List[Dict], List[Dict]]:
        """Compare simulation results against baseline."""
        regressions = []
        improvements = []

        for r in results:
            if r.spec not in self._baseline or r.energy is None:
                continue

            base = self._baseline[r.spec]
            base_energy = base.get("energy")
            if base_energy is None:
                continue

            delta = r.energy - base_energy
            rel_delta = abs(delta) / max(abs(base_energy), 1e-10)

            entry = {
                "spec": r.spec,
                "baseline_energy": base_energy,
                "current_energy": r.energy,
                "delta": delta,
                "relative_delta": rel_delta,
                "converged": r.converged,
                "baseline_converged": base.get("converged", False),
            }

            # Energy should decrease (more negative = better) for relaxation
            if delta > 0 and rel_delta > 0.01:
                regressions.append(entry)
            elif delta < 0 and rel_delta > 0.01:
                improvements.append(entry)

        return regressions, improvements

    # ========================================================================
    # Phase C.4: Generate tasks
    # ========================================================================

    def generate_tasks(self, flagged: List[FlaggedFile],
                       regressions: List[Dict]) -> List[Dict]:
        """Generate improvement task queue."""
        tasks = []

        # High-priority: files flagged for destruction
        for f in flagged:
            if "DESTRUCTION" in f.flags or "QUARANTINED" in f.flags:
                tasks.append({
                    "type": "remove_or_refactor",
                    "file": f.path,
                    "priority": f.priority,
                    "flags": f.flags,
                    "reason": f"File has {f.flag_count} flags including: {', '.join(f.flags)}",
                })

        # Medium-priority: regression fixes
        for reg in regressions:
            tasks.append({
                "type": "fix_regression",
                "spec": reg["spec"],
                "priority": 10,
                "reason": f"Energy regressed by {reg['delta']:.4f} "
                          f"({reg['relative_delta']*100:.1f}%)",
            })

        # Low-priority: cleanup FIXME/HACK
        for f in flagged:
            if "FIXME" in f.flags or "HACK" in f.flags:
                tasks.append({
                    "type": "cleanup",
                    "file": f.path,
                    "priority": f.priority,
                    "flags": f.flags,
                    "reason": f"Code quality: {', '.join(f.flags)}",
                })

        # Sort by priority
        tasks.sort(key=lambda t: t["priority"], reverse=True)
        return tasks

    # ========================================================================
    # Main iteration
    # ========================================================================

    def run_iteration(self) -> ImprovementReport:
        """Run one complete improvement loop iteration."""
        timestamp = datetime.now().isoformat()
        print(f"\n{'='*60}")
        print(f"[ImprovementLoop] Iteration {self._iteration} — {timestamp}")
        print(f"{'='*60}")

        # Step 1: Scan flagged files
        print("\n[Step 1] Scanning for flagged files...")
        flagged = self.scan_flagged_files()
        cmake_flagged = self.scan_cmake_disabled()
        all_flagged = flagged + cmake_flagged
        print(f"  Found {len(flagged)} flagged source files, "
              f"{len(cmake_flagged)} CMake disabled targets")

        # Top 5 by priority
        for f in all_flagged[:5]:
            print(f"  [{f.priority:3d}] {f.path} — {', '.join(f.flags)}")

        # Step 2: Run simulations
        print("\n[Step 2] Running simulation suite...")
        sim_results = self.run_simulations()
        successful = sum(1 for r in sim_results if r.energy is not None)
        print(f"  {successful}/{len(sim_results)} simulations produced energy")

        # Step 3: Compare against baseline
        print("\n[Step 3] Comparing against baseline...")
        regressions, improvements = self.compare_results(sim_results)
        print(f"  Regressions: {len(regressions)}, Improvements: {len(improvements)}")

        # Step 4: Generate tasks
        print("\n[Step 4] Generating improvement tasks...")
        tasks = self.generate_tasks(all_flagged, regressions)
        print(f"  Generated {len(tasks)} tasks")
        for t in tasks[:5]:
            print(f"  [{t['priority']:3d}] {t['type']}: {t.get('file', t.get('spec', ''))}")

        # Step 5: Update baseline if this is the first run
        if not self._baseline and sim_results:
            print("\n[Step 5] Saving initial baseline...")
            self._save_baseline(sim_results)

        # Build summary
        summary = {
            "iteration": self._iteration,
            "total_flagged_files": len(all_flagged),
            "total_flag_instances": sum(f.flag_count for f in all_flagged),
            "top_flags": self._count_flags(all_flagged),
            "simulations_run": len(sim_results),
            "simulations_successful": successful,
            "regressions": len(regressions),
            "improvements_detected": len(improvements),
            "tasks_generated": len(tasks),
            "destruction_candidates": sum(
                1 for f in all_flagged if "DESTRUCTION" in f.flags
            ),
        }

        report = ImprovementReport(
            iteration=self._iteration,
            timestamp=timestamp,
            flagged_files=all_flagged,
            simulation_results=sim_results,
            regressions=regressions,
            improvements=improvements,
            tasks=tasks,
            summary=summary,
        )

        self._iteration += 1
        return report

    def _count_flags(self, flagged: List[FlaggedFile]) -> Dict[str, int]:
        """Count total occurrences of each flag type."""
        counts = {}
        for f in flagged:
            for flag in f.flags:
                counts[flag] = counts.get(flag, 0) + 1
        return dict(sorted(counts.items(), key=lambda x: x[1], reverse=True))

    def save_report(self, report: ImprovementReport):
        """Save iteration report to disk."""
        # JSON report
        path = self._output_dir / f"report_{report.iteration:04d}.json"
        with open(path, "w") as f:
            json.dump(report.to_dict(), f, indent=2, default=str)

        # Task queue (latest)
        task_path = self._output_dir / "task_queue.json"
        with open(task_path, "w") as f:
            json.dump(report.tasks, f, indent=2)

        # Flagged files CSV
        csv_path = self._output_dir / "flagged_files.csv"
        with open(csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["path", "flags", "flag_count", "priority", "file_hash"])
            for ff in report.flagged_files:
                writer.writerow([
                    ff.path, "|".join(ff.flags), ff.flag_count,
                    ff.priority, ff.file_hash,
                ])

        # Summary CSV (append)
        summary_csv = self._output_dir / "iteration_summary.csv"
        write_header = not summary_csv.exists()
        with open(summary_csv, "a", newline="") as f:
            writer = csv.writer(f)
            if write_header:
                writer.writerow([
                    "iteration", "timestamp", "flagged_files", "flag_instances",
                    "sims_run", "sims_ok", "regressions", "improvements", "tasks",
                ])
            s = report.summary
            writer.writerow([
                s["iteration"], report.timestamp,
                s["total_flagged_files"], s["total_flag_instances"],
                s["simulations_run"], s["simulations_successful"],
                s["regressions"], s["improvements_detected"],
                s["tasks_generated"],
            ])

        print(f"\n[ImprovementLoop] Report saved: {path}")

    def run_loop(self, max_iterations: int = 0, delay: float = 60.0):
        """
        Run the improvement loop continuously.

        Args:
            max_iterations: Max iterations (0 = infinite)
            delay: Seconds between iterations
        """
        import time
        import signal

        stop = [False]
        def handler(sig, frame):
            print("\n[ImprovementLoop] Stop requested...")
            stop[0] = True
        signal.signal(signal.SIGINT, handler)

        print(f"[ImprovementLoop] Starting closed slow-loop")
        print(f"  Project root: {self._root}")
        print(f"  Output: {self._output_dir}")
        print(f"  Max iterations: {'infinite' if max_iterations == 0 else max_iterations}")
        print(f"  Delay: {delay}s between iterations")

        iteration_count = 0
        while not stop[0]:
            if max_iterations > 0 and iteration_count >= max_iterations:
                break

            report = self.run_iteration()
            self.save_report(report)
            iteration_count += 1

            if not stop[0] and (max_iterations == 0 or iteration_count < max_iterations):
                print(f"\n[ImprovementLoop] Next iteration in {delay}s...")
                time.sleep(delay)

        print(f"\n[ImprovementLoop] Loop complete: {iteration_count} iterations")


def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="Phase C: Closed slow-loop autonomous improvement"
    )
    parser.add_argument("--iterations", type=int, default=1,
                        help="Max iterations (0=infinite)")
    parser.add_argument("--delay", type=float, default=60.0,
                        help="Seconds between iterations")
    parser.add_argument("--output", default="out/improvement",
                        help="Output directory")
    parser.add_argument("--scan-only", action="store_true",
                        help="Only scan flagged files, skip simulations")

    args = parser.parse_args()

    loop = ImprovementLoop(output_dir=args.output)

    if args.scan_only:
        flagged = loop.scan_flagged_files()
        cmake = loop.scan_cmake_disabled()
        all_f = flagged + cmake
        print(f"Found {len(all_f)} flagged files:")
        for f in all_f[:20]:
            print(f"  [{f.priority:3d}] {f.path}: {', '.join(f.flags)} ({f.flag_count} instances)")
    else:
        loop.run_loop(max_iterations=args.iterations, delay=args.delay)


if __name__ == "__main__":
    main()
