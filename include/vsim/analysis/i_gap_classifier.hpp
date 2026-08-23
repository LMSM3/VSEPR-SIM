#pragma once
/**
 * i_gap_classifier.hpp  —  WO-74A: IGapClassifier interface
 * ===========================================================
 * Abstract interface for gap classification strategies.
 * Both the legacy 3-label threshold classifier and the 14-label adaptive
 * classifier implement this interface and self-register via AutoRegister.
 *
 * Registry keys:
 *   "threshold"  — 3-label (good_match / overextended_field / underresolved_scale)
 *                  Wraps the legacy classify_gap() free function.
 *   "adaptive"   — 14-label (lambda+energy, reference_count guard)
 *                  Wraps the GapClassifier class from gap_classifier.hpp.
 *
 * Lookup example:
 *   auto clf = vsim::ModuleRegistry<IGapClassifier>::get().create("adaptive");
 *   auto result = clf->classify({ lambda_sim, lambda_emp, e_sim, e_emp, 10 });
 */

#include "vsim/module_registry.hpp"
#include "multiscale/gap_classifier.hpp"   // GapClassificationResult

#include <string_view>

namespace vsepr {
namespace multiscale {

// ============================================================================
// GapInput — unified input for all classifier strategies
// ============================================================================

struct GapInput {
	double lambda_sim   = 0.0;   // Simulated length scale (nm)
	double lambda_emp   = 0.0;   // Empirical reference length scale (nm)
	double e_sim        = 0.0;   // Simulation energy (eV or arbitrary units)
	double e_emp        = 0.0;   // Empirical reference energy
	int    ref_count    = 10;    // Number of reference data points
	double tolerance_nm = 0.05;  // Absolute tolerance used by "threshold" strategy
};

// ============================================================================
// IGapClassifier
// ============================================================================

class IGapClassifier {
public:
	virtual ~IGapClassifier() = default;

	// Canonical registry key — "threshold" or "adaptive".
	[[nodiscard]] virtual std::string_view strategy_name() const = 0;

	// Classify a gap tuple.  Returns a GapClassificationResult.
	[[nodiscard]] virtual GapClassificationResult classify(const GapInput& in) const = 0;

	// Human-readable description for vsepr doctor / audit log.
	[[nodiscard]] virtual std::string_view description() const { return strategy_name(); }
};

} // namespace multiscale
} // namespace vsepr
