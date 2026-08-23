/**
 * test_xyza_dynamic_state.cpp
 * ===========================
 * VSEPR-SIM  |  WO-VSIM-66A  |  .xyza Dynamic State Gate
 * Group 48
 *
 * Regression tests confirming all 10 items of the WO-66A scope checklist.
 *
 * Test groups:
 *   G48-01  Fixed 11-column atom-line support
 *   G48-02  Missing Q/V/F trailing values default to zero
 *   G48-03  properties= declaration compatibility
 *   G48-04  Charge model passthrough (charge_model= tag)
 *   G48-05  Velocity and force arrays enter runtime XyzaAtomRecord
 *   G48-06  Q/V/F summaries from XyzaFrame helpers
 *   G48-07  Multi-frame archive writer (XyzaDynamicWriter)
 *   G48-08  Bridge: XyzaFrame -> XYZFrame -> XyzaFrame round-trip
 *   G48-09  Formation pre-state: from_xyz_frame smoke test
 */

#include "../include/vsim/io/xyza_dynamic.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

using namespace vsepr::io;

// ============================================================================
// Minimal test harness (same pattern as existing VSEPR-SIM test files)
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
	static void name(); \
	struct _Reg_##name { _Reg_##name() { \
		try { \
			name(); \
			std::cout << "  PASS  " #name "\n"; \
			++g_passed; \
		} catch (const std::exception& ex) { \
			std::cout << "  FAIL  " #name "  -  " << ex.what() << "\n"; \
			++g_failed; \
		} \
	}} _reg_##name; \
	static void name()

#define ASSERT(cond) \
	do { if (!(cond)) throw std::runtime_error("assertion failed: " #cond); } while (0)

#define ASSERT_NEAR(a, b, tol) \
	do { if (std::abs((a)-(b)) > (tol)) throw std::runtime_error( \
		"expected |" #a " - " #b "| <= " #tol); } while (0)

// ============================================================================
// Temporary-file RAII helper
// ============================================================================

struct TmpFile {
	std::string path;
	explicit TmpFile(const std::string& suffix = ".xyza") {
		path = std::filesystem::temp_directory_path().string()
			 + "/vsepr_66a_test_" + std::to_string(std::rand()) + suffix;
	}
	~TmpFile() { std::remove(path.c_str()); }
};

// ============================================================================
// Shared: build a minimal water frame with full Q/V/F/E columns
// ============================================================================

static XyzaFrame make_water_frame(int step = 0) {
	XyzaFrame f;
	f.step          = static_cast<uint64_t>(step);
	f.time_fs       = step * 0.5;
	f.temperature_K = 300.0;
	f.energy_eV     = -14.31;  // ~H2O energy ballpark
	f.label         = "H2O";
	f.has_charge    = true;
	f.has_velocity  = true;
	f.has_force     = true;
	f.has_energy_col= true;
	f.charge_model  = ChargeModel::Partial;

	// O atom
	XyzaAtomRecord o;
	o.species           = "O";
	o.position_A        = {0.000,  0.000,  0.000};
	o.charge_e          = -0.834;
	o.velocity_A_per_fs = { 0.003, -0.012,  0.007};
	o.force_eV_per_A    = {-0.041,  0.183, -0.092};
	o.energy_eV         = -8.21;

	// H atoms
	XyzaAtomRecord h1, h2;
	h1.species           = "H";
	h1.position_A        = { 0.757,  0.586,  0.000};
	h1.charge_e          =  0.417;
	h1.velocity_A_per_fs = {-0.015,  0.008, -0.003};
	h1.force_eV_per_A    = { 0.023, -0.094,  0.042};
	h1.energy_eV         = -3.05;

	h2.species           = "H";
	h2.position_A        = {-0.757,  0.586,  0.000};
	h2.charge_e          =  0.417;
	h2.velocity_A_per_fs = { 0.012,  0.004,  0.001};
	h2.force_eV_per_A    = { 0.018,  0.089, -0.051};
	h2.energy_eV         = -3.05;

	f.atoms = {o, h1, h2};
	return f;
}

// ============================================================================
// G48-01  11-column atom-line support
// ============================================================================

TEST(G48_01_eleven_column_round_trip) {
	TmpFile tmp;
	auto frame = make_water_frame(0);

	// Write
	bool ok = write_xyza_dynamic(tmp.path, {frame});
	ASSERT(ok);

	// Read back
	auto res = read_xyza_dynamic(tmp.path, ZeroFillPolicy::Silent);
	ASSERT(res.ok());
	ASSERT(res.frame_count() == 1);

	const auto& rf = res.frames[0];
	ASSERT(rf.atom_count() == 3);

	// Confirm O position round-trips
	ASSERT_NEAR(rf.atoms[0].position_A.x, 0.000, 1e-5);
	ASSERT_NEAR(rf.atoms[0].position_A.y, 0.000, 1e-5);

	// Confirm charge column present and round-trips
	ASSERT(rf.has_charge);
	ASSERT_NEAR(rf.atoms[0].charge_e, -0.834, 1e-4);

	// Confirm velocity column
	ASSERT(rf.has_velocity);
	ASSERT_NEAR(rf.atoms[0].velocity_A_per_fs.x, 0.003, 1e-4);

	// Confirm force column (eV/Å round-trip)
	ASSERT(rf.has_force);
	ASSERT_NEAR(rf.atoms[0].force_eV_per_A.x, -0.041, 5e-4);

	// Confirm per-atom energy column (eV round-trip)
	ASSERT(rf.has_energy_col);
	ASSERT_NEAR(rf.atoms[0].energy_eV, -8.21, 5e-4);
}

// ============================================================================
// G48-02  Missing Q/V/F trailing columns default to zero
// ============================================================================

TEST(G48_02_zero_fill_trailing) {
	// Write a position-only frame using the low-level unified layer,
	// then read back through xyza_dynamic and confirm zeros.

	// Build a raw .xyza-compatible string with no extended columns
	const std::string raw =
		"2\n"
		"MinimalTest step 0\n"
		"O    0.000000      0.000000      0.000000\n"
		"H    0.757000      0.586000      0.000000\n";

	TmpFile tmp;
	{
		std::ofstream out(tmp.path);
		out << raw;
	}

	auto res = read_xyza_dynamic(tmp.path, ZeroFillPolicy::Silent);
	ASSERT(res.ok());
	const auto& rf = res.frames[0];
	ASSERT(rf.atom_count() == 2);

	// No properties= => all flags false => all values zero
	ASSERT(!rf.has_charge);
	ASSERT(!rf.has_velocity);
	ASSERT(!rf.has_force);
	ASSERT(rf.atoms[0].charge_e == 0.0);
	ASSERT(rf.atoms[0].velocity_A_per_fs.x == 0.0);
	ASSERT(rf.atoms[0].force_eV_per_A.x    == 0.0);
}

// ============================================================================
// G48-03  properties= declaration compatibility
// ============================================================================

TEST(G48_03_properties_declaration_roundtrip) {
	// Write a frame with charge+velocity only (no force, no energy col)
	XyzaFrame f;
	f.step         = 5;
	f.has_charge   = true;
	f.has_velocity = true;
	f.has_force    = false;
	f.has_energy_col = false;
	f.label        = "NaCl";

	XyzaAtomRecord na;
	na.species           = "Na";
	na.position_A        = {0.0, 0.0, 0.0};
	na.charge_e          =  1.0;
	na.velocity_A_per_fs = {0.01, 0.0, 0.0};

	XyzaAtomRecord cl;
	cl.species           = "Cl";
	cl.position_A        = {2.36, 0.0, 0.0};
	cl.charge_e          = -1.0;
	cl.velocity_A_per_fs = {-0.01, 0.0, 0.0};

	f.atoms = {na, cl};

	TmpFile tmp;
	ASSERT(write_xyza_dynamic(tmp.path, {f}));

	auto res = read_xyza_dynamic(tmp.path, ZeroFillPolicy::Silent);
	ASSERT(res.ok());
	const auto& rf = res.frames[0];
	ASSERT(rf.has_charge);
	ASSERT(rf.has_velocity);
	ASSERT(!rf.has_force);
	ASSERT(!rf.has_energy_col);
	ASSERT_NEAR(rf.atoms[0].charge_e,          1.0, 1e-6);
	ASSERT_NEAR(rf.atoms[1].charge_e,         -1.0, 1e-6);
	ASSERT_NEAR(rf.atoms[0].velocity_A_per_fs.x, 0.01, 1e-5);
	// force must be zero since not declared
	ASSERT(rf.atoms[0].force_eV_per_A.x == 0.0);
}

// ============================================================================
// G48-04  Charge model passthrough
// ============================================================================

TEST(G48_04_charge_model_passthrough) {
	XyzaFrame f = make_water_frame(0);
	f.charge_model = ChargeModel::Partial;

	TmpFile tmp;
	ASSERT(write_xyza_dynamic(tmp.path, {f}));

	auto res = read_xyza_dynamic(tmp.path, ZeroFillPolicy::Silent);
	ASSERT(res.ok());
	// charge_model= tag must survive round-trip
	ASSERT(res.frames[0].charge_model == ChargeModel::Partial);
}

// ============================================================================
// G48-05  Velocity and force arrays enter XyzaAtomRecord (non-optional)
// ============================================================================

TEST(G48_05_velocity_force_in_record) {
	XyzaFrame f = make_water_frame(0);

	TmpFile tmp;
	ASSERT(write_xyza_dynamic(tmp.path, {f}));

	auto res = read_xyza_dynamic(tmp.path, ZeroFillPolicy::Warn);
	ASSERT(res.ok());
	const auto& a0 = res.frames[0].atoms[0];  // Oxygen

	// Velocity in Å/fs
	ASSERT_NEAR(a0.velocity_A_per_fs.x,  0.003, 1e-4);
	ASSERT_NEAR(a0.velocity_A_per_fs.y, -0.012, 1e-4);
	ASSERT_NEAR(a0.velocity_A_per_fs.z,  0.007, 1e-4);

	// Force in eV/Å (round-trip through kcal/mol conversion)
	ASSERT_NEAR(a0.force_eV_per_A.x, -0.041, 1e-3);
	ASSERT_NEAR(a0.force_eV_per_A.y,  0.183, 1e-3);

	// velocity_magnitude helper
	double vmag = a0.velocity_magnitude_A_per_fs();
	ASSERT(vmag > 0.0);

	// force_magnitude helper
	double fmag = a0.force_magnitude_eV_per_A();
	ASSERT(fmag > 0.0);

	// No diagnostics expected for a fully-specified file
	ASSERT(!res.has_diag());
}

// ============================================================================
// G48-06  Q/V/F summaries from XyzaFrame helpers
// ============================================================================

TEST(G48_06_frame_summary_helpers) {
	auto f = make_water_frame(0);

	// total_charge must be approximately 0 (−0.834 + 0.417 + 0.417 = 0)
	ASSERT_NEAR(f.total_charge_e(), 0.0, 1e-6);

	// max_force must be > 0
	ASSERT(f.max_force_eV_per_A() > 0.0);

	// rms_velocity must be > 0
	ASSERT(f.rms_velocity_A_per_fs() > 0.0);

	// summarise() helper
	auto s = summarise(f, 0);
	ASSERT(s.atom_count == 3);
	ASSERT_NEAR(s.total_charge_e, 0.0, 1e-6);
	ASSERT(s.max_force_eV_per_A > 0.0);
	ASSERT(s.charge_model == ChargeModel::Partial);

	// print_qvf_summary should produce at least one data row
	std::ostringstream oss;
	print_qvf_summary(oss, {s});
	ASSERT(oss.str().find("0.0000") != std::string::npos);
}

// ============================================================================
// G48-07  Multi-frame archive writer (XyzaDynamicWriter)
// ============================================================================

TEST(G48_07_archive_writer_streaming) {
	TmpFile tmp;
	{
		XyzaDynamicWriter w(tmp.path);
		for (int i = 0; i < 4; ++i) {
			auto f = make_water_frame(i);
			f.time_fs = i * 0.5;
			w.append(f);
		}
		ASSERT(w.frames_written() == 4);
		// destructor flushes
	}

	// Read back all 4 frames
	auto res = read_xyza_dynamic(tmp.path, ZeroFillPolicy::Silent);
	ASSERT(res.ok());
	ASSERT(res.frame_count() == 4);

	// Atoms should be consistent across frames
	for (const auto& rf : res.frames) {
		ASSERT(rf.atom_count() == 3);
		ASSERT(rf.has_velocity);
		ASSERT(rf.has_force);
	}
}

// ============================================================================
// G48-08  Bridge: XyzaFrame -> XYZFrame -> XyzaFrame
// ============================================================================

TEST(G48_08_bridge_round_trip) {
	auto orig = make_water_frame(3);

	auto xyz = to_xyz_frame(orig);
	ASSERT(static_cast<int>(xyz.atoms.size()) == 3);
	ASSERT(xyz.has_charge);
	ASSERT(xyz.has_velocity);
	ASSERT(xyz.has_force);

	// O charge should survive (no unit change for charge)
	ASSERT_NEAR(*xyz.atoms[0].q, -0.834, 1e-6);

	// Force in kcal/(mol·Å) on the XYZFrame side
	double fx_kcal = orig.atoms[0].force_eV_per_A.x * eV_A_to_kcal_mol_A;
	ASSERT_NEAR(xyz.atoms[0].f->x, fx_kcal, 1e-4);

	// Bridge back
	auto restored = from_xyz_frame(xyz);
	ASSERT(restored.atom_count() == 3);
	ASSERT_NEAR(restored.atoms[0].charge_e, -0.834, 1e-6);
	ASSERT_NEAR(restored.atoms[0].force_eV_per_A.x, orig.atoms[0].force_eV_per_A.x, 1e-4);
	ASSERT_NEAR(restored.atoms[0].velocity_A_per_fs.y, orig.atoms[0].velocity_A_per_fs.y, 1e-6);
}

// ============================================================================
// G48-09  Formation pre-state: from_xyz_frame smoke test
// ============================================================================

TEST(G48_09_formation_prestate_smoke) {
	// Simulate a formation engine providing an XYZFrame and the bridge
	// converting it to dynamic state for WO-66B consumption.
	XYZFrame src;
	src.N = 2;
	src.has_charge   = true;
	src.has_velocity = true;
	src.has_force    = false;
	src.has_energy_col = false;
	src.frame_index  = 0;
	src.temperature  = 300.0;

	AtomRecord na, cl;
	na.symbol = "Na"; na.Z = 11; na.x = 0.0; na.y = 0.0; na.z = 0.0;
	na.q = 1.0; na.v = vsepr::Vec3{0.02, 0.0, 0.0};
	cl.symbol = "Cl"; cl.Z = 17; cl.x = 2.36; cl.y = 0.0; cl.z = 0.0;
	cl.q = -1.0; cl.v = vsepr::Vec3{-0.02, 0.0, 0.0};
	src.atoms = {na, cl};

	auto dyn = from_xyz_frame(src);
	ASSERT(dyn.atom_count() == 2);
	ASSERT(dyn.has_charge);
	ASSERT(dyn.has_velocity);
	ASSERT(!dyn.has_force);
	ASSERT_NEAR(dyn.temperature_K, 300.0, 1e-6);
	ASSERT_NEAR(dyn.atoms[0].charge_e, 1.0, 1e-6);
	ASSERT_NEAR(dyn.atoms[0].velocity_A_per_fs.x, 0.02, 1e-6);
	ASSERT_NEAR(dyn.atoms[1].charge_e, -1.0, 1e-6);
	// force must be zero (not declared)
	ASSERT(dyn.atoms[0].force_eV_per_A.x == 0.0);
}

// ============================================================================
// Entry point
// ============================================================================

int main() {
	std::cout << "\n=== VSEPR-SIM  WO-66A  .xyza Dynamic State Gate  (Group 48) ===\n\n";
	if (g_failed == 0)
		std::cout << "\nAll " << g_passed << " tests PASSED.\n";
	else
		std::cout << "\n" << g_passed << " passed, " << g_failed << " FAILED.\n";
	return g_failed > 0 ? 1 : 0;
}
