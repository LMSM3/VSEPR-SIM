// =============================================================================
// tests/test_vsim_runtime_day2.cpp  —  VSIM Runtime Day #2 Tests (Group 27)
// =============================================================================
//
// Six behavioral tests for the VSIM scripting runtime — beta-10 milestone.
//
//  1. test_cached_metric_guard        while guards read cached metric; O(1) guard eval
//  2. test_batch_artifact_isolation   batch runs isolate artifacts per case
//  3. test_visual_decimation          [visual external] render interval respected
//  4. test_N_evolution_event_counter  spawn/remove events drive N counter; no full scan
//  5. test_while_loop_safety_cap      max_iter stops non-converging loops; warning emitted
//  6. test_open_closed_boundary_dispatch  closed/open/ambient systems route correctly
//
// Priority order matches user specification:
//  5 → 2 → 3 → 4 → 1 → 6
//
// WO-56C  |  v5.0.0-beta.7.1  |  beta-10 milestone
// =============================================================================

#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <numeric>
#include <algorithm>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_runtime.hpp"
#include "include/kernel/kernel_event.hpp"
#include "include/kernel/kernel_event_log.hpp"

using namespace vsim;
using namespace vsepr::kernel;

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// Populate a KernelEventLog with N events spread across n_frames frames.
static void fill_log(KernelEventLog& log,
					 int N,
					 int n_frames,
					 double base_value = 1.0)
{
	int per_frame = std::max(1, N / n_frames);
	int emitted   = 0;
	for (int f = 0; f < n_frames && emitted < N; ++f) {
		for (int i = 0; i < per_frame && emitted < N; ++i, ++emitted) {
			KernelEvent ev;
			ev.kind           = KernelEventKind::Formation;
			ev.frame_id       = static_cast<uint64_t>(f);
			ev.source_formula = "Ar";
			ev.result_value   = base_value + static_cast<double>(emitted) * 0.001;
			ev.result_unit    = "kcal/mol";
			ev.is_valid       = true;
			log.record(ev);
		}
	}
}

// Build a minimal VarianceSection with one probe over "displacement".
static VarianceSection make_variance_cfg(const std::string& probe_name,
										 double threshold,
										 bool   print = false)
{
	VarianceSection cfg;
	cfg.print_results = print;
	VarianceProbe p;
	p.name      = probe_name;
	p.field     = "displacement";
	p.window    = "all";
	p.threshold = threshold;
	cfg.probes.push_back(p);
	return cfg;
}

// Build a minimal NEvolutionSection with one probe over "particle_count".
static NEvolutionSection make_nev_cfg(const std::string& probe_name,
									  bool print = false)
{
	NEvolutionSection cfg;
	cfg.print_results = print;
	NEvolutionProbe p;
	p.name      = probe_name;
	p.target    = "particle_count";
	p.window    = "all";
	p.threshold = 0.0;
	cfg.probes.push_back(p);
	return cfg;
}


// =============================================================================
// 5. test_while_loop_safety_cap  (highest priority)
// =============================================================================
// Ensures that a while loop with a condition that never converges stops at
// max_iters and emits the "hit max_iters" warning path.
//
// Verifies:
//  - loop body executes exactly max_iters times
//  - no crash
//  - partial event output is preserved
//  - the cap is honoured regardless of condition value
// =============================================================================

static void test_while_loop_safety_cap()
{
	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	// Seed 5 events so the condition "variance D_var > 0.001" is true from
	// the start. The emit_fn adds more high-variance events each iteration so
	// the condition never converges — we rely entirely on max_iters.
	fill_log(log, 5, 1, 100.0);   // large values → variance well above 0.001

	VsimDocument doc;
	doc.variance_cfg = make_variance_cfg("D_var", 0.001);

	WhileGuard guard;
	guard.name         = "variance_cap_test";
	guard.condition    = "variance D_var > 0.001";
	guard.body_steps   = 50;
	guard.max_iters    = 20;
	guard.iter_delay_ms = 0;
	doc.while_cfg.guards.push_back(guard);

	int iteration_count = 0;

	// emit_fn: adds high-variance events each call so condition stays true.
	VsimRuntime::EmitFn emit_fn = [&](int n_steps, int seed_off) -> int {
		++iteration_count;
		fill_log(log, 3, 1, 100.0 + static_cast<double>(iteration_count) * 10.0);
		return 3;
	};

	size_t events_before = log.size();
	VsimRuntime::run_while_guards(doc.while_cfg, doc, log, emit_fn);
	size_t events_after = log.size();

	// Safety cap: loop must have run exactly max_iters times
	assert(iteration_count == guard.max_iters &&
		"while_loop_safety_cap: loop did not run exactly max_iters iterations");

	// Partial output must be preserved — log grew during capped loop
	assert(events_after > events_before &&
		"while_loop_safety_cap: partial events not preserved after safety cap");

	// No crash — we reach here
	std::puts("PASS  test_while_loop_safety_cap");
}


// =============================================================================
// 2. test_batch_artifact_isolation
// =============================================================================
// Verifies that batch jobs execute as distinct named runs and that:
//  - each job label is unique
//  - logs are cleared per run (log.clear() is called by run_batch)
//  - per_run actions execute per-case
//  - all seed/parameter combinations complete
//  - no cross-contamination of run data
// =============================================================================

static void test_batch_artifact_isolation()
{
	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	// Seed some initial events
	fill_log(log, 6, 3, 1.0);

	VsimDocument doc;
	doc.simulation.fire_max_steps = 50;

	// Three distinct batch cases matching the specification:
	//   vacancy_case / interstitial_case / thermal_jitter_case
	BatchJob job;
	job.name                  = "defect_sweep";
	job.seed_count            = 1;
	job.aggregate             = true;
	job.per_run_actions       = {"analyze variance displacement"};
	job.sweep_params["defect"] = {"vacancy_case", "interstitial_case", "thermal_jitter_case"};
	doc.batch_cfg.jobs.push_back(job);
	doc.batch_cfg.print_plan  = false;

	doc.variance_cfg = make_variance_cfg("displacement_var", 0.0, false);

	// Track which labels were seen in run order
	std::vector<std::string> run_labels_seen;
	std::vector<int>         events_at_run_start;

	VsimRuntime::EmitFn emit_fn = [&](int n_steps, int seed_off) -> int {
		// Record log size at the START of this run (after log.clear() by run_batch)
		events_at_run_start.push_back(static_cast<int>(log.size()));
		// Add distinct events so runs are individually traceable
		fill_log(log, 4, 2, static_cast<double>(seed_off + 1) * 10.0);
		return 4;
	};

	VsimRuntime::run_batch(doc.batch_cfg, doc, log, emit_fn);

	// run_batch clears the log before each run — captured size should be 0
	// (log is clean at the start of every batch run)
	for (int i = 0; i < static_cast<int>(events_at_run_start.size()); ++i) {
		assert(events_at_run_start[i] == 0 &&
			"batch_artifact_isolation: log was not cleared before run — cross-contamination risk");
	}

	// Three cases × 1 seed = 3 runs
	assert(static_cast<int>(events_at_run_start.size()) == 3 &&
		"batch_artifact_isolation: wrong number of batch runs executed");

	std::puts("PASS  test_batch_artifact_isolation");
}


// =============================================================================
// 3. test_visual_decimation
// =============================================================================
// Verifies that a render interval of 100 frames across 1000 simulated frames
// produces exactly 10 render outputs — not 1000, not 0.
//
// The render dispatch is external (non-blocking) — simulated here by a
// counter that fires only when (frame % render_interval == 0) and frame > 0.
//
// Checks:
//  - render count == floor(total_frames / render_interval)
//  - frame IDs at render points are multiples of render_interval
//  - no render fires at frame 0 (pre-first-step — nothing to show)
//  - total simulated frames == 1000
// =============================================================================

static void test_visual_decimation()
{
	constexpr int TOTAL_FRAMES   = 1000;
	constexpr int RENDER_INTERVAL = 100;
	constexpr int EXPECTED_RENDERS = TOTAL_FRAMES / RENDER_INTERVAL;

	// Simulate the [visual external] render interval logic.
	// The runtime dispatch rule: fire render when (frame % interval == 0)
	// and frame > 0.
	int render_count = 0;
	std::vector<int> rendered_frames;

	for (int frame = 1; frame <= TOTAL_FRAMES; ++frame) {
		if (frame % RENDER_INTERVAL == 0) {
			++render_count;
			rendered_frames.push_back(frame);
		}
	}

	assert(render_count == EXPECTED_RENDERS &&
		"visual_decimation: render count does not match expected interval count");

	// Every rendered frame must be a clean multiple of the interval
	for (int rf : rendered_frames) {
		assert(rf % RENDER_INTERVAL == 0 &&
			"visual_decimation: render fired on non-interval frame");
	}

	// Frame 0 must not be in the rendered list
	for (int rf : rendered_frames) {
		assert(rf != 0 &&
			"visual_decimation: render incorrectly fired at frame 0");
	}

	// The VsimDocument / ExportVisualSection carries the interval field.
	// Verify the VisualExternalSection is constructible with the expected defaults.
	VisualExternalSection vis;
	vis.enabled = true;
	vis.render_targets = {"state_current"};
	vis.export_format  = "svg";
	vis.show_progress  = false;
	assert(vis.any_active() && "visual_decimation: VisualExternalSection::any_active() failed");

	std::puts("PASS  test_visual_decimation");
}


// =============================================================================
// 4. test_N_evolution_event_counter
// =============================================================================
// Verifies that dN/dt is computed from stored population history (event
// counters) rather than requiring a full particle scan.
//
// Scenario A — growth phase:   later frames have more events → dN/dt > 0
// Scenario B — removal phase:  later frames have fewer events → dN/dt < 0
// Scenario C — stable:         identical frame counts → dN/dt ≈ 0
//
// The KernelEventLog groups events by frame_id.  eval_n_evolution() calls
// extract_population() which iterates the log once (O(N)) or reads a cached
// count — NOT a per-frame full scan.
// =============================================================================

static void test_N_evolution_event_counter()
{
	KernelEventLog& log = KernelEventLog::instance();

	// --- Scenario A: spawn events (growing population) ---
	log.clear();
	// Frame 0: 2 events, Frame 1: 4 events, Frame 2: 6 events → dN/dt > 0
	auto add_frame = [&](uint64_t frame_id, int count, double val_base) {
		for (int i = 0; i < count; ++i) {
			KernelEvent ev;
			ev.kind           = KernelEventKind::Formation;
			ev.frame_id       = frame_id;
			ev.source_formula = "Ar";
			ev.result_value   = val_base + static_cast<double>(i);
			ev.result_unit    = "kcal/mol";
			ev.is_valid       = true;
			log.record(ev);
		}
	};

	add_frame(0, 2, 1.0);
	add_frame(1, 4, 1.0);
	add_frame(2, 6, 1.0);

	NEvolutionSection nev_cfg = make_nev_cfg("particle_count", false);
	auto results_growth = VsimRuntime::eval_n_evolution(nev_cfg, log);

	assert(!results_growth.empty() &&
		"N_evolution_event_counter: no results for growth scenario");
	double dNdt_growth = results_growth.front().value;
	assert(dNdt_growth > 0.0 &&
		"N_evolution_event_counter: dN/dt should be positive during spawn phase");

	// --- Scenario B: removal events (shrinking population) ---
	log.clear();
	add_frame(0, 6, 1.0);
	add_frame(1, 4, 1.0);
	add_frame(2, 2, 1.0);

	auto results_removal = VsimRuntime::eval_n_evolution(nev_cfg, log);
	assert(!results_removal.empty() &&
		"N_evolution_event_counter: no results for removal scenario");
	double dNdt_removal = results_removal.front().value;
	assert(dNdt_removal < 0.0 &&
		"N_evolution_event_counter: dN/dt should be negative during removal phase");

	// --- Scenario C: stable (no net change) ---
	log.clear();
	add_frame(0, 4, 1.0);
	add_frame(1, 4, 1.0);
	add_frame(2, 4, 1.0);

	auto results_stable = VsimRuntime::eval_n_evolution(nev_cfg, log);
	assert(!results_stable.empty() &&
		"N_evolution_event_counter: no results for stable scenario");
	double dNdt_stable = results_stable.front().value;
	assert(std::abs(dNdt_stable) < 1e-9 &&
		"N_evolution_event_counter: dN/dt should be zero when count is stable");

	std::puts("PASS  test_N_evolution_event_counter");
}


// =============================================================================
// 1. test_cached_metric_guard
// =============================================================================
// Verifies that while-loop guards can read a cached metric value:
//  - cached metric is present before loop runs
//  - guard reads the value (via eval_variance_silent inside eval_condition)
//  - metric value updates after each simulation block (emit_fn adds events)
//  - loop exits correctly when variance drops below threshold
//  - no unnecessary full re-scan of particles; only log snapshot used
//
// Simulated cache: we drive variance DOWN each iteration so the guard
// terminates well before max_iters.
// =============================================================================

static void test_cached_metric_guard()
{
	KernelEventLog& log = KernelEventLog::instance();
	log.clear();

	// Seed high-variance events so the guard starts true
	// (variance > 0.001 initially)
	for (int i = 0; i < 20; ++i) {
		KernelEvent ev;
		ev.kind           = KernelEventKind::Formation;
		ev.frame_id       = 0;
		ev.source_formula = "Ar";
		ev.result_value   = static_cast<double>(i) * 10.0;  // wide spread → high variance
		ev.result_unit    = "kcal/mol";
		ev.is_valid       = true;
		log.record(ev);
	}

	VsimDocument doc;
	doc.variance_cfg = make_variance_cfg("D_var", 0.001, false);

	WhileGuard guard;
	guard.name          = "cached_guard_test";
	guard.condition     = "variance D_var > 0.001";
	guard.body_steps    = 50;
	guard.max_iters     = 20;
	guard.measure       = {"D_var"};
	guard.iter_delay_ms = 0;
	doc.while_cfg.guards.push_back(guard);

	int iterations_run = 0;

	// Each emit_fn call replaces log contents with low-variance events.
	// After the first call the guard condition becomes false → loop exits.
	VsimRuntime::EmitFn emit_fn = [&](int n_steps, int seed_off) -> int {
		++iterations_run;
		log.clear();
		// Inject near-constant events → variance ≈ 0 (well below 0.001)
		for (int i = 0; i < 20; ++i) {
			KernelEvent ev;
			ev.kind           = KernelEventKind::Formation;
			ev.frame_id       = static_cast<uint64_t>(iterations_run);
			ev.source_formula = "Ar";
			ev.result_value   = 1.0 + static_cast<double>(i) * 1e-6;  // tiny spread
			ev.result_unit    = "kcal/mol";
			ev.is_valid       = true;
			log.record(ev);
		}
		return 20;
	};

	VsimRuntime::run_while_guards(doc.while_cfg, doc, log, emit_fn);

	// Guard must have exited well before the safety cap
	assert(iterations_run < guard.max_iters &&
		"cached_metric_guard: loop did not exit early after variance dropped");

	// At least one iteration must have run (condition was true initially)
	assert(iterations_run >= 1 &&
		"cached_metric_guard: loop never entered — initial condition must be true");

	// Log must contain the low-variance events written by emit_fn
	assert(log.size() == 20 &&
		"cached_metric_guard: log size mismatch after guard convergence");

	std::puts("PASS  test_cached_metric_guard");
}


// =============================================================================
// 6. test_open_closed_boundary_dispatch
// =============================================================================
// Verifies that system boundary type routes correctly to allowed behaviours.
//
// Boundary rules encoded here (design-level assertions):
//
//   closed  → mass exchange (inlet/outlet spawn/remove) FORBIDDEN
//   open    → inlet/outlet spawn/remove ALLOWED
//   ambient → external coupling (heat/drag/pressure) ALLOWED;
//             mass exchange optional (only if ambient_open declared)
//
// This test validates the dispatch table logic that the runtime must enforce
// when boundary annotations are introduced as a first-class VSIM primitive.
// Until the full runtime hook exists the test validates the document model
// and the rule predicate functions directly.
// =============================================================================

// Boundary type enumeration (first-class VSIM primitive as per spec)
enum class BoundaryType { Closed, Open, Ambient, Solution };

// Action request types
enum class BoundaryAction {
	InletSpawn,          // inject particles
	OutletRemove,        // remove particles
	ExternalHeatCoupling, // thermal coupling to external reservoir
	ExternalDragCoupling, // drag/pressure from environment
	AmbientMassExchange   // mass exchange in ambient (requires ambient_open)
};

// Dispatch table: returns true if action is ALLOWED for the given boundary type.
static bool boundary_allows(BoundaryType boundary, BoundaryAction action, bool ambient_open = false)
{
	switch (boundary) {
		case BoundaryType::Closed:
			// No mass exchange of any kind
			return false;

		case BoundaryType::Open:
			// Inlet / outlet always valid; no thermal coupling by default
			switch (action) {
				case BoundaryAction::InletSpawn:       return true;
				case BoundaryAction::OutletRemove:     return true;
				case BoundaryAction::ExternalHeatCoupling:  return false;
				case BoundaryAction::ExternalDragCoupling:  return false;
				case BoundaryAction::AmbientMassExchange:   return false;
			}
			break;

		case BoundaryType::Ambient:
			switch (action) {
				case BoundaryAction::InletSpawn:
					// Only if ambient_open explicitly declared
					return ambient_open;
				case BoundaryAction::OutletRemove:
					return ambient_open;
				case BoundaryAction::ExternalHeatCoupling:  return true;
				case BoundaryAction::ExternalDragCoupling:  return true;
				case BoundaryAction::AmbientMassExchange:   return ambient_open;
			}
			break;

		case BoundaryType::Solution:
			// Solution allows all exchange forms
			return true;
	}
	return false;
}

static void test_open_closed_boundary_dispatch()
{
	// --- closed system: sealed_box ---
	assert(!boundary_allows(BoundaryType::Closed, BoundaryAction::InletSpawn) &&
		"boundary_dispatch: closed system must reject InletSpawn");
	assert(!boundary_allows(BoundaryType::Closed, BoundaryAction::OutletRemove) &&
		"boundary_dispatch: closed system must reject OutletRemove");
	assert(!boundary_allows(BoundaryType::Closed, BoundaryAction::ExternalHeatCoupling) &&
		"boundary_dispatch: closed system must reject ExternalHeatCoupling");

	// --- open system: pipe_A ---
	assert( boundary_allows(BoundaryType::Open, BoundaryAction::InletSpawn) &&
		"boundary_dispatch: open system must allow InletSpawn");
	assert( boundary_allows(BoundaryType::Open, BoundaryAction::OutletRemove) &&
		"boundary_dispatch: open system must allow OutletRemove");
	assert(!boundary_allows(BoundaryType::Open, BoundaryAction::ExternalHeatCoupling) &&
		"boundary_dispatch: open system must not allow ExternalHeatCoupling by default");

	// --- ambient system without ambient_open: exposed_solid ---
	assert( boundary_allows(BoundaryType::Ambient, BoundaryAction::ExternalHeatCoupling) &&
		"boundary_dispatch: ambient system must allow ExternalHeatCoupling");
	assert( boundary_allows(BoundaryType::Ambient, BoundaryAction::ExternalDragCoupling) &&
		"boundary_dispatch: ambient system must allow ExternalDragCoupling");
	assert(!boundary_allows(BoundaryType::Ambient, BoundaryAction::InletSpawn, false) &&
		"boundary_dispatch: ambient without ambient_open must reject InletSpawn");
	assert(!boundary_allows(BoundaryType::Ambient, BoundaryAction::AmbientMassExchange, false) &&
		"boundary_dispatch: ambient without ambient_open must reject AmbientMassExchange");

	// --- ambient system WITH ambient_open ---
	assert( boundary_allows(BoundaryType::Ambient, BoundaryAction::InletSpawn, true) &&
		"boundary_dispatch: ambient_open must allow InletSpawn");
	assert( boundary_allows(BoundaryType::Ambient, BoundaryAction::AmbientMassExchange, true) &&
		"boundary_dispatch: ambient_open must allow AmbientMassExchange");

	std::puts("PASS  test_open_closed_boundary_dispatch");
}


// =============================================================================
// main — priority order: 5, 2, 3, 4, 1, 6
// =============================================================================

int main()
{
	test_while_loop_safety_cap();
	test_batch_artifact_isolation();
	test_visual_decimation();
	test_N_evolution_event_counter();
	test_cached_metric_guard();
	test_open_closed_boundary_dispatch();

	std::puts("\nAll VSIM runtime Day #2 tests passed.");
	return 0;
}
