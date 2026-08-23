#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace vsepr {
namespace cli {

enum class ExpansionKind {
	MissingProperty,
	GeometryVariant,
	MaterialVariant,
	ParameterSweep,
	ClassificationHint,
	RuntimeAlternative,
	ReportAnnotation,
	AIHookPacket
};

struct ExpansionCandidate {
	ExpansionKind kind;
	std::string label;
	std::string reason;
	double confidence = 0.0;
	bool executable = false;
	bool selected = false;
};

enum class BuilderRecordOrigin {
	Written,
	Inferred
};

enum class BuilderRecordStatus {
	PreviewOnly,
	FutureRunnable,
	Executed,
	Unsupported
};

struct BuilderExpansionRecord {
	std::string path;
	std::string value;
	BuilderRecordOrigin origin = BuilderRecordOrigin::Written;
	BuilderRecordStatus status = BuilderRecordStatus::PreviewOnly;
	std::string reason;
};

struct DistributionExpansion {
	std::string name;
	std::string target;
	std::string species;
	int count = 0;
	std::string placement;
	std::string shape;
	bool complete = false;
	bool executable = false;
	bool executed = false;
};

struct ScriptExpansion {
	std::string source_name;
	std::vector<ExpansionCandidate> candidates;
	std::vector<BuilderExpansionRecord> builder_records;
	std::vector<DistributionExpansion> distributions;

	std::size_t missing_property_count = 0;
	std::size_t executable_candidate_count = 0;
	std::size_t report_annotation_count = 0;
	std::size_t written_record_count = 0;
	std::size_t inferred_record_count = 0;
	std::size_t future_runnable_record_count = 0;
	std::size_t executed_record_count = 0;

	bool ai_ready = false;
	bool canonical_parser = false;
};

std::string to_string(ExpansionKind kind);
std::string to_string(BuilderRecordOrigin origin);
std::string to_string(BuilderRecordStatus status);

ScriptExpansion expand_script_file(const std::filesystem::path& script_path);

std::string format_script_expansion(const ScriptExpansion& expansion);

int run_script_expansion_preview(const std::filesystem::path& script_path);

} // namespace cli
} // namespace vsepr
