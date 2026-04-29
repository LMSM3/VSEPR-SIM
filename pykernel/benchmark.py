"""
benchmark.py — Python benchmark orchestrator for GPU vs CPU measurement.

Wraps the C++ bench_gpu_cpu binary, parses its CSV output, and pipes
the data through PolyFitter and EigenCounter for scaling analysis.

Pipeline:
    bench_gpu_cpu (CSV stdout)
        → parse rows
        → Pipe[dict] "benchmark_raw"
        → FanOut
            → Transform → Pipe[float] "cpu_timings"   → PolyFitter
            → Transform → Pipe[float] "gpu_timings"   → PolyFitter
            → Transform → Pipe[float] "speedup_curve" → EigenCounter (property mode)
            → CSVSink "out/benchmark.csv"
            → JSONSink "out/benchmark.jsonl"

Anti-black-box: every timing, every fit, every eigenvalue is stored.
"""

import csv
import io
import os
import subprocess
import time
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import List, Optional, Dict, Any

import numpy as np

from pykernel.pipe import Pipe, Transform, FanOut, CSVSink, JSONSink, Accumulator
from pykernel.poly_fitter import PolyFitter
from pykernel.eigen_counter import EigenCounter, EigenSnapshot
from pykernel.gpu_bridge import GPUBridge


# ============================================================================
# Data structures
# ============================================================================

@dataclass
class BenchmarkRow:
    """Single benchmark measurement row."""
    n_atoms: int
    cpu_ms: float
    gpu_ms: float
    speedup: float
    energy_cpu: float
    energy_gpu: float
    delta_E: float
    backend: str = ""


@dataclass
class BenchmarkSummary:
    """Aggregated benchmark summary with scaling fits."""
    rows: List[BenchmarkRow] = field(default_factory=list)
    cpu_fit: Optional[Dict] = None
    gpu_fit: Optional[Dict] = None
    speedup_eigen: Optional[Dict] = None
    backend: str = ""
    timestamp: str = ""
    total_time_ms: float = 0.0

    def to_dict(self) -> dict:
        return {
            "timestamp": self.timestamp,
            "backend": self.backend,
            "total_time_ms": self.total_time_ms,
            "n_rows": len(self.rows),
            "cpu_fit": self.cpu_fit,
            "gpu_fit": self.gpu_fit,
            "speedup_eigen": self.speedup_eigen,
            "rows": [
                {
                    "n_atoms": r.n_atoms,
                    "cpu_ms": r.cpu_ms,
                    "gpu_ms": r.gpu_ms,
                    "speedup": r.speedup,
                }
                for r in self.rows
            ],
        }


# ============================================================================
# CSV parser
# ============================================================================

def parse_benchmark_csv(csv_text: str) -> List[BenchmarkRow]:
    """Parse CSV output from bench_gpu_cpu into typed rows."""
    rows: List[BenchmarkRow] = []
    reader = csv.DictReader(io.StringIO(csv_text))
    for row in reader:
        try:
            rows.append(BenchmarkRow(
                n_atoms=int(row.get("n_atoms", 0)),
                cpu_ms=float(row.get("cpu_ms", 0)),
                gpu_ms=float(row.get("gpu_ms", 0)),
                speedup=float(row.get("speedup", 0)),
                energy_cpu=float(row.get("energy_cpu", 0)),
                energy_gpu=float(row.get("energy_gpu", 0)),
                delta_E=float(row.get("delta_E", 0)),
                backend=row.get("backend", ""),
            ))
        except (ValueError, KeyError):
            continue
    return rows


# ============================================================================
# Benchmark Orchestrator
# ============================================================================

class BenchmarkOrchestrator:
    """
    Orchestrates GPU vs CPU benchmarks and pipes results through analysis.

    Usage:
        orch = BenchmarkOrchestrator()
        summary = orch.run()
        orch.report()

    Or with custom args:
        summary = orch.run(min_atoms=128, max_atoms=4096, steps=6, repeats=5)
    """

    def __init__(self, output_dir: str = "out/benchmark",
                 build_dir: Optional[str] = None):
        self._root = Path(__file__).parent.parent.absolute()
        self._output_dir = Path(output_dir)
        self._output_dir.mkdir(parents=True, exist_ok=True)

        self._bridge = GPUBridge(build_dir=build_dir)
        self._bench_exe = self._find_benchmark()

        # Analysis engines
        self._poly_fitter = PolyFitter()
        self._eigen_counter = EigenCounter()

        # Pipes
        self.raw_pipe = Pipe[dict]("benchmark_raw")
        self.cpu_pipe = Pipe[float]("cpu_timings")
        self.gpu_pipe = Pipe[float]("gpu_timings")
        self.speedup_pipe = Pipe[float]("speedup_curve")
        self.size_pipe = Pipe[int]("atom_sizes")

        # Sinks
        self._csv_sink = CSVSink(
            self.raw_pipe,
            str(self._output_dir / "benchmark.csv"),
            ["n_atoms", "cpu_ms", "gpu_ms", "speedup",
             "energy_cpu", "energy_gpu", "delta_E", "backend"],
        )
        self._json_sink = JSONSink(
            self.raw_pipe,
            str(self._output_dir / "benchmark.jsonl"),
        )

        # Fan-out: raw → cpu, gpu, speedup, size
        self._fan = FanOut(self.raw_pipe, [])

        # Transforms extract fields from raw dict
        self._cpu_xform = Transform(
            self.raw_pipe, self.cpu_pipe,
            lambda d: d.get("cpu_ms", 0.0),
            name="raw→cpu",
        )
        self._gpu_xform = Transform(
            self.raw_pipe, self.gpu_pipe,
            lambda d: d.get("gpu_ms", 0.0),
            name="raw→gpu",
        )
        self._speedup_xform = Transform(
            self.raw_pipe, self.speedup_pipe,
            lambda d: d.get("speedup", 0.0),
            name="raw→speedup",
        )
        self._size_xform = Transform(
            self.raw_pipe, self.size_pipe,
            lambda d: d.get("n_atoms", 0),
            name="raw→size",
        )

        # State
        self._summary: Optional[BenchmarkSummary] = None

    def _find_benchmark(self) -> Optional[Path]:
        """Find the bench_gpu_cpu executable in the build tree."""
        build_dir = self._bridge._build_dir
        if not build_dir.exists():
            return None

        for ext in [".exe", ""]:
            name = f"bench_gpu_cpu{ext}"
            for root, dirs, files in os.walk(build_dir):
                if name in files:
                    return Path(root) / name
        return None

    @property
    def benchmark_available(self) -> bool:
        return self._bench_exe is not None and self._bench_exe.exists()

    # ========================================================================
    # Run
    # ========================================================================

    def run(self, min_atoms: int = 64, max_atoms: int = 8192,
            steps: int = 8, repeats: int = 3,
            seed: int = 42) -> BenchmarkSummary:
        """
        Run the GPU vs CPU benchmark and pipe results through analysis.

        Returns a BenchmarkSummary with rows, fits, and eigenvalues.
        """
        t0 = time.time()

        rows = self._execute_benchmark(min_atoms, max_atoms, steps, repeats, seed)

        # Push each row through pipes
        for row in rows:
            self.raw_pipe.push(row.__dict__, source="bench_gpu_cpu")

        # Fit polynomial scaling curves
        cpu_fit = self._fit_scaling(rows, "cpu")
        gpu_fit = self._fit_scaling(rows, "gpu")

        # Eigenvalue analysis on speedup curve
        speedup_eigen = self._analyze_speedup(rows)

        summary = BenchmarkSummary(
            rows=rows,
            cpu_fit=cpu_fit,
            gpu_fit=gpu_fit,
            speedup_eigen=speedup_eigen,
            backend=self._bridge.backend,
            timestamp=datetime.now().isoformat(),
            total_time_ms=(time.time() - t0) * 1000,
        )
        self._summary = summary
        return summary

    def _execute_benchmark(self, min_atoms: int, max_atoms: int,
                           steps: int, repeats: int,
                           seed: int) -> List[BenchmarkRow]:
        """Execute the C++ benchmark binary and parse output."""
        if not self.benchmark_available:
            print("[Benchmark] bench_gpu_cpu not found, generating synthetic data")
            return self._synthetic_benchmark(min_atoms, max_atoms, steps, seed)

        args = [
            str(self._bench_exe),
            "--min", str(min_atoms),
            "--max", str(max_atoms),
            "--steps", str(steps),
            "--repeats", str(repeats),
            "--seed", str(seed),
            "--csv",
        ]
        try:
            proc = subprocess.run(
                args,
                capture_output=True, text=True,
                timeout=600,
                cwd=str(self._root),
            )
            if proc.returncode != 0:
                print(f"[Benchmark] bench_gpu_cpu exited with code {proc.returncode}")
                print(f"  stderr: {proc.stderr[:500]}")
                return []

            return parse_benchmark_csv(proc.stdout)
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            print(f"[Benchmark] Execution failed: {e}")
            return []

    def _synthetic_benchmark(self, min_atoms: int, max_atoms: int,
                             steps: int, seed: int) -> List[BenchmarkRow]:
        """Generate synthetic benchmark data when binary is unavailable."""
        rng = np.random.RandomState(seed)
        log_min = np.log(min_atoms)
        log_max = np.log(max_atoms)
        sizes = np.unique(np.exp(
            np.linspace(log_min, log_max, steps)
        ).astype(int))

        rows = []
        for n in sizes:
            # O(N²) scaling model + noise
            cpu_ms = 0.001 * n * n + rng.normal(0, 0.05 * n)
            gpu_ms = max(0.01, 0.0001 * n * n + 0.5 + rng.normal(0, 0.02 * n))
            speedup = cpu_ms / max(gpu_ms, 1e-9)
            energy = -0.01 * n * (n - 1) * rng.uniform(0.8, 1.2)

            rows.append(BenchmarkRow(
                n_atoms=int(n),
                cpu_ms=max(0.01, cpu_ms),
                gpu_ms=max(0.01, gpu_ms),
                speedup=speedup,
                energy_cpu=energy,
                energy_gpu=energy + rng.normal(0, abs(energy) * 1e-6),
                delta_E=abs(rng.normal(0, abs(energy) * 1e-6)),
                backend="synthetic",
            ))
        return rows

    # ========================================================================
    # Analysis: polynomial scaling fits
    # ========================================================================

    def _fit_scaling(self, rows: List[BenchmarkRow],
                     target: str) -> Optional[Dict]:
        """Fit polynomial to timing vs N curve."""
        if len(rows) < 4:
            return None

        x = np.array([r.n_atoms for r in rows], dtype=float)
        if target == "cpu":
            y = np.array([r.cpu_ms for r in rows], dtype=float)
        else:
            y = np.array([r.gpu_ms for r in rows], dtype=float)

        # Use log-log for power law: log(t) ~ a * log(N) + b
        x_log = np.log(x + 1)
        y_log = np.log(np.maximum(y, 1e-12))

        # Fit degrees 2-5 (not 11-15; these are small datasets)
        best_fit = None
        best_r2 = -1.0
        for deg in range(2, min(6, len(rows))):
            result = self._poly_fitter.fit_single(x_log, y_log, degree=deg,
                                                  run_id=f"bench_{target}",
                                                  target_name=f"{target}_scaling")
            if result.r_squared > best_r2:
                best_r2 = result.r_squared
                best_fit = result

        if best_fit is None:
            return None

        # Estimate power-law exponent from log-log slope
        slope = best_fit.coefficients[1] if len(best_fit.coefficients) > 1 else 0.0

        return {
            "target": target,
            "degree": best_fit.degree,
            "r_squared": best_fit.r_squared,
            "power_law_exponent": float(slope),
            "condition_number": best_fit.condition_number,
            "coefficients": best_fit.coefficients.tolist(),
        }

    # ========================================================================
    # Analysis: eigenvalue decomposition on speedup curve
    # ========================================================================

    def _analyze_speedup(self, rows: List[BenchmarkRow]) -> Optional[Dict]:
        """Eigenvalue analysis on the speedup trend."""
        if len(rows) < 3:
            return None

        sizes = np.array([r.n_atoms for r in rows], dtype=float)
        speedups = np.array([r.speedup for r in rows], dtype=float)

        # Build a small covariance matrix from sliding windows
        data = np.column_stack([sizes / sizes.max(), speedups / max(speedups.max(), 1e-9)])
        cov = np.cov(data.T)
        eigenvalues = np.linalg.eigvalsh(cov)[::-1]

        snap = self._eigen_counter.record(
            eigenvalues=eigenvalues,
            run_id="bench_speedup",
            source="covariance",
            n_atoms=len(rows),
            label="speedup_scaling",
        )

        return {
            "eigenvalues": eigenvalues.tolist(),
            "condition_number": snap.condition_number,
            "spectral_gap": snap.spectral_gap,
            "trace": snap.trace,
        }

    # ========================================================================
    # Reporting
    # ========================================================================

    def report(self):
        """Print human-readable benchmark report."""
        if not self._summary:
            print("[Benchmark] No results. Run benchmark first.")
            return

        s = self._summary
        print(f"\n{'='*60}")
        print(f"  VSEPR-SIM GPU vs CPU Benchmark Report")
        print(f"{'='*60}")
        print(f"  Backend:       {s.backend}")
        print(f"  Timestamp:     {s.timestamp}")
        print(f"  Total time:    {s.total_time_ms:.0f} ms")
        print(f"  Data points:   {len(s.rows)}")

        if s.rows:
            print(f"\n  {'N':>6s}  {'CPU ms':>10s}  {'GPU ms':>10s}  {'Speedup':>8s}")
            print(f"  {'─'*40}")
            for r in s.rows:
                print(f"  {r.n_atoms:6d}  {r.cpu_ms:10.3f}  {r.gpu_ms:10.3f}  {r.speedup:8.2f}x")

        if s.cpu_fit:
            print(f"\n  CPU Scaling Fit:")
            print(f"    Power-law exponent: {s.cpu_fit['power_law_exponent']:.3f}")
            print(f"    R²: {s.cpu_fit['r_squared']:.6f}")

        if s.gpu_fit:
            print(f"\n  GPU Scaling Fit:")
            print(f"    Power-law exponent: {s.gpu_fit['power_law_exponent']:.3f}")
            print(f"    R²: {s.gpu_fit['r_squared']:.6f}")

        if s.speedup_eigen:
            print(f"\n  Speedup Eigenanalysis:")
            print(f"    Eigenvalues: {s.speedup_eigen['eigenvalues']}")
            print(f"    Condition:   {s.speedup_eigen['condition_number']:.3f}")

        print(f"{'='*60}")

    def export(self, path: Optional[str] = None):
        """Export full benchmark report to JSON."""
        if not self._summary:
            return
        path = path or str(self._output_dir / "benchmark_report.json")
        import json
        with open(path, "w") as f:
            json.dump(self._summary.to_dict(), f, indent=2, default=str)
        print(f"[Benchmark] Report exported to {path}")

    @property
    def summary(self) -> Optional[BenchmarkSummary]:
        return self._summary

    @property
    def pipe_stats(self) -> Dict[str, Dict]:
        """Return stats for all pipes in the orchestrator."""
        return {
            "raw": self.raw_pipe.stats(),
            "cpu": self.cpu_pipe.stats(),
            "gpu": self.gpu_pipe.stats(),
            "speedup": self.speedup_pipe.stats(),
            "size": self.size_pipe.stats(),
        }
