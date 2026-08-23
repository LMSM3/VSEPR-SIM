// =============================================================================
// test_eigenmine_small.cpp  —  WO-XSUITE-02B acceptance tests  EIG-1..6
// =============================================================================
// EIG-1  eigenmine block parses from .X file
// EIG-2  target_solves supports 10,000,000 as metadata
// EIG-3  small run with 1,000 solves succeeds
// EIG-4  batch records are seed-indexed
// EIG-5  eigen candidates are clustered (retained modes non-empty after gate)
// EIG-6  mode recurrence score is exported
// =============================================================================

#include "vsim/xsuite.hpp"
#include "vsim/eigenmine.hpp"
#include "vsim/basis_archive.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string write_temp_x(const std::string& body) {
	fs::path p = fs::temp_directory_path() / "test_eigenmine_tmp.X";
	std::ofstream f(p);
	f << body;
	return p.string();
}

static void check(bool cond, const char* msg) {
	if (!cond) {
		std::cerr << "[FAIL] " << msg << "\n";
		std::exit(1);
	}
	std::cout << "[OK]   " << msg << "\n";
}

// ---------------------------------------------------------------------------
// Minimal short-solve stub: fills SolveVector with deterministic data
// ---------------------------------------------------------------------------

static bool stub_solve(vsepr::presolve::SolveVector& sv) {
	int n = 6;
	sv.x.assign(n, static_cast<double>(sv.seed % 13 + sv.system_idx + 1));
	sv.v.assign(n, 0.1 * (sv.seed % 7 + 1));
	sv.f.assign(n, -0.05 * (sv.system_idx + 1));
	sv.p = {1.0, 0.5, 0.25};
	sv.e = {static_cast<double>(sv.seed % 3)};
	return true;
}

// ---------------------------------------------------------------------------
// EIG-1: eigenmine block parses from .X file
// ---------------------------------------------------------------------------

static void test_EIG1_parse() {
	std::string body = R"(
[xsuite]
name = test_eigenmine
version = 5.2
mode = presolve_suite
created_by = test

[run]
entry = dummy.vsim
mode = presolve
num_steps = 100
dt = 1e-15

[files]
snapshot = dummy.xyz

[eigenmine]
enabled               = true
target_solves         = 10000000
batch_count           = 2
seeds_per_batch       = 5
systems_per_seed      = 10
matrix_source         = state_force_event
mode_rank_limit       = 8
min_recurrence        = 0.50
max_reconstruction_error = 0.10
write_modes           = /tmp/test_modes.ndjson
write_basis           = /tmp/test_basis.bin
write_summary         = /tmp/test_summary.md
)";
	auto path = write_temp_x(body);
	auto res = vsepr::xsuite::xsuite_parse(path);
	check(res.ok, "EIG-1: parse succeeds");
	check(res.suite.eigenmine_enabled, "EIG-1: eigenmine.enabled = true");
	check(res.suite.eigenmine_batch_count == 2, "EIG-1: eigenmine.batch_count = 2");
	check(res.suite.eigenmine_seeds_per_batch == 5, "EIG-1: eigenmine.seeds_per_batch = 5");
	check(res.suite.eigenmine_matrix_source == "state_force_event",
		  "EIG-1: eigenmine.matrix_source");
}

// ---------------------------------------------------------------------------
// EIG-2: target_solves supports 10,000,000 as metadata
// ---------------------------------------------------------------------------

static void test_EIG2_target_solves() {
	std::string body = R"(
[xsuite]
name = test_target_solves
version = 5.2
mode = presolve_suite
created_by = test

[run]
entry = dummy.vsim
mode = presolve
num_steps = 1
dt = 1e-15

[files]
snapshot = dummy.xyz

[eigenmine]
enabled       = true
target_solves = 10000000
batch_count   = 1
seeds_per_batch   = 1
systems_per_seed  = 1
)";
	auto path = write_temp_x(body);
	auto res = vsepr::xsuite::xsuite_parse(path);
	check(res.ok, "EIG-2: parse succeeds");
	check(res.suite.eigenmine_target_solves == 10'000'000LL,
		  "EIG-2: target_solves = 10,000,000");
}

// ---------------------------------------------------------------------------
// EIG-3: small run with 1,000 solves succeeds
// ---------------------------------------------------------------------------

static void test_EIG3_small_run() {
	vsepr::presolve::EigenMineConfig cfg;
	cfg.batch_count        = 2;
	cfg.seeds_per_batch    = 10;
	cfg.systems_per_seed   = 50;  // 2*10*50 = 1000 solves
	cfg.mode_rank_limit    = 4;
	cfg.min_recurrence     = 0.0; // accept all for this test
	cfg.max_recon_error    = 1.0;

	auto res = vsepr::presolve::eigenmine_run(cfg, stub_solve, std::cout);
	check(res.ok, "EIG-3: eigenmine_run returns ok");
	check(res.solves_run == 1000, "EIG-3: solves_run = 1,000");
}

// ---------------------------------------------------------------------------
// EIG-4: batch records are seed-indexed
// ---------------------------------------------------------------------------

static void test_EIG4_seed_indexed() {
	vsepr::presolve::EigenMineConfig cfg;
	cfg.batch_count        = 3;
	cfg.seeds_per_batch    = 2;
	cfg.systems_per_seed   = 1;
	cfg.mode_rank_limit    = 2;
	cfg.min_recurrence     = 0.0;
	cfg.max_recon_error    = 1.0;

	auto res = vsepr::presolve::eigenmine_run(cfg, stub_solve, std::cout);
	check(res.ok, "EIG-4: run ok");
	// Each retained mode must carry a seed value
	for (const auto& m : res.retained_modes)
		check(m.seed >= 0, "EIG-4: mode has valid seed");
	check(!res.retained_modes.empty(), "EIG-4: at least one mode retained");
}

// ---------------------------------------------------------------------------
// EIG-5: eigen candidates are clustered (modes pass gate when thresholds lenient)
// ---------------------------------------------------------------------------

static void test_EIG5_candidates_retained() {
	vsepr::presolve::EigenMineConfig cfg;
	cfg.batch_count        = 1;
	cfg.seeds_per_batch    = 5;
	cfg.systems_per_seed   = 4;
	cfg.mode_rank_limit    = 3;
	cfg.min_recurrence     = 0.0;
	cfg.max_recon_error    = 1.0;

	auto res = vsepr::presolve::eigenmine_run(cfg, stub_solve, std::cout);
	check(res.ok, "EIG-5: run ok");
	check(res.modes_retained > 0, "EIG-5: modes are retained after gate");
}

// ---------------------------------------------------------------------------
// EIG-6: mode recurrence score is exported (present in retained mode records)
// ---------------------------------------------------------------------------

static void test_EIG6_recurrence_exported() {
	vsepr::presolve::EigenMineConfig cfg;
	cfg.batch_count        = 1;
	cfg.seeds_per_batch    = 2;
	cfg.systems_per_seed   = 2;
	cfg.mode_rank_limit    = 2;
	cfg.min_recurrence     = 0.0;
	cfg.max_recon_error    = 1.0;

	// Write to a temp ndjson and read back
	fs::path modes_path = fs::temp_directory_path() / "test_eig6_modes.ndjson";
	cfg.write_modes = modes_path.string();
	if (fs::exists(modes_path)) fs::remove(modes_path);

	auto res = vsepr::presolve::eigenmine_run(cfg, stub_solve, std::cout);
	check(res.ok, "EIG-6: run ok");

	auto modes = vsepr::presolve::basis_archive_read_modes(modes_path.string());
	check(!modes.empty(), "EIG-6: ndjson has modes");
	for (const auto& m : modes) {
		check(m.recurrence >= 0.0 && m.recurrence <= 1.0,
			  "EIG-6: recurrence in [0,1]");
		check(!m.basis_hash.empty(), "EIG-6: basis_hash exported");
	}
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	std::cout << "=== EIG-1..6 tests ===\n";
	test_EIG1_parse();
	test_EIG2_target_solves();
	test_EIG3_small_run();
	test_EIG4_seed_indexed();
	test_EIG5_candidates_retained();
	test_EIG6_recurrence_exported();
	std::cout << "All EIG-1..6 tests passed.\n";
	return 0;
}
