"""
Tests for the PyKernel polynomial fitter.

Verifies:
  - Single polynomial fit correctness
  - 11-15 degree sweep
  - Condition number reporting
  - R² accuracy on known functions
  - Export to JSON and CSV
  - Edge cases (insufficient points, degenerate data)
"""

import numpy as np
import pytest
import tempfile
import json
import os

from pykernel.poly_fitter import PolyFitter, PolyFitResult, PolyFitSweep


class TestPolyFitSingle:
    """Test single polynomial fits."""

    def test_fit_linear(self):
        """Degree-1 fit on perfectly linear data."""
        x = np.linspace(0, 10, 100)
        y = 3.0 * x + 7.0
        fitter = PolyFitter(min_degree=1, max_degree=1)
        result = fitter.fit_single(x, y, degree=1)
        assert result.r_squared > 0.9999
        assert result.residual_norm < 1e-8

    def test_fit_quadratic(self):
        """Degree-2 fit on parabolic data."""
        x = np.linspace(-5, 5, 200)
        y = 2.0 * x ** 2 - 3.0 * x + 1.0
        fitter = PolyFitter(min_degree=2, max_degree=2)
        result = fitter.fit_single(x, y, degree=2)
        assert result.r_squared > 0.9999

    def test_fit_high_degree(self):
        """Degree-13 fit on smooth function."""
        x = np.linspace(0, 2 * np.pi, 500)
        y = np.sin(x) + 0.5 * np.cos(3 * x)
        fitter = PolyFitter()
        result = fitter.fit_single(x, y, degree=13)
        assert result.r_squared > 0.99
        assert result.n_samples == 500
        assert result.degree == 13

    def test_condition_number_reported(self):
        """Condition number is finite and positive."""
        x = np.linspace(0, 1, 100)
        y = np.random.randn(100)
        fitter = PolyFitter()
        result = fitter.fit_single(x, y, degree=11)
        assert result.condition_number > 0
        assert np.isfinite(result.condition_number)

    def test_insufficient_points_raises(self):
        """Fitting with too few points raises ValueError."""
        x = np.array([1.0, 2.0, 3.0])
        y = np.array([1.0, 4.0, 9.0])
        fitter = PolyFitter()
        with pytest.raises(ValueError):
            fitter.fit_single(x, y, degree=11)

    def test_result_to_dict(self):
        """PolyFitResult serializes to dict."""
        x = np.linspace(0, 1, 50)
        y = x ** 2
        fitter = PolyFitter(min_degree=2, max_degree=2)
        result = fitter.fit_single(x, y, degree=2, run_id="test_001")
        d = result.to_dict()
        assert d["degree"] == 2
        assert d["run_id"] == "test_001"
        assert isinstance(d["coefficients"], list)


class TestPolyFitSweep:
    """Test 11-15 degree sweep."""

    def test_sweep_produces_five_fits(self):
        """Sweep from 11 to 15 produces 5 fit results."""
        x = np.linspace(0, 2 * np.pi, 500)
        y = np.sin(x)
        fitter = PolyFitter()
        sweep = fitter.fit_sweep(x, y, run_id="sweep_test")
        assert len(sweep.fits) == 5
        for f in sweep.fits:
            assert 11 <= f.degree <= 15

    def test_sweep_selects_best(self):
        """Best degree has highest R² (modulo condition penalty)."""
        x = np.linspace(0, 4, 300)
        y = np.exp(-x) * np.sin(5 * x)
        fitter = PolyFitter()
        sweep = fitter.fit_sweep(x, y)
        best = sweep.select_best()
        assert best is not None
        assert 11 <= best.degree <= 15
        assert sweep.best_r_squared > 0

    def test_sweep_data_hash_deterministic(self):
        """Same data produces same hash."""
        x = np.array([1.0, 2.0, 3.0, 4.0, 5.0] * 20)
        y = x ** 2
        fitter = PolyFitter(min_degree=2, max_degree=3)
        s1 = fitter.fit_sweep(x, y)
        s2 = fitter.fit_sweep(x, y)
        assert s1.data_hash == s2.data_hash

    def test_history_accumulates(self):
        """Fitter history grows with each sweep."""
        fitter = PolyFitter(min_degree=2, max_degree=3)
        x = np.linspace(0, 1, 50)
        fitter.fit_sweep(x, x ** 2)
        fitter.fit_sweep(x, x ** 3)
        assert len(fitter.history) == 2


class TestPolyFitExport:
    """Test JSON and CSV export."""

    def test_export_json(self):
        x = np.linspace(0, 1, 100)
        y = np.sin(x)
        fitter = PolyFitter(min_degree=3, max_degree=5)
        fitter.fit_sweep(x, y, run_id="export_test")

        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "fits.json")
            fitter.export_history(path)
            assert os.path.exists(path)
            with open(path) as f:
                data = json.load(f)
            assert len(data) == 1
            assert len(data[0]["fits"]) == 3

    def test_export_csv(self):
        x = np.linspace(0, 1, 100)
        y = x ** 2
        fitter = PolyFitter(min_degree=2, max_degree=4)
        fitter.fit_sweep(x, y, run_id="csv_test")

        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "fits.csv")
            fitter.export_csv(path)
            assert os.path.exists(path)
            with open(path) as f:
                lines = f.readlines()
            # Header + 3 fits
            assert len(lines) == 4
