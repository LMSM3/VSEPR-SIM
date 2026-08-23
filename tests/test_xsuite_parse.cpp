// =============================================================================
// tests/test_xsuite_parse.cpp  (WO-XYZSUITE-X)
// =============================================================================
// XP1  minimal valid .X  — parse succeeds, required fields populated
// XP2  missing entry script  — parse returns error
// XP3  missing optional trajectory  — parse ok, warning issued
// XP4  hash field silently ignored  — parse ok, no error
// XP5  action flags parsed correctly  — compile/run/validate/replay/export
// =============================================================================

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <filesystem>

#include "include/vsim/xsuite.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string tmp_file(const std::string& name) {
	return (std::filesystem::temp_directory_path() / name).string();
}

static void write_file(const std::string& path, const std::string& content) {
	std::ofstream f(path);
	assert(f.is_open());
	f << content;
}

static int failures = 0;

#define CHECK(cond, msg) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "  [FAIL] %s: %s\n", __FUNCTION__, msg); \
			++failures; \
		} else { \
			std::fprintf(stdout, "  [PASS] %s: %s\n", __FUNCTION__, msg); \
		} \
	} while (0)

// ---------------------------------------------------------------------------
// XP1: minimal valid .X
// ---------------------------------------------------------------------------
static void XP1_minimal_valid() {
	std::string path = tmp_file("xp1_minimal.X");
	write_file(path, R"(
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
	CHECK(res.ok,                             "parse succeeds");
	CHECK(res.error.empty(),                  "no error message");
	CHECK(res.suite.name == "test_minimal",   "name parsed");
	CHECK(res.suite.entry_script == "nacl.vsim", "entry parsed from [files].script");
	CHECK(res.suite.num_steps == 500,         "num_steps parsed");
	CHECK(res.suite.version == "5.2",         "version parsed");
	std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// XP2: missing entry script
// ---------------------------------------------------------------------------
static void XP2_missing_entry_script() {
	std::string path = tmp_file("xp2_no_entry.X");
	write_file(path, R"(
[xsuite]
name = "no_entry"

[run]
# entry intentionally omitted
num_steps = 100

[files]
snapshot = "snap.xyz"
)");

	auto res = vsepr::xsuite::xsuite_parse(path);
	CHECK(!res.ok,              "parse fails");
	CHECK(!res.error.empty(),   "error message non-empty");
	std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// XP3: missing optional trajectory — parse ok, warning issued
// ---------------------------------------------------------------------------
static void XP3_missing_optional_trajectory() {
	std::string path = tmp_file("xp3_no_traj.X");
	write_file(path, R"(
[xsuite]
name = "no_traj"

[run]
entry = "mol.vsim"

[files]
script = "mol.vsim"
# trajectory intentionally omitted
)");

	auto res = vsepr::xsuite::xsuite_parse(path);
	CHECK(res.ok,   "parse succeeds despite missing trajectory");

	bool has_traj_warning = false;
	for (const auto& w : res.warnings) {
		if (w.find("trajectory") != std::string::npos ||
			w.find(".xyzf")      != std::string::npos) {
			has_traj_warning = true;
		}
	}
	CHECK(has_traj_warning, "warning issued for missing trajectory");
	std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// XP4: hash field silently ignored — no error, no crash
// ---------------------------------------------------------------------------
static void XP4_hash_field_ignored_safely() {
	std::string path = tmp_file("xp4_hash.X");
	write_file(path, R"(
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
	CHECK(res.ok,                                    "parse succeeds with hash section");
	CHECK(res.suite.script_hash == "abc123def456",   "script_hash captured");
	CHECK(res.suite.suite_hash  == "000000000000",   "suite_hash captured");
	std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// XP5: action flags parsed correctly
// ---------------------------------------------------------------------------
static void XP5_action_flags() {
	std::string path = tmp_file("xp5_actions.X");
	write_file(path, R"(
[xsuite]
name = "action_test"

[run]
entry = "demo.vsim"

[files]
script = "demo.vsim"

[actions]
compile = true
run     = false
validate = true
replay  = true
export  = false
)");

	auto res = vsepr::xsuite::xsuite_parse(path);
	CHECK(res.ok,                   "parse succeeds");
	CHECK(res.suite.compile == true,         "compile = true");
	CHECK(res.suite.run == false,            "run = false");
	CHECK(res.suite.validate == true,        "validate = true");
	CHECK(res.suite.replay == true,          "replay = true");
	CHECK(res.suite.export_outputs == false, "export = false");
	std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
	std::fprintf(stdout, "=== test_xsuite_parse (WO-XYZSUITE-X) ===\n");

	XP1_minimal_valid();
	XP2_missing_entry_script();
	XP3_missing_optional_trajectory();
	XP4_hash_field_ignored_safely();
	XP5_action_flags();

	if (failures == 0) {
		std::fprintf(stdout, "\nAll tests PASSED\n");
		return 0;
	}
	std::fprintf(stderr, "\n%d test(s) FAILED\n", failures);
	return 1;
}
