#pragma once
/**
 * gap_classifier.hpp  —  WO-74C
 * ================================
 * GapClassifier: classify a (Lambda_sim, Lambda_empirical, E_sim, E_empirical)
 * tuple into one of the 14 standardised gap labels from gap_label_registry.csv.
 *
 * Asymmetry rule (WO-73D / WO-74C):
 *   lambda_high  (L_sim > L_emp)  — above-ideal, soft penalty, severity 1
 *   lambda_low   (L_sim < L_emp)  — under-resolved, hard penalty, severity 2
 *
 * Thresholds (WO-74C default):
 *   small    : |epsilon| < 0.1 * reference
 *   moderate : 0.1 <= |epsilon| < 0.3 * reference
 *   large    : |epsilon| >= 0.3 * reference
 */

#include <string>
#include <cmath>

namespace vsepr {
namespace multiscale {

// ============================================================================
// Classification thresholds (fractions of the reference value)
// ============================================================================

struct GapThresholds {
	double small_fraction    = 0.10;   // < 10 % -> small
	double moderate_fraction = 0.30;   // 10-30 % -> moderate
	// >= moderate_fraction -> large
};

// ============================================================================
// GapClassificationResult
// ============================================================================

struct GapClassificationResult {
	std::string gap_label;
	int         severity     = 0;   // 0 = none, 1 = low, 2 = medium, 3 = high
	std::string action;             // none / flag_review / rerun_* / expand_db
	double      epsilon_lambda = 0.0; // L_sim - L_emp
	double      epsilon_energy = 0.0; // E_sim - E_emp
};

// ============================================================================
// GapClassifier
// ============================================================================

class GapClassifier {
public:
	explicit GapClassifier(const GapThresholds& t = GapThresholds{});

	/**
	 * Classify the gap between simulation and empirical values.
	 *
	 * @param Lambda_sim      Simulation recovered length scale (nm)
	 * @param Lambda_emp      Empirical reference length scale (nm)
	 * @param E_sim           Simulation energy
	 * @param E_emp           Empirical reference energy
	 * @param reference_count Number of reference data points (< 3 -> insufficient_data)
	 */
	GapClassificationResult classify(double Lambda_sim,
									  double Lambda_emp,
									  double E_sim,
									  double E_emp,
									  int    reference_count = 10) const;

	const GapThresholds& thresholds() const { return t_; }

private:
	GapThresholds t_;

	enum class Mag { SMALL, MODERATE, LARGE };
	Mag magnitude(double epsilon, double reference) const;
};

} // namespace multiscale
} // namespace vsepr
