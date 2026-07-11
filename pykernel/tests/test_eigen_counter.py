"""
Tests for the PyKernel eigen counter.

Verifies:
  - Recording from raw eigenvalues
  - Recording from symmetric matrices
  - Inertia tensor computation
  - Summary statistics
  - CSV and JSON export
  - Condition number and spectral gap
"""

import numpy as np
import pytest
import tempfile
import json
import os

from pykernel.eigen_counter import EigenCounter, EigenSnapshot


class TestEigenSnapshot:
    """Test EigenSnapshot properties."""

    def test_condition_number(self):
        ev = np.array([10.0, 5.0, 1.0])
        snap = EigenSnapshot(run_id="t1", source="test", eigenvalues=ev)
        assert abs(snap.condition_number - 10.0) < 1e-10

    def test_spectral_gap(self):
        ev = np.array([10.0, 3.0, 1.0])
        snap = EigenSnapshot(run_id="t2", source="test", eigenvalues=ev)
        assert abs(snap.spectral_gap - 7.0) < 1e-10

    def test_trace(self):
        ev = np.array([4.0, 3.0, 2.0])
        snap = EigenSnapshot(run_id="t3", source="test", eigenvalues=ev)
        assert abs(snap.trace - 9.0) < 1e-10

    def test_rank(self):
        ev = np.array([5.0, 1e-15, 0.0])
        snap = EigenSnapshot(run_id="t4", source="test", eigenvalues=ev)
        assert snap.rank == 1

    def test_to_dict(self):
        ev = np.array([3.0, 2.0, 1.0])
        snap = EigenSnapshot(run_id="d1", source="inertia", eigenvalues=ev, n_atoms=10)
        d = snap.to_dict()
        assert d["run_id"] == "d1"
        assert d["source"] == "inertia"
        assert len(d["eigenvalues"]) == 3


class TestEigenCounter:
    """Test EigenCounter accumulator."""

    def test_record_raw(self):
        counter = EigenCounter()
        snap = counter.record(np.array([5.0, 3.0, 1.0]), source="test")
        assert len(counter.snapshots) == 1
        assert snap.eigenvalues[0] == 5.0  # Sorted descending

    def test_record_from_matrix(self):
        counter = EigenCounter()
        # Diagonal matrix: eigenvalues are diagonal entries
        M = np.diag([4.0, 2.0, 1.0])
        snap = counter.record_from_matrix(M, source="diag")
        assert len(snap.eigenvalues) == 3
        np.testing.assert_allclose(snap.eigenvalues, [4.0, 2.0, 1.0], atol=1e-10)

    def test_record_from_nonsquare_raises(self):
        counter = EigenCounter()
        with pytest.raises(ValueError):
            counter.record_from_matrix(np.ones((3, 4)))

    def test_record_inertia_simple(self):
        """Three atoms along x-axis: inertia about y,z should be nonzero."""
        counter = EigenCounter()
        positions = np.array([[0, 0, 0], [1, 0, 0], [2, 0, 0]], dtype=float)
        masses = np.array([1.0, 1.0, 1.0])
        snap = counter.record_inertia(positions, masses, run_id="inertia_test")
        assert snap.eigenvalues[0] > 0
        # One eigenvalue should be ~0 (rotation about x-axis)
        assert snap.eigenvalues[-1] < 1e-10

    def test_record_inertia_shape_mismatch_raises(self):
        counter = EigenCounter()
        with pytest.raises(ValueError):
            counter.record_inertia(
                np.ones((3, 3)), np.ones(5)
            )

    def test_summary(self):
        counter = EigenCounter()
        for i in range(10):
            counter.record(
                np.array([float(i + 3), float(i + 1), 1.0]),
                source="test", run_id=f"run_{i}",
            )
        s = counter.summary()
        assert s["n_snapshots"] == 10
        assert "test" in s["sources"]
        assert s["sources"]["test"] == 10

    def test_auto_run_id(self):
        counter = EigenCounter()
        s1 = counter.record(np.array([1.0]))
        s2 = counter.record(np.array([2.0]))
        assert s1.run_id != s2.run_id
        assert s1.run_id.startswith("auto_")


class TestEigenExport:
    """Test export to JSON and CSV."""

    def test_export_json(self):
        counter = EigenCounter()
        counter.record(np.array([5.0, 3.0, 1.0]), source="test", run_id="j1")

        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "eigen.json")
            counter.export_summary(path)
            assert os.path.exists(path)
            with open(path) as f:
                data = json.load(f)
            assert "summary" in data
            assert "snapshots" in data
            assert len(data["snapshots"]) == 1

    def test_export_csv(self):
        counter = EigenCounter()
        counter.record(np.array([4.0, 2.0]), source="cov", run_id="c1")
        counter.record(np.array([6.0, 1.0]), source="cov", run_id="c2")

        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "eigen.csv")
            counter.export_csv(path)
            assert os.path.exists(path)
            with open(path) as f:
                lines = f.readlines()
            assert len(lines) == 3  # Header + 2 rows
