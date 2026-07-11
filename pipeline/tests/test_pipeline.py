"""
pipeline/tests/test_pipeline.py — Tests for the VSEPR-SIM pipeline components.

Covers:
  - postprocess.py: data loading, normalisation, CSV write, HTML render
  - host.py:        FastAPI routes (status, results, figures, rerun)
  - Integration:    full postprocess → host round-trip
"""

from __future__ import annotations

import csv
import json
import os
import tempfile
from pathlib import Path

import pytest

# ── Ensure pipeline package is importable ────────────────────────────────────
import sys
ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT))

from pipeline.postprocess import (
    load_results,
    write_test_csv,
    collect_figures,
    render_html,
)

# FastAPI test client
from fastapi.testclient import TestClient


# =============================================================================
# Fixtures
# =============================================================================

@pytest.fixture
def tmp(tmp_path):
    return tmp_path


@pytest.fixture
def result_dir(tmp):
    d = tmp / "results"
    d.mkdir()
    return d


@pytest.fixture
def reports_dir(tmp):
    d = tmp / "reports"
    d.mkdir()
    return d


@pytest.fixture
def fig_dir(tmp):
    d = tmp / "figures"
    d.mkdir()
    # Create some dummy PNG files
    for name in ["overview_arch.png", "mol_water.png", "metal_Fe.png"]:
        (d / name).write_bytes(b"\x89PNG\r\n\x1a\n" + b"\x00" * 100)
    return d


def _write_ctest_json(result_dir: Path, passed=40, failed=2, exit_code=1):
    total = passed + failed
    status = "pass" if exit_code == 0 else "fail"
    data = {
        "suite": "ctest",
        "timestamp": "2026-01-01T00:00:00+00:00",
        "total": total,
        "passed": passed,
        "failed": failed,
        "exit_code": exit_code,
        "status": status,
        "build_dir": "/build",
    }
    (result_dir / "ctest_results.json").write_text(json.dumps(data))
    return data


def _write_pytest_json(result_dir: Path, passed=38, failed=0, exit_code=0):
    total = passed + failed
    status = "pass" if exit_code == 0 else "fail"
    data = {
        "suite": "pytest",
        "timestamp": "2026-01-01T00:00:00+00:00",
        "total": total,
        "passed": passed,
        "failed": failed,
        "exit_code": exit_code,
        "status": status,
    }
    (result_dir / "pytest_results.json").write_text(json.dumps(data))
    return data


def _write_pipeline_summary(result_dir: Path, elapsed=42):
    data = {
        "pipeline": "vsepr-sim",
        "version": "3.0.0",
        "timestamp": "2026-01-01T00:00:00+00:00",
        "total_elapsed_s": elapsed,
        "stages": [
            {"id": 2, "name": "ctest",       "status": "fail", "elapsed": "20s"},
            {"id": 3, "name": "pytest",      "status": "pass", "elapsed": "5s"},
            {"id": 4, "name": "figures",     "status": "pass", "elapsed": "10s"},
            {"id": 5, "name": "postprocess", "status": "pass", "elapsed": "2s"},
        ],
    }
    (result_dir / "pipeline_summary.json").write_text(json.dumps(data))
    return data


# =============================================================================
# postprocess.py — load_results
# =============================================================================

class TestLoadResults:
    def test_all_pass(self, result_dir):
        _write_ctest_json(result_dir, passed=49, failed=0, exit_code=0)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)

        data = load_results(result_dir)
        assert data["overall"] == "pass"
        assert data["total_tests"] == 87
        assert data["total_pass"]  == 87
        assert data["total_fail"]  == 0
        assert data["pass_rate"]   == 100.0

    def test_partial_fail(self, result_dir):
        _write_ctest_json(result_dir, passed=40, failed=2, exit_code=1)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)

        data = load_results(result_dir)
        assert data["overall"]    == "fail"
        assert data["total_fail"] == 2
        assert data["total_pass"] == 78
        assert data["total_tests"] == 80

    def test_missing_files(self, result_dir):
        # Should not raise; returns zeroed data
        data = load_results(result_dir)
        assert data["total_tests"] == 0
        assert data["overall"] in ("pass", "unknown", "fail")

    def test_pytest_json_report_format(self, result_dir):
        # pytest-json-report uses nested "summary" key
        data = {
            "summary": {"passed": 35, "failed": 3, "total": 38},
            "exit_code": 1,
        }
        (result_dir / "pytest_results.json").write_text(json.dumps(data))
        _write_ctest_json(result_dir, passed=10, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)

        result = load_results(result_dir)
        assert result["pytest"]["passed"] == 35
        assert result["pytest"]["failed"] == 3

    def test_stage_count(self, result_dir):
        _write_ctest_json(result_dir)
        _write_pytest_json(result_dir)
        _write_pipeline_summary(result_dir)

        data = load_results(result_dir)
        assert len(data["stages"]) == 4

    def test_version_propagated(self, result_dir):
        _write_ctest_json(result_dir)
        _write_pytest_json(result_dir)
        _write_pipeline_summary(result_dir)

        data = load_results(result_dir)
        assert data["version"] == "3.0.0"


# =============================================================================
# postprocess.py — write_test_csv
# =============================================================================

class TestWriteTestCsv:
    def test_csv_written(self, result_dir, reports_dir):
        _write_ctest_json(result_dir, passed=49, failed=0, exit_code=0)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)
        data = load_results(result_dir)

        csv_path = reports_dir / "test_log.csv"
        write_test_csv(data, csv_path)
        assert csv_path.exists()

        with open(csv_path, newline="") as f:
            rows = list(csv.DictReader(f))

        assert len(rows) == 2
        suites = {r["suite"] for r in rows}
        assert "ctest" in suites
        assert "pytest" in suites

    def test_csv_values(self, result_dir, reports_dir):
        _write_ctest_json(result_dir, passed=49, failed=0, exit_code=0)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)
        data = load_results(result_dir)

        csv_path = reports_dir / "test_log.csv"
        write_test_csv(data, csv_path)

        with open(csv_path, newline="") as f:
            rows = {r["suite"]: r for r in csv.DictReader(f)}

        assert int(rows["ctest"]["passed"])  == 49
        assert int(rows["pytest"]["passed"]) == 38
        assert rows["ctest"]["status"]  == "pass"
        assert rows["pytest"]["status"] == "pass"


# =============================================================================
# postprocess.py — collect_figures
# =============================================================================

class TestCollectFigures:
    def test_count(self, fig_dir):
        figs = collect_figures(fig_dir)
        assert len(figs) == 3

    def test_groups(self, fig_dir):
        figs = collect_figures(fig_dir)
        groups = {f["group"] for f in figs}
        assert "Overview" in groups   # overview_arch.png
        assert "Molecular" in groups  # mol_water.png

    def test_missing_dir(self, tmp):
        figs = collect_figures(tmp / "nonexistent")
        assert figs == []

    def test_sorted(self, fig_dir):
        figs = collect_figures(fig_dir)
        names = [f["name"] for f in figs]
        assert names == sorted(names)


# =============================================================================
# postprocess.py — render_html
# =============================================================================

class TestRenderHTML:
    def _make_data(self, result_dir):
        _write_ctest_json(result_dir, passed=49, failed=0, exit_code=0)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)
        return load_results(result_dir)

    def test_html_created(self, result_dir, reports_dir, fig_dir):
        data = self._make_data(result_dir)
        figs = collect_figures(fig_dir)
        render_html(data, figs, "test log content", reports_dir)
        assert (reports_dir / "index.html").exists()

    def test_html_has_pass_status(self, result_dir, reports_dir, fig_dir):
        data = self._make_data(result_dir)
        figs = collect_figures(fig_dir)
        render_html(data, figs, "", reports_dir)
        html = (reports_dir / "index.html").read_text(encoding="utf-8")
        assert "status-pass" in html
        # The tick may be stored as the literal character or as an HTML entity
        assert ("✔" in html or "&#10004;" in html or "ALL PASS" in html)

    def test_html_has_fail_status(self, result_dir, reports_dir, fig_dir):
        _write_ctest_json(result_dir, passed=40, failed=2, exit_code=1)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)
        data = load_results(result_dir)
        figs = collect_figures(fig_dir)
        render_html(data, figs, "", reports_dir)
        html = (reports_dir / "index.html").read_text(encoding="utf-8")
        assert "status-fail" in html

    def test_html_figure_gallery(self, result_dir, reports_dir, fig_dir):
        data = self._make_data(result_dir)
        figs = collect_figures(fig_dir)
        render_html(data, figs, "", reports_dir)
        html = (reports_dir / "index.html").read_text(encoding="utf-8")
        assert "overview_arch.png" in html
        assert "mol_water.png"     in html

    def test_html_log_content(self, result_dir, reports_dir, fig_dir):
        data = self._make_data(result_dir)
        render_html(data, [], "HELLO_LOG_MARKER", reports_dir)
        html = (reports_dir / "index.html").read_text(encoding="utf-8")
        assert "HELLO_LOG_MARKER" in html

    def test_html_version_visible(self, result_dir, reports_dir):
        data = self._make_data(result_dir)
        render_html(data, [], "", reports_dir)
        html = (reports_dir / "index.html").read_text(encoding="utf-8")
        assert "3.0.0" in html


# =============================================================================
# host.py — FastAPI routes
# =============================================================================

class TestHostRoutes:
    """Integration tests against the FastAPI app."""

    @pytest.fixture(autouse=True)
    def patch_paths(self, monkeypatch, tmp_path, result_dir, reports_dir, fig_dir):
        """Redirect host module paths to temp dirs."""
        import pipeline.host as host
        monkeypatch.setattr(host, "RESULTS_DIR",  result_dir)
        monkeypatch.setattr(host, "REPORTS_DIR",  reports_dir)
        monkeypatch.setattr(host, "FIG_DIR",      fig_dir)
        monkeypatch.setattr(host, "LOG_FILE",      tmp_path / "pipeline.log")
        self._result_dir  = result_dir
        self._reports_dir = reports_dir
        self._fig_dir     = fig_dir

    @pytest.fixture
    def client(self):
        from pipeline.host import app
        return TestClient(app, raise_server_exceptions=False)

    def test_root_redirects(self, client):
        r = client.get("/", follow_redirects=False)
        assert r.status_code in (301, 302, 307, 308)
        assert "/dashboard" in r.headers.get("location", "")

    def test_dashboard_fallback(self, client):
        r = client.get("/dashboard")
        assert r.status_code == 200
        assert "VSEPR-SIM" in r.text

    def test_dashboard_full(self, client, result_dir, reports_dir, fig_dir):
        _write_ctest_json(result_dir, passed=49, failed=0, exit_code=0)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)
        data = load_results(result_dir)
        figs = collect_figures(fig_dir)
        render_html(data, figs, "pipeline ran", reports_dir)

        r = client.get("/dashboard")
        assert r.status_code == 200
        assert "VSEPR-SIM" in r.text

    def test_api_status_no_data(self, client):
        r = client.get("/api/status")
        assert r.status_code == 200
        body = r.json()
        assert "overall" in body
        assert "total_tests" in body

    def test_api_status_with_data(self, client, result_dir, reports_dir, fig_dir):
        _write_ctest_json(result_dir, passed=49, failed=0, exit_code=0)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)
        data = load_results(result_dir)
        (reports_dir / "results.json").write_text(json.dumps(data))

        r = client.get("/api/status")
        assert r.status_code == 200
        body = r.json()
        assert body["overall"]     == "pass"
        assert body["total_pass"]  == 87
        assert body["total_tests"] == 87

    def test_api_results_no_data(self, client):
        r = client.get("/api/results")
        assert r.status_code == 404

    def test_api_results_with_data(self, client, result_dir, reports_dir, fig_dir):
        _write_ctest_json(result_dir, passed=49, failed=0, exit_code=0)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)
        data = load_results(result_dir)
        (reports_dir / "results.json").write_text(json.dumps(data))

        r = client.get("/api/results")
        assert r.status_code == 200
        body = r.json()
        assert body["total_pass"] == 87
        assert "ctest"  in body
        assert "pytest" in body

    def test_api_figures(self, client):
        r = client.get("/api/figures")
        assert r.status_code == 200
        body = r.json()
        assert "figures" in body
        assert "count"   in body
        assert body["count"] == 3

    def test_api_figures_urls(self, client):
        r = client.get("/api/figures")
        body = r.json()
        for fig in body["figures"]:
            assert fig["url"].startswith("/figures/")

    def test_api_pipeline_no_data(self, client):
        r = client.get("/api/pipeline")
        assert r.status_code == 404

    def test_api_pipeline_with_data(self, client, result_dir):
        _write_pipeline_summary(result_dir)
        r = client.get("/api/pipeline")
        assert r.status_code == 200
        body = r.json()
        assert len(body["stages"]) == 4

    def test_serve_figure(self, client, fig_dir):
        r = client.get("/figures/overview_arch.png")
        assert r.status_code == 200
        assert r.headers["content-type"] == "image/png"

    def test_serve_figure_missing(self, client):
        r = client.get("/figures/nonexistent.png")
        assert r.status_code == 404

    def test_serve_figure_traversal(self, client):
        r = client.get("/figures/../../etc/passwd")
        assert r.status_code == 404

    def test_api_log_empty(self, client):
        r = client.get("/api/log")
        assert r.status_code == 200
        assert "lines" in r.json()

    def test_api_log_with_content(self, client, monkeypatch, tmp_path):
        log_path = tmp_path / "pipeline.log"
        log_path.write_text("line 1\nline 2\nline 3\n")
        import pipeline.host as host
        monkeypatch.setattr(host, "LOG_FILE", log_path)

        r = client.get("/api/log?lines=2")
        assert r.status_code == 200
        body = r.json()
        assert body["lines"] == ["line 2", "line 3"]

    def test_api_rerun_starts(self, client, monkeypatch):
        # Patch the background runner to do nothing
        import pipeline.host as host
        monkeypatch.setattr(host, "_run_pipeline_background", lambda s: None)

        r = client.post("/api/rerun?stages=3,5")
        assert r.status_code == 200
        body = r.json()
        assert body["started"] is True
        assert body["stages"] == "3,5"

    def test_api_rerun_status(self, client):
        r = client.get("/api/rerun/status")
        assert r.status_code == 200
        body = r.json()
        assert "running" in body
        assert "last_start" in body

    def test_api_csv_missing(self, client):
        r = client.get("/api/csv")
        assert r.status_code == 404

    def test_api_csv_present(self, client, result_dir, reports_dir, fig_dir):
        _write_ctest_json(result_dir, passed=49, failed=0, exit_code=0)
        _write_pytest_json(result_dir, passed=38, failed=0, exit_code=0)
        _write_pipeline_summary(result_dir)
        data = load_results(result_dir)
        write_test_csv(data, reports_dir / "test_log.csv")

        r = client.get("/api/csv")
        assert r.status_code == 200
        assert "text/csv" in r.headers["content-type"]
