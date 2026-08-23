#pragma once
/**
 * property_invariant.hpp  -  Physical invariant registry for property-based training.
 *
 * A property invariant is a physically motivated assertion that must hold for
 * every FormationResult produced by the continual runner.  Invariants are
 * evaluated after each simulation result and their pass/fail state is recorded
 * in the session ledger and rendered on the live ASCII dashboard.
 *
 * Design rules (matches VSEPR-SIM subprocess discipline):
 *   - Each invariant has one assertion goal (Single Responsibility)
 *   - Pass criterion is stated before the run, not inferred from output
 *   - All tolerances are physically justified in inline comments
 *   - No mutable global state; invariants are stateless predicates
 *
 * Usage:
 *   #include "training/property_invariant.hpp"
 *   using namespace vsepr::training;
 *
 *   auto reg = InvariantRegistry::default_registry();
 *   for (auto& inv : reg.invariants) {
 *       auto r = inv.check(result);
 *       // r.passed, r.margin, r.message
 *   }
 *
 * VSEPR-SIM v5.0.0-main | WO-VSIM-74-PROP-TRAIN
 */

#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <sstream>
#include <iomanip>

// Forward-declare FormationResult shape so this header stays self-contained.
// The actual struct is defined in apps/continual_runner.cpp; we mirror the
// fields we need here via a lightweight alias struct.
namespace vsepr {
namespace training {

// ============================================================================
// FormationSnapshot  -  lightweight mirror of continual_runner::FormationResult
// Only the fields consumed by invariants.
// ============================================================================

struct FormationSnapshot {
	std::string formula;
	uint32_t    seed          = 0;
	std::string tier;
	int         num_atoms     = 0;
	int         steps_taken   = 0;
	int         max_steps     = 0;
	double      energy        = 0.0;  // kcal/mol
	double      energy_per_atom = 0.0;
	double      rms_force     = 0.0;
	double      max_force     = 0.0;
	double      alpha_final   = 0.0;
	double      dt_final      = 0.0;
	bool        converged     = false;
	std::string classification;
	double      wall_time_ms  = 0.0;
};

// ============================================================================
// InvariantResult  -  outcome of a single invariant check
// ============================================================================

struct InvariantResult {
	std::string invariant_id;   // e.g., "INV-01"
	std::string name;
	bool        passed  = false;
	double      margin  = 0.0;  // how far from threshold (positive = safe side)
	std::string observed;       // human-readable observed value
	std::string threshold;      // human-readable threshold
	std::string message;        // diagnostic detail on failure
};

// ============================================================================
// Invariant  -  single named physical assertion
// ============================================================================

struct Invariant {
	std::string id;
	std::string name;
	std::string physical_justification;

	// The check function: pure predicate, no side effects
	std::function<InvariantResult(const FormationSnapshot&)> check;
};

// ============================================================================
// InvariantRegistry  -  ordered collection of invariants
// ============================================================================

struct InvariantRegistry {
	std::vector<Invariant> invariants;

	// Build the canonical set of VSEPR-SIM physical invariants.
	static InvariantRegistry default_registry();

	int size() const { return static_cast<int>(invariants.size()); }
};

// ============================================================================
// InvariantRegistry::default_registry()  -  implementation
// ============================================================================

inline InvariantRegistry InvariantRegistry::default_registry() {
	InvariantRegistry reg;

	// ------------------------------------------------------------------
	// INV-01: Energy finiteness
	// Physical basis: any non-finite energy signals catastrophic collapse
	// or divide-by-zero in the force kernel.  Always hard-fail.
	// ------------------------------------------------------------------
	reg.invariants.push_back({
		"INV-01", "Energy Finiteness",
		"energy must be finite (not NaN/Inf) for any physical system",
		[](const FormationSnapshot& s) -> InvariantResult {
			bool ok = std::isfinite(s.energy);
			std::ostringstream obs, thr;
			obs << std::scientific << std::setprecision(4) << s.energy << " kcal/mol";
			thr << "finite";
			double margin = ok ? 1.0 : -1.0;
			return { "INV-01", "Energy Finiteness", ok, margin,
					 obs.str(), thr.str(),
					 ok ? "" : "energy is NaN or Inf — force kernel collapse" };
		}
	});

	// ------------------------------------------------------------------
	// INV-02: Energy per atom upper bound
	// Physical basis: LJ-dominated systems have E/atom in [-50, +200]
	// kcal/mol.  Values > 1000 indicate overlapping atoms / bad geometry.
	// Threshold 1000 kcal/mol/atom matches continual_runner classify().
	// ------------------------------------------------------------------
	reg.invariants.push_back({
		"INV-02", "Energy/Atom Upper Bound",
		"E/atom < 1000 kcal/mol — values above indicate atom overlap",
		[](const FormationSnapshot& s) -> InvariantResult {
			constexpr double LIMIT = 1000.0;
			bool ok = std::isfinite(s.energy_per_atom) && s.energy_per_atom < LIMIT;
			double margin = ok ? LIMIT - s.energy_per_atom : s.energy_per_atom - LIMIT;
			std::ostringstream obs, thr;
			obs << std::fixed << std::setprecision(2) << s.energy_per_atom << " kcal/mol/atom";
			thr << "< " << LIMIT << " kcal/mol/atom";
			return { "INV-02", "Energy/Atom Upper Bound", ok, margin,
					 obs.str(), thr.str(),
					 ok ? "" : "E/atom exceeds collapse threshold" };
		}
	});

	// ------------------------------------------------------------------
	// INV-03: RMS force convergence (for converged runs)
	// Physical basis: FIRE convergence criterion is eps_force = 1e-4.
	// A converged flag with rms_force > 1e-3 is contradictory.
	// ------------------------------------------------------------------
	reg.invariants.push_back({
		"INV-03", "Converged RMS Force",
		"if converged=true then rms_force < 1e-3 (10x tolerance above FIRE eps)",
		[](const FormationSnapshot& s) -> InvariantResult {
			constexpr double LIMIT = 1e-3;
			if (!s.converged) {
				// Invariant vacuously satisfied for non-converged runs
				return { "INV-03", "Converged RMS Force", true, 1.0,
						 "n/a (not converged)", "< 1e-3 (if converged)", "" };
			}
			bool ok = s.rms_force < LIMIT;
			double margin = ok ? LIMIT - s.rms_force : s.rms_force - LIMIT;
			std::ostringstream obs, thr;
			obs << std::scientific << std::setprecision(3) << s.rms_force;
			thr << "< " << LIMIT;
			return { "INV-03", "Converged RMS Force", ok, margin,
					 obs.str(), thr.str(),
					 ok ? "" : "converged=true but rms_force exceeds 1e-3 (inconsistent state)" };
		}
	});

	// ------------------------------------------------------------------
	// INV-04: Atom count positivity
	// Physical basis: num_atoms == 0 means the formula compiler produced
	// an empty state — downstream results are meaningless.
	// ------------------------------------------------------------------
	reg.invariants.push_back({
		"INV-04", "Atom Count Positive",
		"num_atoms >= 1 — empty systems must not reach the runner",
		[](const FormationSnapshot& s) -> InvariantResult {
			bool ok = s.num_atoms >= 1;
			std::ostringstream obs;
			obs << s.num_atoms << " atoms";
			return { "INV-04", "Atom Count Positive", ok, ok ? (double)s.num_atoms : -1.0,
					 obs.str(), ">= 1",
					 ok ? "" : "num_atoms == 0 — formula produced empty state" };
		}
	});

	// ------------------------------------------------------------------
	// INV-05: Max force >= RMS force
	// Physical basis: by definition max_force >= rms_force for any
	// non-empty system.  Violation indicates a force accumulation bug.
	// ------------------------------------------------------------------
	reg.invariants.push_back({
		"INV-05", "Max Force >= RMS Force",
		"max_force >= rms_force — enforced by vector norm definition",
		[](const FormationSnapshot& s) -> InvariantResult {
			constexpr double EPS = 1e-12;
			bool ok = s.max_force + EPS >= s.rms_force;
			double margin = s.max_force - s.rms_force;
			std::ostringstream obs, thr;
			obs << std::scientific << std::setprecision(3)
				<< "max=" << s.max_force << " rms=" << s.rms_force;
			thr << "max_force >= rms_force";
			return { "INV-05", "Max Force >= RMS Force", ok, margin,
					 obs.str(), thr.str(),
					 ok ? "" : "max_force < rms_force — force accumulation bug" };
		}
	});

	// ------------------------------------------------------------------
	// INV-06: FIRE alpha in (0, 1]
	// Physical basis: FIRE adaptive mixing parameter alpha must remain
	// in (0, 1].  Out-of-range values indicate integrator divergence.
	// ------------------------------------------------------------------
	reg.invariants.push_back({
		"INV-06", "FIRE Alpha Bounds",
		"alpha_final in (0, 1] — FIRE mixing parameter physical range",
		[](const FormationSnapshot& s) -> InvariantResult {
			bool ok = s.alpha_final > 0.0 && s.alpha_final <= 1.0 + 1e-9;
			double margin = std::min(s.alpha_final, 1.0 - s.alpha_final);
			std::ostringstream obs;
			obs << std::fixed << std::setprecision(4) << s.alpha_final;
			return { "INV-06", "FIRE Alpha Bounds", ok, margin,
					 obs.str(), "(0, 1]",
					 ok ? "" : "alpha_final out of (0,1] — FIRE integrator diverged" };
		}
	});

	// ------------------------------------------------------------------
	// INV-07: Step count <= max_steps
	// Physical basis: the integrator must not exceed its budget.
	// Violation indicates an off-by-one or loop escape bug.
	// ------------------------------------------------------------------
	reg.invariants.push_back({
		"INV-07", "Step Budget",
		"steps_taken <= max_steps — integrator must not exceed budget",
		[](const FormationSnapshot& s) -> InvariantResult {
			bool ok = s.steps_taken <= s.max_steps;
			double margin = static_cast<double>(s.max_steps - s.steps_taken);
			std::ostringstream obs, thr;
			obs << s.steps_taken << " steps";
			thr << "<= " << s.max_steps;
			return { "INV-07", "Step Budget", ok, margin,
					 obs.str(), thr.str(),
					 ok ? "" : "steps_taken exceeds max_steps — integrator loop escape" };
		}
	});

	// ------------------------------------------------------------------
	// INV-08: Wall time reasonableness
	// Physical basis: any formation taking > 120 000 ms (2 min) per seed
	// is either deadlocked or misconfigured.  Soft warning, not hard fail.
	// ------------------------------------------------------------------
	reg.invariants.push_back({
		"INV-08", "Wall Time Reasonable",
		"wall_time_ms < 120000 — 2 min hard cap per formation (deadlock guard)",
		[](const FormationSnapshot& s) -> InvariantResult {
			constexpr double LIMIT = 120000.0;
			bool ok = s.wall_time_ms < LIMIT;
			double margin = LIMIT - s.wall_time_ms;
			std::ostringstream obs, thr;
			obs << std::fixed << std::setprecision(1) << s.wall_time_ms << " ms";
			thr << "< " << LIMIT << " ms";
			return { "INV-08", "Wall Time Reasonable", ok, margin,
					 obs.str(), thr.str(),
					 ok ? "" : "formation exceeded 2-minute wall-time budget" };
		}
	});

	return reg;
}

// ============================================================================
// Aggregate check helper
// ============================================================================

struct InvariantSummary {
	int total     = 0;
	int passed    = 0;
	int failed    = 0;
	double worst_margin = 1e18;  // smallest margin seen (most-at-risk invariant)
	std::string worst_invariant_id;
	std::vector<InvariantResult> results;

	bool all_passed() const { return failed == 0; }
	double pass_rate() const {
		return total > 0 ? 100.0 * passed / total : 0.0;
	}
};

inline InvariantSummary check_all(
	const InvariantRegistry& reg,
	const FormationSnapshot& snap)
{
	InvariantSummary summary;
	summary.total = reg.size();
	for (const auto& inv : reg.invariants) {
		auto r = inv.check(snap);
		summary.results.push_back(r);
		if (r.passed) {
			++summary.passed;
		} else {
			++summary.failed;
		}
		if (r.margin < summary.worst_margin) {
			summary.worst_margin = r.margin;
			summary.worst_invariant_id = r.invariant_id;
		}
	}
	return summary;
}

} // namespace training
} // namespace vsepr
