"""Tests for pykernel.gas -- gas thermodynamics and pipe flow."""

import math
import pytest

from pykernel.gas import (
    R_UNIVERSAL, ATM_PA, GAS_DB, GasMixture, GasPipe,
    ideal_pressure, ideal_volume, ideal_density,
    vdw_pressure, vdw_solve_volume,
    compressibility_factor,
    speed_of_sound, mach_number,
    isentropic_T_ratio, isentropic_P_ratio,
    reynolds_number, colebrook_white, darcy_weisbach_dp,
    compute_pipe_flow, lookup_gas,
)


class TestGasDB:
    def test_species_count(self):
        assert len(GAS_DB) >= 10

    def test_all_gamma_positive(self):
        for sym, g in GAS_DB.items():
            assert g.gamma > 1.0, f"{sym} gamma={g.gamma}"

    def test_lookup(self):
        assert lookup_gas("N2") is not None
        assert lookup_gas("FAKE") is None

    def test_specific_properties(self):
        n2 = GAS_DB["N2"]
        assert abs(n2.cp_specific - n2.cp_molar / n2.molar_mass) < 1e-10
        assert abs(n2.R_specific - R_UNIVERSAL / n2.molar_mass) < 1e-10


class TestIdealGas:
    def test_pressure_volume_roundtrip(self):
        P = ideal_pressure(1.0, 300.0, 0.025)
        V = ideal_volume(1.0, 300.0, P)
        assert abs(V - 0.025) < 1e-8

    def test_density_at_stp(self):
        n2 = GAS_DB["N2"]
        rho = ideal_density(n2, 273.15, ATM_PA)
        assert 1.1 < rho < 1.4  # ~1.25 kg/m3


class TestVanDerWaals:
    def test_vdw_reduces_to_ideal_at_low_P(self):
        n2 = GAS_DB["N2"]
        Vm_ideal = R_UNIVERSAL * 500.0 / 1000.0  # very low P
        P_vdw = vdw_pressure(n2, 500.0, Vm_ideal)
        assert abs(P_vdw - 1000.0) / 1000.0 < 0.05

    def test_vdw_solve_volume(self):
        n2 = GAS_DB["N2"]
        Vm = vdw_solve_volume(n2, 300.0, ATM_PA)
        Vm_ideal = R_UNIVERSAL * 300.0 / ATM_PA
        assert abs(Vm - Vm_ideal) / Vm_ideal < 0.02


class TestCompressibility:
    def test_Z_near_unity_at_low_P(self):
        n2 = GAS_DB["N2"]
        Z = compressibility_factor(n2, 300.0, ATM_PA)
        assert abs(Z - 1.0) < 0.01

    def test_Z_deviates_at_high_P(self):
        co2 = GAS_DB["CO2"]
        Z = compressibility_factor(co2, 310.0, 50e5)
        assert Z < 0.95  # noticeable deviation near Tc


class TestMixture:
    def test_air_mixture(self):
        air = GasMixture({"N2": 0.78, "O2": 0.21, "Ar": 0.01})
        assert 28.0 < air.molar_mass < 30.0
        assert 1.35 < air.gamma < 1.45

    def test_normalisation(self):
        mix = GasMixture({"N2": 2.0, "O2": 1.0})
        total = sum(mix.components.values())
        assert abs(total - 1.0) < 1e-10

    def test_density(self):
        air = GasMixture({"N2": 0.78, "O2": 0.21, "Ar": 0.01})
        rho = air.density(293.0, ATM_PA)
        assert 1.1 < rho < 1.3


class TestIsentropic:
    def test_T_ratio_at_M0(self):
        assert isentropic_T_ratio(1.4, 0.0) == 1.0

    def test_P_ratio_at_M0(self):
        assert abs(isentropic_P_ratio(1.4, 0.0) - 1.0) < 1e-10

    def test_speed_of_sound_N2(self):
        a = speed_of_sound(GAS_DB["N2"], 293.0)
        assert 340 < a < 360  # ~349 m/s


class TestPipeFlow:
    def test_reynolds(self):
        Re = reynolds_number(1.2, 10.0, 0.1, 1.8e-5)
        assert 60000 < Re < 70000

    def test_laminar_friction(self):
        f = colebrook_white(1000, 0.001)
        assert abs(f - 0.064) < 1e-6

    def test_turbulent_friction(self):
        f = colebrook_white(1e5, 0.001)
        assert 0.01 < f < 0.05

    def test_darcy_weisbach(self):
        dp = darcy_weisbach_dp(0.02, 100.0, 0.1, 1.2, 10.0)
        assert dp > 0

    def test_compute_pipe_flow(self):
        r = compute_pipe_flow("N2", velocity_m_s=15.0, pipe_D_m=0.05, pipe_L_m=50.0)
        assert r.Re > 0
        assert r.dP_Pa > 0
        assert r.mach < 1.0
        assert r.mass_flow_kg_s > 0
        d = r.as_dict()
        assert "gas" in d and "dP_kPa" in d


class TestGasPipe:
    def test_velocity_sweep(self):
        gp = GasPipe("N2", pipe_D_m=0.05, pipe_L_m=50.0)
        results = gp.sweep_velocity([1, 5, 10, 20])
        assert len(results) == 4
        assert results[-1].dP_Pa > results[0].dP_Pa

    def test_temperature_sweep(self):
        gp = GasPipe("O2")
        results = gp.sweep_temperature([300, 500, 800], velocity=10.0)
        assert len(results) == 3

    def test_gas_sweep(self):
        gp = GasPipe("N2")
        results = gp.sweep_gases(["N2", "He", "CO2"])
        assert len(results) == 3

    def test_summary_table(self):
        gp = GasPipe("N2")
        gp.sweep_velocity([5, 10])
        table = gp.summary_table()
        assert "N2" in table
        assert "Re" in table

    def test_empty_summary(self):
        gp = GasPipe("N2")
        assert gp.summary_table() == "(no results)"
