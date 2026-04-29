"""
Tests for pykernel.benchmark — GPU vs CPU benchmark orchestrator.
"""

import json
import os
import tempfile
import pytest
import numpy as np

from pykernel.benchmark import (
    BenchmarkRow,
    BenchmarkSummary,
    BenchmarkOrchestrator,
    parse_benchmark_csv,
)


# ============================================================================
# CSV parsing
# ============================================================================

class TestParseBenchmarkCSV:
    def test_parse_valid(self):
        csv_text = (
            "n_atoms,cpu_ms,gpu_ms,speedup,energy_cpu,energy_gpu,delta_E,backend\n"
            "64,0.5,0.3,1.67,-10.5,-10.5,0.0,CPU_Fallback\n"
            "128,2.0,1.0,2.00,-50.0,-50.0,0.0,CPU_Fallback\n"
        )
        rows = parse_benchmark_csv(csv_text)
        assert len(rows) == 2
        assert rows[0].n_atoms == 64
        assert rows[0].cpu_ms == 0.5
        assert rows[0].backend == "CPU_Fallback"
        assert rows[1].n_atoms == 128
        assert rows[1].speedup == 2.0

    def test_parse_empty(self):
        assert parse_benchmark_csv("") == []

    def test_parse_header_only(self):
        assert parse_benchmark_csv("n_atoms,cpu_ms\n") == []

    def test_parse_malformed_row(self):
        csv_text = (
            "n_atoms,cpu_ms,gpu_ms,speedup,energy_cpu,energy_gpu,delta_E,backend\n"
            "not_a_number,bad,data,here,no,good,values,x\n"
        )
        rows = parse_benchmark_csv(csv_text)
        assert len(rows) == 0

    def test_parse_single_row(self):
        csv_text = (
            "n_atoms,cpu_ms,gpu_ms,speedup,energy_cpu,energy_gpu,delta_E,backend\n"
            "1024,15.5,3.2,4.84,-500.0,-500.001,0.001,CUDA\n"
        )
        rows = parse_benchmark_csv(csv_text)
        assert len(rows) == 1
        assert rows[0].n_atoms == 1024
        assert rows[0].delta_E == 0.001
        assert rows[0].backend == "CUDA"


# ============================================================================
# BenchmarkRow
# ============================================================================

class TestBenchmarkRow:
    def test_creation(self):
        r = BenchmarkRow(
            n_atoms=256, cpu_ms=5.0, gpu_ms=2.0,
            speedup=2.5, energy_cpu=-100.0,
            energy_gpu=-100.0, delta_E=0.0,
            backend="CPU_Fallback",
        )
        assert r.n_atoms == 256
        assert r.speedup == 2.5

    def test_defaults(self):
        r = BenchmarkRow(
            n_atoms=64, cpu_ms=1.0, gpu_ms=0.5,
            speedup=2.0, energy_cpu=-10.0,
            energy_gpu=-10.0, delta_E=0.0,
        )
        assert r.backend == ""


# ============================================================================
# BenchmarkSummary
# ============================================================================

class TestBenchmarkSummary:
    def test_empty_summary(self):
        s = BenchmarkSummary()
        d = s.to_dict()
        assert d["n_rows"] == 0
        assert d["rows"] == []
        assert d["cpu_fit"] is None
        assert d["gpu_fit"] is None

    def test_summary_with_rows(self):
        rows = [
            BenchmarkRow(64, 1.0, 0.5, 2.0, -10.0, -10.0, 0.0, "test"),
            BenchmarkRow(128, 4.0, 1.5, 2.67, -50.0, -50.0, 0.0, "test"),
        ]
        s = BenchmarkSummary(rows=rows, backend="CPU_Fallback",
                             timestamp="2025-01-01T00:00:00")
        d = s.to_dict()
        assert d["n_rows"] == 2
        assert d["backend"] == "CPU_Fallback"
        assert d["rows"][0]["n_atoms"] == 64


# ============================================================================
# BenchmarkOrchestrator — synthetic mode
# ============================================================================

class TestOrchestratorSynthetic:
    """Test orchestrator using synthetic data (no C++ binary needed)."""

    def _make_orch(self, tmp_path):
        return BenchmarkOrchestrator(
            output_dir=str(tmp_path / "bench_out"),
            build_dir=str(tmp_path / "nonexistent_build"),
        )

    def test_benchmark_not_available(self, tmp_path):
        orch = self._make_orch(tmp_path)
        assert not orch.benchmark_available

    def test_run_synthetic(self, tmp_path):
        orch = self._make_orch(tmp_path)
        summary = orch.run(min_atoms=64, max_atoms=512, steps=4, repeats=1)
        assert summary is not None
        assert len(summary.rows) > 0
        assert summary.backend == "numpy_fallback"

    def test_synthetic_rows_valid(self, tmp_path):
        orch = self._make_orch(tmp_path)
        summary = orch.run(min_atoms=100, max_atoms=1000, steps=5)
        for r in summary.rows:
            assert r.n_atoms > 0
            assert r.cpu_ms > 0
            assert r.gpu_ms > 0
            assert r.backend == "synthetic"

    def test_cpu_fit_produced(self, tmp_path):
        orch = self._make_orch(tmp_path)
        summary = orch.run(min_atoms=64, max_atoms=4096, steps=8)
        assert summary.cpu_fit is not None
        assert "r_squared" in summary.cpu_fit
        assert "power_law_exponent" in summary.cpu_fit

    def test_gpu_fit_produced(self, tmp_path):
        orch = self._make_orch(tmp_path)
        summary = orch.run(min_atoms=64, max_atoms=4096, steps=8)
        assert summary.gpu_fit is not None
        assert summary.gpu_fit["r_squared"] > 0

    def test_speedup_eigen(self, tmp_path):
        orch = self._make_orch(tmp_path)
        summary = orch.run(min_atoms=64, max_atoms=4096, steps=8)
        assert summary.speedup_eigen is not None
        assert "eigenvalues" in summary.speedup_eigen
        assert "condition_number" in summary.speedup_eigen

    def test_pipe_stats(self, tmp_path):
        orch = self._make_orch(tmp_path)
        orch.run(min_atoms=64, max_atoms=512, steps=4)
        stats = orch.pipe_stats
        assert stats["raw"]["total_pushed"] > 0
        assert stats["cpu"]["total_pushed"] > 0
        assert stats["gpu"]["total_pushed"] > 0

    def test_csv_output_written(self, tmp_path):
        orch = self._make_orch(tmp_path)
        orch.run(min_atoms=64, max_atoms=512, steps=4)
        csv_path = tmp_path / "bench_out" / "benchmark.csv"
        assert csv_path.exists()
        with open(csv_path) as f:
            lines = f.readlines()
        assert len(lines) > 1  # header + data

    def test_jsonl_output_written(self, tmp_path):
        orch = self._make_orch(tmp_path)
        orch.run(min_atoms=64, max_atoms=512, steps=4)
        jsonl_path = tmp_path / "bench_out" / "benchmark.jsonl"
        assert jsonl_path.exists()
        with open(jsonl_path) as f:
            lines = f.readlines()
        assert len(lines) > 0
        obj = json.loads(lines[0])
        assert "data" in obj

    def test_export_report(self, tmp_path):
        orch = self._make_orch(tmp_path)
        orch.run(min_atoms=64, max_atoms=512, steps=4)
        report_path = str(tmp_path / "report.json")
        orch.export(report_path)
        assert os.path.exists(report_path)
        with open(report_path) as f:
            data = json.load(f)
        assert data["n_rows"] > 0

    def test_report_print(self, tmp_path, capsys):
        orch = self._make_orch(tmp_path)
        orch.run(min_atoms=64, max_atoms=512, steps=4)
        orch.report()
        captured = capsys.readouterr()
        assert "GPU vs CPU Benchmark" in captured.out

    def test_no_report_before_run(self, tmp_path, capsys):
        orch = self._make_orch(tmp_path)
        orch.report()
        captured = capsys.readouterr()
        assert "No results" in captured.out
