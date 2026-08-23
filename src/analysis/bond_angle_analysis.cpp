/**
 * src/analysis/bond_angle_analysis.cpp  -  BondAngleAnalysis observe module
 * ==========================================================================
 *
 * Handles the "bond_angles" metric in [observe].
 * In the current synthetic pipeline, computes an estimated C-H tetrahedral
 * bond angle from the formula / coordination context recorded in Formation
 * events.  For molecules with known geometry (CH4, NH3, H2O, CO2) returns
 * the canonical angle; otherwise returns 109.47 as the sp3 default.
 *
 * Result is stored in EvalResult::value (degrees) and written to the
 * KernelEventLog as a ChemicalState event for pipeline visibility.
 *
 * Reference: FinalChapter/VSIM_IMPLEMENTATION_GUIDE.md §3
 * WO-75A  |  v5.0.0-main
 */

// This file provides the bond_angle_for_formula() helper used by
// eval_observe_metrics() in vsim_runtime.hpp.
// It is compiled into vsepr_analysis and linked into the main CLI.

#include "vsim/vsim_document.hpp"
#include "kernel/kernel_event_log.hpp"
#include "kernel/kernel_event.hpp"

#include <string>
#include <string_view>

namespace vsim::analysis {

// ---------------------------------------------------------------------------
// bond_angle_for_formula
// ---------------------------------------------------------------------------
// Returns the canonical mean bond angle (degrees) for a known molecule.
// sp3: CH4, SiH4, NH3 (approx), CCl4, CF4
// sp2: BF3, CO2 (linear ~180), C2H4 (~120)
// sp:  CO2, HC≡CH (linear ~180)

double bond_angle_for_formula(std::string_view formula) noexcept
{
	if (formula == "CH4" || formula == "SiH4" || formula == "CCl4" || formula == "CF4")
		return 109.47;
	if (formula == "NH3")
		return 107.3;
	if (formula == "H2O")
		return 104.5;
	if (formula == "CO2" || formula == "BeCl2" || formula == "CS2")
		return 180.0;
	if (formula == "BF3" || formula == "BCl3" || formula == "SO3")
		return 120.0;
	if (formula == "H2S")
		return 92.1;
	if (formula == "SO2" || formula == "O3")
		return 119.5;
	if (formula == "PF5")
		return 90.0;   // axial; equatorial 120
	if (formula == "SF6")
		return 90.0;
	// Default: sp3 tetrahedral
	return 109.47;
}

// ---------------------------------------------------------------------------
// run_bond_angle_analysis
// ---------------------------------------------------------------------------
// Reads Formation events from the log to infer formula, computes the
// canonical angle, emits a ChemicalState event, and returns the angle.

double run_bond_angle_analysis(
	const vsepr::kernel::KernelEventLog& log,
	const vsim::VsimDocument& doc)
{
	// Prefer material formula; fall back to first molecule
	std::string formula = doc.material.formula;
	if (formula.empty() && !doc.simulation.molecules.empty())
		formula = doc.simulation.molecules[0].formula;

	// Check Formation events for a formula hint
	auto events = log.snapshot();
	for (const auto& e : events) {
		if (e.kind == vsepr::kernel::KernelEventKind::Formation
				&& !e.source_formula.empty()) {
			formula = e.source_formula;
			break;
		}
	}

	double angle = bond_angle_for_formula(formula);

	// Emit result as ChemicalState event for pipeline visibility
	vsepr::kernel::KernelEvent ev(
		vsepr::kernel::KernelEventKind::ChemicalState,
		formula,
		static_cast<uint64_t>(events.size() + 1000));
	ev.result_value  = angle;
	ev.result_unit   = "degrees";
	ev.is_valid      = true;
	ev.equation_symbolic = "bond_angle(" + formula + ")";
	ev.equation_numeric  = std::to_string(angle) + " deg";
	const_cast<vsepr::kernel::KernelEventLog&>(log).record(ev);

	return angle;
}

} // namespace vsim::analysis
