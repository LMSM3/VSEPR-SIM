"""
gpu_bridge.py — Python bridge to VSEPR-SIM GPU backend.

Wraps the C++ GPU backend (CUDA/OpenCL/CPU fallback) for use
from Python via subprocess calls to the vsepr CLI. Also provides
a pure-numpy fallback for polynomial fitting and eigenvalue
computation when GPU is not available.

The bridge does NOT embed a C extension — it communicates with the
compiled C++ engine via the CLI and structured file I/O (CSV/JSON).
This keeps the Python layer thin and the C++ kernel authoritative.
"""

import subprocess
import json
import os
import platform
import shutil
from pathlib import Path
from typing import Optional, Dict, List, Tuple
import numpy as np


class GPUBridge:
    """
    Bridge to VSEPR-SIM GPU backend via CLI.

    Discovers the compiled vsepr binary and invokes it for
    GPU-accelerated computation. Falls back to numpy for
    polynomial fitting and eigen operations.

    Usage:
        bridge = GPUBridge()
        print(f"Backend: {bridge.backend}")
        print(f"Device: {bridge.device_name}")

        # GPU-accelerated energy via CLI
        energy = bridge.compute_energy("NaCl@crystal", supercell="4,4,4")
    """

    def __init__(self, build_dir: Optional[str] = None):
        self._root = Path(__file__).parent.parent.absolute()
        self._build_dir = Path(build_dir) if build_dir else self._find_build_dir()
        self._vsepr_bin = self._find_binary()
        self._backend = "numpy_fallback"
        self._device_name = "CPU (numpy)"
        self._gpu_available = False

        # Probe the actual GPU status via CLI if binary exists
        if self._vsepr_bin:
            self._probe_gpu()

    @property
    def backend(self) -> str:
        return self._backend

    @property
    def device_name(self) -> str:
        return self._device_name

    @property
    def gpu_available(self) -> bool:
        return self._gpu_available

    @property
    def vsepr_binary(self) -> Optional[Path]:
        return self._vsepr_bin

    def _find_build_dir(self) -> Path:
        """Search for build directory."""
        candidates = [
            self._root / "build",
            self._root / "build" / "Release",
            self._root / "build" / "Debug",
            self._root / "cmake-build-release",
            self._root / "cmake-build-debug",
            self._root / "out" / "build",
        ]
        for c in candidates:
            if c.is_dir():
                return c
        return self._root / "build"

    def _find_binary(self) -> Optional[Path]:
        """Find the vsepr CLI binary."""
        ext = ".exe" if platform.system() == "Windows" else ""
        names = [f"vsepr{ext}", f"vsepr-cli{ext}"]

        # Search in build directory tree
        if self._build_dir.exists():
            for root, dirs, files in os.walk(self._build_dir):
                for name in names:
                    if name in files:
                        return Path(root) / name

        # Search in PATH
        for name in names:
            found = shutil.which(name)
            if found:
                return Path(found)

        return None

    def _probe_gpu(self):
        """Probe GPU status via the CLI."""
        try:
            result = subprocess.run(
                [str(self._vsepr_bin), "--gpu-info"],
                capture_output=True, text=True, timeout=10,
            )
            output = result.stdout + result.stderr
            if "CUDA" in output:
                self._backend = "CUDA"
                self._gpu_available = True
            elif "OpenCL" in output:
                self._backend = "OpenCL"
                self._gpu_available = True
            else:
                self._backend = "CPU_Fallback"

            # Try to extract device name
            for line in output.split("\n"):
                if "device" in line.lower() or "gpu" in line.lower():
                    self._device_name = line.strip()
                    break
        except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
            pass

    def run_cli(self, args: List[str], timeout: int = 300) -> subprocess.CompletedProcess:
        """Run the VSEPR CLI with given arguments."""
        if not self._vsepr_bin:
            raise RuntimeError("VSEPR binary not found. Build the project first.")

        cmd = [str(self._vsepr_bin)] + args
        return subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout,
            cwd=str(self._root),
        )

    def compute_energy(self, spec: str, **kwargs) -> Optional[float]:
        """Compute energy for a molecular spec via CLI."""
        args = [spec, "emit"]
        for key, val in kwargs.items():
            args.append(f"--{key.replace('_', '-')}")
            args.append(str(val))

        try:
            result = self.run_cli(args)
            # Parse energy from output
            for line in result.stdout.split("\n"):
                if "energy" in line.lower() and ("=" in line or ":" in line):
                    parts = line.replace("=", ":").split(":")
                    if len(parts) >= 2:
                        try:
                            return float(parts[-1].strip().split()[0])
                        except (ValueError, IndexError):
                            pass
        except (subprocess.TimeoutExpired, RuntimeError):
            pass
        return None

    def run_simulation(self, spec: str, action: str = "relax",
                       extra_args: Optional[List[str]] = None,
                       timeout: int = 600) -> Dict:
        """Run a simulation and return structured results."""
        args = [spec, action]
        if extra_args:
            args.extend(extra_args)

        try:
            result = self.run_cli(args, timeout=timeout)
            return {
                "success": result.returncode == 0,
                "stdout": result.stdout,
                "stderr": result.stderr,
                "returncode": result.returncode,
            }
        except subprocess.TimeoutExpired:
            return {"success": False, "error": "timeout", "stdout": "", "stderr": ""}

    def poly_fit_gpu(self, x: np.ndarray, y: np.ndarray,
                     degree: int) -> Tuple[np.ndarray, float]:
        """
        Polynomial fit — uses numpy (GPU would require cupy/cuBLAS).

        Returns (coefficients, condition_number).
        This is the compute path called by ContinuousRunner on each run.
        """
        x = np.asarray(x, dtype=np.float64)
        y = np.asarray(y, dtype=np.float64)

        # Scale to [-1, 1]
        x_min, x_max = x.min(), x.max()
        rng = x_max - x_min
        if rng < 1e-30:
            xs = np.zeros_like(x)
        else:
            xs = 2.0 * (x - x_min) / rng - 1.0

        V = np.vander(xs, N=degree + 1, increasing=True)
        cond = np.linalg.cond(V)

        # Regularized solve
        VtV = V.T @ V + 1e-10 * np.eye(degree + 1)
        Vty = V.T @ y
        coeffs = np.linalg.solve(VtV, Vty)

        return coeffs, float(cond)

    def eigen_decompose(self, matrix: np.ndarray) -> np.ndarray:
        """
        Eigenvalue decomposition — numpy (deterministic, inspectable).
        Returns eigenvalues sorted descending.
        """
        M = np.asarray(matrix, dtype=np.float64)
        M = 0.5 * (M + M.T)
        ev = np.linalg.eigvalsh(M)
        return np.sort(ev)[::-1]

    def info(self) -> Dict:
        """Return bridge status information."""
        return {
            "backend": self._backend,
            "device_name": self._device_name,
            "gpu_available": self._gpu_available,
            "vsepr_binary": str(self._vsepr_bin) if self._vsepr_bin else None,
            "build_dir": str(self._build_dir),
            "project_root": str(self._root),
        }
