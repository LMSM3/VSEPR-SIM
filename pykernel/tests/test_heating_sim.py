"""
Tests for pykernel.heating_sim — Time-dependent heating simulation.

Validates:
  - Single-part constant heating: T rises monotonically
  - Energy conservation: total energy ≈ m·<cp>·ΔT
  - Heat schedules: constant, ramp, pulse, custom
  - Assembly-based multi-part simulation
  - Edge cases: zero power, zero mass, extreme dt
  - Quick-heat convenience function

VSEPR-SIM 3.0.0
"""

import math
import pytest

from pykernel.step_parser import StepAssembly, NamedPart, Vec3
from pykernel.metallic_cp import METAL_DB, compute_cp
from pykernel.heating_sim import (
    HeatSchedule,
    PartThermalConfig,
    PartThermalResult,
    ThermalTimeStep,
    HeatingSimConfig,
    HeatingSimulation,
    quick_heat_single,
    heat_assembly,
)


# ═══════════════════════════════════════════════════════════════════════
# Heat schedule tests
# ═══════════════════════════════════════════════════════════════════════

class TestHeatSchedule:
    def test_constant(self):
        s = HeatSchedule(mode="constant", power=100.0)
        assert s.Q_in(0.0) == 100.0
        assert s.Q_in(5.0) == 100.0
        assert s.Q_in(1000.0) == 100.0

    def test_ramp(self):
        s = HeatSchedule(mode="ramp", power=200.0, ramp_time=10.0)
        assert s.Q_in(0.0) == 0.0
        assert abs(s.Q_in(5.0) - 100.0) < 0.01
        assert abs(s.Q_in(10.0) - 200.0) < 0.01
        assert abs(s.Q_in(15.0) - 200.0) < 0.01  # capped

    def test_ramp_zero_time(self):
        s = HeatSchedule(mode="ramp", power=100.0, ramp_time=0.0)
        assert s.Q_in(0.0) == 100.0

    def test_pulse(self):
        s = HeatSchedule(mode="pulse", power=500.0, pulse_start=2.0, pulse_width=1.0)
        assert s.Q_in(1.0) == 0.0
        assert s.Q_in(2.0) == 500.0
        assert s.Q_in(2.5) == 500.0
        assert s.Q_in(3.0) == 500.0
        assert s.Q_in(4.0) == 0.0

    def test_custom(self):
        s = HeatSchedule(mode="custom", custom_fn=lambda t: t * 10)
        assert abs(s.Q_in(5.0) - 50.0) < 1e-10


# ═══════════════════════════════════════════════════════════════════════
# Part thermal config
# ═══════════════════════════════════════════════════════════════════════

class TestPartThermalConfig:
    def test_resolve_metal(self):
        cfg = PartThermalConfig(
            part_name="block", metal_symbol="Al", mass_kg=1.0
        )
        cfg.resolve_metal()
        assert cfg.metal is not None
        assert cfg.metal.name == "Aluminum"

    def test_resolve_unknown_metal(self):
        cfg = PartThermalConfig(
            part_name="block", metal_symbol="Xx", mass_kg=1.0
        )
        with pytest.raises(ValueError, match="Unknown metal"):
            cfg.resolve_metal()


# ═══════════════════════════════════════════════════════════════════════
# Single-part heating
# ═══════════════════════════════════════════════════════════════════════

class TestSinglePartHeating:
    def test_constant_heating_al(self):
        """Al block heated at 100 W for 1 s: T should increase."""
        result = quick_heat_single("Al", mass_kg=0.1, power_W=100.0, t_end=1.0, dt=0.01)
        assert result.T_final > result.T_initial
        assert result.T_final > 298.0

    def test_monotonic_temperature(self):
        """Constant heating → T must be non-decreasing."""
        result = quick_heat_single("Fe", mass_kg=0.5, power_W=200.0, t_end=2.0, dt=0.01)
        temps = result.temperatures()
        for i in range(1, len(temps)):
            assert temps[i] >= temps[i - 1] - 1e-10

    def test_energy_conservation(self):
        """Total energy in ≈ m·<cp>·ΔT."""
        result = quick_heat_single("Cu", mass_kg=0.1, power_W=100.0, t_end=1.0, dt=0.01)
        delta_T = result.T_final - result.T_initial
        mass_g = result.mass_kg * 1000.0
        # Approximate: avg cp_specific * mass_g * delta_T ≈ total_energy
        avg_cp = result.total_energy_J / (mass_g * max(delta_T, 1e-12))
        # Should be roughly Cu's cp_specific ≈ 0.385 J/(g·K)
        cu = METAL_DB["Cu"]
        expected_cp = cu.cp_298 / cu.molar_mass
        assert abs(avg_cp - expected_cp) / expected_cp < 0.20, (
            f"avg_cp = {avg_cp:.4f}, expected ~{expected_cp:.4f}"
        )

    def test_zero_power(self):
        """No heating → T should not change."""
        result = quick_heat_single("Al", mass_kg=1.0, power_W=0.0, t_end=1.0, dt=0.1)
        assert abs(result.T_final - 298.0) < 0.01

    def test_positive_cp_all_steps(self):
        result = quick_heat_single("Fe", mass_kg=0.5, power_W=100.0, t_end=1.0, dt=0.01)
        for s in result.steps:
            assert s.cp_molar > 0
            assert s.cp_specific > 0

    def test_steps_count(self):
        result = quick_heat_single("Al", mass_kg=0.1, power_W=100.0, t_end=1.0, dt=0.1)
        # t_end/dt + 1 = 11
        assert len(result.steps) == 11

    def test_dict_rows(self):
        """as_dict_rows should produce flat dicts for piping."""
        result = quick_heat_single("Al", mass_kg=0.1, power_W=50.0, t_end=0.1, dt=0.05)
        rows = result.as_dict_rows()
        assert len(rows) > 0
        assert "part" in rows[0]
        assert "T_K" in rows[0]
        assert "cp_molar_J_molK" in rows[0]


# ═══════════════════════════════════════════════════════════════════════
# Multi-part and assembly
# ═══════════════════════════════════════════════════════════════════════

class TestMultiPart:
    def test_two_parts_different_metals(self):
        sim = HeatingSimulation(HeatingSimConfig(dt=0.1, t_end=1.0))
        sim.add_part(PartThermalConfig(
            part_name="Al_plate", metal_symbol="Al", mass_kg=0.1,
            schedule=HeatSchedule(power=100.0),
        ))
        sim.add_part(PartThermalConfig(
            part_name="Fe_block", metal_symbol="Fe", mass_kg=0.5,
            schedule=HeatSchedule(power=100.0),
        ))
        results = sim.run()
        assert len(results) == 2
        # Al (lighter, higher cp per gram) should heat faster
        al_dT = results[0].T_final - results[0].T_initial
        fe_dT = results[1].T_final - results[1].T_initial
        assert al_dT > fe_dT, "Lighter Al plate should heat faster"

    def test_assembly_based(self):
        """Test add_parts_from_assembly."""
        asm = StepAssembly()
        asm.parts = [
            NamedPart(id=1, name="Housing"),
            NamedPart(id=2, name="Shaft"),
        ]
        material_map = {"Housing": "Al", "Shaft": "Fe"}
        mass_map = {"Housing": 0.2, "Shaft": 1.0}

        sim = HeatingSimulation(HeatingSimConfig(dt=0.1, t_end=0.5))
        sim.add_parts_from_assembly(asm, material_map, mass_map)
        results = sim.run()
        assert len(results) == 2
        assert results[0].part_name == "Housing"
        assert results[1].part_name == "Shaft"

    def test_heat_assembly_convenience(self):
        asm = StepAssembly()
        asm.parts = [NamedPart(id=1, name="Plate")]
        results = heat_assembly(
            asm,
            material_map={"Plate": "Cu"},
            mass_map={"Plate": 0.5},
            power_W=50.0, t_end=0.5, dt=0.1,
        )
        assert len(results) == 1
        assert results[0].T_final > 298.0


# ═══════════════════════════════════════════════════════════════════════
# Ramp and pulse heating
# ═══════════════════════════════════════════════════════════════════════

class TestScheduleHeating:
    def test_ramp_slower_than_constant(self):
        """Ramp heating should produce less total ΔT than constant at same power."""
        r_const = quick_heat_single("Al", 0.1, 100.0, t_end=2.0, dt=0.01)
        sim = HeatingSimulation(HeatingSimConfig(dt=0.01, t_end=2.0))
        sim.add_part(PartThermalConfig(
            part_name="ramp", metal_symbol="Al", mass_kg=0.1,
            schedule=HeatSchedule(mode="ramp", power=100.0, ramp_time=2.0),
        ))
        r_ramp = sim.run()[0]
        assert r_ramp.T_final < r_const.T_final

    def test_pulse_limited_energy(self):
        """Pulse: energy only during the pulse window."""
        sim = HeatingSimulation(HeatingSimConfig(dt=0.01, t_end=5.0))
        sim.add_part(PartThermalConfig(
            part_name="pulse", metal_symbol="Cu", mass_kg=0.5,
            schedule=HeatSchedule(mode="pulse", power=1000.0,
                                  pulse_start=1.0, pulse_width=0.5),
        ))
        result = sim.run()[0]
        # Energy should be approximately power * pulse_width = 500 J
        assert abs(result.total_energy_J - 500.0) < 20.0


# ═══════════════════════════════════════════════════════════════════════
# Simulation summary
# ═══════════════════════════════════════════════════════════════════════

class TestSimSummary:
    def test_summary_dict(self):
        sim = HeatingSimulation(HeatingSimConfig(dt=0.1, t_end=1.0))
        sim.add_part(PartThermalConfig(
            part_name="block", metal_symbol="Al", mass_kg=0.1,
            schedule=HeatSchedule(power=50.0),
        ))
        sim.run()
        s = sim.summary()
        assert s["n_parts"] == 1
        assert s["dt_s"] == 0.1
        assert len(s["parts"]) == 1
        assert "T_final_K" in s["parts"][0]
