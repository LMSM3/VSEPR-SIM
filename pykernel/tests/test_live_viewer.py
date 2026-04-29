"""
Tests for pykernel.live_viewer — xyzA writer, synthetic driver, pipe analysis.
"""

import os
import time
import tempfile
import pytest
import numpy as np

from pykernel.live_viewer import (
    XYZAFrame,
    XYZAFrameWriter,
    SyntheticDriver,
    LiveAnalysis,
    LiveViewer,
)


# ============================================================================
# XYZAFrame
# ============================================================================

class TestXYZAFrame:
    def _make_frame(self, n=4, with_forces=True, with_charges=True):
        rng = np.random.RandomState(0)
        return XYZAFrame(
            step=1,
            n_atoms=n,
            elements=["C", "H", "O", "N"][:n],
            positions=rng.uniform(-5, 5, (n, 3)),
            charges=rng.normal(0, 0.1, n) if with_charges else None,
            velocities=rng.normal(0, 0.1, (n, 3)),
            forces=rng.normal(0, 1.0, (n, 3)) if with_forces else None,
            total_energy=-42.5,
        )

    def test_max_force(self):
        f = self._make_frame()
        assert f.max_force >= 0.0

    def test_max_force_none(self):
        f = self._make_frame(with_forces=False)
        assert f.max_force == 0.0

    def test_avg_charge(self):
        f = self._make_frame()
        assert isinstance(f.avg_charge, float)

    def test_avg_charge_none(self):
        f = self._make_frame(with_charges=False)
        assert f.avg_charge == 0.0

    def test_total_energy_field(self):
        f = self._make_frame()
        assert f.total_energy == -42.5


# ============================================================================
# XYZAFrameWriter
# ============================================================================

class TestXYZAFrameWriter:
    def _make_frame(self, step=1, n=3):
        rng = np.random.RandomState(step)
        return XYZAFrame(
            step=step,
            n_atoms=n,
            elements=["C", "H", "O"][:n],
            positions=rng.uniform(-3, 3, (n, 3)),
            charges=np.zeros(n),
            velocities=np.zeros((n, 3)),
            forces=np.zeros((n, 3)),
            energies=np.zeros(n),
            total_energy=-10.0 * step,
        )

    def test_write_creates_file(self, tmp_path):
        path = str(tmp_path / "traj.xyzA")
        writer = XYZAFrameWriter(path)
        writer.write(self._make_frame())
        assert os.path.exists(path)

    def test_write_correct_atom_count(self, tmp_path):
        path = str(tmp_path / "traj.xyzA")
        writer = XYZAFrameWriter(path)
        writer.write(self._make_frame(n=3))
        with open(path) as f:
            first_line = f.readline().strip()
        assert int(first_line) == 3

    def test_write_comment_has_step(self, tmp_path):
        path = str(tmp_path / "traj.xyzA")
        writer = XYZAFrameWriter(path)
        writer.write(self._make_frame(step=42))
        with open(path) as f:
            f.readline()
            comment = f.readline()
        assert "step=42" in comment

    def test_write_comment_has_energy(self, tmp_path):
        path = str(tmp_path / "traj.xyzA")
        writer = XYZAFrameWriter(path)
        writer.write(self._make_frame(step=1))
        with open(path) as f:
            f.readline()
            comment = f.readline()
        assert "energy=" in comment

    def test_overwrite_mode(self, tmp_path):
        path = str(tmp_path / "traj.xyzA")
        writer = XYZAFrameWriter(path, keep_history=False)
        writer.write(self._make_frame(step=1, n=3))
        writer.write(self._make_frame(step=2, n=3))
        with open(path) as f:
            content = f.read()
        # Overwrite: only latest frame (5 lines: count, comment, 3 atoms)
        lines = [l for l in content.strip().split("\n") if l.strip()]
        assert len(lines) == 5  # 1 count + 1 comment + 3 atoms

    def test_append_mode(self, tmp_path):
        path = str(tmp_path / "traj.xyzA")
        writer = XYZAFrameWriter(path, keep_history=True)
        writer.write(self._make_frame(step=1, n=2))
        writer.write(self._make_frame(step=2, n=2))
        with open(path) as f:
            lines = [l for l in f.read().strip().split("\n") if l.strip()]
        # 2 frames × (1 count + 1 comment + 2 atoms) = 8 lines
        assert len(lines) == 8

    def test_element_symbols_written(self, tmp_path):
        path = str(tmp_path / "traj.xyzA")
        writer = XYZAFrameWriter(path)
        writer.write(self._make_frame(n=2))
        with open(path) as f:
            lines = f.readlines()
        assert lines[2].split()[0] == "C"
        assert lines[3].split()[0] == "H"

    def test_no_forces_writes_zeros(self, tmp_path):
        path = str(tmp_path / "traj.xyzA")
        writer = XYZAFrameWriter(path)
        frame = XYZAFrame(
            step=1, n_atoms=1,
            elements=["C"],
            positions=np.zeros((1, 3)),
            total_energy=0.0,
        )
        writer.write(frame)
        with open(path) as f:
            lines = f.readlines()
        parts = lines[2].split()
        # element x y z charge vx vy vz fx fy fz energy = 12 fields
        assert len(parts) == 12


# ============================================================================
# SyntheticDriver
# ============================================================================

class TestSyntheticDriver:
    def test_step_returns_frame(self):
        driver = SyntheticDriver(n_atoms=8, seed=0)
        frame = driver.step()
        assert isinstance(frame, XYZAFrame)
        assert frame.n_atoms == 8
        assert frame.step == 1

    def test_step_increments(self):
        driver = SyntheticDriver(n_atoms=4, seed=0)
        for i in range(5):
            f = driver.step()
        assert f.step == 5

    def test_positions_shape(self):
        driver = SyntheticDriver(n_atoms=10, seed=0)
        f = driver.step()
        assert f.positions.shape == (10, 3)

    def test_forces_shape(self):
        driver = SyntheticDriver(n_atoms=6, seed=0)
        f = driver.step()
        assert f.forces is not None
        assert f.forces.shape == (6, 3)

    def test_elements_correct_length(self):
        driver = SyntheticDriver(n_atoms=12, seed=0)
        f = driver.step()
        assert len(f.elements) == 12

    def test_energy_is_float(self):
        driver = SyntheticDriver(n_atoms=4, seed=0)
        f = driver.step()
        assert isinstance(f.total_energy, float)

    def test_stream_yields_n_frames(self):
        driver = SyntheticDriver(n_atoms=4, seed=0)
        frames = list(driver.stream(10))
        assert len(frames) == 10

    def test_stream_steps_sequential(self):
        driver = SyntheticDriver(n_atoms=4, seed=0)
        frames = list(driver.stream(5))
        steps = [f.step for f in frames]
        assert steps == [1, 2, 3, 4, 5]

    def test_deterministic_with_seed(self):
        d1 = SyntheticDriver(n_atoms=4, seed=99)
        d2 = SyntheticDriver(n_atoms=4, seed=99)
        f1 = d1.step()
        f2 = d2.step()
        np.testing.assert_allclose(f1.positions, f2.positions, atol=1e-10)


# ============================================================================
# LiveAnalysis
# ============================================================================

class TestLiveAnalysis:
    def _make_frame(self, step=1, n=4, energy=-10.0):
        rng = np.random.RandomState(step)
        return XYZAFrame(
            step=step, n_atoms=n,
            elements=["C"] * n,
            positions=rng.uniform(-3, 3, (n, 3)),
            forces=rng.normal(0, 0.5, (n, 3)),
            charges=np.zeros(n),
            total_energy=energy,
        )

    def test_push_increments_pipe(self, tmp_path):
        analysis = LiveAnalysis(output_dir=str(tmp_path / "live"))
        analysis.push(self._make_frame(step=1), frame_ms=5.0)
        analysis.push(self._make_frame(step=2), frame_ms=6.0)
        assert analysis.frame_pipe.total_pushed == 2

    def test_energy_pipe_receives_values(self, tmp_path):
        analysis = LiveAnalysis(output_dir=str(tmp_path / "live"))
        analysis.push(self._make_frame(step=1, energy=-10.0), frame_ms=1.0)
        assert analysis.energy_pipe.total_pushed == 1
        assert analysis.energy_pipe.last() == pytest.approx(-10.0, abs=1e-9)

    def test_timing_pipe_receives_values(self, tmp_path):
        analysis = LiveAnalysis(output_dir=str(tmp_path / "live"))
        analysis.push(self._make_frame(step=1), frame_ms=12.3)
        assert analysis.timing_pipe.last() == pytest.approx(12.3, abs=1e-9)

    def test_csv_sink_written(self, tmp_path):
        analysis = LiveAnalysis(output_dir=str(tmp_path / "live"))
        for i in range(3):
            analysis.push(self._make_frame(step=i+1), frame_ms=float(i))
        csv_path = tmp_path / "live" / "frames.csv"
        assert csv_path.exists()
        with open(csv_path) as f:
            lines = f.readlines()
        assert len(lines) == 4  # header + 3 rows

    def test_jsonl_sink_written(self, tmp_path):
        import json
        analysis = LiveAnalysis(output_dir=str(tmp_path / "live"))
        analysis.push(self._make_frame(step=1), frame_ms=1.0)
        jsonl_path = tmp_path / "live" / "frames.jsonl"
        assert jsonl_path.exists()
        with open(jsonl_path) as f:
            obj = json.loads(f.readline())
        assert "data" in obj
        assert obj["data"]["step"] == 1

    def test_stats_returns_dict(self, tmp_path):
        analysis = LiveAnalysis(output_dir=str(tmp_path / "live"))
        s = analysis.stats
        assert "frame" in s
        assert "energy" in s
        assert "timing" in s


# ============================================================================
# LiveViewer
# ============================================================================

class TestLiveViewer:
    def test_headless_run(self, tmp_path):
        viewer = LiveViewer(
            n_atoms=8,
            output_dir=str(tmp_path / "live"),
            launch_viewer=False,
            seed=0,
        )
        viewer.run(steps=20, fps=1000.0)  # fast as possible
        assert viewer._frame_count == 20

    def test_xyza_file_written(self, tmp_path):
        viewer = LiveViewer(
            n_atoms=4,
            output_dir=str(tmp_path / "live"),
            launch_viewer=False,
            seed=0,
        )
        viewer.run(steps=5, fps=1000.0)
        assert viewer.xyza_path.exists()

    def test_csv_produced(self, tmp_path):
        viewer = LiveViewer(
            n_atoms=4,
            output_dir=str(tmp_path / "live"),
            launch_viewer=False,
            seed=0,
        )
        viewer.run(steps=5, fps=1000.0)
        csv_path = viewer.output_dir / "frames.csv"
        assert csv_path.exists()
        with open(csv_path) as f:
            lines = f.readlines()
        assert len(lines) > 1

    def test_report_prints(self, tmp_path, capsys):
        viewer = LiveViewer(
            n_atoms=4,
            output_dir=str(tmp_path / "live"),
            launch_viewer=False,
            seed=0,
        )
        viewer.run(steps=10, fps=1000.0)
        viewer.report()
        out = capsys.readouterr().out
        assert "Frames written" in out

    def test_push_frame(self, tmp_path):
        driver = SyntheticDriver(n_atoms=4, seed=0)
        viewer = LiveViewer(
            n_atoms=4,
            output_dir=str(tmp_path / "live"),
            launch_viewer=False,
            seed=0,
        )
        for _ in range(5):
            frame = driver.step()
            viewer.push_frame(frame)
        assert viewer._frame_count == 5

    def test_analysis_accessible(self, tmp_path):
        viewer = LiveViewer(
            n_atoms=4,
            output_dir=str(tmp_path / "live"),
            launch_viewer=False,
            seed=0,
        )
        viewer.run(steps=5, fps=1000.0)
        assert viewer.analysis.frame_pipe.total_pushed == 5

    def test_custom_xyza_path(self, tmp_path):
        custom = str(tmp_path / "custom_traj.xyzA")
        viewer = LiveViewer(
            n_atoms=4,
            output_dir=str(tmp_path / "live"),
            xyza_path=custom,
            launch_viewer=False,
            seed=0,
        )
        viewer.run(steps=3, fps=1000.0)
        assert os.path.exists(custom)
