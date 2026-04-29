/**
 * test_export_flush.cpp — Group 29: Script-Declared Export Flush
 * ==============================================================
 *
 * Acceptance tests proving that VsimRuntime::flush_exports() correctly
 * writes KernelEventLog content to disk when [export] flags are set.
 *
 * Doctrine under test:
 *   Reporting is script-declared.  The [export] section in a .vsim script
 *   is the only mechanism that triggers file output.  VsimRuntime honours
 *   it; nothing else writes audit files on the user's behalf.
 *
 * WO-56C / beta-7 export wiring
 */

#include "vsim/vsim_runtime.hpp"
#include "vsim/vsim_document.hpp"
#include "kernel/kernel_event_log.hpp"
#include "kernel/kernel_event.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace vsim;
using namespace vsepr::kernel;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string tmp_dir(const std::string& tag) {
	return (fs::temp_directory_path() / ("vsim_export_test_" + tag)).string();
}

static std::string read_file(const std::string& path) {
	std::ifstream f(path);
	return std::string(std::istreambuf_iterator<char>(f),
					   std::istreambuf_iterator<char>());
}

static void seed_log(KernelEventLog& log, int n = 3) {
	log.clear();
	for (int i = 0; i < n; ++i) {
		FormationEvent ev;
		ev.source_formula  = "H2O";
		ev.frame_id        = static_cast<uint64_t>(i);
		ev.n_beads         = 3;
		ev.fire_steps      = 100 + i * 10;
		ev.converged       = true;
		ev.final_energy    = -42.0 - i;
		ev.packing_fraction = 0.64 + 0.01 * i;
		ev.compute();
		log.record(ev);
	}
}

// ---------------------------------------------------------------------------
// Test 1 — no flags set → no files written
// ---------------------------------------------------------------------------

static void test_no_flags_no_output() {
	std::string dir = tmp_dir("no_flags");
	fs::remove_all(dir);

	ExportSection exp;
	exp.write_events_json = false;
	exp.write_report_md   = false;
	exp.output_dir        = dir;

	KernelEventLog& log = KernelEventLog::instance();
	seed_log(log);

	VsimRuntime::flush_exports(exp, log);

	// Directory should not have been created (no work to do)
	bool jsonl_exists = fs::exists(dir + "/events.jsonl");
	bool md_exists    = fs::exists(dir + "/events.md");
	assert(!jsonl_exists && "events.jsonl must not be created when write_events_json=false");
	assert(!md_exists    && "events.md must not be created when write_report_md=false");
	std::printf("  [PASS] test_no_flags_no_output\n");
}

// ---------------------------------------------------------------------------
// Test 2 — write_events_json=true → events.jsonl written with correct content
// ---------------------------------------------------------------------------

static void test_jsonl_written() {
	std::string dir = tmp_dir("jsonl");
	fs::remove_all(dir);

	ExportSection exp;
	exp.write_events_json = true;
	exp.write_report_md   = false;
	exp.output_dir        = dir;

	KernelEventLog& log = KernelEventLog::instance();
	seed_log(log, 2);

	VsimRuntime::flush_exports(exp, log, "jsonl_run");

	std::string path = dir + "/events.jsonl";
	assert(fs::exists(path) && "events.jsonl must be created");

	std::string content = read_file(path);
	assert(content.find("\"kind\":\"Formation\"") != std::string::npos
		   && "events.jsonl must contain Formation events");
	assert(content.find("\"formula\":\"H2O\"") != std::string::npos
		   && "events.jsonl must contain source formula");
	assert(content.find("// run: jsonl_run") != std::string::npos
		   && "events.jsonl must contain run label comment");

	std::printf("  [PASS] test_jsonl_written\n");
	fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// Test 3 — write_report_md=true → events.md written with Markdown table
// ---------------------------------------------------------------------------

static void test_markdown_written() {
	std::string dir = tmp_dir("md");
	fs::remove_all(dir);

	ExportSection exp;
	exp.write_events_json = false;
	exp.write_report_md   = true;
	exp.output_dir        = dir;

	KernelEventLog& log = KernelEventLog::instance();
	seed_log(log, 2);

	VsimRuntime::flush_exports(exp, log, "md_run");

	std::string path = dir + "/events.md";
	assert(fs::exists(path) && "events.md must be created");

	std::string content = read_file(path);
	assert(content.find("## Run: md_run") != std::string::npos
		   && "events.md must contain run label heading");
	assert(content.find("| ID |") != std::string::npos
		   && "events.md must contain Markdown table header");
	assert(content.find("Formation") != std::string::npos
		   && "events.md must contain Formation event rows");

	std::printf("  [PASS] test_markdown_written\n");
	fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// Test 4 — both flags set → both files written in one call
// ---------------------------------------------------------------------------

static void test_both_flags_both_files() {
	std::string dir = tmp_dir("both");
	fs::remove_all(dir);

	ExportSection exp;
	exp.write_events_json = true;
	exp.write_report_md   = true;
	exp.output_dir        = dir;

	KernelEventLog& log = KernelEventLog::instance();
	seed_log(log, 1);

	VsimRuntime::flush_exports(exp, log, "both_run");

	assert(fs::exists(dir + "/events.jsonl") && "events.jsonl must exist");
	assert(fs::exists(dir + "/events.md")    && "events.md must exist");

	std::printf("  [PASS] test_both_flags_both_files\n");
	fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// Test 5 — multi-run appends accumulate into single file
// ---------------------------------------------------------------------------

static void test_multi_run_appends() {
	std::string dir = tmp_dir("append");
	fs::remove_all(dir);

	ExportSection exp;
	exp.write_events_json = true;
	exp.write_report_md   = false;
	exp.output_dir        = dir;

	KernelEventLog& log = KernelEventLog::instance();

	seed_log(log, 1);
	VsimRuntime::flush_exports(exp, log, "run_1");

	seed_log(log, 1);
	VsimRuntime::flush_exports(exp, log, "run_2");

	std::string content = read_file(dir + "/events.jsonl");
	size_t first  = content.find("// run: run_1");
	size_t second = content.find("// run: run_2");
	assert(first  != std::string::npos && "run_1 label must appear");
	assert(second != std::string::npos && "run_2 label must appear");
	assert(first < second && "run_1 must precede run_2 in file");

	std::printf("  [PASS] test_multi_run_appends\n");
	fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// Test 6 — empty log produces empty but valid files (not a crash)
// ---------------------------------------------------------------------------

static void test_empty_log_produces_valid_files() {
	std::string dir = tmp_dir("empty");
	fs::remove_all(dir);

	ExportSection exp;
	exp.write_events_json = true;
	exp.write_report_md   = true;
	exp.output_dir        = dir;

	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	VsimRuntime::flush_exports(exp, log, "empty_run");

	assert(fs::exists(dir + "/events.jsonl") && "events.jsonl must exist even for empty log");
	assert(fs::exists(dir + "/events.md")    && "events.md must exist even for empty log");

	std::printf("  [PASS] test_empty_log_produces_valid_files\n");
	fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
	std::printf("\n=== Group 29: Script-Declared Export Flush ===\n\n");

	test_no_flags_no_output();
	test_jsonl_written();
	test_markdown_written();
	test_both_flags_both_files();
	test_multi_run_appends();
	test_empty_log_produces_valid_files();

	std::printf("\n  All Group 29 export flush tests passed.\n\n");
	return 0;
}
