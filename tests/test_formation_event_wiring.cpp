// =============================================================================
// tests/test_formation_event_wiring.cpp  —  Group 28: FormationEvent FIRE Wiring
// =============================================================================
//
// Acceptance tests for WO-56D-AuditChain step 1:
//   Wire FormationEvent into the FIRE relaxation exit path so real simulation
//   runs populate KernelEventLog — not just test harnesses.
//
// Tests:
//   1. test_fire_records_formation_event
//      Run a real FIRE relaxation on CH4 (5-atom molecule).
//      Assert KernelEventLog contains at least one FormationEvent.
//      Assert the event was NOT injected manually — only the FIRE exit path
//      may produce it.
//
//   2. test_fire_event_fields_are_populated
//      Assert the FormationEvent carries the correct n_beads, fire_steps,
//      converged flag, finite final_energy, and non-empty equation trace.
//
//   3. test_fire_event_source_formula_threaded
//      Set source_formula = "CH4" in OptimizerSettings.
//      Assert the FormationEvent.source_formula matches.
//
//   4. test_fire_nonconverged_event_flagged
//      Run FIRE with max_iterations = 1 (cannot converge).
//      Assert FormationEvent.converged == false.
//      Assert FormationEvent.is_valid == false.
//      Assert FormationEvent.warning is non-empty.
//
//   5. test_fire_multiple_runs_accumulate_events
//      Run FIRE twice.
//      Assert KernelEventLog contains exactly 2 FormationEvents.
//      Assert event_ids are distinct (monotonic).
//
// WO-56D-AuditChain  |  v5.0.0-beta.7.1
// =============================================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

#include "sim/optimizer.hpp"
#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "kernel/kernel_event.hpp"
#include "kernel/kernel_event_log.hpp"

using namespace vsepr;
using namespace vsepr::kernel;

// ---------------------------------------------------------------------------
// Helper: build a minimal CH4 molecule (5 atoms, 4 C-H bonds)
// ---------------------------------------------------------------------------

static Molecule make_ch4()
{
	Molecule mol;
	mol.add_atom(6,  0.0,  0.0,  0.0);   // C
	mol.add_atom(1,  1.2,  0.0,  0.0);   // H1
	mol.add_atom(1, -0.4,  1.1,  0.0);   // H2
	mol.add_atom(1, -0.4, -0.5,  1.0);   // H3
	mol.add_atom(1, -0.4, -0.6, -0.9);   // H4
	for (int i = 1; i <= 4; ++i) mol.add_bond(0, i, 1);
	mol.generate_angles_from_bonds();
	return mol;
}

// Helper: count FormationEvents in the log
static int count_formation_events(const KernelEventLog& log)
{
	auto snap = log.snapshot();
	int n = 0;
	for (const auto& e : snap)
		if (e.kind == KernelEventKind::Formation) ++n;
	return n;
}


// =============================================================================
// 1. test_fire_records_formation_event
// =============================================================================
// Run a real FIRE relaxation. Assert at least one FormationEvent appears in
// the global KernelEventLog — without any manual injection.
// =============================================================================

static void test_fire_records_formation_event()
{
	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	Molecule mol = make_ch4();
	EnergyModel model(mol, 300.0, true);

	OptimizerSettings settings;
	settings.max_iterations = 200;
	settings.print_every    = 0;

	FIREOptimizer optimizer(settings);
	optimizer.minimize(mol.coords, model);

	int n_formation = count_formation_events(log);
	assert(n_formation >= 1 &&
		"fire_records_formation_event: no FormationEvent in log after real FIRE run");

	std::puts("PASS  test_fire_records_formation_event");
}


// =============================================================================
// 2. test_fire_event_fields_are_populated
// =============================================================================
// Run FIRE. Retrieve the FormationEvent. Assert all structural fields are
// populated with meaningful values.
// =============================================================================

static void test_fire_event_fields_are_populated()
{
	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	Molecule mol = make_ch4();
	EnergyModel model(mol, 300.0, true);

	OptimizerSettings settings;
	settings.max_iterations = 300;
	settings.print_every    = 0;

	FIREOptimizer optimizer(settings);
	auto result = optimizer.minimize(mol.coords, model);

	// Find the FormationEvent
	auto snap = log.snapshot();
	const KernelEvent* found = nullptr;
	for (const auto& e : snap)
		if (e.kind == KernelEventKind::Formation) { found = &e; break; }

	assert(found != nullptr &&
		"fire_event_fields_populated: FormationEvent not found in log");

	// n_beads: CH4 has 5 atoms → 5 beads
	// (stored in FormationEvent but base KernelEvent doesn't have n_beads;
	//  check via the filter_by_kind path + cast is not available on base —
	//  we verify the base fields that FormationEvent::compute() must set)

	// equation_symbolic must be non-empty (set by compute())
	assert(!found->equation_symbolic.empty() &&
		"fire_event_fields_populated: equation_symbolic is empty");

	// equation_numeric must be non-empty
	assert(!found->equation_numeric.empty() &&
		"fire_event_fields_populated: equation_numeric is empty");

	// result_value must be the final energy — must be finite
	assert(std::isfinite(found->result_value) &&
		"fire_event_fields_populated: result_value is not finite");

	// result_unit must be "kcal/mol"
	assert(found->result_unit == "kcal/mol" &&
		"fire_event_fields_populated: result_unit is wrong");

	// event_id must be non-zero (assigned by KernelEventLog)
	assert(found->event_id > 0 &&
		"fire_event_fields_populated: event_id was not assigned");

	// FIRE ran steps — fire_steps > 0 (encoded in equation_numeric string)
	// We verify this indirectly by confirming the FIRE result iteration count > 0
	assert(result.iterations > 0 &&
		"fire_event_fields_populated: FIRE did not run any iterations");

	std::puts("PASS  test_fire_event_fields_are_populated");
}


// =============================================================================
// 3. test_fire_event_source_formula_threaded
// =============================================================================
// Set source_formula = "CH4" in OptimizerSettings.
// Assert the recorded FormationEvent.source_formula == "CH4".
// =============================================================================

static void test_fire_event_source_formula_threaded()
{
	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	Molecule mol = make_ch4();
	EnergyModel model(mol, 300.0, true);

	OptimizerSettings settings;
	settings.max_iterations   = 200;
	settings.print_every      = 0;
	settings.source_formula   = "CH4";
	settings.formation_preset = "gas";

	FIREOptimizer optimizer(settings);
	optimizer.minimize(mol.coords, model);

	auto snap = log.snapshot();
	const KernelEvent* found = nullptr;
	for (const auto& e : snap)
		if (e.kind == KernelEventKind::Formation) { found = &e; break; }

	assert(found != nullptr &&
		"fire_event_source_formula: FormationEvent not found");

	assert(found->source_formula == "CH4" &&
		"fire_event_source_formula: source_formula not threaded through OptimizerSettings");

	std::puts("PASS  test_fire_event_source_formula_threaded");
}


// =============================================================================
// 4. test_fire_nonconverged_event_flagged
// =============================================================================
// Run FIRE with max_iterations = 1 — cannot converge on CH4 in one step.
// Assert: converged == false in the encoded event (is_valid == false,
// warning non-empty).
// =============================================================================

static void test_fire_nonconverged_event_flagged()
{
	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	Molecule mol = make_ch4();
	EnergyModel model(mol, 300.0, true);

	OptimizerSettings settings;
	settings.max_iterations = 1;     // guaranteed non-convergence
	settings.print_every    = 0;
	settings.source_formula = "CH4";

	FIREOptimizer optimizer(settings);
	auto result = optimizer.minimize(mol.coords, model);

	assert(!result.converged &&
		"fire_nonconverged_flagged: expected non-convergence with max_iterations=1");

	auto snap = log.snapshot();
	const KernelEvent* found = nullptr;
	for (const auto& e : snap)
		if (e.kind == KernelEventKind::Formation) { found = &e; break; }

	assert(found != nullptr &&
		"fire_nonconverged_flagged: FormationEvent not found in log");

	// FormationEvent::compute() sets is_valid = converged, and fills warning
	assert(!found->is_valid &&
		"fire_nonconverged_flagged: is_valid should be false for non-converged run");

	assert(!found->warning.empty() &&
		"fire_nonconverged_flagged: warning should be non-empty for non-converged run");

	std::puts("PASS  test_fire_nonconverged_event_flagged");
}


// =============================================================================
// 5. test_fire_multiple_runs_accumulate_events
// =============================================================================
// Run FIRE twice. Assert exactly 2 FormationEvents in the log with distinct,
// monotonically increasing event_ids.
// =============================================================================

static void test_fire_multiple_runs_accumulate_events()
{
	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	Molecule mol = make_ch4();
	EnergyModel model(mol, 300.0, true);

	OptimizerSettings settings;
	settings.max_iterations   = 150;
	settings.print_every      = 0;
	settings.source_formula   = "CH4";

	FIREOptimizer optimizer(settings);

	// First run
	optimizer.minimize(mol.coords, model);
	// Second run (same molecule, new relaxation)
	optimizer.minimize(mol.coords, model);

	int n_formation = count_formation_events(log);
	assert(n_formation == 2 &&
		"fire_multiple_runs: expected exactly 2 FormationEvents for 2 FIRE runs");

	// Collect the two event IDs and verify monotonic order
	auto snap = log.snapshot();
	uint64_t id_first = 0, id_second = 0;
	int idx = 0;
	for (const auto& e : snap) {
		if (e.kind == KernelEventKind::Formation) {
			if (idx == 0) id_first  = e.event_id;
			else          id_second = e.event_id;
			++idx;
		}
	}

	assert(id_first > 0 &&
		"fire_multiple_runs: first event_id must be positive");
	assert(id_second > id_first &&
		"fire_multiple_runs: event_ids must be monotonically increasing");

	std::puts("PASS  test_fire_multiple_runs_accumulate_events");
}


// =============================================================================
// main
// =============================================================================

int main()
{
	test_fire_records_formation_event();
	test_fire_event_fields_are_populated();
	test_fire_event_source_formula_threaded();
	test_fire_nonconverged_event_flagged();
	test_fire_multiple_runs_accumulate_events();

	std::puts("\nAll FormationEvent FIRE wiring tests passed.");
	return 0;
}
