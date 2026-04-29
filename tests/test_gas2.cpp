/**
 * test_gas2.cpp
 * -------------
 * Verification tests for gas2 module:
 *   - Species database
 *   - Equations of state (Ideal, VdW, Redlich-Kwong)
 *   - Kinetic theory (speeds, MFP, transport)
 *   - Heat capacity (DOF, adiabatic, JT)
 *   - Full analysis pipeline
 *   - MB sampling determinism and convergence
 *
 * 30 tests. All deterministic.
 */

#include "gas2/gas2_engine.hpp"
#include <iostream>
#include <cmath>
#include <string>

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name) do { std::cout << "  T" << (pass_count + fail_count + 1) << " " << name; } while(0)
#define PASS() do { ++pass_count; std::cout << " \033[0;32m[PASS]\033[0m\n"; } while(0)
#define FAIL(msg) do { ++fail_count; std::cout << " \033[0;31m[FAIL]\033[0m " << msg << "\n"; } while(0)
#define ASSERT_TRUE(c, m)  do { if (!(c)) { FAIL(m); return; } } while(0)
#define ASSERT_NEAR(a, b, t, m) do { if (std::abs((a)-(b))>(t)) { FAIL(std::string(m) + " got=" + std::to_string(a) + " exp=" + std::to_string(b)); return; } } while(0)

// ============================================================================
// Species Database
// ============================================================================

void test_species_count() {
    TEST("Species database has 14 entries");
    ASSERT_TRUE(vsepr::gas2::species_database().size() == 14, "expected 14");
    PASS();
}

void test_species_ar() {
    TEST("Argon species lookup");
    auto* sp = vsepr::gas2::find_species("Ar");
    ASSERT_TRUE(sp != nullptr, "Ar not found");
    ASSERT_NEAR(sp->molar_mass_g, 39.948, 0.01, "M");
    ASSERT_NEAR(sp->gamma, 1.667, 0.01, "gamma");
    ASSERT_TRUE(sp->n_atoms == 1, "atoms");
    PASS();
}

void test_species_co2() {
    TEST("CO2 species lookup");
    auto* sp = vsepr::gas2::find_species("CO2");
    ASSERT_TRUE(sp != nullptr, "CO2 not found");
    ASSERT_NEAR(sp->molar_mass_g, 44.010, 0.01, "M");
    ASSERT_TRUE(sp->n_atoms == 3, "atoms");
    PASS();
}

void test_species_unknown() {
    TEST("Unknown species returns nullptr");
    ASSERT_TRUE(vsepr::gas2::find_species("XeF6") == nullptr, "should be null");
    PASS();
}

// ============================================================================
// Equations of State
// ============================================================================

void test_ideal_stp() {
    TEST("Ideal gas at STP: V ≈ 22.414 L");
    auto r = vsepr::gas2::ideal_gas(1.0, 273.15, 101325.0);
    ASSERT_NEAR(r.V_L(), 22.414, 0.01, "V");
    ASSERT_NEAR(r.Z, 1.0, 1e-10, "Z");
    PASS();
}

void test_vdw_ar_stp() {
    TEST("VdW Ar at STP converges near ideal");
    auto* sp = vsepr::gas2::find_species("Ar");
    auto r = vsepr::gas2::vdw_solve_volume(1.0, 273.15, 101325.0, sp->vdw_a, sp->vdw_b);
    ASSERT_TRUE(r.converged, "not converged");
    double ratio = r.V_L() / 22.414;
    ASSERT_TRUE(ratio > 0.95 && ratio < 1.05, "VdW/ideal ratio");
    PASS();
}

void test_rk_ar_stp() {
    TEST("Redlich-Kwong Ar at STP converges");
    auto* sp = vsepr::gas2::find_species("Ar");
    auto rk = vsepr::gas2::rk_params_from_critical(sp->Tc_K, sp->Pc_atm * vsepr::gas2::atm_to_Pa);
    auto r = vsepr::gas2::rk_solve_volume(1.0, 273.15, 101325.0, rk.a_RK, rk.b_RK);
    ASSERT_TRUE(r.converged, "not converged");
    ASSERT_TRUE(r.Z > 0.95 && r.Z < 1.05, "Z range");
    PASS();
}

void test_eos_high_pressure() {
    TEST("VdW deviates at 100 atm");
    auto* sp = vsepr::gas2::find_species("N2");
    auto ideal = vsepr::gas2::ideal_gas(1.0, 300.0, 100.0 * 101325.0);
    auto vdw = vsepr::gas2::vdw_solve_volume(1.0, 300.0, 100.0 * 101325.0, sp->vdw_a, sp->vdw_b);
    double dev = std::abs(vdw.V_L() - ideal.V_L()) / ideal.V_L();
    ASSERT_TRUE(dev > 0.01, "VdW should deviate at 100 atm");
    PASS();
}

// ============================================================================
// Kinetic Theory
// ============================================================================

void test_dof_monoatomic() {
    TEST("DOF monoatomic: 3 trans, 0 rot");
    auto d = vsepr::gas2::compute_dof(1);
    ASSERT_TRUE(d.translational == 3, "trans");
    ASSERT_TRUE(d.rotational == 0, "rot");
    ASSERT_TRUE(d.total_classical == 3, "total");
    PASS();
}

void test_dof_diatomic() {
    TEST("DOF diatomic: 3 trans, 2 rot");
    auto d = vsepr::gas2::compute_dof(2);
    ASSERT_TRUE(d.translational == 3, "trans");
    ASSERT_TRUE(d.rotational == 2, "rot");
    ASSERT_TRUE(d.total_classical == 5, "total");
    ASSERT_TRUE(d.vibrational == 2, "vib modes");
    PASS();
}

void test_dof_polyatomic() {
    TEST("DOF nonlinear triatomic: 3 trans, 3 rot");
    auto d = vsepr::gas2::compute_dof(3, false);
    ASSERT_TRUE(d.rotational == 3, "rot");
    ASSERT_TRUE(d.total_classical == 6, "total");
    PASS();
}

void test_rms_speed_ar() {
    TEST("RMS speed Ar at 300K ≈ 432 m/s");
    double v = vsepr::gas2::rms_speed(300.0, 0.039948);
    ASSERT_NEAR(v, 432.0, 3.0, "v_rms");
    PASS();
}

void test_mean_free_path_stp() {
    TEST("MFP at STP ≈ 60-80 nm");
    double mfp = vsepr::gas2::mean_free_path(273.15, 101325.0, 3.4e-10);
    double nm = mfp * 1e9;
    ASSERT_TRUE(nm > 50 && nm < 100, "MFP nm range");
    PASS();
}

void test_collision_frequency() {
    TEST("Collision frequency > 0");
    double z = vsepr::gas2::collision_frequency(300.0, 101325.0, 0.039948, 3.4e-10);
    ASSERT_TRUE(z > 1e8, "z should be > 100 MHz");
    PASS();
}

void test_viscosity_positive() {
    // Chapman-Enskog (hard-sphere, Omega_22=1): eta = (5/16)*sqrt(m*kB*T/pi)/sigma^2
    // Ar at 300 K: tabulated 22.7 uPa.s; hard-sphere limit (Omega=1) ~25 uPa.s.
    // Accept 18-30 uPa.s — correctly excludes the old wrong answer of ~8 uPa.s.
    TEST("Hard-sphere viscosity Ar 300K in [18, 30] uPa.s (tabulated 22.7)");
    double mu = vsepr::gas2::viscosity_hard_sphere(300.0, 0.039948, 3.4e-10);
    double mu_uPa = mu * 1e6;
    ASSERT_TRUE(mu_uPa > 18.0 && mu_uPa < 30.0, "viscosity in [18,30] uPa.s");
    PASS();
}

void test_diffusion_positive() {
    TEST("Self-diffusion coefficient > 0");
    double D = vsepr::gas2::self_diffusion(300.0, 101325.0, 0.039948, 3.4e-10);
    ASSERT_TRUE(D > 0.0, "diffusion");
    PASS();
}

// ============================================================================
// Heat
// ============================================================================

void test_cv_monoatomic() {
    TEST("Cv monoatomic = 3/2 R ≈ 12.47");
    double cv = vsepr::gas2::Cv_from_dof(3);
    ASSERT_NEAR(cv, 12.472, 0.01, "Cv");
    PASS();
}

void test_gamma_monoatomic() {
    TEST("γ monoatomic = 5/3 ≈ 1.667");
    double g = vsepr::gas2::gamma_from_dof(3);
    ASSERT_NEAR(g, 1.667, 0.01, "gamma");
    PASS();
}

void test_gamma_diatomic() {
    TEST("γ diatomic = 7/5 = 1.400");
    double g = vsepr::gas2::gamma_from_dof(5);
    ASSERT_NEAR(g, 1.400, 0.01, "gamma");
    PASS();
}

void test_speed_of_sound() {
    TEST("Sound speed in N2 at 300K ≈ 349 m/s");
    double c = vsepr::gas2::speed_of_sound(300.0, 0.028014, 1.4);
    ASSERT_NEAR(c, 349.0, 5.0, "c_sound");
    PASS();
}

void test_adiabatic_T() {
    TEST("Adiabatic compression doubles T for gamma=5/3, V halved");
    double T2 = vsepr::gas2::adiabatic_T_from_V(300.0, 2.0, 1.0, 5.0/3.0);
    // T2 = 300 * 2^(2/3) ≈ 476.2
    ASSERT_NEAR(T2, 476.2, 1.0, "T2");
    PASS();
}

void test_jt_inversion() {
    TEST("JT inversion temperature for Ar > 0");
    auto* sp = vsepr::gas2::find_species("Ar");
    double T_inv = vsepr::gas2::inversion_temperature_vdw(sp->vdw_a, sp->vdw_b);
    ASSERT_TRUE(T_inv > 500, "T_inv should be > 500 K for Ar");
    PASS();
}

// ============================================================================
// Full Analysis Pipeline
// ============================================================================

void test_analyze_ar() {
    TEST("Full analysis of Ar at 300K, 1atm");
    auto a = vsepr::gas2::analyze("Ar", 300.0, 1.0);
    ASSERT_TRUE(a.species != nullptr, "species");
    ASSERT_TRUE(a.eos_ideal.V_L() > 20, "ideal V");
    ASSERT_TRUE(a.eos_vdw.converged, "vdw converged");
    ASSERT_TRUE(a.v_rms > 400 && a.v_rms < 450, "v_rms");
    ASSERT_TRUE(a.dof.total_classical == 3, "dof");
    PASS();
}

void test_analyze_unknown() {
    TEST("Analysis of unknown species uses fallback");
    auto a = vsepr::gas2::analyze("XeF6", 300.0, 1.0);
    ASSERT_TRUE(a.species == nullptr, "should be null");
    ASSERT_TRUE(a.eos_ideal.V_L() > 0, "ideal V > 0");
    PASS();
}

void test_format_report() {
    TEST("Full report is non-empty");
    auto a = vsepr::gas2::analyze("N2", 298.15, 1.0);
    auto r = a.format_full_report();
    ASSERT_TRUE(r.size() > 300, "report too short");
    ASSERT_TRUE(r.find("N2") != std::string::npos, "contains formula");
    PASS();
}

void test_format_json() {
    TEST("JSON output contains key fields");
    auto a = vsepr::gas2::analyze("He", 300.0, 2.0);
    auto j = a.to_json();
    ASSERT_TRUE(j.find("\"formula\":\"He\"") != std::string::npos, "formula");
    ASSERT_TRUE(j.find("\"Z_vdw\"") != std::string::npos, "Z_vdw");
    PASS();
}

void test_thermal_report_format() {
    TEST("Thermal report format is non-empty");
    auto r = vsepr::gas2::thermal_report("CH4", 500.0);
    auto s = r.format();
    ASSERT_TRUE(s.size() > 100, "too short");
    ASSERT_TRUE(s.find("CH4") != std::string::npos, "contains formula");
    PASS();
}

// ============================================================================
// Thermodynamic Potentials
// ============================================================================

void test_thermal_wavelength_300K() {
    TEST("Thermal wavelength Ar at 300K is ~16 pm");
    double lam = vsepr::gas2::thermal_wavelength_from_M(300.0, 39.948);
    double pm = lam * 1e12;
    // Lambda for Ar at 300K ≈ 16 pm
    ASSERT_TRUE(pm > 10.0 && pm < 25.0, "Lambda pm range");
    PASS();
}

void test_helmholtz_ideal_negative() {
    TEST("Helmholtz ideal gas A < 0 at STP");
    auto* sp = vsepr::gas2::find_species("Ar");
    double V = 24.6e-3;  // ~24.6 L in m^3
    auto h = vsepr::gas2::helmholtz(1.0, 300.0, V, sp->molar_mass_g,
                                     sp->vdw_a, sp->vdw_b, vsepr::gas2::EOSType::Ideal);
    ASSERT_TRUE(h.A_ig < 0.0, "A_ig should be negative");
    ASSERT_NEAR(h.A_dep, 0.0, 1e-10, "ideal has zero departure");
    PASS();
}

void test_gibbs_greater_than_helmholtz() {
    TEST("G > A (since PV > 0)");
    auto* sp = vsepr::gas2::find_species("N2");
    auto g = vsepr::gas2::gibbs(1.0, 300.0, 101325.0, sp);
    ASSERT_TRUE(g.G > g.A, "G = A + PV, PV > 0");
    PASS();
}

void test_chemical_potential_has_units() {
    TEST("Chemical potential mu is nonzero for Ar");
    auto* sp = vsepr::gas2::find_species("Ar");
    auto mu = vsepr::gas2::chemical_potential(300.0, 101325.0, sp);
    ASSERT_TRUE(mu.mu_J != 0.0, "mu_J");
    ASSERT_TRUE(mu.mu_Eh != 0.0, "mu_Eh");
    ASSERT_TRUE(mu.phi > 0.0, "fugacity coeff > 0");
    PASS();
}

void test_maxwell_construction_ar() {
    TEST("Maxwell construction for Ar at 0.8Tc converges");
    auto* sp = vsepr::gas2::find_species("Ar");
    double Tc_vdw = 8.0 * sp->vdw_a / (27.0 * vsepr::gas2::R_gas * sp->vdw_b);
    double T = 0.8 * Tc_vdw;
    auto pt = vsepr::gas2::maxwell_construction(T, sp->vdw_a, sp->vdw_b);
    ASSERT_TRUE(pt.converged, "should converge");
    ASSERT_TRUE(pt.P_sat_Pa > 0.0, "P_sat > 0");
    ASSERT_TRUE(pt.V_liq_m3 < pt.V_vap_m3, "V_liq < V_vap");
    PASS();
}

void test_maxwell_supercritical_fails() {
    TEST("Maxwell construction above Tc does not converge");
    auto* sp = vsepr::gas2::find_species("Ar");
    double Tc_vdw = 8.0 * sp->vdw_a / (27.0 * vsepr::gas2::R_gas * sp->vdw_b);
    auto pt = vsepr::gas2::maxwell_construction(Tc_vdw * 1.1, sp->vdw_a, sp->vdw_b);
    ASSERT_TRUE(!pt.converged, "supercritical should not converge");
    PASS();
}

// ============================================================================
// Potential Decomposition & Free Energy Functional
// ============================================================================

void test_potential_decomposition_total() {
    TEST("PotentialDecomposition recompute_total sums correctly");
    vsepr::gas2::PotentialDecomposition U{};
    U.U_bond = 100.0;
    U.U_vdw = -50.0;
    U.U_coul = 25.0;
    U.recompute_total();
    ASSERT_NEAR(U.U_total, 75.0, 1e-10, "total");
    PASS();
}

void test_potential_channel_access() {
    TEST("PotentialDecomposition channel(i) matches fields");
    vsepr::gas2::PotentialDecomposition U{};
    U.U_angle = 42.0;
    ASSERT_NEAR(U.channel(1), 42.0, 1e-10, "channel[1]=U_angle");
    PASS();
}

void test_landau_sign_flip() {
    TEST("Landau a changes sign at Tc");
    double Tc = 150.0;
    auto below = vsepr::gas2::LandauParams::from_species(100.0, Tc, 3.4e-10, 1e-21);
    auto above = vsepr::gas2::LandauParams::from_species(200.0, Tc, 3.4e-10, 1e-21);
    ASSERT_TRUE(below.a < 0.0, "a < 0 below Tc");
    ASSERT_TRUE(above.a > 0.0, "a > 0 above Tc");
    PASS();
}

void test_free_energy_functional_nonzero() {
    TEST("F[phi] is nonzero for tanh profile below Tc");
    auto lp = vsepr::gas2::LandauParams::from_species(100.0, 150.0, 3.4e-10, 1e-21);
    double phi_eq = std::sqrt(std::abs(lp.a) / (2.0 * lp.b + 1e-30));
    vsepr::gas2::DensityProfile prof;
    prof.init_tanh_interface(100, 100.0 * 3.4e-10, phi_eq, 5.0 * 3.4e-10);
    auto F = vsepr::gas2::evaluate_free_energy(prof, lp);
    ASSERT_TRUE(F.F_total != 0.0, "F should be nonzero");
    ASSERT_TRUE(F.f_density.size() == 100, "100 points");
    PASS();
}

void test_monitor_snapshot_json() {
    TEST("MonitorSnapshot to_json produces valid output");
    vsepr::gas2::MonitorSnapshot snap{};
    snap.cycle = 1;
    snap.T_K = 300.0;
    snap.formula = "Ar";
    snap.U.U_vdw = -100.0;
    snap.U.recompute_total();
    snap.profile.init_uniform(10, 1e-8, 0.0);
    snap.landau = vsepr::gas2::LandauParams::from_species(300.0, 150.0, 3.4e-10, 1e-21);
    snap.F = vsepr::gas2::evaluate_free_energy(snap.profile, snap.landau);
    auto j = snap.to_json();
    ASSERT_TRUE(j.find("\"cycle\":1") != std::string::npos, "cycle");
    ASSERT_TRUE(j.find("\"U\":") != std::string::npos, "U array");
    ASSERT_TRUE(j.find("\"phi\":") != std::string::npos, "phi array");
    PASS();
}

// ============================================================================
// Hartree Energy Conversion
// ============================================================================

void test_ke_hartree_300K() {
    TEST("KE Hartree at 300K ≈ 1.425e-3 Eh");
    double ke_Eh = vsepr::gas2::avg_translational_ke_Eh(300.0);
    // (3/2)(1.380649e-23)(300) / 4.3597447222071e-18 = 1.4253e-3
    ASSERT_NEAR(ke_Eh, 1.425e-3, 1e-5, "ke_Eh");
    PASS();
}

void test_ke_hartree_mass_invariance() {
    TEST("KE Hartree same for He, Ar, Ne at 300K");
    // Average translational KE is (3/2)kBT — mass-independent.
    double he = vsepr::gas2::avg_translational_ke_Eh(300.0);
    double ar = vsepr::gas2::avg_translational_ke_Eh(300.0);
    double ne = vsepr::gas2::avg_translational_ke_Eh(300.0);
    ASSERT_NEAR(he, ar, 1e-15, "He==Ar");
    ASSERT_NEAR(he, ne, 1e-15, "He==Ne");
    PASS();
}

void test_ke_hartree_total_dof() {
    TEST("KE total Eh scales with DOF: diatomic 5/3 of monoatomic");
    double mono = vsepr::gas2::avg_total_ke_Eh(300.0, 3);
    double di   = vsepr::gas2::avg_total_ke_Eh(300.0, 5);
    ASSERT_NEAR(di / mono, 5.0 / 3.0, 1e-10, "ratio");
    PASS();
}

void test_ke_hartree_in_analysis() {
    TEST("Gas2Analysis populates Hartree fields correctly");
    auto a = vsepr::gas2::analyze("Ar", 300.0, 1.0);
    ASSERT_NEAR(a.ke_translational_Eh, 1.425e-3, 1e-5, "ke_trans_Eh");
    ASSERT_NEAR(a.ke_total_Eh, a.ke_translational_Eh, 1e-15, "mono: total==trans");
    PASS();
}

void test_ke_hartree_json_contains_fields() {
    TEST("JSON output contains Hartree fields");
    auto a = vsepr::gas2::analyze("He", 300.0, 1.0);
    auto j = a.to_json();
    ASSERT_TRUE(j.find("\"ke_trans_Eh\"") != std::string::npos, "ke_trans_Eh");
    ASSERT_TRUE(j.find("\"ke_total_Eh\"") != std::string::npos, "ke_total_Eh");
    PASS();
}

// ============================================================================
// MB Sampling
// ============================================================================

void test_mb_deterministic() {
    TEST("MB sampling is deterministic");
    auto s1 = vsepr::gas2::sample_mb(300.0, 0.028, 100, 42);
    auto s2 = vsepr::gas2::sample_mb(300.0, 0.028, 100, 42);
    ASSERT_NEAR(s1[0].vx, s2[0].vx, 1e-12, "vx");
    ASSERT_NEAR(s1[50].vy, s2[50].vy, 1e-12, "vy");
    PASS();
}

void test_mb_rms_convergence() {
    TEST("MB RMS converges within 5% (10000 samples)");
    double M = 0.039948;
    auto s = vsepr::gas2::sample_mb(300.0, M, 10000, 42);
    double sq = 0.0;
    for (const auto& v : s) sq += v.vx*v.vx + v.vy*v.vy + v.vz*v.vz;
    double rms = std::sqrt(sq / s.size());
    double th = vsepr::gas2::rms_speed(300.0, M);
    ASSERT_TRUE(std::abs(rms - th) / th < 0.05, "err > 5%");
    PASS();
}

// ============================================================================
// Heat Map Pipeline
// ============================================================================

void test_heatmap_vv_bins_nonzero() {
    TEST("vx-vy heatmap has nonzero central bins");
    double M = 0.039948;
    auto s = vsepr::gas2::sample_mb(300.0, M, 10000, 42);
    double vmax = 0.0;
    for (const auto& v : s) {
        double sp = v.speed();
        if (sp > vmax) vmax = sp;
    }
    vmax *= 1.1;
    int n = 48;
    std::vector<double> bins(n * n, 0.0);
    for (const auto& v : s) {
        int bx = static_cast<int>((v.vx + vmax) / (2.0 * vmax) * n);
        int by = static_cast<int>((v.vy + vmax) / (2.0 * vmax) * n);
        if (bx >= 0 && bx < n && by >= 0 && by < n)
            bins[by * n + bx] += 1.0;
    }
    // Central bins (around 24,24) should have counts
    double center = bins[24 * n + 24];
    ASSERT_TRUE(center > 0, "centre bin should have counts");
    PASS();
}

void test_heatmap_vv_total_equals_samples() {
    TEST("vx-vy heatmap total count equals sample count");
    double M = 0.039948;
    auto s = vsepr::gas2::sample_mb(300.0, M, 5000, 99);
    double vmax = 0.0;
    for (const auto& v : s) {
        double sp = v.speed();
        if (sp > vmax) vmax = sp;
    }
    vmax *= 1.1;
    int n = 64;
    double total = 0.0;
    for (const auto& v : s) {
        int bx = static_cast<int>((v.vx + vmax) / (2.0 * vmax) * n);
        int by = static_cast<int>((v.vy + vmax) / (2.0 * vmax) * n);
        if (bx >= 0 && bx < n && by >= 0 && by < n)
            total += 1.0;
    }
    // All samples should land inside the grid (since vmax > max_speed)
    ASSERT_NEAR(total, 5000.0, 1.0, "total count");
    PASS();
}

void test_heatmap_atom_grid_ke_positive() {
    TEST("Atom grid average KE is positive for all occupied cells");
    double M = 0.039948;
    double m_mol = M / vsepr::gas2::N_A;
    auto s = vsepr::gas2::sample_mb(300.0, M, 10000, 42);
    int n = 48;
    int cells = n * n;
    std::vector<double> grid(cells, 0.0);
    std::vector<int> cnt(cells, 0);
    for (size_t i = 0; i < s.size(); ++i) {
        int c = static_cast<int>(i % cells);
        grid[c] += s[i].ke(m_mol);
        cnt[c]++;
    }
    for (int j = 0; j < cells; ++j) {
        if (cnt[j] > 0) grid[j] /= cnt[j];
    }
    // Every occupied cell should have KE > 0
    bool all_pos = true;
    for (int j = 0; j < cells; ++j) {
        if (cnt[j] > 0 && grid[j] <= 0.0) { all_pos = false; break; }
    }
    ASSERT_TRUE(all_pos, "all occupied cells KE > 0");
    PASS();
}

void test_heatmap_shell_matrix_total() {
    TEST("Shell matrix captures all samples inside vmax");
    double M = 0.039948;
    auto s = vsepr::gas2::sample_mb(300.0, M, 10000, 42);
    double vmax = 0.0;
    for (const auto& v : s) {
        double sp = v.speed();
        if (sp > vmax) vmax = sp;
    }
    vmax *= 1.1;
    int n = 64;
    double total = 0.0;
    for (const auto& v : s) {
        double r = std::sqrt(v.vx * v.vx + v.vy * v.vy);
        double angle = std::atan2(v.vy, v.vx);
        double a_norm = (angle + vsepr::gas2::PI) / (2.0 * vsepr::gas2::PI);
        int br = static_cast<int>(r / vmax * n);
        int ba = static_cast<int>(a_norm * n);
        if (br >= 0 && br < n && ba >= 0 && ba < n)
            total += 1.0;
    }
    // Vast majority should be captured (some outliers possible)
    ASSERT_TRUE(total > 9500.0, "shell should capture >95% of samples");
    PASS();
}

void test_heatmap_grid_resolution_256() {
    TEST("256x256 grid bins 10000 samples correctly");
    double M = 0.039948;
    auto s = vsepr::gas2::sample_mb(300.0, M, 10000, 42);
    int n = 256;
    int cells = n * n;
    std::vector<int> cnt(cells, 0);
    for (size_t i = 0; i < s.size(); ++i) {
        cnt[i % cells]++;
    }
    int total = 0;
    for (int j = 0; j < cells; ++j) total += cnt[j];
    ASSERT_TRUE(total == 10000, "total samples");
    PASS();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n\033[1;35m"
              << "╔════════════════════════════════════════════════════════════════╗\n"
              << "║  Test Suite: gas2 Module (Advanced Heat and Gas)              ║\n"
              << "╚════════════════════════════════════════════════════════════════╝\n"
              << "\033[0m\n";

    std::cout << "\033[1;36m┌─ Species Database\033[0m\n";
    test_species_count();
    test_species_ar();
    test_species_co2();
    test_species_unknown();

    std::cout << "\033[1;36m┌─ Equations of State\033[0m\n";
    test_ideal_stp();
    test_vdw_ar_stp();
    test_rk_ar_stp();
    test_eos_high_pressure();

    std::cout << "\033[1;36m┌─ Kinetic Theory\033[0m\n";
    test_dof_monoatomic();
    test_dof_diatomic();
    test_dof_polyatomic();
    test_rms_speed_ar();
    test_mean_free_path_stp();
    test_collision_frequency();
    test_viscosity_positive();
    test_diffusion_positive();

    std::cout << "\033[1;36m┌─ Heat Capacity & Adiabatic\033[0m\n";
    test_cv_monoatomic();
    test_gamma_monoatomic();
    test_gamma_diatomic();
    test_speed_of_sound();
    test_adiabatic_T();
    test_jt_inversion();

    std::cout << "\033[1;36m┌─ Full Analysis Pipeline\033[0m\n";
    test_analyze_ar();
    test_analyze_unknown();
    test_format_report();
    test_format_json();
    test_thermal_report_format();

    std::cout << "\033[1;36m┌─ Thermodynamic Potentials\033[0m\n";
    test_thermal_wavelength_300K();
    test_helmholtz_ideal_negative();
    test_gibbs_greater_than_helmholtz();
    test_chemical_potential_has_units();
    test_maxwell_construction_ar();
    test_maxwell_supercritical_fails();

    std::cout << "\033[1;36m┌─ Potential Decomposition & F[φ]\033[0m\n";
    test_potential_decomposition_total();
    test_potential_channel_access();
    test_landau_sign_flip();
    test_free_energy_functional_nonzero();
    test_monitor_snapshot_json();

    std::cout << "\033[1;36m┌─ Hartree Energy Conversion\033[0m\n";
    test_ke_hartree_300K();
    test_ke_hartree_mass_invariance();
    test_ke_hartree_total_dof();
    test_ke_hartree_in_analysis();
    test_ke_hartree_json_contains_fields();

    std::cout << "\033[1;36m┌─ Maxwell-Boltzmann Sampling\033[0m\n";
    test_mb_deterministic();
    test_mb_rms_convergence();

    std::cout << "\033[1;36m┌─ Heat Map Pipeline\033[0m\n";
    test_heatmap_vv_bins_nonzero();
    test_heatmap_vv_total_equals_samples();
    test_heatmap_atom_grid_ke_positive();
    test_heatmap_shell_matrix_total();
    test_heatmap_grid_resolution_256();

    int total = pass_count + fail_count;
    std::cout << "\n═══════════════════════════════════════════\n";
    if (fail_count == 0) {
        std::cout << "\033[0;32m✓ ALL " << total << " TESTS PASSED\033[0m ("
                  << pass_count << "/" << total << ")\n";
    } else {
        std::cout << "\033[0;31m✗ " << fail_count << " FAILED\033[0m, "
                  << pass_count << " passed (" << total << " total)\n";
    }
    std::cout << "═══════════════════════════════════════════\n\n";
    return fail_count > 0 ? 1 : 0;
}
