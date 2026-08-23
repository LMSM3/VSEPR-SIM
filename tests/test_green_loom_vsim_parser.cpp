#include "vsim/vsim_parser.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
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

	const std::vector<std::string> candidates = {
		"build/vsepr.exe",
		"build/Release/vsepr.exe",
		"build/Debug/vsepr.exe",
	};
	for (const auto& c : candidates) {
		std::error_code ec;
		if (std::filesystem::is_regular_file(c, ec)) return native_path(c);
	}
	return native_path("build/vsepr.exe");
}

int run_cli(const std::string& exe, const std::string& script, const std::string& log)
{
	const std::string cmd = "cmd /C \"\"" + exe + "\" run \"" + script + "\" > \"" +
		log + "\" 2>&1\"";
	return std::system(cmd.c_str());
}

std::string read_all(const std::string& path)
{
	std::ifstream in(path);
	if (!in) return "";
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

bool json_has_key(const std::string& json, const std::string& key)
{
	return json.find("\"" + key + "\"") != std::string::npos;
}

std::string extract_json_string(const std::string& json, const std::string& key)
{
	const std::string quoted = "\"" + key + "\"";
	auto pos = json.find(quoted);
	if (pos == std::string::npos) return "";
	pos = json.find(':', pos);
	if (pos == std::string::npos) return "";
	++pos;
	while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
	if (pos >= json.size()) return "";
	if (json[pos] == '"') {
		auto end = json.find('"', pos + 1);
		if (end == std::string::npos) return "";
		return json.substr(pos + 1, end - pos - 1);
	}
	auto end = json.find_first_of(",}\n", pos);
	return json.substr(pos, end - pos);
}

void validate_humidity_stress()
{
	const auto doc = vsim::VsimParser::parse_file(
		"scripts/green_energy/green_loom_humidity_stress.vsim");

	require(doc.pillar.present, "humidity: [pillar] was not parsed");
	require(doc.pillar.id == "green_loom_humidity_stress", "humidity: pillar id mismatch");
	require(doc.species_registry.species.size() == 12, "humidity: species registry size mismatch");
	require(doc.reaction_network.channels.size() == 5, "humidity: reaction channel count mismatch");
	require(doc.reaction_stages.size() == 2, "humidity: reaction stage count mismatch");
	require(doc.exports.write_xyzf, "humidity: XYZF replay export is disabled");
	require(doc.exports.write_xyzfull, "humidity: XYZFull replay export is disabled");
	require(doc.exports.snapshot_interval == 5, "humidity: dense snapshot interval mismatch");
	require(doc.open.enabled && doc.open.mode == "advanced", "humidity: end-of-run advanced viewer is disabled");
	require(doc.open.advanced.trajectory_controls, "humidity: trajectory controls are disabled");
	require(doc.open.advanced.event_overlay, "humidity: event overlay is disabled");

	require(doc.run.dense_record, "humidity: [run] dense_record not enabled");
	require(doc.dense_record.enabled, "humidity: [dense_record] not enabled");
	require(doc.dense_record.energy_force_interval == 5, "humidity: dense cadence mismatch");
	require(doc.dense_record.max_records == 1200, "humidity: dense max_records mismatch");
}

void validate_regeneration_retention()
{
	const auto doc = vsim::VsimParser::parse_file(
		"scripts/green_energy/green_loom_regeneration_retention.vsim");

	require(doc.pillar.present, "retention: [pillar] was not parsed");
	require(doc.pillar.id == "green_loom_regeneration_retention", "retention: pillar id mismatch");
	require(doc.species_registry.species.size() == 11, "retention: species registry size mismatch");
	require(doc.reaction_network.channels.size() == 6, "retention: reaction channel count mismatch");
	require(doc.reaction_stages.size() == 4, "retention: reaction stage count mismatch");
	require(doc.exports.write_xyzf, "retention: XYZF replay export is disabled");
	require(doc.exports.write_xyzfull, "retention: XYZFull replay export is disabled");
	require(doc.exports.snapshot_interval == 5, "retention: dense snapshot interval mismatch");
	require(doc.open.enabled && doc.open.mode == "advanced", "retention: end-of-run advanced viewer is disabled");
	require(doc.open.advanced.trajectory_controls, "retention: trajectory controls are disabled");
	require(doc.open.advanced.event_overlay, "retention: event overlay is disabled");

	require(doc.run.dense_record, "retention: [run] dense_record not enabled");
	require(doc.dense_record.enabled, "retention: [dense_record] not enabled");
	require(doc.dense_record.energy_force_interval == 5, "retention: dense cadence mismatch");
	require(doc.dense_record.max_records == 1400, "retention: dense max_records mismatch");
}

void validate_humidity_runtime_artifacts()
{
	const std::string exe = find_vsepr_exe();
	const std::string script = "scripts/green_energy/green_loom_humidity_stress.vsim";
	const std::string log = "tmp_green_loom_humidity.log";
	std::error_code ec;
	std::filesystem::remove(log, ec);

	std::fprintf(stdout, "[runtime] using executable: %s\n", exe.c_str());
	int rc = run_cli(exe, script, log);
	require(rc == 0, "green_loom_humidity_stress CLI returned non-zero exit");

	const auto out_dir = std::string("out/green_energy/green_loom_humidity_stress");
	const auto sidecar = out_dir + "/green_loom_humidity_stress_dense_record.json";

	std::fprintf(stdout, "[runtime] checking sidecar: %s\n", native_path(sidecar).c_str());
	require(std::filesystem::is_regular_file(sidecar, ec),
		"humidity: dense_record sidecar missing");

	const std::string json = read_all(sidecar);
	require(!json.empty(), "humidity: dense_record json is empty");
	require(json_has_key(json, "rng_seed"), "humidity: sidecar missing rng_seed");
	require(json_has_key(json, "energy_trace"), "humidity: sidecar missing energy_trace");
	require(json_has_key(json, "rms_force_trace"), "humidity: sidecar missing rms_force_trace");
	require(json_has_key(json, "terminal_gates"), "humidity: sidecar missing terminal_gates");
	require(extract_json_string(json, "mode") == "dense", "humidity: sidecar mode should be dense");

	auto to_double = [](const std::string& s) -> double {
		try { return std::stod(s); } catch (...) { return -1.0; }
	};
	const double wall_ms = to_double(extract_json_string(json, "wall_ms"));
	const double comp_ms = to_double(extract_json_string(json, "comp_ms"));
	const double orch_ms = to_double(extract_json_string(json, "orch_ms"));
	require(wall_ms >= 0.0 && comp_ms >= 0.0 && orch_ms >= 0.0,
		"humidity: timing values must be non-negative");
	require(wall_ms + 1.0 >= comp_ms + orch_ms,
		"humidity: wall_ms less than comp_ms + orch_ms");
}

} // namespace

int main()
{
	try {
		validate_humidity_stress();
		validate_regeneration_retention();
		validate_humidity_runtime_artifacts();
	} catch (const std::exception& error) {
		std::fprintf(stderr, "[FAIL] parse exception: %s\n", error.what());
		return 1;
	}

	if (failures != 0) {
		std::fprintf(stderr, "Green Loom VSIM parser test: %d failure(s)\n", failures);
		return 1;
	}

	std::puts("Green Loom VSIM parser test: all checks passed.");
	return 0;
}
