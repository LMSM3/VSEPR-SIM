#pragma once
/**
 * include/batch/batch_document.hpp
 * ==================================
 * WO-VSIM-62C — Batch Layer Parser & Static Axis Runtime
 *
 * BatchDocument is the top-level struct for a parsed study (.vsim file with
 * a [study] block).  It owns all sub-sections from the spec §1.9.
 *
 * Stochastic axis runtime (kind = "stochastic") → v5.1.0
 * Formation stage execution (kind = "formation") → v5.2.0
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/vsim/vsim_document.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace vsim {
namespace batch {

struct BatchDocument {
	// Core study sections
	StudySection        study;
	BatchBaseSection    base;
	BatchDesignSection  design;
	SeedSection         seed;
	BatchExpandSection  expand;

	// Axes and cases
	std::vector<BatchAxisEntry>  axes;
	std::vector<BatchCaseEntry>  cases;

	// WO-62B verification sections (structs already in vsim_document.hpp)
	BatchVerifyPolicySection      verify_policy;
	BatchOverrideVerifySection    override_verify;
	BatchAggregateVerifySection   aggregate_verify;
	BatchScoreSection             score;
	BatchRequireSection           require;

	// Formation library (parsed but runtime deferred to v5.2.0)
	std::map<std::string, FormationLibraryEntry> formation_library;

	// Inline base document (when base.mode = "inline")
	// The parser populates this from the same file's non-study sections.
	std::optional<VsimDocument> inline_base;

	// Source path (set by BatchParser::parse_file)
	std::string source_path;

	bool valid() const { return study.populated && !study.name.empty(); }
};

} // namespace batch
} // namespace vsim
