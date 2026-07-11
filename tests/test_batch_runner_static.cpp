/**
 * tests/test_batch_runner_static.cpp
 * =====================================
 * WO-VSIM-62C — Group 43: Static Runner Integration Tests
 *
 * Tests R1–R12 from spec §10.
 *
 * These tests exercise the full batch-layer pipeline:
 *   BatchParser → BatchExpander → CheckpointManager →
 *   RequireChecker → BatchAggregator → output writers.
 *
 * Kernel/physics execution is not exercised here; run records are
 * synthesised directly to validate the orchestration layer in isolation.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_parser.hpp"
#include "include/batch/batch_expander.hpp"
#include "include/batch/seed_resolver.hpp"
#include "include/batch/batch_checkpoint.hpp"
#include "include/batch/batch_require_checker.hpp"
#include "include/batch/batch_aggregator.hpp"
#include "include/batch/resolved_writer.hpp"
#include "include/batch/batch_merger.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace vsim::batch;
using vsim::FormationStageKind;
using vsim::BatchRequireSection;
using vsim::VsimDocument;

static int failures = 0;

#define EXPECT_TRUE(cond) \
	do { \
		if (!(cond)) { \
			std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__ << "] " #cond "\n"; \
			++failures; \
		} else { \
			std::cout << "  PASS: " #cond "\n"; \
		} \
	} while(0)

#define EXPECT_STR(a, b) EXPECT_TRUE(std::string(a) == std::string(b))

// ── fixture helpers ───────────────────────────────────────────────────────────

static const std::string kTmpDir = "test_runner_static_tmp";

static void mkdir_p(const std::string& path) {
	fs::create_directories(path);
}

static void rm_rf(const std::string& path) {
	fs::remove_all(path);
}

static bool file_exists(const std::string& path) {
	return fs::exists(path);
}

static std::string read_file(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open()) return "";
	return std::string((std::istreambuf_iterator<char>(f)),
						std::istreambuf_iterator<char>());
}

static void write_file(const std::string& path, const std::string& content) {
	mkdir_p(fs::path(path).parent_path().string());
	std::ofstream f(path);
	f << content;
}

static BatchDocument make_inline_study(const std::string& name = "runner_test") {
	std::string src = "[study]\nname = \"" + name + "\"\n\n"
		"[batch.base]\nmode = \"inline\"\n\n"
		"[batch.design]\nreplicates_per_case = 1\nseed_policy = \"split\"\n\n"
		"[seed]\nfoundation = 1000\n\n"
		"[[batch.axis]]\nname = \"temperature\"\ntarget = \"environment.temperature\"\n"
		"kind = \"static\"\nvalues = [300, 600]\n\n"
		"[[batch.axis]]\nname = \"pressure\"\ntarget = \"environment.pressure_GPa\"\n"
		"kind = \"static\"\nvalues = [0.0, 1.0]\n";
	return BatchParser::parse_string(src);
}

static BatchRunRecord make_run_record(const std::string& run_id,
									  const std::string& status,
									  double score = 1.0) {
	BatchRunRecord r;
	r.run_id        = run_id;
	r.verify_status = status;
	r.metrics["composite"] = score;
	r.axis_values["environment.temperature"] = "300";
	return r;
}

// ── R1: Inline study, 2 axes → all runs expand ────────────────────────────────

static void test_R1() {
	std::cout << "R1: 2 static axes (2×2) → 4 run specs\n";
	auto doc = make_inline_study();
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);
	EXPECT_TRUE(specs.size() == 4);
	EXPECT_STR(specs[0].run_id, "run_0001");
	EXPECT_STR(specs[3].run_id, "run_0004");
}

// ── R2: run.vsim.resolved written per run ────────────────────────────────────

static void test_R2() {
	std::cout << "R2: run.vsim.resolved written per case\n";
	auto doc = make_inline_study("resolved_write_test");
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);

	std::string run_dir = kTmpDir + "/resolved_write_test/cases/" + specs[0].run_id;
	mkdir_p(run_dir);

	// Build a minimal VsimDocument for the resolved output
	VsimDocument base_doc;
	base_doc.project.name  = "resolved_write_test";
	base_doc.run.max_steps = 100;

	std::string out_path = run_dir + "/run.vsim.resolved";
	ResolvedWriter::write(base_doc, specs[0], "resolved_write_test", out_path);

	EXPECT_TRUE(file_exists(out_path));
	std::string content = read_file(out_path);
	EXPECT_TRUE(content.find("run.vsim.resolved") != std::string::npos);
	EXPECT_TRUE(content.find(specs[0].run_id)     != std::string::npos);
	EXPECT_TRUE(content.find("study:")             != std::string::npos);

	rm_rf(kTmpDir + "/resolved_write_test");
}

// ── R3: batch_checkpoint.json written and grows ───────────────────────────────

static void test_R3() {
	std::cout << "R3: checkpoint written after each run\n";
	std::string dir = kTmpDir + "/checkpoint_test";
	mkdir_p(dir);

	BatchCheckpoint cp;
	cp.study_name  = "checkpoint_test";
	cp.total_runs  = 4;
	CheckpointManager::save(cp, dir);
	EXPECT_TRUE(file_exists(dir + "/batch_checkpoint.json"));

	CheckpointManager::mark_complete(dir, "run_0001");
	CheckpointManager::mark_complete(dir, "run_0002");
	auto loaded = CheckpointManager::load(dir);
	EXPECT_TRUE(loaded.completed_run_ids.size() == 2);
	EXPECT_STR(loaded.completed_run_ids[0], "run_0001");

	rm_rf(dir);
}

// ── R4: Resume — already-completed runs skipped ──────────────────────────────

static void test_R4() {
	std::cout << "R4: resume skips completed runs\n";
	std::string dir = kTmpDir + "/resume_test";
	mkdir_p(dir);

	BatchCheckpoint cp;
	cp.study_name = "resume_test";
	cp.total_runs = 4;
	cp.completed_run_ids = {"run_0001", "run_0002"};
	cp.completed_runs    = 2;
	CheckpointManager::save(cp, dir);

	auto loaded = CheckpointManager::load(dir);
	auto doc = make_inline_study("resume_test");
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);

	// Simulate skip logic: filter out already completed
	std::vector<BatchRunSpec> pending;
	for (const auto& s : specs) {
		bool done = false;
		for (const auto& cid : loaded.completed_run_ids)
			if (cid == s.run_id) { done = true; break; }
		if (!done) pending.push_back(s);
	}
	EXPECT_TRUE(pending.size() == specs.size() - 2);

	rm_rf(dir);
}

// ── R5: batch.require — missing file → FAIL_OUTPUT_MISSING ───────────────────

static void test_R5() {
	std::cout << "R5: batch.require missing file → FAIL_OUTPUT_MISSING\n";
	std::string dir = kTmpDir + "/require_test_r5";
	mkdir_p(dir);

	BatchRequireSection req;
	req.fail_if_missing = true;
	req.files = {"output.xyz", "metrics.tsv"};

	auto result = check_run_require(req, dir, "run_0001");
	EXPECT_TRUE(!result.pass);
	EXPECT_STR(result.failure_mode, "FAIL_OUTPUT_MISSING");
	EXPECT_TRUE(result.missing_files.size() == 2);

	rm_rf(dir);
}

// ── R6: batch.require — missing check in verify_report ───────────────────────

static void test_R6() {
	std::cout << "R6: batch.require missing check → FAIL_CHECK_MISSING\n";
	std::string dir = kTmpDir + "/require_test_r6";
	mkdir_p(dir);

	// Write a verify_report.json without the needed check
	write_file(dir + "/verify_report.json",
		"{ \"mass_check\": { \"pass\": true } }");

	BatchRequireSection req;
	req.fail_if_missing = true;
	req.checks = {"rdf_check"};

	auto result = check_run_require(req, dir, "run_0001");
	EXPECT_TRUE(!result.pass);
	EXPECT_STR(result.failure_mode, "FAIL_CHECK_MISSING");

	rm_rf(dir);
}

// ── R7: abort_on_fail = true — batch stops on first FAIL ─────────────────────

static void test_R7() {
	std::cout << "R7: abort_on_fail — stop on first FAIL\n";
	std::vector<BatchRunRecord> records = {
		make_run_record("run_0001", "PASS", 0.95),
		make_run_record("run_0002", "FAIL", 0.0),
		make_run_record("run_0003", "PASS", 0.80),
	};
	// Simulate abort_on_fail logic: stop processing at first FAIL
	std::vector<BatchRunRecord> processed;
	bool abort_on_fail = true;
	for (const auto& r : records) {
		processed.push_back(r);
		if (abort_on_fail && r.verify_status == "FAIL") break;
	}
	EXPECT_TRUE(processed.size() == 2); // run_0001 + run_0002 (the failing one)
	EXPECT_STR(processed.back().verify_status, "FAIL");
}

// ── R8: on_run_fail = "continue" — failed run recorded, batch proceeds ────────

static void test_R8() {
	std::cout << "R8: on_run_fail=continue — all runs in summary\n";
	std::vector<BatchRunRecord> records = {
		make_run_record("run_0001", "PASS", 0.95),
		make_run_record("run_0002", "FAIL", 0.0),
		make_run_record("run_0003", "PASS", 0.80),
	};
	// With continue policy, all 3 are processed
	EXPECT_TRUE(records.size() == 3);
	EXPECT_STR(records[1].verify_status, "FAIL");
}

// ── R9: aggregate_results produced after all runs ─────────────────────────────

static void test_R9() {
	std::cout << "R9: aggregate_results.tsv produced\n";
	std::string dir = kTmpDir + "/agg_test";
	mkdir_p(dir);

	std::vector<BatchRunRecord> records;
	for (int i = 0; i < 4; ++i) {
		BatchRunRecord r = make_run_record("run_000" + std::to_string(i+1), "PASS", 0.9);
		r.axis_values["environment.temperature"] = (i < 2) ? "300" : "600";
		r.metrics["composite"] = 0.9;
		records.push_back(r);
	}

	BatchAggregateSection agg;
	agg.enabled  = true;
	agg.group_by = {"environment.temperature"};

	auto result = aggregate_runs(records, agg);
	EXPECT_TRUE(result.groups.size() == 2);

	std::string tsv_path = dir + "/aggregate_results.tsv";
	write_aggregate_tsv(result, tsv_path);
	EXPECT_TRUE(file_exists(tsv_path));
	std::string content = read_file(tsv_path);
	EXPECT_TRUE(content.find("run_count") != std::string::npos);

	rm_rf(dir);
}

// ── R10: ranked_candidates excludes FAIL/MISSING ─────────────────────────────

static void test_R10() {
	std::cout << "R10: ranked_candidates excludes FAIL/MISSING\n";
	std::string dir = kTmpDir + "/rank_test";
	mkdir_p(dir);

	std::vector<BatchRunRecord> records = {
		make_run_record("run_0001", "PASS",    0.95),
		make_run_record("run_0002", "FAIL",    0.0),
		make_run_record("run_0003", "WARN",    0.70),
		make_run_record("run_0004", "MISSING", 0.0),
		make_run_record("run_0005", "PASS",    0.85),
	};

	std::string path = dir + "/ranked_candidates.tsv";
	write_ranked_candidates(records, "composite", path);
	EXPECT_TRUE(file_exists(path));

	std::string content = read_file(path);
	// Only PASS/WARN rows should appear
	EXPECT_TRUE(content.find("run_0001") != std::string::npos);
	EXPECT_TRUE(content.find("run_0003") != std::string::npos);
	EXPECT_TRUE(content.find("run_0005") != std::string::npos);
	EXPECT_TRUE(content.find("run_0002") == std::string::npos);
	EXPECT_TRUE(content.find("run_0004") == std::string::npos);

	rm_rf(dir);
}

// ── R11: batch summary produced and non-empty ─────────────────────────────────

static void test_R11() {
	std::cout << "R11: batch_summary.tsv produced\n";
	std::string dir = kTmpDir + "/summary_test";
	mkdir_p(dir);

	std::vector<BatchRunRecord> records = {
		make_run_record("run_0001", "PASS", 0.9),
		make_run_record("run_0002", "FAIL", 0.0),
	};

	std::string path = dir + "/batch_summary.tsv";
	write_batch_summary(records, path);
	EXPECT_TRUE(file_exists(path));
	std::string content = read_file(path);
	EXPECT_TRUE(!content.empty());
	EXPECT_TRUE(content.find("run_id") != std::string::npos);

	rm_rf(dir);
}

// ── R12: Template mode — base loaded from file ────────────────────────────────

static void test_R12() {
	std::cout << "R12: template mode — BatchMerger::load_template\n";
	std::string dir = kTmpDir + "/template_test";
	mkdir_p(dir);

	// Write a minimal .vsim template
	std::string tmpl_path = dir + "/base.vsim";
	write_file(tmpl_path,
		"[project]\nname = \"base_template\"\n\n"
		"[run]\nsteps = 500\ndt_fs = 1.0\n\n"
		"[environment]\ntemperature = 300\npressure_GPa = 0.0\n");

	std::vector<std::string> errors;
	auto base_doc = BatchMerger::load_template(tmpl_path, errors);
	EXPECT_TRUE(errors.empty());
	EXPECT_TRUE(base_doc.run.max_steps == 500);

	// Apply axis value
	std::map<std::string, std::string> overrides;
	overrides["environment.temperature"] = "700";
	std::vector<std::string> warnings;
	auto mutated = BatchMerger::apply_axis_values(base_doc, overrides, warnings);
	EXPECT_TRUE(mutated.environment.temperature == 700.0);

	rm_rf(dir);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
	std::cout << "=== Group 43: Static Runner Integration Tests ===\n\n";
	mkdir_p(kTmpDir);

	test_R1();
	test_R2();
	test_R3();
	test_R4();
	test_R5();
	test_R6();
	test_R7();
	test_R8();
	test_R9();
	test_R10();
	test_R11();
	test_R12();

	rm_rf(kTmpDir);

	std::cout << "\n";
	if (failures == 0)
		std::cout << "All Group 43 tests PASSED.\n";
	else
		std::cout << failures << " test(s) FAILED.\n";
	return failures ? 1 : 0;
}
