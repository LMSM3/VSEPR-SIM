// src/v4/uff/run_stage.hpp
// Formation Engine v4.1.0 -- UFX RunStage status system
//
// C++ is the authoritative source of stage information.
// The Bash spinner wrapper is optional visual sugar only.
// It must never make decisions about solver or validation state.

#pragma once

#include <iostream>
#include <string_view>

namespace vsepr::uff {

enum class RunStage {
	LoadingUFF,
	AutoCreating,
	WritingLogs,
	SpotChecking,
	RunningMacroTest,
	CompressingFields,
	ExportingXYZ,
	Complete,
	Failed
};

constexpr std::string_view to_string(RunStage stage) noexcept {
	switch (stage) {
		case RunStage::LoadingUFF:        return "Loading UFF table";
		case RunStage::AutoCreating:      return "Creating missing entries";
		case RunStage::WritingLogs:       return "Writing logs";
		case RunStage::SpotChecking:      return "Running spot checks";
		case RunStage::RunningMacroTest:  return "Running macro test";
		case RunStage::CompressingFields: return "Compressing fields";
		case RunStage::ExportingXYZ:      return "Exporting XYZ";
		case RunStage::Complete:          return "Complete";
		case RunStage::Failed:            return "Failed";
	}
	return "Unknown";
}

inline void print_stage_status(RunStage stage) {
	std::cout << "[UFX] " << to_string(stage) << '\n';
}

} // namespace vsepr::uff
