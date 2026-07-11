"""
test_chart_helpers.py — Automated tests for pykernel.chart_helpers

Validates all chart types, data I/O, LaTeX generators, and molecule renderers.
"""

from __future__ import annotations

import os
import csv
import json
import tempfile
from pathlib import Path

import numpy as np
import pytest

from pykernel.chart_helpers import (
    # Data I/O
    TableData, load_csv, save_csv, load_json, save_json,
    # Style
    configure_style, PALETTE, PALETTE_CYCLE,
    # Chart primitives
    chart_line, chart_scatter, chart_bar, chart_barh,
    chart_histogram, chart_heatmap, chart_radar, chart_box,
    chart_stacked_area, chart_pie,
    # Diagram helpers
    draw_box, draw_arrow, create_diagram_axes,
    # Molecular rendering
    parse_xyz, infer_bonds,
    render_molecule_3d, render_molecule_2d, render_bond_diagram,
    hex_to_rgb,
    # LaTeX
    latex_figure, latex_subfigure_pair, latex_subfigure_grid, latex_table,
    # Save
    save_figure,
    # Manifest
    render_manifest,
    # Constants
    CPK_COLOURS, VDW_RADII, COV_RADII,
)

import matplotlib.pyplot as plt


# ═══════════════════════════════════════════════════════════════════════
# Fixtures
# ═══════════════════════════════════════════════════════════════════════

@pytest.fixture
def tmp_dir():
    with tempfile.TemporaryDirectory(prefix="chart_test_") as d:
        yield Path(d)


@pytest.fixture
def sample_csv(tmp_dir):
    path = tmp_dir / "sample.csv"
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["x", "y", "label"])
        w.writerow(["1.0", "2.0", "A"])
        w.writerow(["3.0", "4.0", "B"])
        w.writerow(["5.0", "6.0", "C"])
    return path


@pytest.fixture
def sample_xyz(tmp_dir):
    path = tmp_dir / "water.xyz"
    path.write_text(
        "3\nWater\n"
        "O  0.000  0.000  0.000\n"
        "H  0.757  0.586  0.000\n"
        "H -0.757  0.586  0.000\n"
    )
    return path


# ═══════════════════════════════════════════════════════════════════════
# Data I/O
# ═══════════════════════════════════════════════════════════════════════

class TestDataIO:
    def test_load_csv(self, sample_csv):
        td = load_csv(sample_csv)
        assert td.num_cols == 3
        assert td.num_rows == 3
        assert td.columns == ["x", "y", "label"]

    def test_column_float(self, sample_csv):
        td = load_csv(sample_csv)
        x = td.column("x")
        np.testing.assert_array_almost_equal(x, [1.0, 3.0, 5.0])

    def test_column_str(self, sample_csv):
        td = load_csv(sample_csv)
        labels = td.column_str("label")
        assert labels == ["A", "B", "C"]

    def test_save_load_csv_roundtrip(self, tmp_dir):
        td = TableData(name="test", columns=["a", "b"],
                       rows=[["1", "2"], ["3", "4"]])
        path = tmp_dir / "rt.csv"
        save_csv(td, path)
        td2 = load_csv(path)
        assert td2.num_rows == 2
        assert td2.columns == ["a", "b"]

    def test_save_load_json_roundtrip(self, tmp_dir):
        data = {"key": "value", "num": 42}
        path = tmp_dir / "rt.json"
        save_json(data, path)
        data2 = load_json(path)
        assert data2["key"] == "value"
        assert data2["num"] == 42


# ═══════════════════════════════════════════════════════════════════════
# Chart Primitives
# ═══════════════════════════════════════════════════════════════════════

class TestChartPrimitives:
    def test_line(self, tmp_dir):
        x = np.linspace(0, 10, 50)
        fig = chart_line([x], [np.sin(x)], labels=["sin"],
                         title="Test", xlabel="x", ylabel="y")
        assert fig is not None
        save_figure(fig, "line.png", outdir=tmp_dir)
        assert (tmp_dir / "line.png").exists()

    def test_line_multi(self, tmp_dir):
        x = np.linspace(0, 10, 50)
        fig = chart_line([x, x], [np.sin(x), np.cos(x)],
                         labels=["sin", "cos"])
        assert fig is not None
        plt.close(fig)

    def test_scatter(self, tmp_dir):
        fig = chart_scatter(np.array([1, 2, 3]), np.array([4, 5, 6]),
                            title="Scatter")
        save_figure(fig, "scatter.png", outdir=tmp_dir)
        assert (tmp_dir / "scatter.png").exists()

    def test_scatter_colormap(self, tmp_dir):
        fig = chart_scatter(np.array([1, 2, 3]), np.array([4, 5, 6]),
                            colors=np.array([0.1, 0.5, 0.9]),
                            colorbar_label="score")
        assert fig is not None
        plt.close(fig)

    def test_bar(self, tmp_dir):
        fig = chart_bar(["A", "B", "C"], [10, 20, 30], title="Bar")
        save_figure(fig, "bar.png", outdir=tmp_dir)
        assert (tmp_dir / "bar.png").exists()

    def test_barh(self, tmp_dir):
        fig = chart_barh(["A", "B"], [10, 20], title="BarH")
        save_figure(fig, "barh.png", outdir=tmp_dir)
        assert (tmp_dir / "barh.png").exists()

    def test_histogram(self, tmp_dir):
        fig = chart_histogram(np.random.randn(200), title="Hist")
        save_figure(fig, "hist.png", outdir=tmp_dir)
        assert (tmp_dir / "hist.png").exists()

    def test_heatmap(self, tmp_dir):
        fig = chart_heatmap(np.random.rand(5, 5), title="Heat",
                            annotate=True)
        save_figure(fig, "heat.png", outdir=tmp_dir)
        assert (tmp_dir / "heat.png").exists()

    def test_radar(self, tmp_dir):
        fig = chart_radar(["A", "B", "C", "D"], [0.8, 0.6, 0.9, 0.4],
                          title="Radar")
        save_figure(fig, "radar.png", outdir=tmp_dir)
        assert (tmp_dir / "radar.png").exists()

    def test_box(self, tmp_dir):
        fig = chart_box([np.random.randn(30), np.random.randn(30)],
                        group_names=["G1", "G2"], title="Box")
        save_figure(fig, "box.png", outdir=tmp_dir)
        assert (tmp_dir / "box.png").exists()

    def test_stacked_area(self, tmp_dir):
        x = np.arange(10)
        fig = chart_stacked_area(x, [x, x * 0.5], labels=["A", "B"],
                                 title="Stack")
        save_figure(fig, "stack.png", outdir=tmp_dir)
        assert (tmp_dir / "stack.png").exists()

    def test_pie(self, tmp_dir):
        fig = chart_pie([40, 30, 20, 10], ["A", "B", "C", "D"],
                        title="Pie")
        save_figure(fig, "pie.png", outdir=tmp_dir)
        assert (tmp_dir / "pie.png").exists()


# ═══════════════════════════════════════════════════════════════════════
# Diagram Helpers
# ═══════════════════════════════════════════════════════════════════════

class TestDiagramHelpers:
    def test_create_diagram_axes(self):
        fig, ax = create_diagram_axes(title="Test Diagram")
        assert fig is not None
        plt.close(fig)

    def test_draw_box(self):
        fig, ax = create_diagram_axes()
        draw_box(ax, 1, 1, 4, 2, "Module", detail="details")
        plt.close(fig)

    def test_draw_arrow(self):
        fig, ax = create_diagram_axes()
        draw_arrow(ax, 1, 1, 5, 5)
        plt.close(fig)


# ═══════════════════════════════════════════════════════════════════════
# Molecular Rendering
# ═══════════════════════════════════════════════════════════════════════

class TestMolecularRendering:
    def test_parse_xyz(self, sample_xyz):
        symbols, positions, title = parse_xyz(sample_xyz)
        assert symbols == ["O", "H", "H"]
        assert positions.shape == (3, 3)
        assert title == "Water"

    def test_infer_bonds(self, sample_xyz):
        symbols, positions, _ = parse_xyz(sample_xyz)
        bonds = infer_bonds(symbols, positions)
        assert len(bonds) == 2  # O-H × 2

    def test_hex_to_rgb(self):
        r, g, b = hex_to_rgb("#FF0000")
        assert r == 1.0
        assert g == 0.0
        assert b == 0.0

    def test_render_3d(self, sample_xyz, tmp_dir):
        symbols, positions, title = parse_xyz(sample_xyz)
        outpath = tmp_dir / "water_3d.png"
        fig = render_molecule_3d(symbols, positions, title, outpath)
        assert outpath.exists()
        plt.close(fig)

    def test_render_2d(self, sample_xyz, tmp_dir):
        symbols, positions, title = parse_xyz(sample_xyz)
        outpath = tmp_dir / "water_2d.png"
        fig = render_molecule_2d(symbols, positions, title, outpath)
        assert outpath.exists()
        plt.close(fig)

    def test_render_bond_diagram(self, sample_xyz, tmp_dir):
        symbols, positions, title = parse_xyz(sample_xyz)
        outpath = tmp_dir / "water_bonds.png"
        fig = render_bond_diagram(symbols, positions, title, outpath)
        assert outpath.exists()
        plt.close(fig)


# ═══════════════════════════════════════════════════════════════════════
# LaTeX Generators
# ═══════════════════════════════════════════════════════════════════════

class TestLaTeX:
    def test_figure(self):
        tex = latex_figure("fig.png", "Caption", "fig:label")
        assert "\\includegraphics" in tex
        assert "fig:label" in tex

    def test_subfigure_pair(self):
        tex = latex_subfigure_pair("a.png", "A", "b.png", "B",
                                   "Overall", "fig:pair")
        assert tex.count("\\begin{subfigure}") == 2

    def test_subfigure_grid(self):
        tex = latex_subfigure_grid(
            ["a.png", "b.png", "c.png", "d.png"],
            ["A", "B", "C", "D"],
            "Grid", "fig:grid")
        assert tex.count("\\begin{subfigure}") == 4

    def test_table(self):
        tex = latex_table(
            ["Name", "Value"],
            [["Fe", "210"], ["Al", "70"]],
            caption="Props", label="tab:p")
        assert "\\toprule" in tex
        assert "tab:p" in tex
        assert "Fe" in tex

    def test_table_escapes(self):
        tex = latex_table(["A_B", "C%D"], [["x&y", "z#w"]])
        assert "\\_" in tex
        assert "\\%" in tex
        assert "\\&" in tex
        assert "\\#" in tex


# ═══════════════════════════════════════════════════════════════════════
# Constants Coverage
# ═══════════════════════════════════════════════════════════════════════

class TestConstants:
    def test_palette_has_core_colors(self):
        assert "blue" in PALETTE
        assert "red" in PALETTE
        assert "green" in PALETTE

    def test_palette_cycle_length(self):
        assert len(PALETTE_CYCLE) >= 10

    def test_cpk_has_common_elements(self):
        for sym in ["H", "C", "N", "O", "Fe", "Au"]:
            assert sym in CPK_COLOURS

    def test_vdw_has_common_elements(self):
        for sym in ["H", "C", "N", "O"]:
            assert sym in VDW_RADII
            assert VDW_RADII[sym] > 0

    def test_cov_has_common_elements(self):
        for sym in ["H", "C", "N", "O"]:
            assert sym in COV_RADII
            assert COV_RADII[sym] > 0


# ═══════════════════════════════════════════════════════════════════════
# Style Configuration
# ═══════════════════════════════════════════════════════════════════════

class TestStyle:
    def test_configure_style(self):
        configure_style(font_size=12, font_family="sans-serif")
        assert plt.rcParams["font.size"] == 12
        assert "sans-serif" in plt.rcParams["font.family"]
        # Restore
        configure_style()


# ═══════════════════════════════════════════════════════════════════════
# Manifest Consumer
# ═══════════════════════════════════════════════════════════════════════

class TestManifest:
    def test_render_manifest(self, tmp_dir):
        # Create a minimal manifest + data CSV
        data_csv = tmp_dir / "test_data.csv"
        with open(data_csv, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["x", "y1", "y2"])
            for i in range(10):
                w.writerow([str(i), str(i * 2), str(i * 3)])

        manifest = {
            "name": "test",
            "data_file": "test_data.csv",
            "charts": [
                {
                    "id": "demo_line",
                    "type": "line",
                    "title": "Demo Line",
                    "xlabel": "X",
                    "ylabel": "Y",
                    "x_col": 0,
                    "y_cols": [1, 2],
                    "series_labels": ["y1", "y2"],
                    "fig_width": 8,
                    "fig_height": 5,
                },
                {
                    "id": "demo_bar",
                    "type": "bar",
                    "title": "Demo Bar",
                    "xlabel": "X",
                    "ylabel": "Y",
                    "x_col": 0,
                    "y_cols": [1],
                    "fig_width": 8,
                    "fig_height": 5,
                },
            ],
        }
        manifest_path = tmp_dir / "test_manifest.json"
        save_json(manifest, manifest_path)

        outdir = tmp_dir / "figs"
        saved = render_manifest(manifest_path, outdir=outdir)
        assert len(saved) == 2
        assert (outdir / "demo_line.png").exists()
        assert (outdir / "demo_bar.png").exists()
