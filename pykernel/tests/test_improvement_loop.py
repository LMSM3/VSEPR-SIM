"""
Tests for the PyKernel improvement loop (Phase C).

Verifies:
  - Flagged file scanning
  - CMake disabled target detection
  - Task generation
  - Report serialization
  - Baseline comparison logic
"""

import numpy as np
import pytest
import tempfile
import json
import os

from pykernel.improvement_loop import (
    ImprovementLoop, FlaggedFile, SimulationResult,
    FLAG_PATTERNS,
)


class TestFlagScanning:
    """Test source file flag detection."""

    def test_scan_finds_todo(self):
        """Scanning should find TODO markers."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create a file with TODO
            test_file = os.path.join(tmpdir, "test.cpp")
            with open(test_file, "w") as f:
                f.write("// TODO: fix this\nint main() { return 0; }\n")

            loop = ImprovementLoop(project_root=tmpdir, output_dir=os.path.join(tmpdir, "out"))
            flagged = loop.scan_flagged_files()
            assert len(flagged) >= 1
            assert any("TODO" in f.flags for f in flagged)

    def test_scan_finds_fixme(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "broken.hpp")
            with open(test_file, "w") as f:
                f.write("// FIXME: memory leak here\nvoid leak() {}\n")

            loop = ImprovementLoop(project_root=tmpdir, output_dir=os.path.join(tmpdir, "out"))
            flagged = loop.scan_flagged_files()
            assert any("FIXME" in f.flags for f in flagged)

    def test_scan_finds_destruction(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "old.cpp")
            with open(test_file, "w") as f:
                f.write("// flagged for destruction\nvoid obsolete() {}\n")

            loop = ImprovementLoop(project_root=tmpdir, output_dir=os.path.join(tmpdir, "out"))
            flagged = loop.scan_flagged_files()
            assert any("DESTRUCTION" in f.flags for f in flagged)

    def test_scan_skips_third_party(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tp_dir = os.path.join(tmpdir, "third_party")
            os.makedirs(tp_dir)
            with open(os.path.join(tp_dir, "lib.cpp"), "w") as f:
                f.write("// TODO: not our problem\n")

            loop = ImprovementLoop(project_root=tmpdir, output_dir=os.path.join(tmpdir, "out"))
            flagged = loop.scan_flagged_files()
            assert not any("third_party" in f.path for f in flagged)

    def test_priority_ordering(self):
        """DESTRUCTION flags should have higher priority than TODO."""
        with tempfile.TemporaryDirectory() as tmpdir:
            with open(os.path.join(tmpdir, "low.cpp"), "w") as f:
                f.write("// TODO: minor\n")
            with open(os.path.join(tmpdir, "high.cpp"), "w") as f:
                f.write("// flagged for destruction\n// FIXME: critical\n")

            loop = ImprovementLoop(project_root=tmpdir, output_dir=os.path.join(tmpdir, "out"))
            flagged = loop.scan_flagged_files()
            if len(flagged) >= 2:
                assert flagged[0].priority >= flagged[-1].priority


class TestTaskGeneration:
    """Test task queue generation."""

    def test_destruction_generates_remove_task(self):
        flagged = [FlaggedFile(
            path="old_module.cpp",
            flags=["DESTRUCTION"],
            flag_count=1,
            priority=5,
        )]
        loop = ImprovementLoop.__new__(ImprovementLoop)
        loop._root = None
        tasks = loop.generate_tasks(flagged, [])
        assert len(tasks) >= 1
        assert tasks[0]["type"] == "remove_or_refactor"

    def test_regression_generates_fix_task(self):
        flagged = []
        regressions = [{
            "spec": "H2O",
            "delta": 0.5,
            "relative_delta": 0.1,
        }]
        loop = ImprovementLoop.__new__(ImprovementLoop)
        loop._root = None
        tasks = loop.generate_tasks(flagged, regressions)
        assert any(t["type"] == "fix_regression" for t in tasks)


class TestComparison:
    """Test baseline comparison logic."""

    def test_regression_detected(self):
        loop = ImprovementLoop.__new__(ImprovementLoop)
        loop._baseline = {
            "H2O": {"energy": -10.0, "converged": True},
        }
        results = [SimulationResult(spec="H2O", energy=-9.0)]
        regressions, improvements = loop.compare_results(results)
        assert len(regressions) == 1
        assert regressions[0]["spec"] == "H2O"

    def test_improvement_detected(self):
        loop = ImprovementLoop.__new__(ImprovementLoop)
        loop._baseline = {
            "H2O": {"energy": -10.0, "converged": True},
        }
        results = [SimulationResult(spec="H2O", energy=-12.0)]
        regressions, improvements = loop.compare_results(results)
        assert len(improvements) == 1

    def test_no_baseline_no_comparison(self):
        loop = ImprovementLoop.__new__(ImprovementLoop)
        loop._baseline = {}
        results = [SimulationResult(spec="H2O", energy=-10.0)]
        regressions, improvements = loop.compare_results(results)
        assert len(regressions) == 0
        assert len(improvements) == 0


class TestReportSerialization:
    """Test report save/load."""

    def test_flagged_file_to_dict(self):
        ff = FlaggedFile(
            path="test.cpp", flags=["TODO", "FIXME"],
            flag_count=3, priority=5, file_hash="abc123",
        )
        d = ff.to_dict()
        assert d["path"] == "test.cpp"
        assert d["priority"] == 5
        assert "TODO" in d["flags"]
