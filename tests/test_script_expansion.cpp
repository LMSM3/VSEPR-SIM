#include "cli/script_expansion.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
	if (!condition) {
		std::cerr << "FAIL line " << line << ": " << expression << '\n';
		++failures;
	}
}

#define CHECK(expression) check((expression), #expression, __LINE__)

std::filesystem::path write_fixture() {
	const auto path = std::filesystem::temp_directory_path() /
		"script_expansion_preview_fixture.vsim";

	std::ofstream out(path);
	out << R"VSIM(
[project]
name = "script_expansion_preview_fixture"
version = "test"

[material]
formula = "NH3"
structure = "molecular"

[distribution.probe_cloud]
target = "material.local_environment"
species = "He"
count = 4
placement = "low_discrepancy"
shape = "sphere"
center = [0.0, 0.0, 4.0]
extent = [2.0, 2.0, 2.0]

[run]
mode = "relax"
max_steps = 25

[analysis.classify]
enabled = true
vsepr_sites = true
organic_candidate = true

[observe]
metrics = ["coordination", "bond_angles", "vsepr_sites", "organic_candidate"]

[export]
write_report_md = true
)VSIM";
	return path;
}

bool has_label(
	const vsepr::cli::ScriptExpansion& expansion,
	const std::string& label
) {
	for (const auto& candidate : expansion.candidates) {
		if (candidate.label == label) {
			return true;
		}
	}
	return false;
}

bool has_builder_path(
	const vsepr::cli::ScriptExpansion& expansion,
	const std::string& path
) {
	for (const auto& record : expansion.builder_records) {
		if (record.path == path)
			return true;
	}
	return false;
}

} // namespace

int main() {
	const auto fixture = write_fixture();
	const auto expansion = vsepr::cli::expand_script_file(fixture);

	CHECK(expansion.source_name == fixture.filename().string());
	CHECK(!expansion.candidates.empty());
	CHECK(expansion.canonical_parser);
	CHECK(expansion.missing_property_count >= 1);
	CHECK(expansion.report_annotation_count >= 1);
	CHECK(expansion.executable_candidate_count == 0);
	CHECK(expansion.ai_ready);
	CHECK(expansion.distributions.size() == 1);
	CHECK(expansion.distributions[0].name == "probe_cloud");
	CHECK(expansion.distributions[0].complete);
	CHECK(!expansion.distributions[0].executable);
	CHECK(!expansion.distributions[0].executed);
	CHECK(expansion.written_record_count >= 8);
	CHECK(expansion.inferred_record_count >= 2);
	CHECK(expansion.future_runnable_record_count >= 1);
	CHECK(expansion.executed_record_count == 0);
	CHECK(has_builder_path(expansion, "material.formula"));
	CHECK(has_builder_path(expansion, "material.prototype"));
	CHECK(has_builder_path(expansion, "distribution.probe_cloud.resolved_target"));

	CHECK(has_label(expansion, "classify_preview_packet"));
	CHECK(has_label(expansion, "bond_order_metadata"));
	CHECK(has_label(expansion, "trigonal_pyramidal_lone_pair_probe"));
	CHECK(has_label(expansion, "direct_script_path"));

	const auto summary = vsepr::cli::format_script_expansion(expansion);
	CHECK(summary.find("direct_execution: not run") != std::string::npos);
	CHECK(summary.find("canonical_parser: true") != std::string::npos);
	CHECK(summary.find("distribution_count: 1") != std::string::npos);
	CHECK(summary.find("[distribution probe_cloud]") != std::string::npos);
	CHECK(summary.find("status: future_runnable") != std::string::npos);
	CHECK(summary.find("executed_records: 0") != std::string::npos);
	CHECK(summary.find("selected_execution: none") != std::string::npos);
	CHECK(summary.find("result: PASS") != std::string::npos);

	std::filesystem::remove(fixture);
	if (failures == 0)
		std::cout << "Script expansion tests passed.\n";
	return failures == 0 ? 0 : 1;
}
