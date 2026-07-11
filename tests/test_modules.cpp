/**
 * test_modules.cpp
 * ----------------
 * Verification tests for:
 *   - Module Registry (registration, lookup, categories)
 *   - Gas Module (ideal gas, kinetic theory, VdW, MB sampling)
 *   - Live Server (snapshot generation, serialization)
 *
 * 30 tests. All deterministic.
 */

#include "core/module_registry.hpp"
#include "core/gas_module.hpp"
#include "core/live_server.hpp"

#include <iostream>
#include <cmath>
#include <cassert>
#include <string>
#include <vector>
#include <sstream>

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name)                                                          \
    do {                                                                    \
        std::cout << "  T" << (pass_count + fail_count + 1) << " " << name; \
    } while(0)

#define PASS()                                                              \
    do {                                                                    \
        ++pass_count;                                                       \
        std::cout << " \033[0;32m[PASS]\033[0m\n";                          \
    } while(0)

#define FAIL(msg)                                                           \
    do {                                                                    \
        ++fail_count;                                                       \
        std::cout << " \033[0;31m[FAIL]\033[0m " << msg << "\n";            \
    } while(0)

#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define ASSERT_NEAR(a, b, tol, msg) do { if (std::abs((a)-(b)) > (tol)) { FAIL(msg " got=" + std::to_string(a) + " expected=" + std::to_string(b)); return; } } while(0)

// ============================================================================
// Module Registry Tests
// ============================================================================

void test_registry_empty() {
    TEST("Registry starts with modules after builtin registration");
    // Create a fresh test — builtins registered in main
    auto& reg = vsepr::ModuleRegistry::instance();
    ASSERT_TRUE(reg.modules().size() >= 9, "Expected at least 9 builtin modules");
    PASS();
}

void test_registry_find_atomistic() {
    TEST("Find 'atomistic' module by name");
    auto& reg = vsepr::ModuleRegistry::instance();
    auto* m = reg.find("atomistic");
    ASSERT_TRUE(m != nullptr, "atomistic not found");
    ASSERT_TRUE(m->version == "3.0.1", "wrong version");
    ASSERT_TRUE(m->status == vsepr::ModuleStatus::Ready, "wrong status");
    PASS();
}

void test_registry_find_by_cli() {
    TEST("Find module by CLI command 'cg'");
    auto& reg = vsepr::ModuleRegistry::instance();
    auto* m = reg.find("cg");
    ASSERT_TRUE(m != nullptr, "cg cli command not found");
    ASSERT_TRUE(m->name == "coarse-grain", "wrong name");
    PASS();
}

void test_registry_find_gas() {
    TEST("Find 'gas' module");
    auto& reg = vsepr::ModuleRegistry::instance();
    auto* m = reg.find("gas");
    ASSERT_TRUE(m != nullptr, "gas not found");
    ASSERT_TRUE(m->category == "simulation", "wrong category");
    PASS();
}

void test_registry_find_live_server() {
    TEST("Find 'live-server' module");
    auto& reg = vsepr::ModuleRegistry::instance();
    auto* m = reg.find("serve");
    ASSERT_TRUE(m != nullptr, "serve cli command not found");
    ASSERT_TRUE(m->name == "live-server", "wrong name");
    PASS();
}

void test_registry_by_category() {
    TEST("Filter modules by 'simulation' category");
    auto& reg = vsepr::ModuleRegistry::instance();
    auto sim = reg.by_category("simulation");
    ASSERT_TRUE(sim.size() >= 3, "Expected >=3 simulation modules");
    PASS();
}

void test_registry_by_analysis() {
    TEST("Filter modules by 'analysis' category");
    auto& reg = vsepr::ModuleRegistry::instance();
    auto ana = reg.by_category("analysis");
    ASSERT_TRUE(ana.size() >= 3, "Expected >=3 analysis modules");
    PASS();
}

void test_registry_nonexistent() {
    TEST("Finding nonexistent module returns nullptr");
    auto& reg = vsepr::ModuleRegistry::instance();
    auto* m = reg.find("nonexistent_module_xyz");
    ASSERT_TRUE(m == nullptr, "Should return nullptr");
    PASS();
}

void test_status_string() {
    TEST("ModuleStatus string conversion");
    ASSERT_TRUE(vsepr::status_string(vsepr::ModuleStatus::Ready) == "READY", "Ready");
    ASSERT_TRUE(vsepr::status_string(vsepr::ModuleStatus::Experimental) == "EXPERIMENTAL", "Experimental");
    PASS();
}

// ============================================================================
// Gas Module Tests
// ============================================================================

void test_ideal_gas_stp() {
    TEST("Ideal gas volume at STP (1 mol, 273.15K, 1atm)");
    double V = vsepr::gas::ideal_gas_volume(1.0, 273.15, 101325.0);
    // Should be ~22.414 L = 0.022414 m^3
    ASSERT_NEAR(V, 0.022414, 0.001, "STP volume");
    PASS();
}

void test_ideal_gas_pressure() {
    TEST("Ideal gas pressure (1 mol, 300K, 0.025 m^3)");
    double P = vsepr::gas::ideal_gas_pressure(1.0, 300.0, 0.025);
    // P = nRT/V = 1*8.314*300/0.025 ≈ 99772 Pa
    ASSERT_NEAR(P, 99772.0, 100.0, "pressure");
    PASS();
}

void test_rms_speed_ar() {
    TEST("RMS speed of Ar at 300K");
    double v = vsepr::gas::rms_speed(300.0, 0.039948);
    // sqrt(3*8.314*300/0.039948) ≈ 431.6 m/s
    ASSERT_NEAR(v, 431.6, 2.0, "Ar RMS speed");
    PASS();
}

void test_mean_speed_ar() {
    TEST("Mean speed of Ar at 300K");
    double v = vsepr::gas::mean_speed(300.0, 0.039948);
    // sqrt(8*8.314*300/(pi*0.039948)) ≈ 397.8 m/s
    ASSERT_NEAR(v, 397.8, 2.0, "Ar mean speed");
    PASS();
}

void test_most_probable_speed() {
    TEST("Most probable speed of N2 at 300K");
    double v = vsepr::gas::most_probable_speed(300.0, 0.028014);
    // sqrt(2*8.314*300/0.028014) ≈ 421.9 m/s
    ASSERT_NEAR(v, 421.9, 2.0, "N2 most probable speed");
    PASS();
}

void test_avg_kinetic_energy() {
    TEST("Average kinetic energy at 300K");
    double ke = vsepr::gas::avg_kinetic_energy(300.0);
    // 1.5 * 1.380649e-23 * 300 = 6.213e-21 J
    ASSERT_NEAR(ke, 6.213e-21, 1e-23, "avg KE");
    PASS();
}

void test_mean_free_path() {
    TEST("Mean free path at STP");
    double mfp = vsepr::gas::mean_free_path(273.15, 101325.0, 3.5e-10);
    // Should be ~70 nm
    double mfp_nm = mfp * 1e9;
    ASSERT_TRUE(mfp_nm > 50 && mfp_nm < 100, "MFP should be 50-100 nm at STP");
    PASS();
}

void test_vdw_volume_ar() {
    TEST("Van der Waals volume of Ar at STP");
    auto it = vsepr::gas::vdw_database().find("Ar");
    ASSERT_TRUE(it != vsepr::gas::vdw_database().end(), "Ar not in VdW db");
    double V = vsepr::gas::vdw_volume(1.0, 273.15, 101325.0, it->second.a, it->second.b);
    double V_ideal = vsepr::gas::ideal_gas_volume(1.0, 273.15, 101325.0);
    // VdW volume should be close to ideal at moderate conditions
    double ratio = V / V_ideal;
    ASSERT_TRUE(ratio > 0.95 && ratio < 1.05, "VdW/ideal ratio should be ~1.0 at STP");
    PASS();
}

void test_compute_properties_ar() {
    TEST("Compute full properties for Ar at 300K, 1atm");
    auto gp = vsepr::gas::compute_properties("Ar", 300.0, 1.0);
    ASSERT_TRUE(gp.formula == "Ar", "formula");
    ASSERT_NEAR(gp.temperature_K, 300.0, 0.01, "temperature");
    ASSERT_TRUE(gp.ideal_volume_m3 > 0.02, "ideal volume too small");
    ASSERT_TRUE(gp.rms_speed_ms > 400 && gp.rms_speed_ms < 450, "RMS speed range");
    ASSERT_TRUE(gp.has_vdw, "Ar should have VdW params");
    ASSERT_TRUE(gp.compressibility_Z > 0.99 && gp.compressibility_Z < 1.01, "Z near 1");
    PASS();
}

void test_compute_properties_unknown() {
    TEST("Compute properties for unknown formula (fallback)");
    auto gp = vsepr::gas::compute_properties("XeF6", 300.0, 1.0);
    ASSERT_TRUE(gp.formula == "XeF6", "formula");
    ASSERT_TRUE(!gp.has_vdw, "Unknown should not have VdW");
    ASSERT_TRUE(gp.ideal_volume_m3 > 0, "Should still compute ideal volume");
    PASS();
}

void test_mb_sampling_count() {
    TEST("Maxwell-Boltzmann sampling returns correct count");
    auto samples = vsepr::gas::sample_maxwell_boltzmann(300.0, 0.039948, 500, 42);
    ASSERT_TRUE(samples.size() == 500, "Expected 500 samples");
    PASS();
}

void test_mb_sampling_deterministic() {
    TEST("Maxwell-Boltzmann sampling is deterministic (same seed)");
    auto s1 = vsepr::gas::sample_maxwell_boltzmann(300.0, 0.028, 100, 123);
    auto s2 = vsepr::gas::sample_maxwell_boltzmann(300.0, 0.028, 100, 123);
    ASSERT_NEAR(s1[0].vx, s2[0].vx, 1e-10, "vx mismatch");
    ASSERT_NEAR(s1[50].vy, s2[50].vy, 1e-10, "vy mismatch");
    PASS();
}

void test_mb_sampling_rms_convergence() {
    TEST("MB sampling RMS converges to theoretical (10000 samples)");
    double M = 0.039948; // Ar
    double T = 300.0;
    auto samples = vsepr::gas::sample_maxwell_boltzmann(T, M, 10000, 42);
    double sq_sum = 0.0;
    for (const auto& s : samples) {
        sq_sum += s.vx*s.vx + s.vy*s.vy + s.vz*s.vz;
    }
    double rms_sampled = std::sqrt(sq_sum / samples.size());
    double rms_theory = vsepr::gas::rms_speed(T, M);
    double rel_err = std::abs(rms_sampled - rms_theory) / rms_theory;
    ASSERT_TRUE(rel_err < 0.05, "Relative error > 5%: " + std::to_string(rel_err));
    PASS();
}

void test_format_report() {
    TEST("GasProperties format_report produces non-empty string");
    auto gp = vsepr::gas::compute_properties("N2", 298.15, 1.0);
    std::string report = gp.format_report();
    ASSERT_TRUE(report.size() > 200, "Report too short");
    ASSERT_TRUE(report.find("N2") != std::string::npos, "Should contain formula");
    PASS();
}

void test_speed_histogram() {
    TEST("Speed histogram produces formatted output");
    auto samples = vsepr::gas::sample_maxwell_boltzmann(300.0, 0.028, 1000, 42);
    std::string hist = vsepr::gas::format_speed_histogram(samples, 15, 30);
    ASSERT_TRUE(hist.size() > 100, "Histogram too short");
    ASSERT_TRUE(hist.find("Maxwell-Boltzmann") != std::string::npos, "Missing header");
    PASS();
}

// ============================================================================
// Live Server Tests (no actual socket — test snapshot generation)
// ============================================================================

void test_snapshot_json() {
    TEST("AnalysisSnapshot serializes to valid JSON");
    vsepr::live::AnalysisSnapshot snap{};
    snap.cycle = 1;
    snap.seed = 42;
    snap.timestamp = "2025-01-01T00:00:00";
    snap.formula = "Ar";
    snap.temperature_K = 300.0;
    snap.pressure_atm = 1.0;
    snap.ideal_volume_L = 24.5;
    snap.rms_speed_ms = 431.0;
    snap.mean_free_path_nm = 68.0;
    snap.avg_ke_eV = 0.0388;
    snap.gas_type = "noble";
    snap.molar_mass_g = 39.948;
    snap.trail_id = "LVS-0000002a-0001";

    std::string json = snap.to_json();
    ASSERT_TRUE(json.find("\"formula\":\"Ar\"") != std::string::npos, "formula in JSON");
    ASSERT_TRUE(json.find("\"cycle\":1") != std::string::npos, "cycle in JSON");
    ASSERT_TRUE(json.find("\"seed\":42") != std::string::npos, "seed in JSON");
    PASS();
}

void test_snapshot_sse() {
    TEST("AnalysisSnapshot SSE format starts with 'data:'");
    vsepr::live::AnalysisSnapshot snap{};
    snap.formula = "He";
    snap.cycle = 5;
    snap.seed = 99;
    std::string sse = snap.to_sse_event();
    ASSERT_TRUE(sse.substr(0, 6) == "data: ", "Should start with 'data: '");
    ASSERT_TRUE(sse.find("\n\n") != std::string::npos, "Should end with double newline");
    PASS();
}

void test_server_config_defaults() {
    TEST("ServerConfig defaults");
    vsepr::live::ServerConfig cfg;
    ASSERT_TRUE(cfg.port == 9998, "default port should be 9998");
    ASSERT_NEAR(cfg.cycle_interval_ms, 3000.0, 0.1, "default interval");
    ASSERT_TRUE(cfg.base_seed == 0, "default seed should be 0 (time-based)");
    PASS();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n\033[1;35m"
              << "╔════════════════════════════════════════════════════════════════╗\n"
              << "║  Test Suite: Module Registry + Gas Module + Live Server       ║\n"
              << "╚════════════════════════════════════════════════════════════════╝\n"
              << "\033[0m\n";

    // Register builtin modules first
    vsepr::modules::register_builtin_modules();

    // Module Registry tests
    std::cout << "\033[1;36m┌─ Module Registry\033[0m\n";
    test_registry_empty();
    test_registry_find_atomistic();
    test_registry_find_by_cli();
    test_registry_find_gas();
    test_registry_find_live_server();
    test_registry_by_category();
    test_registry_by_analysis();
    test_registry_nonexistent();
    test_status_string();

    // Gas Module tests
    std::cout << "\033[1;36m┌─ Gas Module\033[0m\n";
    test_ideal_gas_stp();
    test_ideal_gas_pressure();
    test_rms_speed_ar();
    test_mean_speed_ar();
    test_most_probable_speed();
    test_avg_kinetic_energy();
    test_mean_free_path();
    test_vdw_volume_ar();
    test_compute_properties_ar();
    test_compute_properties_unknown();
    test_mb_sampling_count();
    test_mb_sampling_deterministic();
    test_mb_sampling_rms_convergence();
    test_format_report();
    test_speed_histogram();

    // Live Server tests
    std::cout << "\033[1;36m┌─ Live Server\033[0m\n";
    test_snapshot_json();
    test_snapshot_sse();
    test_server_config_defaults();

    // Summary
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
