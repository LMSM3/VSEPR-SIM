/**
 * length_scale_fitter.cpp  —  WO-73D
 * ====================================
 * Implements LengthScaleFitter: four fitting modes for Lambda_X recovery curves.
 *
 * Asymmetry rule (WO-73D):
 *   L > Lambda_X  =>  weaker recovery (above-ideal)   — soft weight
 *   L < Lambda_X  =>  non-ideal / under-resolved      — hard weight (2x)
 *   These two failure modes have different physical causes.
 */

#include "multiscale/length_scale_fitter.hpp"
#include <algorithm>
#include <numeric>
#include <cassert>
#include <cstring>

namespace vsepr {
namespace multiscale {

static constexpr double TARGET_R = 1.0 - 1.0 / M_E;  // ~0.6321

// ============================================================================
// Constructor
// ============================================================================

LengthScaleFitter::LengthScaleFitter(FitMode mode, double tolerance_nm, int max_iterations)
	: mode_(mode), tol_(tolerance_nm), max_iter_(max_iterations)
{}

// ============================================================================
// Normalise samples -> R_X values in [0,1]
// ============================================================================

std::vector<double> LengthScaleFitter::normalise(const std::vector<RecoverySample>& s)
{
	std::vector<double> R(s.size());
	for (std::size_t i = 0; i < s.size(); ++i) {
		const double denom = s[i].total_value;
		R[i] = (std::abs(denom) > 1e-15)
			   ? s[i].recovered_value / denom
			   : 0.0;
		// Clamp to [0,1]
		R[i] = std::clamp(R[i], 0.0, 1.0);
	}
	return R;
}

// ============================================================================
// Natural cubic spline evaluation
// ============================================================================

double LengthScaleFitter::cubic_spline_eval(const std::vector<double>& xs,
											 const std::vector<double>& ys,
											 double x)
{
	const int n = static_cast<int>(xs.size());
	if (n < 2) return ys.empty() ? 0.0 : ys[0];

	// Clamp x to domain
	if (x <= xs.front()) return ys.front();
	if (x >= xs.back())  return ys.back();

	// Build natural cubic spline coefficients (tridiagonal system)
	std::vector<double> h(n - 1), alpha(n - 1);
	for (int i = 0; i < n - 1; ++i) h[i] = xs[i+1] - xs[i];
	for (int i = 1; i < n - 1; ++i)
		alpha[i] = (3.0/h[i]) * (ys[i+1] - ys[i]) - (3.0/h[i-1]) * (ys[i] - ys[i-1]);

	std::vector<double> l(n, 1.0), mu(n, 0.0), z(n, 0.0);
	for (int i = 1; i < n - 1; ++i) {
		l[i]  = 2.0 * (xs[i+1] - xs[i-1]) - h[i-1] * mu[i-1];
		mu[i] = h[i] / l[i];
		z[i]  = (alpha[i] - h[i-1] * z[i-1]) / l[i];
	}

	std::vector<double> c(n, 0.0), b(n-1), d(n-1);
	for (int j = n - 2; j >= 0; --j) {
		c[j] = z[j] - mu[j] * c[j+1];
		b[j] = (ys[j+1] - ys[j]) / h[j] - h[j] * (c[j+1] + 2.0*c[j]) / 3.0;
		d[j] = (c[j+1] - c[j]) / (3.0 * h[j]);
	}

	// Find segment
	int seg = 0;
	for (int i = 0; i < n - 2; ++i) {
		if (x >= xs[i] && x < xs[i+1]) { seg = i; break; }
		if (i == n - 2) seg = n - 2;
	}
	if (x >= xs[n-2]) seg = n - 2;

	const double dx = x - xs[seg];
	return ys[seg] + b[seg]*dx + c[seg]*dx*dx + d[seg]*dx*dx*dx;
}

// ============================================================================
// Linear interpolation fit: find L where R(L) == TARGET_R
// ============================================================================

double LengthScaleFitter::fit_linear(const std::vector<double>& L,
									  const std::vector<double>& R) const
{
	for (std::size_t i = 0; i + 1 < L.size(); ++i) {
		if (R[i] <= TARGET_R && R[i+1] >= TARGET_R) {
			const double dR = R[i+1] - R[i];
			if (std::abs(dR) < 1e-15) return L[i];
			const double t = (TARGET_R - R[i]) / dR;
			return L[i] + t * (L[i+1] - L[i]);
		}
	}
	// Extrapolate from last two points
	const std::size_t n = L.size();
	if (n < 2) return L.empty() ? 0.0 : L[0];
	const double dL = L[n-1] - L[n-2];
	const double dR = R[n-1] - R[n-2];
	if (std::abs(dR) < 1e-15) return L[n-1];
	return L[n-2] + dL * (TARGET_R - R[n-2]) / dR;
}

// ============================================================================
// Spline fit: interpolate R(L) with natural cubic spline, then bisect
// ============================================================================

double LengthScaleFitter::fit_spline(const std::vector<double>& L,
									  const std::vector<double>& R) const
{
	// Bisection on spline within [L.front(), L.back()]
	double lo = L.front(), hi = L.back();
	const double R_lo = cubic_spline_eval(L, R, lo);
	const double R_hi = cubic_spline_eval(L, R, hi);
	if (R_lo >= TARGET_R) return lo;
	if (R_hi <= TARGET_R) {
		// Extrapolate linearly beyond last point
		const std::size_t n = L.size();
		const double slope = (R[n-1] - R[n-2]) / (L[n-1] - L[n-2] + 1e-15);
		return (slope > 1e-15) ? L.back() + (TARGET_R - R_hi) / slope : L.back();
	}

	for (int iter = 0; iter < max_iter_; ++iter) {
		const double mid = 0.5 * (lo + hi);
		const double Rmid = cubic_spline_eval(L, R, mid);
		if (std::abs(Rmid - TARGET_R) < tol_) return mid;
		if (Rmid < TARGET_R) lo = mid; else hi = mid;
		if (hi - lo < tol_) return 0.5 * (lo + hi);
	}
	return 0.5 * (lo + hi);
}

// ============================================================================
// Log-space fit: fit in log(L), then invert
// ============================================================================

double LengthScaleFitter::fit_log(const std::vector<double>& L,
								   const std::vector<double>& R) const
{
	std::vector<double> logL(L.size());
	for (std::size_t i = 0; i < L.size(); ++i)
		logL[i] = std::log(std::max(L[i], 1e-15));

	const double logLambda = fit_spline(logL, R);
	return std::exp(logLambda);
}

// ============================================================================
// Asymmetric fit: weighted residual with different above/below penalties
// WO-73D: "below-ideal has different physical cause — do not treat as equivalent"
// ============================================================================

double LengthScaleFitter::fit_asymmetric(const std::vector<double>& L,
										  const std::vector<double>& R) const
{
	// Start with spline estimate
	double Lambda = fit_spline(L, R);

	// Refine with asymmetric gradient descent
	double step = (L.back() - L.front()) * 0.1;
	for (int iter = 0; iter < max_iter_ && step > tol_; ++iter) {
		// Evaluate weighted cost at Lambda +/- step
		auto cost = [&](double lam) -> double {
			double c = 0.0;
			for (std::size_t i = 0; i < L.size(); ++i) {
				// R_model at L[i]: simple exponential approximation
				const double R_model = 1.0 - std::exp(-L[i] / std::max(lam, 1e-15));
				const double residual = R[i] - R_model;
				// Asymmetric weight: below-ideal (L < Lambda) gets harder penalty
				const double w = (L[i] < lam) ? 2.0 : 1.0;
				c += w * residual * residual;
			}
			return c;
		};

		const double c0   = cost(Lambda);
		const double cUp  = cost(Lambda + step);
		const double cDn  = cost(Lambda - step);

		if (cDn < c0 && cDn <= cUp)       Lambda -= step;
		else if (cUp < c0 && cUp <= cDn)  Lambda += step;
		else                               step *= 0.5;
	}
	return Lambda;
}

// ============================================================================
// Sigma estimate (leave-one-out residuals)
// ============================================================================

double LengthScaleFitter::estimate_sigma(const std::vector<double>& L,
										  const std::vector<double>& R,
										  double Lambda_fit)
{
	if (L.size() < 2) return 0.0;
	// Sigma ~ std-dev of individual point implied Lambda estimates via exponential model
	std::vector<double> implied;
	implied.reserve(L.size());
	for (std::size_t i = 0; i < L.size(); ++i) {
		const double r = std::clamp(R[i], 1e-6, 1.0 - 1e-6);
		const double lam = -L[i] / std::log(1.0 - r);
		if (lam > 0.0 && lam < 1e6) implied.push_back(lam);
	}
	if (implied.empty()) return std::abs(Lambda_fit) * 0.1;
	const double mean = std::accumulate(implied.begin(), implied.end(), 0.0)
						/ static_cast<double>(implied.size());
	double var = 0.0;
	for (double v : implied) var += (v - mean) * (v - mean);
	return std::sqrt(var / static_cast<double>(implied.size()));
}

// ============================================================================
// RMS residual
// ============================================================================

double LengthScaleFitter::rms_residual(const std::vector<double>& L,
										const std::vector<double>& R,
										double Lambda_fit)
{
	if (L.empty()) return 0.0;
	double sum = 0.0;
	for (std::size_t i = 0; i < L.size(); ++i) {
		const double R_model = 1.0 - std::exp(-L[i] / std::max(Lambda_fit, 1e-15));
		const double e = R[i] - R_model;
		sum += e * e;
	}
	return std::sqrt(sum / static_cast<double>(L.size()));
}

// ============================================================================
// Public fit()
// ============================================================================

FitResult LengthScaleFitter::fit(const std::vector<RecoverySample>& samples,
								  const std::string& object_id,
								  const std::string& state,
								  const std::string& family) const
{
	FitResult result;
	result.object_id = object_id;
	result.state     = state;
	result.family    = family;

	if (samples.size() < 3) {
		result.valid = false;
		return result;
	}

	// Extract L vector; verify sorted
	std::vector<double> L(samples.size()), R;
	for (std::size_t i = 0; i < samples.size(); ++i)
		L[i] = samples[i].L;

	R = normalise(samples);

	// Dispatch to fitting mode
	double Lambda_fit = 0.0;
	switch (mode_) {
		case FitMode::LINEAR:     Lambda_fit = fit_linear(L, R);     break;
		case FitMode::SPLINE:     Lambda_fit = fit_spline(L, R);     break;
		case FitMode::LOG:        Lambda_fit = fit_log(L, R);        break;
		case FitMode::ASYMMETRIC: Lambda_fit = fit_asymmetric(L, R); break;
	}

	result.Lambda_fit_nm = Lambda_fit;
	result.sigma_fit_nm  = estimate_sigma(L, R, Lambda_fit);
	result.fit_error     = rms_residual(L, R, Lambda_fit);

	// Confidence: penalise high fit_error and low coverage
	const double coverage = static_cast<double>(samples.size()) / 20.0; // normalise to ~20 pts
	result.confidence = std::exp(-result.fit_error) * std::min(coverage, 1.0);
	result.valid      = (Lambda_fit > 0.0 && result.fit_error < 0.5);
	return result;
}

} // namespace multiscale
} // namespace vsepr
