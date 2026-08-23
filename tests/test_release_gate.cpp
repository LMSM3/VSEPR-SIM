// =============================================================================
// test_release_gate.cpp  —  WO-XSUITE-02B acceptance tests  EIG-9..10
// =============================================================================
// EIG-9   release gate compares baseline vs candidate
// EIG-10  final report declares PASS/BLOCKED
// =============================================================================

#include "vsim/xsuite.hpp"
#include "vsim/release_gate.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static void check(bool cond, const char* msg) {
	if (!cond) { std::cerr << "[FAIL] " << msg << "\n"; std::exit(1); }
	std::cout << "[OK]   " << msg << "\n";
}

// ---------------------------------------------------------------------------
// EIG-9a: PASS scenario — candidate meets all gates
// ---------------------------------------------------------------------------

static void test_EIG9_pass() {
	vsepr::presolve::ReleaseGateConfig cfg;
	cfg.baseline_version       = "v5.1.2";
	cfg.candidate_version      = "v5.1.3";
	cfg.require_hash_success   = 0.999;
	cfg.require_curvefit_score = 0.85;
	cfg.require_eigen_quality  = 0.80;
	cfg.max_failure_rate       = 0.01;

	vsepr::presolve::VersionMetrics baseline;
	baseline.hash_success_rate = 0.998;
	baseline.eigen_quality     = 0.78;
	baseline.curvefit_score    = 0.82;
	baseline.failure_rate      = 0.015;
	baseline.runtime_speed_s   = 12.0;
	baseline.energy_stability  = 0.001;

	vsepr::presolve::VersionMetrics candidate;
	candidate.hash_success_rate = 0.9995;
	candidate.eigen_quality     = 0.88;
	candidate.curvefit_score    = 0.91;
	candidate.failure_rate      = 0.005;
	candidate.runtime_speed_s   = 10.0;
	candidate.energy_stability  = 0.0008;

	auto res = vsepr::presolve::release_gate_evaluate(cfg, baseline, candidate, std::cout);
	check(res.passed, "EIG-9: PASS when all gates met");
	check(res.verdict == "PASS", "EIG-9: verdict == PASS");
	check(res.release_score > 0.0, "EIG-9: release_score positive");
}

// ---------------------------------------------------------------------------
// EIG-9b: BLOCKED scenario — hash success too low
// ---------------------------------------------------------------------------

static void test_EIG9_blocked() {
	vsepr::presolve::ReleaseGateConfig cfg;
	cfg.baseline_version       = "v5.1.2";
	cfg.candidate_version      = "v5.1.3-bad";
	cfg.require_hash_success   = 0.999;
	cfg.require_curvefit_score = 0.85;
	cfg.require_eigen_quality  = 0.80;
	cfg.max_failure_rate       = 0.01;

	vsepr::presolve::VersionMetrics baseline;
	baseline.hash_success_rate = 0.998;
	baseline.eigen_quality     = 0.78;
	baseline.curvefit_score    = 0.82;
	baseline.failure_rate      = 0.015;

	vsepr::presolve::VersionMetrics candidate;
	candidate.hash_success_rate = 0.990;  // below threshold
	candidate.eigen_quality     = 0.88;
	candidate.curvefit_score    = 0.91;
	candidate.failure_rate      = 0.005;

	auto res = vsepr::presolve::release_gate_evaluate(cfg, baseline, candidate, std::cout);
	check(!res.passed, "EIG-9b: BLOCKED when hash_success below threshold");
	check(res.verdict == "BLOCKED", "EIG-9b: verdict == BLOCKED");
	check(!res.block_reason.empty(), "EIG-9b: block_reason populated");
}

// ---------------------------------------------------------------------------
// EIG-10: final report declares PASS/BLOCKED in written file
// ---------------------------------------------------------------------------

static void test_EIG10_report() {
	fs::path report_path = fs::temp_directory_path() / "test_eig10_gate.md";
	fs::path cmp_path    = fs::temp_directory_path() / "test_eig10_cmp.ndjson";
	for (const auto& p : {report_path, cmp_path})
		if (fs::exists(p)) fs::remove(p);

	vsepr::presolve::ReleaseGateConfig cfg;
	cfg.baseline_version       = "v5.1.2";
	cfg.candidate_version      = "v5.1.3";
	cfg.require_hash_success   = 0.999;
	cfg.require_curvefit_score = 0.85;
	cfg.require_eigen_quality  = 0.80;
	cfg.max_failure_rate       = 0.01;
	cfg.write_gate_report      = report_path.string();
	cfg.write_comparison       = cmp_path.string();

	vsepr::presolve::VersionMetrics baseline;
	baseline.hash_success_rate = 0.997;
	baseline.eigen_quality     = 0.75;
	baseline.curvefit_score    = 0.80;
	baseline.failure_rate      = 0.02;

	vsepr::presolve::VersionMetrics candidate;
	candidate.hash_success_rate = 0.9992;
	candidate.eigen_quality     = 0.85;
	candidate.curvefit_score    = 0.87;
	candidate.failure_rate      = 0.008;

	auto res = vsepr::presolve::release_gate_evaluate(cfg, baseline, candidate, std::cout);

	check(fs::exists(report_path), "EIG-10: report file written");
	check(fs::exists(cmp_path),    "EIG-10: comparison NDJSON written");

	// Report must contain verdict string
	{
		std::ifstream f(report_path);
		std::string content((std::istreambuf_iterator<char>(f)),
							 std::istreambuf_iterator<char>());
		bool has_verdict = content.find("PASS") != std::string::npos
						|| content.find("BLOCKED") != std::string::npos;
		check(has_verdict, "EIG-10: report declares PASS or BLOCKED");
	}

	// Comparison NDJSON must contain verdict field
	{
		std::ifstream f(cmp_path);
		std::string line;
		std::getline(f, line);
		check(line.find("\"verdict\"") != std::string::npos,
			  "EIG-10: comparison NDJSON has verdict field");
	}
}

// ---------------------------------------------------------------------------
// EIG-9c: release_gate block parses from .X file
// ---------------------------------------------------------------------------

static void test_EIG9c_xparse() {
	std::string body = R"(
[xsuite]
name = test_release_gate
version = 5.2
mode = release_gate
created_by = test

[run]
entry = dummy.vsim
mode = presolve
num_steps = 1
dt = 1e-15

[files]
snapshot = dummy.xyz

[release_gate]
enabled                = true
baseline_version       = v5.1.2
candidate_version      = v5.1.3
require_hash_success   = 0.999
require_curvefit_score = 0.85
require_eigen_quality  = 0.80
max_failure_rate       = 0.01
write_gate_report      = reports/release_gate.md
write_comparison       = archive/release_comparison.ndjson
)";
	auto tmp = fs::temp_directory_path() / "test_release_gate.X";
	{ std::ofstream f(tmp); f << body; }
	auto res = vsepr::xsuite::xsuite_parse(tmp.string());
	check(res.ok, "EIG-9c: parse ok");
	check(res.suite.release_gate_enabled, "EIG-9c: release_gate.enabled");
	check(res.suite.release_gate_baseline  == "v5.1.2", "EIG-9c: baseline_version");
	check(res.suite.release_gate_candidate == "v5.1.3", "EIG-9c: candidate_version");
	check(res.suite.release_gate_require_hash == 0.999,  "EIG-9c: require_hash_success");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	std::cout << "=== EIG-9..10 tests ===\n";
	test_EIG9c_xparse();
	test_EIG9_pass();
	test_EIG9_blocked();
	test_EIG10_report();
	std::cout << "All EIG-9..10 tests passed.\n";
	return 0;
}
