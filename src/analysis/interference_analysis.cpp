/**
 * src/analysis/interference_analysis.cpp  -  InterferenceAnalysis
 * ================================================================
 *
 * Handles the "interference" metric in [observe].
 *
 * Computes a synthetic wave-interference quality score for the current
 * formation.  In the synthetic pipeline this is derived from the
 * variance of Formation event energies — high variance indicates
 * destructive interference patterns; low variance indicates constructive.
 *
 * Result:
 *   EvalResult::value   - interference_score in [0.0, 1.0]
 *                         (0 = fully destructive, 1 = fully constructive)
 *   EvalResult::window  - "ensemble"
 *
 * Reference: FinalChapter/VSIM_IMPLEMENTATION_GUIDE.md §3
 * WO-75A  |  v5.0.0-main
 */

#include "vsim/vsim_document.hpp"
#include "kernel/kernel_event_log.hpp"
#include "kernel/kernel_event.hpp"

#include <cmath>
#include <numeric>
#include <string>
#include <vector>

namespace vsim::analysis {

// ---------------------------------------------------------------------------
// run_interference_analysis
// ---------------------------------------------------------------------------

double run_interference_analysis(
	const vsepr::kernel::KernelEventLog& log,
	const vsim::VsimDocument& doc)
{
	auto events = log.snapshot();

	// Collect Formation event energies
	std::vector<double> energies;
	for (const auto& e : events) {
		if (e.kind == vsepr::kernel::KernelEventKind::Formation)
			energies.push_back(e.result_value);
	}

	double score = 1.0;  // default: fully constructive (no events = no interference)

	if (energies.size() >= 2) {
		double mean = std::accumulate(energies.begin(), energies.end(), 0.0)
					  / static_cast<double>(energies.size());
		double variance = 0.0;
		for (double v : energies) {
			double d = v - mean;
			variance += d * d;
		}
		variance /= static_cast<double>(energies.size());

		// Score: e^(-variance / scale).  Scale calibrated so typical
		// Formation variance (~500 (kcal/mol)^2) gives score ~0.6.
		double scale = 800.0;
		score = std::exp(-variance / scale);
		score = std::max(0.0, std::min(1.0, score));
	}

	// Formula hint
	std::string formula = doc.material.formula;
	if (formula.empty() && !doc.simulation.molecules.empty())
		formula = doc.simulation.molecules[0].formula;

	// Emit ChemicalState event
	vsepr::kernel::KernelEvent ev(
		vsepr::kernel::KernelEventKind::ChemicalState,
		formula.empty() ? "VSIM" : formula,
		static_cast<uint64_t>(events.size() + 3000));
	ev.result_value  = score;
	ev.result_unit   = "dimensionless";
	ev.is_valid      = true;
	ev.equation_symbolic = "interference_score";
	ev.equation_numeric  = std::to_string(score);
	const_cast<vsepr::kernel::KernelEventLog&>(log).record(ev);

	return score;
}

} // namespace vsim::analysis
