/**
 * tests/test_x_framework.cpp
 * ===========================
 * Consolidated .X framework test inlet.
 *
 * Covers both halves of the .X ecosystem in one binary:
 *
 *   Section A: XBundle (.X XBUNDLE archive format)  — WO-72A
 *   -------------------------------------------------------
 *   XB-01  write_string produces XBUNDLE magic header
 *   XB-02  write_string contains [manifest] block
 *   XB-03  write_string contains [[member]] block
 *   XB-04  round-trip: read_string recovers manifest.name
 *   XB-05  round-trip: read_string recovers entry name
 *   XB-06  round-trip: read_string recovers entry content
 *   XB-07  validate() passes on a well-formed bundle
 *   XB-08  validate() rejects bundle with no entries (V-03)
 *   XB-09  validate() rejects entry with empty name (V-04)
 *   XB-10  entry_point() returns first vsim entry when none specified
 *
 *   Section B: XSuite (.X INI saved-run-suite format)  — WO-XYZSUITE-X
 *   ------------------------------------------------------------------
 *   XP-01  minimal valid .X — parse succeeds, required fields populated
 *   XP-02  missing entry script — parse returns error
 *   XP-03  missing optional trajectory — parse ok, warning issued
 *   XP-04  hash field silently ignored — parse ok, no error
 *   XP-05  action flags parsed correctly — compile/run/validate/replay/export
 *
 * Exit code: 0 = all pass, 1 = one or more failures.
 *
 * Alias (post-install): vsepr-x-audit.exe
 * CMake target:         test_x_framework
 * CTest name:           XFrameworkAudit
 */

// ---- XBundle headers ----
#include "include/xbundle/xbundle_document.hpp"
#include "include/xbundle/xbundle_reader.hpp"
#include "include/xbundle/xbundle_writer.hpp"
#include "include/xbundle/xbundle_validator.hpp"

// ---- XSuite header ----
#include "include/vsim/xsuite.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace vsim::xbundle;

// =============================================================================
// Unified result tracking
// =============================================================================

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(id, cond, msg)                                                   \
	do {                                                                       \
		if (cond) {                                                            \
			std::printf("  PASS %-6s %s\n", id, msg);                         \
			++g_pass;                                                          \
		} else {                                                               \
			std::printf("  FAIL %-6s %s\n", id, msg);                         \
			++g_fail;                                                          \
		}                                                                      \
	} while (0)

// =============================================================================
// Section A helpers
// =============================================================================

static XBundle make_bundle() {
	XBundle b;
	b.manifest.name        = "smoke_suite";
	b.manifest.description = "X framework consolidated test";
	b.manifest.author      = "vsepr-sim";
	b.manifest.populated   = true;

	XBundleEntry e1;
	e1.name    = "run_a";
	e1.kind    = XBundleEntryKind::vsim;
	e1.content = "kernel.channel.reset()\nruntime.run_case(\"a\")\n";

	XBundleEntry e2;
	e2.name    = "run_b";
	e2.kind    = XBundleEntryKind::vsim;
	e2.content = "kernel.channel.reset()\nruntime.run_case(\"b\")\n";

	b.entries.push_back(e1);
	b.entries.push_back(e2);
	return b;
}

// =============================================================================
// Section B helpers
// =============================================================================

static std::string tmp_path(const std::string& name) {
	return (std::filesystem::temp_directory_path() / name).string();
}

static void write_tmp(const std::string& path, const std::string& content) {
	std::ofstream f(path);
	assert(f.is_open());
	f << content;
}

// =============================================================================
// Section A: XBundle tests (XB-01 .. XB-10)
// =============================================================================

static void section_a_xbundle() {
	std::printf("\n--- Section A: XBundle (XBUNDLE archive) ---\n");

	// XB-01
	{
		const auto text = XBundleWriter::write_string(make_bundle());
		CHECK("XB-01", text.find("XBUNDLE 1") != std::string::npos,
			  "write_string contains XBUNDLE magic");
	}
	// XB-02
	{
		const auto text = XBundleWriter::write_string(make_bundle());
		CHECK("XB-02", text.find("[manifest]") != std::string::npos,
			  "write_string contains [manifest] block");
	}
	// XB-03
	{
		const auto text = XBundleWriter::write_string(make_bundle());
		CHECK("XB-03", text.find("[[member]]") != std::string::npos,
			  "write_string contains [[member]] block");
	}
	// XB-04
	{
		const auto text   = XBundleWriter::write_string(make_bundle());
		const auto bundle = XBundleReader::read_string(text, "smoke.X");
		CHECK("XB-04", bundle.manifest.name == "smoke_suite",
			  "round-trip: manifest.name == \"smoke_suite\"");
	}
	// XB-05
	{
		const auto text   = XBundleWriter::write_string(make_bundle());
		const auto bundle = XBundleReader::read_string(text, "smoke.X");
		CHECK("XB-05", !bundle.entries.empty() &&
						bundle.entries[0].name == "run_a",
			  "round-trip: first entry name == \"run_a\"");
	}
	// XB-06
	{
		const auto text   = XBundleWriter::write_string(make_bundle());
		const auto bundle = XBundleReader::read_string(text, "smoke.X");
		bool ok = !bundle.entries.empty() &&
				  bundle.entries[0].content.find("run_case") != std::string::npos;
		CHECK("XB-06", ok,
			  "round-trip: entry content contains \"run_case\"");
	}
	// XB-07
	{
		const auto errs = XBundleValidator::validate(make_bundle());
		CHECK("XB-07", errs.empty(),
			  "validate() returns no errors for well-formed bundle");
	}
	// XB-08
	{
		XBundle b;
		b.manifest.name      = "empty_suite";
		b.manifest.populated = true;
		const auto errs = XBundleValidator::validate(b);
		bool has_v03 = false;
		for (const auto& e : errs)
			if (e.find("V-03") != std::string::npos) { has_v03 = true; break; }
		CHECK("XB-08", has_v03,
			  "validate() emits V-03 for bundle with no entries");
	}
	// XB-09
	{
		XBundle b = make_bundle();
		b.entries[0].name = "";
		const auto errs = XBundleValidator::validate(b);
		bool has_v04 = false;
		for (const auto& e : errs)
			if (e.find("V-04") != std::string::npos) { has_v04 = true; break; }
		CHECK("XB-09", has_v04,
			  "validate() emits V-04 for entry with empty name");
	}
	// XB-10
	{
		const auto b  = make_bundle();
		const auto* ep = b.entry_point();
		CHECK("XB-10", ep != nullptr && ep->name == "run_a",
			  "entry_point() returns first vsim entry \"run_a\" when unspecified");
	}
}

// =============================================================================
// Section B: XSuite tests (XP-01 .. XP-05)
// =============================================================================

static void section_b_xsuite() {
	std::printf("\n--- Section B: XSuite (INI saved-run-suite) ---\n");

	// XP-01: minimal valid
	{
		std::string path = tmp_path("xp1_minimal.X");
		write_tmp(path, R"(
[xsuite]
name = "test_minimal"
version = "5.2"

[run]
entry = "nacl.vsim"
num_steps = 500
dt = 1.0e-15

[files]
script = "nacl.vsim"
snapshot = "nacl.xyz"
)");
		auto res = vsepr::xsuite::xsuite_parse(path);
		CHECK("XP-01a", res.ok,                              "parse succeeds");
		CHECK("XP-01b", res.error.empty(),                   "no error message");
		CHECK("XP-01c", res.suite.name == "test_minimal",    "name parsed");
		CHECK("XP-01d", res.suite.entry_script == "nacl.vsim","entry parsed");
		CHECK("XP-01e", res.suite.num_steps == 500,          "num_steps parsed");
		CHECK("XP-01f", res.suite.version == "5.2",          "version parsed");
		std::filesystem::remove(path);
	}

	// XP-02: missing entry script → parse fails
	{
		std::string path = tmp_path("xp2_no_entry.X");
		write_tmp(path, R"(
[xsuite]
name = "no_entry"

[run]
# entry intentionally omitted
num_steps = 100

[files]
snapshot = "snap.xyz"
)");
		auto res = vsepr::xsuite::xsuite_parse(path);
		CHECK("XP-02a", !res.ok,             "parse fails on missing entry");
		CHECK("XP-02b", !res.error.empty(),  "error message non-empty");
		std::filesystem::remove(path);
	}

	// XP-03: missing optional trajectory → parse ok, warning issued
	{
		std::string path = tmp_path("xp3_no_traj.X");
		write_tmp(path, R"(
[xsuite]
name = "no_traj"

[run]
entry = "mol.vsim"

[files]
script = "mol.vsim"
)");
		auto res = vsepr::xsuite::xsuite_parse(path);
		CHECK("XP-03a", res.ok, "parse succeeds without trajectory");
		bool has_traj_warn = false;
		for (const auto& w : res.warnings)
			if (w.find("trajectory") != std::string::npos ||
				w.find(".xyzf")      != std::string::npos)
				has_traj_warn = true;
		CHECK("XP-03b", has_traj_warn, "warning issued for missing trajectory");
		std::filesystem::remove(path);
	}

	// XP-04: hash field silently ignored
	{
		std::string path = tmp_path("xp4_hash.X");
		write_tmp(path, R"(
[xsuite]
name = "with_hash"

[run]
entry = "h2o.vsim"

[files]
script = "h2o.vsim"

[hash]
script_hash = "abc123def456"
suite_hash  = "000000000000"
unknown_future_key = "should_be_ignored"
)");
		auto res = vsepr::xsuite::xsuite_parse(path);
		CHECK("XP-04a", res.ok,                                  "parse succeeds with hash section");
		CHECK("XP-04b", res.suite.script_hash == "abc123def456", "script_hash captured");
		CHECK("XP-04c", res.suite.suite_hash  == "000000000000", "suite_hash captured");
		std::filesystem::remove(path);
	}

	// XP-05: action flags
	{
		std::string path = tmp_path("xp5_actions.X");
		write_tmp(path, R"(
[xsuite]
name = "action_test"

[run]
entry = "demo.vsim"

[files]
script = "demo.vsim"

[actions]
compile  = true
run      = false
validate = true
replay   = true
export   = false
)");
		auto res = vsepr::xsuite::xsuite_parse(path);
		CHECK("XP-05a", res.ok,                          "parse succeeds");
		CHECK("XP-05b", res.suite.compile == true,       "compile = true");
		CHECK("XP-05c", res.suite.run == false,          "run = false");
		CHECK("XP-05d", res.suite.validate == true,      "validate = true");
		CHECK("XP-05e", res.suite.replay == true,        "replay = true");
		CHECK("XP-05f", res.suite.export_outputs == false,"export = false");
		std::filesystem::remove(path);
	}
}

// =============================================================================
// main
// =============================================================================

int main() {
	std::printf("=============================================================\n");
	std::printf("  vsepr-x-audit  |  .X Framework Consolidated Test Inlet\n");
	std::printf("  XBundle (WO-72A) + XSuite (WO-XYZSUITE-X)  |  v5.0.14\n");
	std::printf("=============================================================\n");

	section_a_xbundle();
	section_b_xsuite();

	std::printf("\n-------------------------------------------------------------\n");
	std::printf("  Result: %d passed, %d failed  (%d total)\n",
				g_pass, g_fail, g_pass + g_fail);
	std::printf("-------------------------------------------------------------\n");

	if (g_fail == 0) {
		std::printf("  STATUS: ALL PASS\n\n");
		return 0;
	}
	std::printf("  STATUS: FAILURES DETECTED\n\n");
	return 1;
}
