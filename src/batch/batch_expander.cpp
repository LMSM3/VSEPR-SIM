/**
 * src/batch/batch_expander.cpp
 * ==============================
 * WO-VSIM-62C — Factorial Run Expansion Implementation
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_expander.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace vsim {
namespace batch {

// ── helpers ───────────────────────────────────────────────────────────────────

// Compute the cartesian product of value-lists.
// Input:  vector of { name, [values] }
// Output: vector of { name → value } maps (one per combination)
static std::vector<std::map<std::string, std::string>> cartesian_product(
	const std::vector<BatchAxisEntry>& axes)
{
	std::vector<std::map<std::string, std::string>> result;
	result.push_back({}); // seed: one empty combo

	for (const auto& ax : axes) {
		std::vector<std::map<std::string, std::string>> next;
		for (const auto& existing : result) {
			for (const auto& val : ax.values) {
				auto combo = existing;
				combo[ax.target] = val;
				combo["__axis_" + ax.name] = val; // keep axis name for plan.tsv
				next.push_back(std::move(combo));
			}
		}
		result = std::move(next);
	}
	return result;
}

// ── BatchExpander::expand ─────────────────────────────────────────────────────

std::vector<BatchRunSpec> BatchExpander::expand(
	const BatchDocument&      doc,
	std::vector<std::string>& warnings_out)
{
	// Collect only static axes; warn about stochastic/formation
	std::vector<BatchAxisEntry> static_axes;
	for (const auto& ax : doc.axes) {
		if (ax.kind == "static") {
			static_axes.push_back(ax);
		} else {
			warnings_out.push_back(
				"[[batch.axis]] '" + ax.name + "' kind='" + ax.kind +
				"' skipped: not wired until v5.1.0/v5.2.0");
		}
	}

	// Factorial combinations across static axes
	auto combos = cartesian_product(static_axes);

	// Seed resolution
	SeedSection resolved_seed = SeedResolver::resolve(doc.seed);
	const std::string& policy = doc.design.seed_policy;

	const int reps = (doc.design.replicates_per_case > 0)
					 ? doc.design.replicates_per_case : 1;

	const std::string study_name = doc.study.name;

	std::vector<BatchRunSpec> specs;
	int run_index = 1;

	// If [batch.expand] cases = true, cross with [[batch.case]] entries
	// Otherwise iterate combos directly.
	bool use_cases = doc.expand.populated && doc.expand.cases && !doc.cases.empty();

	auto make_specs = [&](const std::map<std::string, std::string>& combo,
						  const std::string& case_name,
						  const std::map<std::string, std::string>& case_overrides)
	{
		for (int r = 0; r < reps; ++r) {
			BatchRunSpec spec;
			std::ostringstream id_ss;
			id_ss << "run_" << std::setw(4) << std::setfill('0') << run_index;
			spec.run_id    = id_ss.str();
			spec.run_index = run_index;
			spec.replicate = r;
			spec.case_name = case_name;

			// Merge: case_overrides first, then axis_values win on conflict
			for (auto& [k, v] : case_overrides) {
				if (k.rfind("__axis_", 0) != 0) // exclude internal markers
					spec.axis_values[k] = v;
			}
			for (auto& [k, v] : combo) {
				if (k.rfind("__axis_", 0) == 0) continue;
				spec.axis_values[k] = v; // axis wins
			}

			// Seeds per replicate
			spec.seed_foundation = SeedResolver::replicate_seed(resolved_seed.foundation, r, policy);
			spec.seed_defect     = SeedResolver::replicate_seed(resolved_seed.resolved_defect, r, policy);
			spec.seed_formation  = SeedResolver::replicate_seed(resolved_seed.resolved_formation, r, policy);
			spec.seed_thermal    = SeedResolver::replicate_seed(resolved_seed.resolved_thermal, r, policy);
			spec.seed_placement  = SeedResolver::replicate_seed(resolved_seed.resolved_placement, r, policy);

			spec.output_dir = study_name + "/cases/" + spec.run_id;
			specs.push_back(std::move(spec));
			++run_index;
		}
	};

	if (use_cases) {
		for (const auto& c : doc.cases) {
			for (const auto& combo : combos) {
				make_specs(combo, c.name, c.overrides);
			}
		}
	} else {
		for (const auto& combo : combos) {
			make_specs(combo, "", {});
		}
	}

	return specs;
}

// ── BatchExpander::write_batch_plan ──────────────────────────────────────────

void BatchExpander::write_batch_plan(const std::vector<BatchRunSpec>& specs,
									 const BatchDocument&             doc,
									 const std::string&               path)
{
	std::ofstream f(path);
	if (!f.is_open())
		throw std::runtime_error("BatchExpander: cannot write batch_plan.tsv: " + path);

	// Header
	f << "run_id\tcase_name\treplicate";
	for (const auto& ax : doc.axes) {
		if (ax.kind == "static") f << "\t" << ax.name;
	}
	f << "\tseed_foundation\tseed_defect\tseed_formation\tseed_thermal\tseed_placement\n";

	for (const auto& spec : specs) {
		f << spec.run_id << "\t" << spec.case_name << "\t" << spec.replicate;
		for (const auto& ax : doc.axes) {
			if (ax.kind == "static") {
				auto it = spec.axis_values.find(ax.target);
				f << "\t" << (it != spec.axis_values.end() ? it->second : "");
			}
		}
		f << "\t" << spec.seed_foundation
		  << "\t" << spec.seed_defect
		  << "\t" << spec.seed_formation
		  << "\t" << spec.seed_thermal
		  << "\t" << spec.seed_placement
		  << "\n";
	}
}

} // namespace batch
} // namespace vsim
