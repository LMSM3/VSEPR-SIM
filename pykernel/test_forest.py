"""
test_forest.py — Tree-structured test discovery, execution, and aggregation.

A "forest" is a collection of test trees. Each tree has:
  - A root domain (e.g., "gpu", "cpu", "atomistic", "coarse_grain")
  - Branches per test category
  - Leaves = individual test cases

The forest discovers tests from:
  1. C++ CTest targets (via cmake/ctest --show-only)
  2. Python pytest modules (via pytest --collect-only)
  3. PyKernel benchmark harnesses (bench_gpu_cpu, etc.)
  4. Custom registered test functions

Results flow through the pipe infrastructure to polynomial fitters,
eigen counters, and CSV/JSON sinks.

Usage:
    forest = TestForest()
    forest.discover()
    results = forest.run_all()
    forest.report()

Or selective:
    forest.run_domain("gpu")
    forest.run_domain("benchmark")
"""

import os
import re
import csv
import json
import time
import subprocess
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Callable, Any, Tuple
from datetime import datetime

from pykernel.pipe import Pipe, CSVSink, JSONSink, FanOut


# ============================================================================
# Data structures
# ============================================================================

@dataclass
class TestCase:
    """A single test leaf in the forest."""
    name: str
    domain: str          # "core", "atomistic", "coarse_grain", "gpu", "benchmark", "pykernel"
    category: str        # "unit", "integration", "benchmark", "regression"
    source: str          # "ctest", "pytest", "custom"
    labels: List[str] = field(default_factory=list)
    command: Optional[List[str]] = None   # How to run it
    timeout: int = 120

    @property
    def full_name(self) -> str:
        return f"{self.domain}/{self.category}/{self.name}"


@dataclass
class TestResult:
    """Result from running a single test."""
    test: TestCase
    passed: bool
    duration_ms: float
    returncode: int = 0
    stdout: str = ""
    stderr: str = ""
    error: str = ""
    metrics: Dict[str, float] = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "name": self.test.full_name,
            "domain": self.test.domain,
            "category": self.test.category,
            "passed": self.passed,
            "duration_ms": self.duration_ms,
            "returncode": self.returncode,
            "error": self.error[:200] if self.error else "",
            "metrics": self.metrics,
        }


@dataclass
class DomainSummary:
    """Aggregated results for one domain."""
    domain: str
    total: int = 0
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    total_ms: float = 0.0
    results: List[TestResult] = field(default_factory=list)

    @property
    def pass_rate(self) -> float:
        return self.passed / max(self.total, 1)


# ============================================================================
# Test Forest
# ============================================================================

class TestForest:
    """
    Tree-structured test discovery and execution.

    Discovers tests from CTest, pytest, and custom registrations,
    organizes them by domain/category, runs them, and pipes results
    to downstream analysis.
    """

    def __init__(self, project_root: Optional[str] = None,
                 build_dir: Optional[str] = None,
                 output_dir: str = "out/forest"):
        self._root = Path(project_root) if project_root else Path(__file__).parent.parent.absolute()
        self._build_dir = Path(build_dir) if build_dir else self._find_build_dir()
        self._output_dir = Path(output_dir)
        self._output_dir.mkdir(parents=True, exist_ok=True)

        self._tests: List[TestCase] = []
        self._results: List[TestResult] = []
        self._custom_tests: Dict[str, Callable[[], TestResult]] = {}

        # Pipes
        self.result_pipe = Pipe[dict]("test_results")
        self.timing_pipe = Pipe[float]("test_timings")
        self.benchmark_pipe = Pipe[dict]("benchmark_data")

        # Default sinks
        self._csv_sink = CSVSink(
            self.result_pipe,
            str(self._output_dir / "results.csv"),
            ["name", "domain", "category", "passed", "duration_ms", "returncode", "error"],
        )

    def _find_build_dir(self) -> Path:
        candidates = [
            self._root / "build",
            self._root / "build" / "Release",
            self._root / "build" / "Debug",
        ]
        for c in candidates:
            if c.is_dir():
                return c
        return self._root / "build"

    # ========================================================================
    # Discovery
    # ========================================================================

    def discover(self):
        """Discover all available tests."""
        self._tests.clear()
        self._discover_ctest()
        self._discover_pytest()
        self._discover_benchmarks()
        self._discover_custom()
        print(f"[Forest] Discovered {len(self._tests)} tests across "
              f"{len(set(t.domain for t in self._tests))} domains")

    def _discover_ctest(self):
        """Discover C++ tests from CTest."""
        try:
            result = subprocess.run(
                ["ctest", "--test-dir", str(self._build_dir), "--show-only=json-v1"],
                capture_output=True, text=True, timeout=15,
            )
            if result.returncode == 0 and result.stdout.strip():
                data = json.loads(result.stdout)
                for test in data.get("tests", []):
                    name = test.get("name", "")
                    props = test.get("properties", [])
                    labels = []
                    for p in props:
                        if p.get("name") == "LABELS":
                            labels = p.get("value", [])

                    domain = self._infer_domain(labels, name)
                    category = self._infer_category(labels, name)

                    cmd_parts = test.get("command", [])

                    self._tests.append(TestCase(
                        name=name,
                        domain=domain,
                        category=category,
                        source="ctest",
                        labels=labels,
                        command=cmd_parts if cmd_parts else None,
                    ))
        except (subprocess.TimeoutExpired, FileNotFoundError, json.JSONDecodeError):
            pass

    def _discover_pytest(self):
        """Discover Python tests from pytest."""
        test_dirs = [
            self._root / "pykernel" / "tests",
        ]
        for test_dir in test_dirs:
            if not test_dir.is_dir():
                continue
            try:
                result = subprocess.run(
                    ["python", "-m", "pytest", str(test_dir), "--collect-only", "-q"],
                    capture_output=True, text=True, timeout=15,
                    cwd=str(self._root),
                )
                for line in result.stdout.split("\n"):
                    line = line.strip()
                    if "::" in line and not line.startswith("="):
                        self._tests.append(TestCase(
                            name=line,
                            domain="pykernel",
                            category="unit",
                            source="pytest",
                            labels=["pykernel", "python"],
                        ))
            except (subprocess.TimeoutExpired, FileNotFoundError):
                pass

    def _discover_benchmarks(self):
        """Discover benchmark executables."""
        bench_names = ["bench_gpu_cpu"]
        for name in bench_names:
            # Search build dir for the executable
            for ext in ["", ".exe"]:
                for root, dirs, files in os.walk(self._build_dir):
                    if name + ext in files:
                        exe_path = os.path.join(root, name + ext)
                        self._tests.append(TestCase(
                            name=name,
                            domain="benchmark",
                            category="benchmark",
                            source="custom",
                            labels=["benchmark", "gpu", "cpu", "performance"],
                            command=[exe_path, "--csv"],
                            timeout=300,
                        ))
                        break

    def _discover_custom(self):
        """Add any manually registered test functions."""
        for name, func in self._custom_tests.items():
            self._tests.append(TestCase(
                name=name,
                domain="custom",
                category="unit",
                source="custom",
                labels=["custom"],
            ))

    def register_test(self, name: str, func: Callable[[], TestResult],
                      domain: str = "custom", category: str = "unit"):
        """Register a custom test function."""
        self._custom_tests[name] = func

    # ========================================================================
    # Classification helpers
    # ========================================================================

    @staticmethod
    def _infer_domain(labels: List[str], name: str) -> str:
        label_set = set(l.lower() for l in labels)
        if "benchmark" in label_set or "gpu" in label_set:
            return "benchmark"
        if "atomistic" in label_set:
            return "atomistic"
        if "coarse_grain" in label_set or "cg_suites" in label_set:
            return "coarse_grain"
        if "core" in label_set:
            return "core"
        if "thermal" in label_set:
            return "thermal"
        if "pbc" in label_set:
            return "pbc"
        if "pykernel" in label_set:
            return "pykernel"
        return "other"

    @staticmethod
    def _infer_category(labels: List[str], name: str) -> str:
        label_set = set(l.lower() for l in labels)
        if "benchmark" in label_set:
            return "benchmark"
        if "quick" in label_set:
            return "unit"
        if "regression" in label_set:
            return "regression"
        if "cg_suites" in label_set:
            return "integration"
        return "integration"

    # ========================================================================
    # Execution
    # ========================================================================

    def run_all(self) -> List[TestResult]:
        """Run all discovered tests."""
        self._results.clear()
        print(f"\n[Forest] Running {len(self._tests)} tests...")

        for tc in self._tests:
            result = self._run_test(tc)
            self._results.append(result)

            # Push through pipes
            self.result_pipe.push(result.to_dict(), source=tc.domain)
            self.timing_pipe.push(result.duration_ms, source=tc.full_name)

            # Benchmark data gets special pipe
            if tc.category == "benchmark" and result.metrics:
                self.benchmark_pipe.push(result.metrics, source=tc.name)

            status = "PASS" if result.passed else "FAIL"
            print(f"  [{status}] {tc.full_name} ({result.duration_ms:.0f}ms)")

        return self._results

    def run_domain(self, domain: str) -> List[TestResult]:
        """Run tests for a specific domain only."""
        domain_tests = [t for t in self._tests if t.domain == domain]
        results = []
        print(f"\n[Forest] Running {len(domain_tests)} {domain} tests...")

        for tc in domain_tests:
            result = self._run_test(tc)
            results.append(result)
            self._results.append(result)

            self.result_pipe.push(result.to_dict(), source=tc.domain)
            self.timing_pipe.push(result.duration_ms, source=tc.full_name)

            status = "PASS" if result.passed else "FAIL"
            print(f"  [{status}] {tc.full_name} ({result.duration_ms:.0f}ms)")

        return results

    def _run_test(self, tc: TestCase) -> TestResult:
        """Execute a single test case."""
        # Custom test function
        if tc.name in self._custom_tests:
            try:
                t0 = time.time()
                result = self._custom_tests[tc.name]()
                result.duration_ms = (time.time() - t0) * 1000
                result.test = tc
                return result
            except Exception as e:
                return TestResult(
                    test=tc, passed=False, duration_ms=0,
                    error=str(e),
                )

        # pytest
        if tc.source == "pytest":
            return self._run_pytest(tc)

        # CTest / executable
        if tc.command:
            return self._run_executable(tc)

        # CTest by name
        return self._run_ctest(tc)

    def _run_executable(self, tc: TestCase) -> TestResult:
        """Run an executable test/benchmark."""
        try:
            t0 = time.time()
            proc = subprocess.run(
                tc.command,
                capture_output=True, text=True,
                timeout=tc.timeout,
                cwd=str(self._root),
            )
            duration = (time.time() - t0) * 1000

            metrics = {}
            # Parse CSV output from benchmarks
            if tc.category == "benchmark" and proc.stdout:
                metrics = self._parse_benchmark_csv(proc.stdout)

            return TestResult(
                test=tc,
                passed=proc.returncode == 0,
                duration_ms=duration,
                returncode=proc.returncode,
                stdout=proc.stdout[:2000],
                stderr=proc.stderr[:1000],
                metrics=metrics,
            )
        except subprocess.TimeoutExpired:
            return TestResult(
                test=tc, passed=False, duration_ms=tc.timeout * 1000,
                error="timeout",
            )
        except FileNotFoundError:
            return TestResult(
                test=tc, passed=False, duration_ms=0,
                error="executable not found",
            )

    def _run_ctest(self, tc: TestCase) -> TestResult:
        """Run a single CTest test by name."""
        try:
            t0 = time.time()
            proc = subprocess.run(
                ["ctest", "--test-dir", str(self._build_dir),
                 "-R", f"^{re.escape(tc.name)}$", "--output-on-failure"],
                capture_output=True, text=True, timeout=tc.timeout,
            )
            duration = (time.time() - t0) * 1000
            return TestResult(
                test=tc,
                passed=proc.returncode == 0,
                duration_ms=duration,
                returncode=proc.returncode,
                stdout=proc.stdout[:2000],
                stderr=proc.stderr[:1000],
            )
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            return TestResult(
                test=tc, passed=False, duration_ms=0,
                error=str(e),
            )

    def _run_pytest(self, tc: TestCase) -> TestResult:
        """Run a single pytest test."""
        try:
            t0 = time.time()
            proc = subprocess.run(
                ["python", "-m", "pytest", tc.name, "-x", "-q"],
                capture_output=True, text=True, timeout=tc.timeout,
                cwd=str(self._root),
            )
            duration = (time.time() - t0) * 1000
            return TestResult(
                test=tc,
                passed=proc.returncode == 0,
                duration_ms=duration,
                returncode=proc.returncode,
                stdout=proc.stdout[:2000],
                stderr=proc.stderr[:1000],
            )
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            return TestResult(
                test=tc, passed=False, duration_ms=0,
                error=str(e),
            )

    @staticmethod
    def _parse_benchmark_csv(stdout: str) -> Dict[str, float]:
        """Parse benchmark CSV output into metrics dict."""
        metrics = {}
        lines = stdout.strip().split("\n")
        if len(lines) < 2:
            return metrics

        header = lines[0].split(",")
        # Take the last data row as summary
        last_row = lines[-1].split(",")
        if len(last_row) == len(header):
            for col, val in zip(header, last_row):
                try:
                    metrics[col.strip()] = float(val.strip())
                except ValueError:
                    metrics[col.strip()] = val.strip()
        return metrics

    # ========================================================================
    # Reporting
    # ========================================================================

    def summary(self) -> Dict[str, DomainSummary]:
        """Compute domain-level summaries."""
        domains: Dict[str, DomainSummary] = {}
        for r in self._results:
            d = r.test.domain
            if d not in domains:
                domains[d] = DomainSummary(domain=d)
            ds = domains[d]
            ds.total += 1
            ds.total_ms += r.duration_ms
            ds.results.append(r)
            if r.passed:
                ds.passed += 1
            else:
                ds.failed += 1
        return domains

    def report(self):
        """Print human-readable summary."""
        domains = self.summary()
        total = sum(d.total for d in domains.values())
        passed = sum(d.passed for d in domains.values())
        failed = sum(d.failed for d in domains.values())

        print(f"\n{'='*60}")
        print(f"[Forest] Test Summary — {total} tests")
        print(f"{'='*60}")

        for name, ds in sorted(domains.items()):
            status = "✓" if ds.failed == 0 else "✗"
            print(f"  {status} {name:20s}  {ds.passed}/{ds.total} passed  "
                  f"({ds.total_ms:.0f}ms)")

        print(f"{'─'*60}")
        print(f"  Total: {passed}/{total} passed, {failed} failed")

    def export_report(self, path: Optional[str] = None):
        """Export full report to JSON."""
        path = path or str(self._output_dir / "forest_report.json")
        domains = self.summary()
        data = {
            "timestamp": datetime.now().isoformat(),
            "total_tests": len(self._results),
            "domains": {},
        }
        for name, ds in domains.items():
            data["domains"][name] = {
                "total": ds.total,
                "passed": ds.passed,
                "failed": ds.failed,
                "total_ms": ds.total_ms,
                "pass_rate": ds.pass_rate,
                "results": [r.to_dict() for r in ds.results],
            }

        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(data, f, indent=2, default=str)
        print(f"[Forest] Report exported to {path}")

    @property
    def tests(self) -> List[TestCase]:
        return list(self._tests)

    @property
    def results(self) -> List[TestResult]:
        return list(self._results)

    @property
    def domains(self) -> List[str]:
        return sorted(set(t.domain for t in self._tests))
