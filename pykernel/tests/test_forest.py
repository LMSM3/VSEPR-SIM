"""
Tests for pykernel.test_forest — tree-structured test discovery and execution.
"""

import os
import json
import tempfile
import pytest

from pykernel.test_forest import TestCase, TestResult, DomainSummary, TestForest


# ============================================================================
# TestCase
# ============================================================================

class TestTestCase:
    def test_full_name(self):
        tc = TestCase(
            name="test_lj_energy",
            domain="core",
            category="unit",
            source="ctest",
        )
        assert tc.full_name == "core/unit/test_lj_energy"

    def test_default_fields(self):
        tc = TestCase(name="t", domain="d", category="c", source="s")
        assert tc.labels == []
        assert tc.command is None
        assert tc.timeout == 120

    def test_with_labels(self):
        tc = TestCase(
            name="bench_gpu",
            domain="benchmark",
            category="benchmark",
            source="custom",
            labels=["benchmark", "gpu", "performance"],
        )
        assert "gpu" in tc.labels
        assert len(tc.labels) == 3


# ============================================================================
# TestResult
# ============================================================================

class TestTestResult:
    def _make_result(self, passed=True, duration=100.0, metrics=None):
        tc = TestCase(name="test_x", domain="core", category="unit", source="ctest")
        return TestResult(
            test=tc,
            passed=passed,
            duration_ms=duration,
            returncode=0 if passed else 1,
            metrics=metrics or {},
        )

    def test_to_dict(self):
        r = self._make_result(passed=True, duration=42.5)
        d = r.to_dict()
        assert d["name"] == "core/unit/test_x"
        assert d["passed"] is True
        assert d["duration_ms"] == 42.5
        assert d["domain"] == "core"
        assert d["category"] == "unit"

    def test_to_dict_with_metrics(self):
        r = self._make_result(metrics={"speedup": 3.5, "n_atoms": 1024})
        d = r.to_dict()
        assert d["metrics"]["speedup"] == 3.5
        assert d["metrics"]["n_atoms"] == 1024

    def test_error_truncation(self):
        tc = TestCase(name="fail", domain="core", category="unit", source="ctest")
        r = TestResult(
            test=tc,
            passed=False,
            duration_ms=0,
            error="x" * 500,
        )
        d = r.to_dict()
        assert len(d["error"]) == 200


# ============================================================================
# DomainSummary
# ============================================================================

class TestDomainSummary:
    def test_pass_rate_all_pass(self):
        ds = DomainSummary(domain="core", total=10, passed=10, failed=0)
        assert ds.pass_rate == 1.0

    def test_pass_rate_mixed(self):
        ds = DomainSummary(domain="gpu", total=4, passed=3, failed=1)
        assert ds.pass_rate == 0.75

    def test_pass_rate_empty(self):
        ds = DomainSummary(domain="empty", total=0, passed=0, failed=0)
        assert ds.pass_rate == 0.0

    def test_defaults(self):
        ds = DomainSummary(domain="test")
        assert ds.total == 0
        assert ds.total_ms == 0.0
        assert ds.results == []


# ============================================================================
# TestForest — domain inference
# ============================================================================

class TestForestDomainInference:
    def test_benchmark_label(self):
        d = TestForest._infer_domain(["benchmark", "gpu"], "bench_gpu_cpu")
        assert d == "benchmark"

    def test_gpu_label(self):
        d = TestForest._infer_domain(["gpu"], "test_gpu_alloc")
        assert d == "benchmark"

    def test_atomistic_label(self):
        d = TestForest._infer_domain(["atomistic"], "test_vdw")
        assert d == "atomistic"

    def test_coarse_grain_label(self):
        d = TestForest._infer_domain(["coarse_grain"], "test_cg_map")
        assert d == "coarse_grain"

    def test_core_label(self):
        d = TestForest._infer_domain(["core"], "test_vec3")
        assert d == "core"

    def test_thermal_label(self):
        d = TestForest._infer_domain(["thermal"], "test_thermo")
        assert d == "thermal"

    def test_pbc_label(self):
        d = TestForest._infer_domain(["pbc"], "test_periodic")
        assert d == "pbc"

    def test_unknown_label(self):
        d = TestForest._infer_domain([], "mystery_test")
        assert d == "other"


# ============================================================================
# TestForest — category inference
# ============================================================================

class TestForestCategoryInference:
    def test_benchmark_category(self):
        c = TestForest._infer_category(["benchmark"], "bench_x")
        assert c == "benchmark"

    def test_quick_category(self):
        c = TestForest._infer_category(["quick"], "test_fast")
        assert c == "unit"

    def test_regression_category(self):
        c = TestForest._infer_category(["regression"], "test_reg")
        assert c == "regression"

    def test_cg_suites_category(self):
        c = TestForest._infer_category(["cg_suites"], "test_cg")
        assert c == "integration"

    def test_default_category(self):
        c = TestForest._infer_category([], "test_x")
        assert c == "integration"


# ============================================================================
# TestForest — benchmark CSV parsing
# ============================================================================

class TestForestCSVParsing:
    def test_parse_benchmark_csv(self):
        csv_text = (
            "n_atoms,cpu_ms,gpu_ms,speedup,energy_cpu,energy_gpu,delta_E,backend\n"
            "64,0.5,0.3,1.67,-10.5,-10.5,0.0,CPU_Fallback\n"
            "256,5.0,2.0,2.50,-100.5,-100.5,0.0,CPU_Fallback\n"
        )
        metrics = TestForest._parse_benchmark_csv(csv_text)
        # Takes last row as summary
        assert metrics["n_atoms"] == 256.0
        assert metrics["cpu_ms"] == 5.0
        assert metrics["speedup"] == 2.5
        assert metrics["backend"] == "CPU_Fallback"

    def test_parse_empty(self):
        metrics = TestForest._parse_benchmark_csv("")
        assert metrics == {}

    def test_parse_header_only(self):
        metrics = TestForest._parse_benchmark_csv("a,b,c\n")
        assert metrics == {}


# ============================================================================
# TestForest — custom test registration
# ============================================================================

class TestForestCustomTests:
    def test_register_and_discover(self):
        forest = TestForest(project_root=tempfile.mkdtemp(),
                            build_dir=tempfile.mkdtemp(),
                            output_dir=tempfile.mkdtemp())

        def my_test():
            tc = TestCase(name="my_test", domain="custom", category="unit", source="custom")
            return TestResult(test=tc, passed=True, duration_ms=1.0)

        forest.register_test("my_test", my_test)
        forest.discover()

        names = [t.name for t in forest.tests]
        assert "my_test" in names

    def test_run_custom_test(self):
        forest = TestForest(project_root=tempfile.mkdtemp(),
                            build_dir=tempfile.mkdtemp(),
                            output_dir=tempfile.mkdtemp())

        def passing_test():
            tc = TestCase(name="pass_test", domain="custom", category="unit", source="custom")
            return TestResult(test=tc, passed=True, duration_ms=0.5)

        forest.register_test("pass_test", passing_test)
        forest.discover()
        results = forest.run_all()

        custom_results = [r for r in results if r.test.name == "pass_test"]
        assert len(custom_results) == 1
        assert custom_results[0].passed is True


# ============================================================================
# TestForest — summary and export
# ============================================================================

class TestForestSummary:
    def _make_forest_with_results(self):
        forest = TestForest(project_root=tempfile.mkdtemp(),
                            build_dir=tempfile.mkdtemp(),
                            output_dir=tempfile.mkdtemp())

        def pass_test():
            tc = TestCase(name="p1", domain="core", category="unit", source="custom")
            return TestResult(test=tc, passed=True, duration_ms=10.0)

        def fail_test():
            tc = TestCase(name="f1", domain="core", category="unit", source="custom")
            return TestResult(test=tc, passed=False, duration_ms=5.0, error="expected fail")

        def gpu_test():
            tc = TestCase(name="g1", domain="benchmark", category="benchmark", source="custom")
            return TestResult(test=tc, passed=True, duration_ms=100.0,
                              metrics={"speedup": 2.0})

        forest.register_test("p1", pass_test, domain="core")
        forest.register_test("f1", fail_test, domain="core")
        forest.register_test("g1", gpu_test, domain="benchmark")
        forest.discover()
        forest.run_all()
        return forest

    def test_summary_domains(self):
        forest = self._make_forest_with_results()
        summ = forest.summary()
        assert "custom" in summ  # custom tests get domain "custom" from discover

    def test_export_report(self, tmp_path):
        forest = self._make_forest_with_results()
        path = str(tmp_path / "report.json")
        forest.export_report(path)
        assert os.path.exists(path)
        with open(path) as f:
            data = json.load(f)
        assert "timestamp" in data
        assert "total_tests" in data
        assert data["total_tests"] > 0

    def test_properties(self):
        forest = self._make_forest_with_results()
        assert len(forest.tests) > 0
        assert len(forest.results) > 0
        assert isinstance(forest.domains, list)

    def test_pipe_receives_results(self):
        received = []
        forest = TestForest(project_root=tempfile.mkdtemp(),
                            build_dir=tempfile.mkdtemp(),
                            output_dir=tempfile.mkdtemp())
        forest.result_pipe.subscribe(lambda rec: received.append(rec.data))

        def test_fn():
            tc = TestCase(name="piped", domain="custom", category="unit", source="custom")
            return TestResult(test=tc, passed=True, duration_ms=1.0)

        forest.register_test("piped", test_fn)
        forest.discover()
        forest.run_all()

        assert len(received) > 0
        assert received[0]["passed"] is True
