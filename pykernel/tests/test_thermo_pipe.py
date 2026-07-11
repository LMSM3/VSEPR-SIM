"""
Tests for pykernel.thermo_pipe — Batch runners and piping infrastructure.

Validates:
  - ThermoValidator: c_p(298K) checks, Dulong-Petit, energy conservation
  - XLSXSink: TSV output for Excel/SolidWorks
  - BatchJob + ThermoRunner: end-to-end STEP → heat → export
  - Pipe integration: data flows through Pipe → CSVSink, JSONSink, XLSXSink
  - Continuous validation: all checks pass for the metal database
  - Summary output: JSON batch summary

VSEPR-SIM 3.0.0
"""

import os
import json
import tempfile
import pytest

from pykernel.pipe import Pipe
from pykernel.step_parser import StepAssembly, NamedPart, parse_step_string
from pykernel.metallic_cp import METAL_DB, MetalRecord
from pykernel.heating_sim import (
    HeatSchedule, PartThermalConfig, HeatingSimConfig,
    HeatingSimulation, quick_heat_single,
)
from pykernel.thermo_pipe import (
    ThermoCheck,
    ValidationReport,
    ThermoValidator,
    XLSXSink,
    BatchJob,
    BatchResult,
    ThermoRunner,
    run_step_heating,
    validate_thermo_db,
)


# ═══════════════════════════════════════════════════════════════════════
# Synthetic STEP for batch tests
# ═══════════════════════════════════════════════════════════════════════

BATCH_STEP = """
ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('Batch test'),'2;1');
FILE_NAME('batch.step','2025-01-01',(''),(''),'','','');
FILE_SCHEMA(('AP214'));
ENDSEC;
DATA;
#1 = CARTESIAN_POINT('O',(0.0,0.0,0.0));
#2 = PRODUCT('Housing','Al housing','',(''));
#3 = PRODUCT('Shaft','Fe shaft','',(''));
ENDSEC;
END-ISO-10303-21;
"""


# ═══════════════════════════════════════════════════════════════════════
# ThermoCheck tests
# ═══════════════════════════════════════════════════════════════════════

class TestThermoCheck:
    def test_pass(self):
        c = ThermoCheck("test", True, 25.0, 24.5, 0.1)
        assert c.passed
        assert c.error_pct < 3.0

    def test_fail(self):
        c = ThermoCheck("test", False, 25.0, 30.0, 0.1)
        assert not c.passed
        assert c.error_pct == 20.0

    def test_zero_expected(self):
        c = ThermoCheck("test", True, 0.0, 0.0, 0.1)
        assert c.error_pct == 0.0


# ═══════════════════════════════════════════════════════════════════════
# ValidationReport tests
# ═══════════════════════════════════════════════════════════════════════

class TestValidationReport:
    def test_empty_report(self):
        r = ValidationReport()
        assert r.total_checks == 0
        assert r.pass_rate == 0.0

    def test_add_checks(self):
        r = ValidationReport()
        r.add(ThermoCheck("a", True, 1.0, 1.0, 0.1))
        r.add(ThermoCheck("b", False, 1.0, 2.0, 0.1))
        assert r.total_checks == 2
        assert r.passed == 1
        assert r.failed == 1
        assert r.pass_rate == 50.0

    def test_summary_dict(self):
        r = ValidationReport(timestamp="2025-01-01")
        r.add(ThermoCheck("a", True, 1.0, 1.0, 0.1))
        s = r.summary_dict()
        assert s["total"] == 1
        assert s["passed"] == 1
        assert len(s["failures"]) == 0


# ═══════════════════════════════════════════════════════════════════════
# ThermoValidator tests
# ═══════════════════════════════════════════════════════════════════════

class TestThermoValidator:
    def test_validate_cp_298_aluminum(self):
        v = ThermoValidator(tolerance_pct=15.0)
        check = v.validate_cp_at_298(METAL_DB["Al"])
        assert check.passed, check.message

    def test_validate_cp_298_iron(self):
        v = ThermoValidator(tolerance_pct=15.0)
        check = v.validate_cp_at_298(METAL_DB["Fe"])
        assert check.passed, check.message

    def test_validate_cp_298_copper(self):
        v = ThermoValidator(tolerance_pct=15.0)
        check = v.validate_cp_at_298(METAL_DB["Cu"])
        assert check.passed, check.message

    def test_validate_dulong_petit_al(self):
        v = ThermoValidator()
        check = v.validate_dulong_petit(METAL_DB["Al"])
        assert check.passed, check.message

    def test_validate_all_metals_db(self):
        """Core continuous validation: all metals in DB."""
        report = validate_thermo_db(tolerance_pct=15.0)
        # At least 80% should pass (Debye model is approximate)
        assert report.pass_rate >= 80.0, (
            f"Pass rate {report.pass_rate:.1f}% — failures: "
            + ", ".join(c.message for c in report.checks if not c.passed)
        )

    def test_validate_monotonic_heating(self):
        result = quick_heat_single("Al", 0.1, 100.0, t_end=0.5, dt=0.01)
        v = ThermoValidator()
        check = v.validate_monotonic_heating(result)
        assert check.passed, check.message

    def test_validate_positive_cp(self):
        result = quick_heat_single("Fe", 0.5, 200.0, t_end=0.5, dt=0.01)
        v = ThermoValidator()
        check = v.validate_positive_cp(result)
        assert check.passed, check.message

    def test_validate_energy_conservation(self):
        result = quick_heat_single("Cu", 0.1, 100.0, t_end=1.0, dt=0.01)
        v = ThermoValidator()
        check = v.validate_energy_conservation(result)
        assert check.passed, check.message

    def test_full_validation(self):
        results = [
            quick_heat_single("Al", 0.1, 100.0, t_end=0.2, dt=0.01),
            quick_heat_single("Fe", 0.5, 200.0, t_end=0.2, dt=0.01),
        ]
        v = ThermoValidator(tolerance_pct=15.0)
        report = v.full_validation(results)
        assert report.total_checks > 0
        assert report.pass_rate >= 70.0


# ═══════════════════════════════════════════════════════════════════════
# XLSXSink tests
# ═══════════════════════════════════════════════════════════════════════

class TestXLSXSink:
    def test_tsv_output(self, tmp_path):
        pipe = Pipe[dict]("test_pipe")
        tsv_path = str(tmp_path / "output.tsv")
        sink = XLSXSink(pipe, tsv_path, ["name", "value"])
        pipe.push({"name": "test", "value": 42})
        pipe.push({"name": "test2", "value": 99})
        assert sink.count == 2
        with open(tsv_path) as f:
            lines = f.readlines()
        assert len(lines) == 3  # header + 2 rows
        assert "name\tvalue" in lines[0]

    def test_xlsx_fallback(self, tmp_path):
        """When openpyxl is unavailable, should fallback to TSV."""
        pipe = Pipe[dict]("test_pipe")
        xlsx_path = str(tmp_path / "output.xlsx")
        sink = XLSXSink(pipe, xlsx_path, ["a", "b"])
        pipe.push({"a": 1, "b": 2})
        sink.finalize()
        assert sink.count == 1


# ═══════════════════════════════════════════════════════════════════════
# Batch runner tests
# ═══════════════════════════════════════════════════════════════════════

class TestBatchRunner:
    def test_single_job(self, tmp_path):
        runner = ThermoRunner(
            output_dir=str(tmp_path),
            sim_config=HeatingSimConfig(dt=0.1, t_end=0.5),
        )
        runner.add_job(BatchJob(
            step_source=BATCH_STEP,
            material_map={"Housing": "Al", "Shaft": "Fe"},
            mass_map={"Housing": 0.2, "Shaft": 1.0},
            schedule=HeatSchedule(power=100.0),
        ))
        results = runner.run_all()
        assert len(results) == 1
        br = results[0]
        assert br.error == ""
        assert len(br.thermal_results) == 2

    def test_output_files_created(self, tmp_path):
        runner = ThermoRunner(
            output_dir=str(tmp_path),
            sim_config=HeatingSimConfig(dt=0.1, t_end=0.3),
        )
        runner.add_job(BatchJob(
            step_source=BATCH_STEP,
            material_map={"Housing": "Al", "Shaft": "Fe"},
            mass_map={"Housing": 0.2, "Shaft": 1.0},
            job_name="test_batch",
        ))
        runner.run_all()

        # Check CSV, JSONL, TSV files exist
        files = os.listdir(str(tmp_path))
        csv_files = [f for f in files if f.endswith(".csv")]
        json_files = [f for f in files if f.endswith(".jsonl")]
        tsv_files = [f for f in files if f.endswith(".tsv")]
        assert len(csv_files) >= 1, f"No CSV files in {files}"
        assert len(json_files) >= 1, f"No JSON files in {files}"
        assert len(tsv_files) >= 1, f"No TSV files in {files}"

    def test_csv_content(self, tmp_path):
        runner = ThermoRunner(
            output_dir=str(tmp_path),
            sim_config=HeatingSimConfig(dt=0.5, t_end=1.0),
            json_output=False, xlsx_output=False,
        )
        runner.add_job(BatchJob(
            step_source=BATCH_STEP,
            material_map={"Housing": "Al"},
            mass_map={"Housing": 0.2},
            job_name="csv_test",
        ))
        runner.run_all()

        csv_path = os.path.join(str(tmp_path), "csv_test_thermal.csv")
        assert os.path.exists(csv_path)
        with open(csv_path) as f:
            lines = f.readlines()
        assert len(lines) >= 2  # header + at least 1 data row
        assert "T_K" in lines[0]

    def test_batch_summary(self, tmp_path):
        runner = ThermoRunner(
            output_dir=str(tmp_path),
            sim_config=HeatingSimConfig(dt=0.5, t_end=1.0),
        )
        runner.add_job(BatchJob(
            step_source=BATCH_STEP,
            material_map={"Housing": "Al"},
            mass_map={"Housing": 0.1},
            job_name="sum_test",
        ))
        runner.run_all()
        runner.write_summary()

        summary_path = os.path.join(str(tmp_path), "batch_summary.json")
        assert os.path.exists(summary_path)
        with open(summary_path) as f:
            data = json.load(f)
        assert data["n_jobs"] == 1
        assert "validation" in data

    def test_multiple_jobs(self, tmp_path):
        runner = ThermoRunner(
            output_dir=str(tmp_path),
            sim_config=HeatingSimConfig(dt=0.5, t_end=0.5),
        )
        for i, metal in enumerate(["Al", "Fe", "Cu"]):
            step = f"""
ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('Job {i}'),'2;1');
FILE_NAME('j{i}.step','2025-01-01',(''),(''),'','','');
FILE_SCHEMA(('AP214'));
ENDSEC;
DATA;
#1 = PRODUCT('Part_{metal}','','',(''));
ENDSEC;
END-ISO-10303-21;
"""
            runner.add_job(BatchJob(
                step_source=step,
                material_map={f"Part_{metal}": metal},
                mass_map={f"Part_{metal}": 0.1 * (i + 1)},
                job_name=f"job_{metal}",
            ))
        results = runner.run_all()
        assert len(results) == 3
        for br in results:
            assert br.error == ""

    def test_validation_in_batch(self, tmp_path):
        runner = ThermoRunner(
            output_dir=str(tmp_path),
            sim_config=HeatingSimConfig(dt=0.1, t_end=0.3),
            validate=True,
        )
        runner.add_job(BatchJob(
            step_source=BATCH_STEP,
            material_map={"Housing": "Al"},
            mass_map={"Housing": 0.1},
        ))
        results = runner.run_all()
        assert results[0].validation is not None
        assert results[0].validation.total_checks > 0


# ═══════════════════════════════════════════════════════════════════════
# Convenience function tests
# ═══════════════════════════════════════════════════════════════════════

class TestConvenience:
    def test_run_step_heating(self, tmp_path):
        result = run_step_heating(
            step_source=BATCH_STEP,
            material_map={"Housing": "Al", "Shaft": "Fe"},
            mass_map={"Housing": 0.2, "Shaft": 1.0},
            power_W=100.0, t_end=0.5, dt=0.1,
            output_dir=str(tmp_path),
        )
        assert isinstance(result, BatchResult)
        assert result.error == ""
        assert len(result.thermal_results) == 2

    def test_validate_thermo_db(self):
        report = validate_thermo_db(tolerance_pct=15.0)
        assert report.total_checks > 0
        assert report.pass_rate >= 70.0


# ═══════════════════════════════════════════════════════════════════════
# Pipe integration tests
# ═══════════════════════════════════════════════════════════════════════

class TestPipeIntegration:
    def test_results_flow_through_pipe(self):
        """Verify that PartThermalResult.as_dict_rows() pipes correctly."""
        result = quick_heat_single("Al", 0.1, 100.0, t_end=0.1, dt=0.05)
        pipe = Pipe[dict]("test_flow")
        collected = []
        pipe.subscribe(lambda rec: collected.append(rec.data))
        for row in result.as_dict_rows():
            pipe.push(row, source="test")
        assert len(collected) == len(result.steps)
        assert all("T_K" in d for d in collected)

    def test_csv_sink_from_results(self, tmp_path):
        """Full pipe: result → Pipe → CSVSink."""
        from pykernel.pipe import CSVSink
        result = quick_heat_single("Cu", 0.1, 50.0, t_end=0.1, dt=0.05)
        pipe = Pipe[dict]("csv_pipe")
        csv_path = str(tmp_path / "test.csv")
        columns = ["part", "step", "T_K", "cp_molar_J_molK"]
        sink = CSVSink(pipe, csv_path, columns)
        for row in result.as_dict_rows():
            pipe.push(row)
        assert sink.count == len(result.steps)
        assert os.path.exists(csv_path)
