#pragma once
/**
 * length_scale_fitter.hpp  —  WO-73D
 * ====================================
 * Fit Lambda_X (recovery length scale, nm) from a sampled recovery curve
 *
 *   R_X(L) = A_X(L) / A_X(inf)
 *   Lambda_X = R_X^{-1}(1 - 1/e)
 *
 * Four fitting modes (as specified in WO-73D):
 *   LINEAR   — piecewise-linear interpolation of R_X(L) then root-find
 *   SPLINE   — cubic spline interpolation then root-find
 *   LOG      — fit in log(L) space, then invert
 *   ASYMMETRIC — separate above/below-ideal penalty; asymmetric weighting
 *
 * Asymmetry rule (WO-73D — not symmetric):
 *   L > Lambda_X  =>  weaker recovery (above-ideal)   — soft penalty
 *   L < Lambda_X  =>  non-ideal / under-resolved      — hard penalty
 *
 * All length units are nanometres. All energy units are caller-defined.
 *
 * Naming canonical per WO-73A:
 *   Lambda_nm, Lambda_fit_nm, sigma_fit_nm, fit_quality
 */

#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>

namespace vsepr {
namespace multiscale {

// ============================================================================
// Fitting mode
// ============================================================================

enum class FitMode {
	LINEAR,
	SPLINE,
	LOG,
	ASYMMETRIC
};

// ============================================================================
// Input sample point
// ============================================================================

struct RecoverySample {
	double L;                 // length (nm)
	double recovered_value;   // A_X(L)
	double total_value;       // A_X(inf)  — set to recovered_value at saturation
};

// ============================================================================
// Fitter output (canonical field names per WO-73A)
// ============================================================================

struct FitResult {
	std::string object_id;
	double Lambda_fit_nm  = 0.0;  // fitted recovery length scale (nm)
	double sigma_fit_nm   = 0.0;  // estimated uncertainty (nm)
	double fit_error      = 0.0;  // RMS residual of R_X(L) fit
	double confidence     = 0.0;  // [0,1] — coverage × fit quality
	std::string state;
	std::string family;
	bool   valid          = false;
};

// ============================================================================
// LengthScaleFitter
// ============================================================================

class LengthScaleFitter {
public:
	explicit LengthScaleFitter(FitMode mode = FitMode::SPLINE,
							   double  tolerance_nm = 1e-4,
							   int     max_iterations = 200);

	/**
	 * Fit Lambda_X from a set of recovery-curve samples.
	 *
	 * @param samples     Sampled (L, A_X(L), A_X_inf) points — must be sorted
	 *                    by L, monotonically increasing, at least 3 points.
	 * @param object_id   Canonical object identifier (e.g. "pi_7")
	 * @param state       Canonical state label (e.g. "biological_pocket")
	 * @param family      Canonical family label (e.g. "pi_N")
	 * @return            FitResult with Lambda_fit_nm, sigma_fit_nm, fit_error
	 */
	FitResult fit(const std::vector<RecoverySample>& samples,
				  const std::string& object_id,
				  const std::string& state,
				  const std::string& family) const;

	FitMode mode()        const { return mode_; }
	double  tolerance()   const { return tol_; }

private:
	FitMode mode_;
	double  tol_;
	int     max_iter_;

	// Internal: build normalised R_X values from samples
	static std::vector<double> normalise(const std::vector<RecoverySample>& s);

	// Internal fitting methods — each returns Lambda_fit_nm
	double fit_linear   (const std::vector<double>& L,
						 const std::vector<double>& R) const;
	double fit_spline   (const std::vector<double>& L,
						 const std::vector<double>& R) const;
	double fit_log      (const std::vector<double>& L,
						 const std::vector<double>& R) const;
	double fit_asymmetric(const std::vector<double>& L,
						  const std::vector<double>& R) const;

	// Estimate sigma via leave-one-out residuals
	static double estimate_sigma(const std::vector<double>& L,
								 const std::vector<double>& R,
								 double Lambda_fit);

	// RMS residual of R_X fit at the solved Lambda
	static double rms_residual(const std::vector<double>& L,
								const std::vector<double>& R,
								double Lambda_fit);

	// Simple natural cubic spline: evaluate at x, given knots xs/ys
	static double cubic_spline_eval(const std::vector<double>& xs,
									const std::vector<double>& ys,
									double x);
};

} // namespace multiscale
} // namespace vsepr
