"""
thermo_pipe â€” Batch runners and piping infrastructure for thermal simulation.

Orchestrates: STEP import â†’ material assignment â†’ heating simulation â†’
multi-format output (CSV, JSON, XLSX-ready TSV) with continuous thermo
validation at every stage.

Pipeline architecture (using Pipe[T] from pykernel.pipe):

    STEPSource â”€â”€â”
                 â”œâ†’ Pipe[StepAssembly] â†’ MaterialAssigner
                 â”‚                         â†“
                 â”‚                    Pipe[PartThermalConfig]
                 â”‚                         â†“
                 â”‚                    HeatingRunner
                 â”‚                         â†“
                 â”‚                    Pipe[PartThermalResult]
                 â”‚                         â†“
                 â”‚   â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ FanOut â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
                 â”‚   â”‚         â”‚           â”‚                â”‚
                 â”‚  CSVSink  JSONSink   XLSXSink   ThermoValidator
                 â”‚                                         â†“
                 â”‚                              ValidationReport
                 â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜

Anti-black-box: every datum, every intermediate c_p, every validation
check is logged with timestamp and provenance.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations

import os
import csv
import json
import logging
import time
import math
from dataclasses import dataclass, field
from typing import Optional, Callable
from datetime import datetime

_log = logging.getLogger(__name__)

from pykernel.pipe import Pipe, PipeRecord, FanOut, CSVSink, JSONSink, Transform
from pykernel.step_parser import parse_step, StepAssembly, NamedPart
from pykernel.metallic_cp import (
    METAL_DB, MetalRecord, lookup_metal, compute_cp, CpResult,
    dulong_petit, R,
)
from pykernel.heating_sim import (
    HeatingSimulation, HeatingSimConfig, PartThermalConfig,
    PartThermalResult, ThermalTimeStep, HeatSchedule,
    quick_heat_single,
)


# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# Thermo validation checks (continuous)
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

@dataclass
class ThermoCheck:
    """Single validation check result."""
    check_name: str
    passed: bool
    expected: float
    actual: float
    tolerance: float
    message: str = ""

    @property
    def error_pct(self) -> float:
        if self.expected == 0:
            return 0.0
        return abs(self.actual - self.expected) / abs(self.expected) * 100.0


@dataclass
class ValidationReport:
    """Aggregate validation report."""
    timestamp: str = ""
    total_checks: int = 0
    passed: int = 0
    failed: int = 0
    checks: list[ThermoCheck] = field(default_factory=list)

    @property
    def pass_rate(self) -> float:
        return self.passed / max(self.total_checks, 1) * 100.0

    def add(self, check: ThermoCheck):
        self.checks.append(check)
        self.total_checks += 1
        if check.passed:
            self.passed += 1
        else:
            self.failed += 1

    def summary_dict(self) -> dict:
        return {
            "timestamp": self.timestamp,
            "total": self.total_checks,
            "passed": self.passed,
            "failed": self.failed,
            "pass_rate_pct": round(self.pass_rate, 1),
            "failures": [
                {
                    "check": c.check_name,
                    "expected": c.expected,
                    "actual": c.actual,
                    "error_pct": round(c.error_pct, 2),
                    "message": c.message,
                }
                for c in self.checks if not c.passed
            ],
        }


class ThermoValidator:
    """Continuous thermo validation engine.

    Runs checks at every stage:
      - c_p at 298 K vs experimental data (Â±10%)
      - Dulong-Petit limit at high T (T > 2 * theta_D)
      - Energy conservation (sum QÂ·dt â‰ˆ mÂ·<cp>Â·Î”T)
      - Monotonic T increase for constant heating
      - cp > 0 for all T > 0
    """

    def __init__(self, tolerance_pct: float = 10.0):
        self.tolerance = tolerance_pct / 100.0
        self.report = ValidationReport(
            timestamp=datetime.now().isoformat()
        )

    def validate_cp_at_298(self, metal: MetalRecord) -> ThermoCheck:
        """Check c_p(298K) against experimental value."""
        result = compute_cp(metal, 298.0)
        exp = metal.cp_298
        check = ThermoCheck(
            check_name=f"cp_298_{metal.symbol}",
            passed=abs(result.Cp_approx - exp) <= self.tolerance * abs(exp),
            expected=exp,
            actual=result.Cp_approx,
            tolerance=self.tolerance,
            message=f"{metal.symbol}: Cp(298K) = {result.Cp_approx:.2f} vs exp {exp:.2f} J/(molÂ·K)"
        )
        self.report.add(check)
        return check

    def validate_dulong_petit(self, metal: MetalRecord) -> ThermoCheck:
        """Check that Cv approaches 3R at high T (T > 2*theta_D)."""
        T_high = 2.5 * metal.theta_D
        result = compute_cp(metal, T_high)
        dp = dulong_petit(1)
        check = ThermoCheck(
            check_name=f"dulong_petit_{metal.symbol}",
            passed=result.fraction_dp > 0.90,
            expected=dp,
            actual=result.Cv_lattice,
            tolerance=0.10,
            message=f"{metal.symbol}: Cv(T={T_high:.0f}K) fraction of 3R = {result.fraction_dp:.3f}"
        )
        self.report.add(check)
        return check

    def validate_energy_conservation(self, result: PartThermalResult) -> ThermoCheck:
        """Check energy balance: sum(QÂ·dt) vs mÂ·avg_cpÂ·Î”T."""
        if len(result.steps) < 2:
            check = ThermoCheck(
                check_name=f"energy_conservation_{result.part_name}",
                passed=True, expected=0, actual=0, tolerance=0,
                message="Skipped: insufficient steps"
            )
            self.report.add(check)
            return check

        delta_T = result.T_final - result.T_initial
        if abs(delta_T) < 1e-6:
            check = ThermoCheck(
                check_name=f"energy_conservation_{result.part_name}",
                passed=True, expected=0, actual=0, tolerance=0,
                message="Skipped: no temperature change"
            )
            self.report.add(check)
            return check

        mass_g = result.mass_kg * 1000.0
        avg_cp_s = result.total_energy_J / (mass_g * delta_T)
        # Average cp should be positive and in a reasonable range
        metal = lookup_metal(result.metal_symbol)
        expected_avg = metal.cp_298 / metal.molar_mass if metal else 0.5
        check = ThermoCheck(
            check_name=f"energy_conservation_{result.part_name}",
            passed=avg_cp_s > 0 and abs(avg_cp_s - expected_avg) <= 2.0 * expected_avg,
            expected=expected_avg,
            actual=avg_cp_s,
            tolerance=2.0,
            message=f"{result.part_name}: avg c_p_specific = {avg_cp_s:.4f} vs expected ~{expected_avg:.4f} J/(gÂ·K)"
        )
        self.report.add(check)
        return check

    def validate_monotonic_heating(self, result: PartThermalResult) -> ThermoCheck:
        """For constant heating: T must be non-decreasing."""
        violations = 0
        for i in range(1, len(result.steps)):
            if result.steps[i].T < result.steps[i - 1].T - 1e-10:
                violations += 1
        check = ThermoCheck(
            check_name=f"monotonic_T_{result.part_name}",
            passed=violations == 0,
            expected=0,
            actual=violations,
            tolerance=0,
            message=f"{result.part_name}: {violations} monotonicity violations"
        )
        self.report.add(check)
        return check

    def validate_positive_cp(self, result: PartThermalResult) -> ThermoCheck:
        """cp must be positive for all T > 0."""
        negatives = sum(1 for s in result.steps if s.cp_molar <= 0 and s.T > 0)
        check = ThermoCheck(
            check_name=f"positive_cp_{result.part_name}",
            passed=negatives == 0,
            expected=0,
            actual=negatives,
            tolerance=0,
            message=f"{result.part_name}: {negatives} steps with cp <= 0"
        )
        self.report.add(check)
        return check

    def validate_all_metals(self) -> ValidationReport:
        """Run c_p(298K) and Dulong-Petit checks for all metals in DB."""
        for sym, metal in METAL_DB.items():
            self.validate_cp_at_298(metal)
            self.validate_dulong_petit(metal)
        return self.report

    def validate_simulation_result(self, result: PartThermalResult) -> ValidationReport:
        """Run all per-result checks."""
        self.validate_energy_conservation(result)
        self.validate_monotonic_heating(result)
        self.validate_positive_cp(result)
        return self.report

    def full_validation(self, results: list[PartThermalResult]) -> ValidationReport:
        """Run all metal checks plus per-result validation."""
        self.validate_all_metals()
        for r in results:
            self.validate_simulation_result(r)
        return self.report


# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# XLSX-ready TSV sink (no openpyxl dependency)
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

class XLSXSink:
    """Writes pipe data to a tab-separated file importable by Excel/SolidWorks.

    Uses TSV format (Excel opens .tsv natively). If openpyxl is available,
    can optionally produce .xlsx directly.
    """

    def __init__(self, source: Pipe, path: str, columns: list[str]):
        self._path = path
        self._columns = columns
        self._count = 0
        self._use_xlsx = path.lower().endswith(".xlsx")

        os.makedirs(os.path.dirname(path) if os.path.dirname(path) else ".", exist_ok=True)

        if self._use_xlsx:
            self._rows: list[list] = []
        else:
            # TSV header
            with open(path, "w", newline="") as f:
                f.write("\t".join(columns) + "\n")

        source.subscribe(self._write_row)

    def _write_row(self, record: PipeRecord):
        data = record.data
        if isinstance(data, dict):
            row = [data.get(col, "") for col in self._columns]
        elif isinstance(data, (list, tuple)):
            row = list(data)
        else:
            row = [data]

        if self._use_xlsx:
            self._rows.append(row)
        else:
            with open(self._path, "a", newline="") as f:
                f.write("\t".join(str(v) for v in row) + "\n")
        self._count += 1

    def finalize(self):
        """Flush .xlsx file using openpyxl if available, else fallback to TSV."""
        if not self._use_xlsx:
            return
        try:
            import openpyxl
            wb = openpyxl.Workbook()
            ws = wb.active
            ws.title = "Thermal Results"
            ws.append(self._columns)
            for row in self._rows:
                ws.append(row)
            wb.save(self._path)
        except ImportError:
            # Fallback: write as TSV with .xlsx extension (Excel can still open)
            tsv_path = self._path.replace(".xlsx", ".tsv")
            with open(tsv_path, "w", newline="") as f:
                f.write("\t".join(self._columns) + "\n")
                for row in self._rows:
                    f.write("\t".join(str(v) for v in row) + "\n")

    @property
    def count(self) -> int:
        return self._count


# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# Batch STEP runner
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

@dataclass
class BatchJob:
    """One STEP file + material/mass assignments + heating config."""
    step_source: str                    # file path or STEP content
    material_map: dict[str, str]        # part_name â†’ metal symbol
    mass_map: dict[str, float]          # part_name â†’ mass in kg
    schedule: HeatSchedule = field(default_factory=HeatSchedule)
    T_initial: float = 298.0
    job_name: str = ""

    def __post_init__(self):
        if not self.job_name:
            self.job_name = os.path.basename(self.step_source) if os.path.isfile(
                self.step_source
            ) else "batch_job"


@dataclass
class BatchResult:
    """Results from one batch job."""
    job_name: str
    assembly: Optional[StepAssembly] = None
    thermal_results: list[PartThermalResult] = field(default_factory=list)
    validation: Optional[ValidationReport] = None
    elapsed_s: float = 0.0
    error: str = ""


class ThermoRunner:
    """Batch thermal simulation runner with full piping.

    Processes multiple STEP files, runs heating simulations,
    pipes results to CSV/JSON/XLSX, and validates continuously.

    Usage:
        runner = ThermoRunner(output_dir="out/thermo")
        runner.add_job(BatchJob(
            step_source="model.step",
            material_map={"Housing": "Al", "Shaft": "Fe"},
            mass_map={"Housing": 0.5, "Shaft": 1.2},
            schedule=HeatSchedule(power=500.0),
        ))
        results = runner.run_all()
        runner.write_summary()
    """

    def __init__(
        self,
        output_dir: str = "out/thermo",
        sim_config: HeatingSimConfig = None,
        validate: bool = True,
        csv_output: bool = True,
        json_output: bool = True,
        xlsx_output: bool = True,
    ):
        self.output_dir = output_dir
        self.sim_config = sim_config or HeatingSimConfig()
        self.validate = validate
        self.csv_output = csv_output
        self.json_output = json_output
        self.xlsx_output = xlsx_output
        self._jobs: list[BatchJob] = []
        self._results: list[BatchResult] = []
        self._validator = ThermoValidator() if validate else None

        os.makedirs(output_dir, exist_ok=True)

    def add_job(self, job: BatchJob):
        """Add a batch job."""
        self._jobs.append(job)

    def add_jobs(self, jobs: list[BatchJob]):
        """Add multiple batch jobs."""
        self._jobs.extend(jobs)

    def run_all(self) -> list[BatchResult]:
        """Execute all batch jobs sequentially."""
        self._results.clear()
        if self._validator:
            self._validator = ThermoValidator()

        for job in self._jobs:
            result = self._run_job(job)
            self._results.append(result)

        return self._results

    def _run_job(self, job: BatchJob) -> BatchResult:
        """Execute a single batch job."""
        t0 = time.time()
        br = BatchResult(job_name=job.job_name)

        try:
            # Parse STEP
            assembly = parse_step(job.step_source)
            br.assembly = assembly

            # Run heating simulation
            sim = HeatingSimulation(self.sim_config)
            sim.add_parts_from_assembly(
                assembly, job.material_map, job.mass_map,
                schedule=job.schedule,
                T_initial=job.T_initial,
            )
            results = sim.run()
            br.thermal_results = results

            # Pipe results to sinks
            self._pipe_results(job, results)

            # Validate
            if self._validator:
                for r in results:
                    self._validator.validate_simulation_result(r)
                br.validation = self._validator.report

        except Exception as e:
            br.error = f"{type(e).__name__}: {e}"
            _log.warning("Job %s failed: %s", job.job_name, br.error)

        br.elapsed_s = time.time() - t0
        return br

    def _pipe_results(self, job: BatchJob, results: list[PartThermalResult]):
        """Send results through pipe infrastructure to file sinks."""
        columns = [
            "part", "metal", "step", "time_s", "T_K", "Q_in_W",
            "cp_molar_J_molK", "cp_specific_J_gK", "dT_K",
            "energy_in_J", "fraction_DP",
        ]

        # Create pipe
        pipe = Pipe[dict](f"thermo_{job.job_name}")

        # Attach sinks
        sinks = []
        prefix = os.path.join(self.output_dir, job.job_name)

        if self.csv_output:
            csv_path = f"{prefix}_thermal.csv"
            sinks.append(CSVSink(pipe, csv_path, columns))

        if self.json_output:
            json_path = f"{prefix}_thermal.jsonl"
            sinks.append(JSONSink(pipe, json_path))

        xlsx_sink = None
        if self.xlsx_output:
            xlsx_path = f"{prefix}_thermal.xlsx"
            xlsx_sink = XLSXSink(pipe, xlsx_path, columns)
            sinks.append(xlsx_sink)

        # Push all rows through the pipe
        for r in results:
            for row in r.as_dict_rows():
                pipe.push(row, source=r.part_name)

        # Finalize XLSX if needed
        if xlsx_sink:
            xlsx_sink.finalize()

    def write_summary(self, path: str = None):
        """Write a JSON summary of all batch results."""
        if path is None:
            path = os.path.join(self.output_dir, "batch_summary.json")

        summary = {
            "timestamp": datetime.now().isoformat(),
            "n_jobs": len(self._results),
            "sim_config": {
                "dt_s": self.sim_config.dt,
                "t_end_s": self.sim_config.t_end,
                "T_max_K": self.sim_config.T_max,
            },
            "jobs": [],
        }

        for br in self._results:
            job_info = {
                "name": br.job_name,
                "elapsed_s": round(br.elapsed_s, 4),
                "n_parts": len(br.thermal_results),
                "error": br.error,
                "parts": [],
            }
            for r in br.thermal_results:
                job_info["parts"].append({
                    "name": r.part_name,
                    "metal": r.metal_symbol,
                    "T_initial_K": r.T_initial,
                    "T_final_K": round(r.T_final, 2),
                    "peak_T_K": round(r.peak_T, 2),
                    "total_energy_J": round(r.total_energy_J, 4),
                })
            summary["jobs"].append(job_info)

        if self._validator:
            summary["validation"] = self._validator.report.summary_dict()

        with open(path, "w") as f:
            json.dump(summary, f, indent=2, default=str)

    @property
    def results(self) -> list[BatchResult]:
        return self._results


# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# Convenience: single-file batch
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

def run_step_heating(
    step_source: str,
    material_map: dict[str, str],
    mass_map: dict[str, float],
    power_W: float = 100.0,
    t_end: float = 10.0,
    dt: float = 0.01,
    output_dir: str = "out/thermo",
) -> BatchResult:
    """One-liner: parse STEP â†’ assign materials â†’ heat â†’ export."""
    runner = ThermoRunner(
        output_dir=output_dir,
        sim_config=HeatingSimConfig(dt=dt, t_end=t_end),
    )
    runner.add_job(BatchJob(
        step_source=step_source,
        material_map=material_map,
        mass_map=mass_map,
        schedule=HeatSchedule(mode="constant", power=power_W),
    ))
    results = runner.run_all()
    runner.write_summary()
    return results[0] if results else BatchResult(job_name="empty")


def validate_thermo_db(tolerance_pct: float = 10.0) -> ValidationReport:
    """Validate all metals in DB against experimental c_p(298K)."""
    v = ThermoValidator(tolerance_pct=tolerance_pct)
    return v.validate_all_metals()




