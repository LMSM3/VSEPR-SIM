/**
 * test_bridge65_atom_events.cpp
 * =============================
 * Unit tests for WO-BRIDGE-65 per-atom event records and logger routing.
 *
 * Tests:
 *   1. test_atom_event_type_names  — enum → string round-trip
 *   2. test_energy_fields          — AtomEventEnergy arithmetic
 *   3. test_force_magnitude        — AtomEventForce construction
 *   4. test_logger_routes_collision — collision accepted when flag enabled
 *   5. test_logger_rejects_bond_when_disabled — bond rejected when flag off
 *   6. test_logger_jsonl_written   — JSONL file created and non-empty
 *   7. test_logger_tsv_header      — TSV header written on first end_step
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "vsim/bridge65/atom_event.hpp"
#include "vsim/bridge65/atom_event_logger.hpp"

namespace fs = std::filesystem;
using namespace vsim::bridge65;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static AtomEvent make_collision_event(int id = 42) {
	AtomEvent ev;
	ev.step                   = 100;
	ev.time_s                 = 1.0e-13;
	ev.atom_id                = id;
	ev.symbol                 = "Fe";
	ev.type                   = AtomEventType::Collision;
	ev.partner_ids            = {77};
	ev.sym_partner            = "Ni";
	ev.energy.kinetic_eV      = 0.021;
	ev.energy.potential_local_eV = -0.48;
	ev.energy.total_local_eV  = -0.459;
	ev.energy.delta_eV        = 0.008;
	ev.force.fx_eV_A          = 3.1;
	ev.force.fy_eV_A          = -8.2;
	ev.force.fz_eV_A          = 16.7;
	ev.force.magnitude_eV_A   = 19.2;
	ev.partner_distance_A     = 2.18;
	ev.cutoff_A               = 2.35;
	ev.confidence             = 0.993;
	ev.reason                 = "distance_energy_gate";
	ev.state_hash             = "aabbccdd";
	ev.event_hash             = "11223344";
	return ev;
}

// ---------------------------------------------------------------------------
// Test 1: enum → string
// ---------------------------------------------------------------------------

static void test_atom_event_type_names() {
	assert(std::string("energy_update")  == atom_event_type_name(AtomEventType::EnergyUpdate));
	assert(std::string("collision")      == atom_event_type_name(AtomEventType::Collision));
	assert(std::string("bond_created")   == atom_event_type_name(AtomEventType::BondCreated));
	assert(std::string("bond_broken")    == atom_event_type_name(AtomEventType::BondBroken));
	assert(std::string("bond_stretched") == atom_event_type_name(AtomEventType::BondStretched));
	assert(std::string("force_spike")    == atom_event_type_name(AtomEventType::ForceSpike));
	assert(std::string("thermal_spike")  == atom_event_type_name(AtomEventType::ThermalSpike));
	assert(std::string("energy_anomaly") == atom_event_type_name(AtomEventType::EnergyAnomaly));
	std::puts("[PASS] test_atom_event_type_names");
}

// ---------------------------------------------------------------------------
// Test 2: energy fields
// ---------------------------------------------------------------------------

static void test_energy_fields() {
	AtomEventEnergy e;
	e.kinetic_eV         = 0.05;
	e.potential_local_eV = -1.20;
	e.total_local_eV     = e.kinetic_eV + e.potential_local_eV;
	e.delta_eV           = 0.003;

	assert(e.total_local_eV < 0.0);
	assert(e.delta_eV > 0.0);
	std::puts("[PASS] test_energy_fields");
}

// ---------------------------------------------------------------------------
// Test 3: force magnitude
// ---------------------------------------------------------------------------

static void test_force_magnitude() {
	AtomEventForce f;
	f.fx_eV_A = 3.0;
	f.fy_eV_A = 4.0;
	f.fz_eV_A = 0.0;
	f.magnitude_eV_A = 5.0;

	// Pythagorean check: 3² + 4² + 0² = 25 → |F| = 5
	double computed = f.fx_eV_A * f.fx_eV_A + f.fy_eV_A * f.fy_eV_A +
					  f.fz_eV_A * f.fz_eV_A;
	assert(computed == 25.0);
	std::puts("[PASS] test_force_magnitude");
}

// ---------------------------------------------------------------------------
// Test 4: logger accepts collision when flag enabled
// ---------------------------------------------------------------------------

static void test_logger_routes_collision() {
	fs::path jtmp = fs::temp_directory_path() / "test_b65_collision2.jsonl";
	fs::path ttmp = fs::temp_directory_path() / "test_b65_collision2.tsv";

	AtomEventLoggerConfig cfg;
	cfg.console          = false;
	cfg.jsonl            = true;
	cfg.jsonl_path       = jtmp.string();
	cfg.tsv_path         = ttmp.string();
	cfg.collision_events = true;
	cfg.run_id           = "test_run";

	{
		AtomEventLogger logger(cfg);
		logger.begin_step(100, 1.0e-13);
		logger.log(make_collision_event());
		logger.end_step(100, 1.0e-13);
	} // destructor flushes

	// Read JSONL and verify
	{
		std::ifstream f(jtmp);
		std::string line;
		std::getline(f, line);
		assert(!line.empty());
		assert(line.find("collision") != std::string::npos);
	}

	std::error_code ec;
	fs::remove(jtmp, ec);
	fs::remove(ttmp, ec);
	std::puts("[PASS] test_logger_routes_collision");
}

// ---------------------------------------------------------------------------
// Test 5: logger rejects bond when flag disabled
// ---------------------------------------------------------------------------

static void test_logger_rejects_bond_when_disabled() {
	fs::path jtmp = fs::temp_directory_path() / "test_b65_no_bond.jsonl";
	fs::path ttmp = fs::temp_directory_path() / "test_b65_no_bond.tsv";

	AtomEventLoggerConfig cfg;
	cfg.console      = false;
	cfg.jsonl        = true;
	cfg.jsonl_path   = jtmp.string();
	cfg.tsv_path     = ttmp.string();
	cfg.bond_events  = false; // bonds disabled

	AtomEvent ev = make_collision_event();
	ev.type = AtomEventType::BondCreated;

	{
		AtomEventLogger logger(cfg);
		logger.begin_step(1, 0.0);
		logger.log(ev);
		logger.end_step(1, 0.0);
	}

	// JSONL should be empty (no accepted events)
	{
		std::ifstream f(jtmp);
		std::string line;
		bool has_line = static_cast<bool>(std::getline(f, line));
		assert(!has_line || line.find("bond_created") == std::string::npos);
	}

	std::error_code ec;
	fs::remove(jtmp, ec);
	fs::remove(ttmp, ec);
	std::puts("[PASS] test_logger_rejects_bond_when_disabled");
}

// ---------------------------------------------------------------------------
// Test 6: JSONL file created and non-empty
// ---------------------------------------------------------------------------

static void test_logger_jsonl_written() {
	fs::path jtmp = fs::temp_directory_path() / "test_b65_jsonl2.jsonl";
	fs::path ttmp = fs::temp_directory_path() / "test_b65_jsonl2.tsv";

	AtomEventLoggerConfig cfg;
	cfg.console    = false;
	cfg.jsonl      = true;
	cfg.jsonl_path = jtmp.string();
	cfg.tsv_path   = ttmp.string();
	cfg.run_id     = "test_jsonl";

	{
		AtomEventLogger logger(cfg);
		for (int i = 0; i < 3; ++i) {
			logger.begin_step(static_cast<uint64_t>(i), i * 1.0e-15);
			logger.log(make_collision_event(i));
			logger.end_step(static_cast<uint64_t>(i), i * 1.0e-15);
		}
	}

	assert(fs::exists(jtmp));
	assert(fs::file_size(jtmp) > 0);

	std::error_code ec;
	fs::remove(jtmp, ec);
	fs::remove(ttmp, ec);
	std::puts("[PASS] test_logger_jsonl_written");
}

// ---------------------------------------------------------------------------
// Test 7: TSV header present
// ---------------------------------------------------------------------------

static void test_logger_tsv_header() {
	fs::path jtmp = fs::temp_directory_path() / "test_b65_hdr.jsonl";
	fs::path ttmp = fs::temp_directory_path() / "test_b65_hdr.tsv";

	AtomEventLoggerConfig cfg;
	cfg.console    = false;
	cfg.jsonl      = true;
	cfg.jsonl_path = jtmp.string();
	cfg.tsv_path   = ttmp.string();

	{
		AtomEventLogger logger(cfg);
		logger.begin_step(0, 0.0);
		logger.log(make_collision_event());
		logger.end_step(0, 0.0);
	}

	assert(fs::exists(ttmp));
	std::ifstream f(ttmp);
	std::string header;
	std::getline(f, header);
	assert(header.find("step") != std::string::npos);
	assert(header.find("collisions") != std::string::npos);
	assert(header.find("limiter_active") != std::string::npos);

	std::error_code ec;
	fs::remove(jtmp, ec);
	fs::remove(ttmp, ec);
	std::puts("[PASS] test_logger_tsv_header");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	test_atom_event_type_names();
	test_energy_fields();
	test_force_magnitude();
	test_logger_routes_collision();
	test_logger_rejects_bond_when_disabled();
	test_logger_jsonl_written();
	test_logger_tsv_header();

	std::puts("\n[BRIDGE65] test_bridge65_atom_events: ALL PASS");
	return 0;
}
