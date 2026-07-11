#pragma once
/**
 * include/batch/batch_expander.hpp
 * ==================================
 * WO-VSIM-62C — Static Axis Run Expansion
 *
 * BatchRunSpec  — one resolved spec per run
 * BatchExpander — factorial expansion of static axes
 *                 (stochastic and formation axes deferred to v5.1.0 / v5.2.0)
 *
 * expand() also writes batch_plan.tsv to the output_root directory.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_document.hpp"
#include "include/batch/seed_resolver.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vsim {
namespace batch {

struct BatchRunSpec {
	std::string run_id;       // "run_0001"
	int         run_index;    // 1-based
	int         replicate;    // 0-based replicate index within case
	std::string case_name;    // "" if no [[batch.case]] used

	std::map<std::string, std::string> axis_values;

	// Resolved seeds for this replicate
	uint64_t seed_foundation;
	uint64_t seed_defect;
	uint64_t seed_formation;
	uint64_t seed_thermal;
	uint64_t seed_placement;

	std::string formation_path; // empty unless formation axis present

	std::string output_dir;     // "<study.name>/cases/<run_id>"
};

class BatchExpander {
public:
	// Expand a BatchDocument into a run list.
	// Only static axes are expanded here.
	// Skips stochastic/formation axes with a warning pushed to warnings_out.
	static std::vector<BatchRunSpec> expand(
		const BatchDocument&      doc,
		std::vector<std::string>& warnings_out);

	// Write batch_plan.tsv to the given path.
	static void write_batch_plan(const std::vector<BatchRunSpec>& specs,
								 const BatchDocument&             doc,
								 const std::string&               path);
};

} // namespace batch
} // namespace vsim
