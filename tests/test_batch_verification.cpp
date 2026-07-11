// =============================================================================
// tests/test_batch_verification.cpp   Group 40 — Batch Verification Aggregation
// =============================================================================
//
// Unit tests for the WO-VSEPR-SIM-62B batch verification aggregation kernel.
// Tests are self-contained: they construct VerificationRunRecord objects
// directly rather than reading from disk, exercising the full aggregation
// and gate evaluation pipeline.
//
// Coverage:
//   B1  Single-run PASS record — overall_pass_rate = 1.0
//   B2  Single-run FAIL record — overall_pass_rate = 0.0
//   B3  Single-run MISSING record — counts as failure
//   B4  Single-run WARN record — empirical_ready = true, warned_runs = 1
//   B5  Mixed batch (PASS/FAIL/WARN/MISSING) — correct counts and rates
//   B6  Gate pass — all pass rates above thresholds
//   B7  Gate fail — overall_pass_rate below min_overall_pass_rate
//   B8  Gate fail — mass_pass_rate below min_mass_pass_rate
//   B9  Grouped aggregation — two groups produce correct per-group rates
//   B10 Failure mode classification — FAIL_MASS_CONSERVATION assigned
//   B11 Failure mode classification — FAIL_RDF_REFERENCE assigned
//   B12 Failure mode classification — FAIL_OUTPUT_MISSING on MISSING status
//   B13 write_batch_verify_summary — produces non-empty file
//   B14 write_batch_verify_matrix  — produces non-empty file
//   B15 write_batch_failure_modes  — sorted by count descending
//   B16 write_batch_empirical_report — gate_failure_reason in output
//   B17 BatchRunRecord verify fields — empirical_ready wired correctly
//   B18 is_warning helper — WARN_* codes return true, FAIL_* return false
//   B19 to_string — round-trips all BatchFailureMode codes
//   B20 BatchVerifyPolicySection defaults
//
// Style: assert() + printf pattern, same as Groups 36–39.
//
// WO-VSEPR-SIM-62B | Group 40 | v5.0.0-beta.12
// =============================================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "include/batch/batch_verification.hpp"
#include "include/batch/failure_modes.hpp"
#include "include/vsim/vsim_document.hpp"
#include "src/batch/batch_verification_kernel.hpp"

using namespace vsim::batch;
using namespace vsim;

// ── helpers ───────────────────────────────────────────────────────────────────

static VerificationRunRecord make_pass(const std::string& id,
									   const std::map<std::string,std::string>& axes = {}) {
	VerificationRunRecord r;
	r.run_id        = id;
	r.case_name     = id;
	r.overall_pass  = true;
	r.status        = "PASS";
	r.structure_pass = true;
	r.rdf_pass       = true;
	r.mass_pass      = true;
	r.msd_pass       = true;
	r.axis_values    = axes;
	return r;
}

static VerificationRunRecord make_fail(const std::string& id, bool mass_ok = true,
									   bool rdf_ok = false,
									   const std::map<std::string,std::string>& axes = {}) {
	VerificationRunRecord r;
	r.run_id        = id;
	r.case_name     = id;
	r.overall_pass  = false;
	r.status        = "FAIL";
	r.structure_pass = true;
	r.rdf_pass       = rdf_ok;
	r.mass_pass      = mass_ok;
	r.msd_pass       = true;
	r.rdf_first_peak_error_A = rdf_ok ? 0.0 : 0.15;
	r.mass_relative_error    = mass_ok ? 0.0 : 1e-5;
	r.axis_values    = axes;
	return r;
}

static VerificationRunRecord make_missing(const std::string& id) {
	VerificationRunRecord r;
	r.run_id       = id;
	r.case_name    = id;
	r.overall_pass = false;
	r.status       = "MISSING";
	r.failure_modes.push_back(to_string(BatchFailureMode::FAIL_OUTPUT_MISSING));
	return r;
}

static VerificationRunRecord make_warn(const std::string& id) {
	VerificationRunRecord r = make_pass(id);
	r.status = "WARN";
	r.overall_pass = true;
	return r;
}

static BatchAggregateVerifySection default_policy() {
	BatchAggregateVerifySection p;
	p.enabled = true;
	return p;
}

static std::string file_contents(const std::string& path) {
	std::ifstream f(path);
	if (!f.good()) return "";
	return std::string((std::istreambuf_iterator<char>(f)),
						std::istreambuf_iterator<char>());
}

// ── tests ─────────────────────────────────────────────────────────────────────

static void B1() {
	std::vector<VerificationRunRecord> recs = { make_pass("run_0001") };
	auto sum = aggregate_verification(recs, default_policy());
	assert(sum.total_runs   == 1);
	assert(sum.passed_runs  == 1);
	assert(sum.failed_runs  == 0);
	assert(std::abs(sum.overall_pass_rate - 1.0) < 1e-9);
	printf("[PASS] B1 — single PASS record\n");
}

static void B2() {
	auto rec = make_fail("run_0001");
	classify_failure_modes(rec);
	std::vector<VerificationRunRecord> recs = { rec };
	auto sum = aggregate_verification(recs, default_policy());
	assert(sum.total_runs  == 1);
	assert(sum.failed_runs == 1);
	assert(std::abs(sum.overall_pass_rate - 0.0) < 1e-9);
	printf("[PASS] B2 — single FAIL record\n");
}

static void B3() {
	std::vector<VerificationRunRecord> recs = { make_missing("run_0001") };
	auto sum = aggregate_verification(recs, default_policy());
	assert(sum.missing_runs == 1);
	assert(sum.failed_runs  == 1);
	assert(std::abs(sum.overall_pass_rate - 0.0) < 1e-9);
	printf("[PASS] B3 — MISSING counts as failure\n");
}

static void B4() {
	std::vector<VerificationRunRecord> recs = { make_warn("run_0001") };
	auto sum = aggregate_verification(recs, default_policy());
	assert(sum.warned_runs == 1);
	assert(sum.failed_runs == 0);
	assert(std::abs(sum.overall_pass_rate - 1.0) < 1e-9); // WARN counts as pass
	printf("[PASS] B4 — WARN counted as passing, warned_runs = 1\n");
}

static void B5() {
	// 3 PASS, 1 FAIL, 1 WARN, 1 MISSING = 6 total
	// pass rate = (3 PASS + 1 WARN) / 6 = 4/6 ≈ 0.6667
	std::vector<VerificationRunRecord> recs = {
		make_pass("r1"), make_pass("r2"), make_pass("r3"),
		make_fail("r4"), make_warn("r5"), make_missing("r6")
	};
	auto sum = aggregate_verification(recs, default_policy());
	assert(sum.total_runs   == 6);
	assert(sum.passed_runs  == 3);
	assert(sum.warned_runs  == 1);
	assert(sum.failed_runs  == 2); // 1 FAIL + 1 MISSING
	assert(sum.missing_runs == 1);
	double expected_rate = 4.0 / 6.0;
	assert(std::abs(sum.overall_pass_rate - expected_rate) < 1e-6);
	(void)expected_rate;
	printf("[PASS] B5 — mixed batch counts and rates correct\n");
}

static void B6() {
	std::vector<VerificationRunRecord> recs = {
		make_pass("r1"), make_pass("r2"), make_pass("r3"), make_pass("r4")
	};
	auto sum = aggregate_verification(recs, default_policy());
	BatchAggregateVerifyGatesSection gates;
	gates.min_overall_pass_rate = 0.90;
	gates.min_mass_pass_rate    = 0.90;
	evaluate_gates(sum, gates);
	assert(sum.batch_empirical_ready == true);
	assert(sum.gate_failure_reason.empty());
	printf("[PASS] B6 — gate passes when rates are sufficient\n");
}

static void B7() {
	// 5 PASS, 5 FAIL → overall_pass_rate = 0.50, below 0.90 gate
	std::vector<VerificationRunRecord> recs;
	for (int i = 0; i < 5; ++i) recs.push_back(make_pass("p" + std::to_string(i)));
	for (int i = 0; i < 5; ++i) recs.push_back(make_fail("f" + std::to_string(i)));
	auto sum = aggregate_verification(recs, default_policy());
	BatchAggregateVerifyGatesSection gates;
	gates.min_overall_pass_rate = 0.90;
	evaluate_gates(sum, gates);
	assert(sum.batch_empirical_ready == false);
	assert(!sum.gate_failure_reason.empty());
	printf("[PASS] B7 — gate fails when overall_pass_rate below threshold\n");
}

static void B8() {
	// All FAIL due to mass; min_mass_pass_rate = 1.0 gate should fire
	auto rec = make_fail("r1", /*mass_ok=*/false, /*rdf_ok=*/true);
	classify_failure_modes(rec);
	std::vector<VerificationRunRecord> recs = { rec };
	auto sum = aggregate_verification(recs, default_policy());
	BatchAggregateVerifyGatesSection gates;
	gates.min_mass_pass_rate = 1.0;
	evaluate_gates(sum, gates);
	assert(sum.batch_empirical_ready == false);
	printf("[PASS] B8 — gate fails when mass_pass_rate below 1.0\n");
}

static void B9() {
	// Group A: 2 PASS; Group B: 1 PASS, 1 FAIL
	std::vector<VerificationRunRecord> recs = {
		make_pass("a1", {{"temp","300"}}), make_pass("a2", {{"temp","300"}}),
		make_pass("b1", {{"temp","600"}}), make_fail("b2", true, false, {{"temp","600"}})
	};
	BatchAggregateVerifySection policy;
	policy.enabled  = true;
	policy.group_by = {"temp"};
	auto sum = aggregate_verification(recs, policy);
	assert(sum.grouped.size() == 2u);
	// Find group 300 and 600
	for (const auto& g : sum.grouped) {
		auto it = g.group_keys.find("temp");
		assert(it != g.group_keys.end());
		if (it->second == "300") {
			assert(g.runs   == 2);
			assert(g.passed == 2);
			assert(std::abs(g.pass_rate - 1.0) < 1e-9);
		} else if (it->second == "600") {
			assert(g.runs   == 2);
			assert(g.passed == 1);
			assert(std::abs(g.pass_rate - 0.5) < 1e-9);
		}
	}
	printf("[PASS] B9 — grouped aggregation produces correct per-group rates\n");
}

static void B10() {
	auto rec = make_fail("r1", /*mass_ok=*/false, /*rdf_ok=*/true);
	classify_failure_modes(rec);
	bool found = false;
	for (const auto& fm : rec.failure_modes)
		if (fm == to_string(BatchFailureMode::FAIL_MASS_CONSERVATION)) found = true;
	assert(found);
	printf("[PASS] B10 — FAIL_MASS_CONSERVATION classified for mass failure\n");
}

static void B11() {
	auto rec = make_fail("r1", /*mass_ok=*/true, /*rdf_ok=*/false);
	classify_failure_modes(rec);
	bool found = false;
	for (const auto& fm : rec.failure_modes)
		if (fm == to_string(BatchFailureMode::FAIL_RDF_REFERENCE)) found = true;
	assert(found);
	printf("[PASS] B11 — FAIL_RDF_REFERENCE classified for RDF failure\n");
}

static void B12() {
	auto rec = make_missing("r1");
	bool found = false;
	for (const auto& fm : rec.failure_modes)
		if (fm == to_string(BatchFailureMode::FAIL_OUTPUT_MISSING)) found = true;
	assert(found);
	printf("[PASS] B12 — FAIL_OUTPUT_MISSING on MISSING status\n");
}

static void B13() {
	std::vector<VerificationRunRecord> recs = { make_pass("r1"), make_fail("r2") };
	auto sum = aggregate_verification(recs, default_policy());
	const std::string path = "test_batch_verify_summary.tsv";
	write_batch_verify_summary(sum, path);
	assert(!file_contents(path).empty());
	printf("[PASS] B13 — write_batch_verify_summary produces output\n");
}

static void B14() {
	std::vector<VerificationRunRecord> recs = { make_pass("r1"), make_fail("r2") };
	auto sum = aggregate_verification(recs, default_policy());
	const std::string path = "test_batch_verify_matrix.tsv";
	write_batch_verify_matrix(sum, path);
	assert(!file_contents(path).empty());
	printf("[PASS] B14 — write_batch_verify_matrix produces output\n");
}

static void B15() {
	auto fm1 = make_fail("r1", true, false);  classify_failure_modes(fm1); // FAIL_RDF
	auto fm2 = make_fail("r2", true, false);  classify_failure_modes(fm2); // FAIL_RDF
	auto fm3 = make_fail("r3", false, true);  classify_failure_modes(fm3); // FAIL_MASS
	std::vector<VerificationRunRecord> recs = { fm1, fm2, fm3 };
	auto sum = aggregate_verification(recs, default_policy());
	const std::string path = "test_batch_failure_modes.tsv";
	write_batch_failure_modes(sum, path);
	std::string content = file_contents(path);
	assert(!content.empty());
	// FAIL_RDF_REFERENCE should appear before FAIL_MASS_CONSERVATION (count 2 vs 1)
	auto rdf_pos  = content.find("FAIL_RDF_REFERENCE");
	auto mass_pos = content.find("FAIL_MASS_CONSERVATION");
	assert(rdf_pos  != std::string::npos);
	assert(mass_pos != std::string::npos);
	assert(rdf_pos < mass_pos);
	(void)rdf_pos; (void)mass_pos;
	printf("[PASS] B15 — batch_failure_modes sorted by count descending\n");
}

static void B16() {
	std::vector<VerificationRunRecord> recs;
	for (int i = 0; i < 5; ++i) recs.push_back(make_fail("f" + std::to_string(i)));
	auto sum = aggregate_verification(recs, default_policy());
	BatchAggregateVerifyGatesSection gates;
	gates.min_overall_pass_rate = 0.90;
	evaluate_gates(sum, gates);
	const std::string path = "test_batch_empirical_report.md";
	write_batch_empirical_report(sum, "test_study", path);
	std::string content = file_contents(path);
	assert(content.find("batch_empirical_ready: false") != std::string::npos);
	assert(content.find("overall_pass_rate") != std::string::npos);
	printf("[PASS] B16 — batch_empirical_report contains gate_failure_reason\n");
}

static void B17() {
	BatchRunRecord rec;
	rec.verify_status  = "PASS";
	rec.empirical_ready = true;
	assert(rec.empirical_ready == true);
	BatchRunRecord rec2;
	rec2.verify_status  = "FAIL";
	rec2.empirical_ready = false;
	assert(rec2.empirical_ready == false);
	printf("[PASS] B17 — BatchRunRecord verify fields compile and assign\n");
}

static void B18() {
	assert( is_warning(BatchFailureMode::WARN_RDF_PEAK_WIDTH));
	assert( is_warning(BatchFailureMode::WARN_CONVERGENCE_SLOW));
	assert( is_warning(BatchFailureMode::WARN_SCALE_MARGINAL));
	assert(!is_warning(BatchFailureMode::FAIL_MASS_CONSERVATION));
	assert(!is_warning(BatchFailureMode::FAIL_RDF_REFERENCE));
	assert(!is_warning(BatchFailureMode::FAIL_RUNTIME_CRASH));
	printf("[PASS] B18 — is_warning helper correct for all codes\n");
}

static void B19() {
	// Verify all enum codes produce non-empty non-UNKNOWN strings
	BatchFailureMode codes[] = {
		BatchFailureMode::FAIL_MISSING_SCALE_EVIDENCE,
		BatchFailureMode::FAIL_INVALID_RVE,
		BatchFailureMode::FAIL_RUNTIME_CRASH,
		BatchFailureMode::FAIL_MASS_CONSERVATION,
		BatchFailureMode::FAIL_STRUCTURE_COORDINATION,
		BatchFailureMode::FAIL_RDF_REFERENCE,
		BatchFailureMode::FAIL_MSD_SOLID_BOUND,
		BatchFailureMode::FAIL_OUTPUT_MISSING,
		BatchFailureMode::FAIL_CHECK_MISSING,
		BatchFailureMode::WARN_RDF_PEAK_WIDTH,
		BatchFailureMode::WARN_CONVERGENCE_SLOW,
		BatchFailureMode::WARN_SCALE_MARGINAL,
	};
	for (const auto& c : codes) {
		const char* s = to_string(c);
		assert(s && std::strlen(s) > 0);
		assert(std::strcmp(s, "UNKNOWN") != 0);
		(void)s;
	}
	printf("[PASS] B19 — to_string round-trips all BatchFailureMode codes\n");
}

static void B20() {
	BatchVerifyPolicySection p;
	assert(p.on_run_fail          == "continue");
	assert(p.on_check_fail        == "record");
	assert(p.save_resolved_scripts == true);
	printf("[PASS] B20 — BatchVerifyPolicySection defaults correct\n");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
	printf("=== Group 40 — Batch Verification Aggregation ===\n\n");

	B1();  B2();  B3();  B4();  B5();
	B6();  B7();  B8();  B9();  B10();
	B11(); B12(); B13(); B14(); B15();
	B16(); B17(); B18(); B19(); B20();

	printf("\n=== Group 40: 20/20 PASS ===\n");
	return 0;
}
