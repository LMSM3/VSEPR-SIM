/**
 * tests/test_wo94a_gas_trajectory.cpp
 *
 * Verifies that a .vsim script declaring corner-region molecules triggers the
 * gas-injection fast-path and produces a non-empty `.xyzf` trajectory file.
 */

#include "vsim/vsim_parser.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const char* message)
{
	if (!condition) {
		std::fprintf(stderr, "[FAIL] %s\n", message);
		++failures;
	}
}

std::string native_path(const std::string& p)
{
	std::error_code ec;
	auto np = std::filesystem::path(p).make_preferred();
	auto abs = std::filesystem::absolute(np, ec);
	return ec ? np.string() : abs.string();
}

std::string find_vsepr_exe()
{
	const char* env = std::getenv("VSEPR_EXE");
	if (env && std::strlen(env) > 0) return native_path(env);

	// Tests run from build/, so the executable is in build/Release or build directly.
	std::error_code ec;
	auto cwd = std::filesystem::current_path(ec);
	const std::vector<std::string> candidates = {
		(cwd / "vsepr.exe").string(),
		(cwd / ".." / "build" / "vsepr.exe").string(),
		(cwd / "Release" / "vsepr.exe").string(),
		(cwd / ".." / "build" / "Release" / "vsepr.exe").string(),
		"build/vsepr.exe",
		"build/Release/vsepr.exe",
		"build/Debug/vsepr.exe",
	};
	for (const auto& c : candidates) {
		std::error_code ec2;
		auto abs = std::filesystem::weakly_canonical(c, ec2);
		if (!ec2 && std::filesystem::is_regular_file(abs, ec2))
			return native_path(abs.string());
	}
	return native_path("build/vsepr.exe");
}

int run_cli(const std::string& exe, const std::string& script, const std::string& log)
{
	std::error_code ec;
	std::filesystem::create_directories(
		std::filesystem::path(script).parent_path() / ".." / ".." / "out" / "wo94a_gas_trajectory_demo", ec);
	const std::string cmd = "cmd /C \"\"" + exe + "\" run \"" + script + "\" > \"" +
		log + "\" 2>&1\"";
	std::fprintf(stdout, "[runtime] invoking: %s\n", cmd.c_str());
	return std::system(cmd.c_str());
}

std::string read_all(const std::string& path)
{
	std::ifstream in(path);
	if (!in) return "";
	return std::string((std::istreambuf_iterator<char>(in)),
					   std::istreambuf_iterator<char>());
}

bool starts_with(const std::string& s, const std::string& prefix)
{
	return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

std::string repo_root()
{
	std::error_code ec;
	auto cwd = std::filesystem::current_path(ec);
	if (!ec) return cwd.string();
	return ".";
}

std::string test_data_path(const std::string& rel)
{
	// When the test runs from build/, walk up one level to repo root.
	std::error_code ec;
	auto root = std::filesystem::path(repo_root());
	auto candidate = root / ".." / rel;
	auto norm = std::filesystem::weakly_canonical(candidate, ec);
	if (!ec && std::filesystem::exists(norm, ec))
		return norm.string();
	candidate = root / rel;
	norm = std::filesystem::weakly_canonical(candidate, ec);
	return norm.string();
}

int main()
{
	const std::string exe    = find_vsepr_exe();
	const std::string script = test_data_path("scripts/demos/wo94a_gas_trajectory_demo.vsim");
	const std::string log    = "tmp_wo94a_gas_trajectory.log";
	const std::string label  = "wo94a_gas_trajectory_demo";
	const std::string out_dir = "out/wo94a_gas_trajectory_demo";
	const std::string xyzf    = out_dir + "/" + label + ".xyzf";

	std::error_code ec;
	std::filesystem::remove(log, ec);
	std::filesystem::remove(xyzf, ec);

	std::fprintf(stdout, "[runtime] using executable: %s\n", exe.c_str());
	std::fprintf(stdout, "[runtime] using script:    %s\n", script.c_str());
	require(std::filesystem::is_regular_file(script, ec),
		"demo script does not exist at resolved path");
	require(std::filesystem::is_regular_file(exe, ec),
		"vsepr executable does not exist at resolved path");
	int rc = run_cli(exe, script, log);
	require(rc == 0, "wo94a_gas_trajectory_demo CLI returned non-zero exit");

	// The parser must recognize this as a gas-injection run.
	auto doc = vsim::VsimParser::parse_file(script);
	bool gas_run = false;
	for (const auto& m : doc.simulation.molecules)
		if (!m.region.empty()) { gas_run = true; break; }
	require(gas_run, "demo has no molecules with corner regions");
	require(doc.exports.write_xyzf, "write_xyzf must be requested in demo");

	// The trajectory file must exist and contain at least one frame.
	require(std::filesystem::is_regular_file(xyzf, ec),
		"gas-injection .xyzf trajectory was not produced");

	const std::string contents = read_all(xyzf);
	require(!contents.empty(), ".xyzf file is empty");

	// First token should be the atom count (128 = 4 species * 16 molecules * 2 atoms avg).
	std::istringstream iss(contents);
	int first_n = -1;
	iss >> first_n;
	require(first_n > 0, ".xyzf first frame does not start with a positive atom count");

	// Frame header should identify velocity properties.
	require(contents.find("gas_mix") != std::string::npos,
		".xyzf missing expected gas_mix frame header");
	require(contents.find("properties=\"velocity\"") != std::string::npos,
		".xyzf missing velocity property marker");

	if (failures == 0) {
		std::printf("OK  WO-94A gas-injection trajectory\n");
		return 0;
	}
	std::fprintf(stderr, "FAILED with %d assertion(s)\n", failures);
	return 1;
}
