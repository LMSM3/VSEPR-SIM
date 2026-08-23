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

void write_script(const std::string& path, const std::string& text)
{
	std::filesystem::create_directories(std::filesystem::path(path).parent_path());
	std::ofstream out(path, std::ios::binary);
	if (!out) throw std::runtime_error("cannot write " + path);
	out << text;
}

bool file_contains(const std::string& path, const std::string& token)
{
	std::ifstream in(path);
	if (!in) return false;
	std::string line;
	while (std::getline(in, line)) {
		if (line.find(token) != std::string::npos) return true;
	}
	return false;
}

std::string read_all(const std::string& path)
{
	std::ifstream in(path);
	if (!in) return "";
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
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

bool json_has_key(const std::string& json, const std::string& key)
{
	return json.find("\"" + key + "\"") != std::string::npos;
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

int run_cli(const std::string& exe, const std::string& script)
{
	const std::string cmd = "cmd /C \"\"" + exe + "\" run \"" + script + "\" > \"" +
		script + ".log\" 2>&1\"";
	return std::system(cmd.c_str());
}

} // namespace

int main()
{
	const std::string work_dir = "tmp_dense_test_work";
	const std::string out_dir = work_dir + "/out/test_dense_run_record";
	const std::string script = work_dir + "/dense_record_test.vsim";
	const std::string script_text = R"vsim(
[project]
name = "dense_record_test"

[simulation]
box_size_ang = 10.0

[[simulation.molecule]]
formula = "H2O"
count = 2
n_layers = 1

[run]
mode = "relax"
max_steps = 30
dense_record = true

[dense_record]
energy_force_interval = 3
max_records = 20
initial_structure = true
final_structure = true
connectivity_before = true
connectivity_after = true
seed = true
potential_checksum = true
terminal_gates = true
timing_split = true
potential_label = "vsim_default_test"

[export]
write_xyz = true
output_dir = ")vsim" + out_dir + R"vsim("
)vsim";

	try {
		write_script(script, script_text);
		const auto doc = vsim::VsimParser::parse_file(script);

		require(doc.run.dense_record, "run.dense_record flag not parsed");
		require(doc.dense_record.enabled, "dense_record.enabled not set");
		require(doc.dense_record.energy_force_interval == 3, "energy_force_interval mismatch");
		require(doc.dense_record.max_records == 20, "max_records mismatch");
		require(doc.dense_record.initial_structure, "initial_structure not parsed");
		require(doc.dense_record.final_structure, "final_structure not parsed");
		require(doc.dense_record.connectivity_before, "connectivity_before not parsed");
		require(doc.dense_record.connectivity_after, "connectivity_after not parsed");
		require(doc.dense_record.seed, "seed not parsed");
		require(doc.dense_record.potential_checksum, "potential_checksum not parsed");
		require(doc.dense_record.terminal_gates, "terminal_gates not parsed");
		require(doc.dense_record.timing_split, "timing_split not parsed");
		require(doc.dense_record.potential_label == "vsim_default_test", "potential_label mismatch");

		std::fprintf(stdout, "Parser sidecar assertions passed.\n");
	} catch (const std::exception& error) {
		std::fprintf(stderr, "[FAIL] %s\n", error.what());
		return 1;
	}

	// -----------------------------------------------------------------
	// End-to-end runtime check: invoke the CLI and inspect artifacts.
	// -----------------------------------------------------------------
	{
		const std::string exe = find_vsepr_exe();
		std::fprintf(stdout, "[runtime] using executable: %s\n", exe.c_str());

		std::error_code ec;
		std::filesystem::remove_all(out_dir, ec);

		int rc = run_cli(exe, script);
		require(rc == 0, "vsepr.exe run returned non-zero exit");

		const auto initial_xyz = out_dir + "/dense_record_test_initial.xyz";
		const auto final_xyz   = out_dir + "/dense_record_test_final.xyz";
		const auto conn_before = out_dir + "/dense_record_test_connectivity_before.json";
		const auto conn_after  = out_dir + "/dense_record_test_connectivity_after.json";
		const auto ef_tsv      = out_dir + "/dense_record_test_energy_force.tsv";
		const auto json_sidecar= out_dir + "/dense_record_test_dense_record.json";

		require(std::filesystem::is_regular_file(initial_xyz, ec), "initial xyz missing");
		require(std::filesystem::is_regular_file(final_xyz, ec), "final xyz missing");
		require(std::filesystem::is_regular_file(conn_before, ec), "connectivity_before json missing");
		require(std::filesystem::is_regular_file(conn_after, ec), "connectivity_after json missing");
		require(std::filesystem::is_regular_file(ef_tsv, ec), "energy_force tsv missing");
		require(std::filesystem::is_regular_file(json_sidecar, ec), "dense_record json sidecar missing");

		const std::string json = read_all(json_sidecar);
		require(!json.empty(), "dense_record json is empty");
		require(json_has_key(json, "run_label"), "sidecar missing run_label");
		require(json_has_key(json, "rng_seed"), "sidecar missing rng_seed");
		require(json_has_key(json, "wall_ms"), "sidecar missing wall_ms");
		require(json_has_key(json, "comp_ms"), "sidecar missing comp_ms");
		require(json_has_key(json, "orch_ms"), "sidecar missing orch_ms");
		require(json_has_key(json, "energy_trace"), "sidecar missing energy_trace");
		require(json_has_key(json, "rms_force_trace"), "sidecar missing rms_force_trace");
		require(json_has_key(json, "terminal_gates"), "sidecar missing terminal_gates");

		const std::string mode = extract_json_string(json, "mode");
		require(mode == "dense", "sidecar mode should be 'dense'");

		// Timing split sanity: orchestration + computation cannot exceed wall time.
		auto to_double = [](const std::string& s) -> double {
			try { return std::stod(s); } catch (...) { return -1.0; }
		};
		const double wall_ms = to_double(extract_json_string(json, "wall_ms"));
		const double comp_ms = to_double(extract_json_string(json, "comp_ms"));
		const double orch_ms = to_double(extract_json_string(json, "orch_ms"));
		require(wall_ms >= 0.0, "wall_ms negative or unreadable");
		require(comp_ms >= 0.0, "comp_ms negative or unreadable");
		require(orch_ms >= 0.0, "orch_ms negative or unreadable");
		require(wall_ms + 0.001 >= comp_ms + orch_ms,
			"wall_ms less than comp_ms + orch_ms");

		require(file_contains(ef_tsv, "energy_kcal_mol"), "tsv missing header");
		require(file_contains(ef_tsv, "dense_record"), "tsv missing notes marker");

		std::fprintf(stdout, "Runtime artifact assertions passed.\n");
	}

	if (failures != 0) {
		std::fprintf(stderr, "Dense record VSIM test: %d failure(s)\n", failures);
		return 1;
	}

	std::puts("Dense record VSIM test: all checks passed.");
	return 0;
}
