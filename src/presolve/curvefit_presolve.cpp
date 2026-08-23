// =============================================================================
// curvefit_presolve.cpp  —  Curve-Fit Surrogate Presolver  (WO-XSUITE-02B)
// =============================================================================

#include "vsim/curvefit_presolve.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// Physics penalty
// ---------------------------------------------------------------------------

double curvefit_physics_penalty(
	double D_eff, double k_eff, double E_drift, double E_max,
	double w1, double w2, double w3)
{
	double p = 0.0;
	if (D_eff < 0.0) p += w1 * (-D_eff);
	if (k_eff < 0.0) p += w2 * (-k_eff);
	if (E_drift > E_max) p += w3 * (E_drift - E_max);
	return p;
}

// ---------------------------------------------------------------------------
// CurvefitModel::predict
// ---------------------------------------------------------------------------

std::vector<double> CurvefitModel::predict(const std::vector<double>& z) const {
	if (coefficients.empty() || basis_terms == 0 || n_targets == 0)
		return std::vector<double>(static_cast<size_t>(n_targets), 0.0);

	// Build basis vector from z
	std::vector<double> basis;
	basis.push_back(1.0); // intercept

	// Linear terms
	for (double zi : z) basis.push_back(zi);

	// Quadratic cross terms (if max_order >= 2)
	for (size_t i = 0; i < z.size(); ++i)
		for (size_t j = i; j < z.size(); ++j)
			basis.push_back(z[i] * z[j]);

	// Log-scaled terms
	for (double zi : z)
		basis.push_back(std::log(1.0 + std::abs(zi)));

	// Pad / truncate to basis_terms
	basis.resize(static_cast<size_t>(basis_terms), 0.0);

	std::vector<double> y(static_cast<size_t>(n_targets), 0.0);
	for (int t = 0; t < n_targets; ++t) {
		double sum = 0.0;
		for (int b = 0; b < basis_terms; ++b)
			sum += coefficients[static_cast<size_t>(t * basis_terms + b)] * basis[static_cast<size_t>(b)];
		y[static_cast<size_t>(t)] = sum;
	}
	return y;
}

// ---------------------------------------------------------------------------
// Internal: build basis matrix for all samples
// ---------------------------------------------------------------------------

namespace detail {

static std::vector<double> build_basis_row(
	const std::vector<double>& z, int max_order)
{
	std::vector<double> b;
	b.push_back(1.0);
	for (double zi : z) b.push_back(zi);
	if (max_order >= 2) {
		for (size_t i = 0; i < z.size(); ++i)
			for (size_t j = i; j < z.size(); ++j)
				b.push_back(z[i] * z[j]);
	}
	if (max_order >= 3) {
		for (size_t i = 0; i < z.size(); ++i)
			for (size_t j = i; j < z.size(); ++j)
				for (size_t k = j; k < z.size(); ++k)
					b.push_back(z[i] * z[j] * z[k]);
	}
	for (double zi : z)
		b.push_back(std::log(1.0 + std::abs(zi)));
	return b;
}

// Solve least-squares via normal equations with Tikhonov regularisation.
// A^T A θ + μI θ = A^T b  →  θ = (A^T A + μI)^{-1} A^T b
// Uses simple Cholesky-like LDL^T for small systems; falls back to LU.
static std::vector<double> ridge_solve(
	const std::vector<std::vector<double>>& A,   // n_samples × n_basis
	const std::vector<double>&              b,   // n_samples
	double                                  mu)
{
	int n = static_cast<int>(A.empty() ? 0 : A[0].size());
	int m = static_cast<int>(A.size());
	if (n == 0 || m == 0) return {};

	// Build ATA + μI
	std::vector<double> ATA(static_cast<size_t>(n * n), 0.0);
	std::vector<double> ATb(static_cast<size_t>(n), 0.0);

	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < n; ++j) {
			for (int k = 0; k < n; ++k)
				ATA[static_cast<size_t>(j * n + k)] += A[static_cast<size_t>(i)][static_cast<size_t>(j)] * A[static_cast<size_t>(i)][static_cast<size_t>(k)];
			ATb[static_cast<size_t>(j)] += A[static_cast<size_t>(i)][static_cast<size_t>(j)] * b[static_cast<size_t>(i)];
		}
	}
	for (int j = 0; j < n; ++j)
		ATA[static_cast<size_t>(j * n + j)] += mu;

	// Gaussian elimination with partial pivoting
	std::vector<double> x = ATb;
	auto& M = ATA;
	for (int col = 0; col < n; ++col) {
		// Find pivot
		int pivot = col;
		for (int row = col + 1; row < n; ++row)
			if (std::abs(M[static_cast<size_t>(row * n + col)]) > std::abs(M[static_cast<size_t>(pivot * n + col)]))
				pivot = row;
		// Swap rows
		for (int k = 0; k < n; ++k)
			std::swap(M[static_cast<size_t>(col * n + k)], M[static_cast<size_t>(pivot * n + k)]);
		std::swap(x[static_cast<size_t>(col)], x[static_cast<size_t>(pivot)]);
		// Eliminate
		double diag = M[static_cast<size_t>(col * n + col)];
		if (std::abs(diag) < 1e-15) continue;
		for (int row = col + 1; row < n; ++row) {
			double factor = M[static_cast<size_t>(row * n + col)] / diag;
			for (int k = col; k < n; ++k)
				M[static_cast<size_t>(row * n + k)] -= factor * M[static_cast<size_t>(col * n + k)];
			x[static_cast<size_t>(row)] -= factor * x[static_cast<size_t>(col)];
		}
	}
	// Back substitution
	for (int col = n - 1; col >= 0; --col) {
		double diag = M[static_cast<size_t>(col * n + col)];
		if (std::abs(diag) < 1e-15) continue;
		x[static_cast<size_t>(col)] /= diag;
		for (int row = col - 1; row >= 0; --row)
			x[static_cast<size_t>(row)] -= M[static_cast<size_t>(row * n + col)] * x[static_cast<size_t>(col)];
	}
	return x;
}

} // namespace detail

// ---------------------------------------------------------------------------
// curvefit_run
// ---------------------------------------------------------------------------

CurvefitResult curvefit_run(
	const CurvefitConfig&              cfg,
	const std::vector<CurvefitSample>& samples,
	std::ostream&                      log)
{
	CurvefitResult res;

	if (samples.empty()) {
		res.error = "curvefit_run: no samples provided";
		return res;
	}

	int n_feat = static_cast<int>(samples[0].z.size());
	int n_tgt  = static_cast<int>(samples[0].y.size());

	if (n_feat == 0 || n_tgt == 0) {
		res.error = "curvefit_run: empty feature or target vector";
		return res;
	}

	// Split train / validation
	size_t n_total = samples.size();
	size_t n_train = static_cast<size_t>(std::max(1.0, std::floor(cfg.train_fraction * static_cast<double>(n_total))));
	size_t n_val   = n_total - n_train;

	log << "[curvefit] samples=" << n_total
		<< "  train=" << n_train << "  val=" << n_val
		<< "  model=" << cfg.model << "  max_order=" << cfg.max_order << "\n";

	// Build example basis row to determine n_basis
	auto example_basis = detail::build_basis_row(samples[0].z, cfg.max_order);
	int n_basis = static_cast<int>(example_basis.size());

	// Build basis matrix for train set
	std::vector<std::vector<double>> A(n_train);
	for (size_t i = 0; i < n_train; ++i)
		A[i] = detail::build_basis_row(samples[i].z, cfg.max_order);

	// Fit one model per target dimension
	std::vector<double> all_coeffs;
	all_coeffs.reserve(static_cast<size_t>(n_tgt * n_basis));

	for (int t = 0; t < n_tgt; ++t) {
		std::vector<double> b_vec(n_train);
		for (size_t i = 0; i < n_train; ++i)
			b_vec[i] = samples[i].y[static_cast<size_t>(t)];
		auto theta = detail::ridge_solve(A, b_vec, cfg.regularization);
		theta.resize(static_cast<size_t>(n_basis), 0.0);
		all_coeffs.insert(all_coeffs.end(), theta.begin(), theta.end());
	}

	// Build model object
	CurvefitModel model;
	model.model_type   = cfg.model;
	model.n_features   = n_feat;
	model.n_targets    = n_tgt;
	model.basis_terms  = n_basis;
	model.coefficients = all_coeffs;

	// Compute train RMSE
	double train_sq = 0.0;
	for (size_t i = 0; i < n_train; ++i) {
		auto y_hat = model.predict(samples[i].z);
		for (int t = 0; t < n_tgt; ++t) {
			double diff = y_hat[static_cast<size_t>(t)] - samples[i].y[static_cast<size_t>(t)];
			train_sq += diff * diff;
		}
	}
	model.train_rmse = std::sqrt(train_sq / static_cast<double>(n_train * n_tgt));

	// Compute validation RMSE
	double val_sq   = 0.0;
	double y_range  = 1.0;
	if (n_val > 0) {
		double y_min = 1e300, y_max = -1e300;
		for (size_t i = n_train; i < n_total; ++i) {
			auto y_hat = model.predict(samples[i].z);
			for (int t = 0; t < n_tgt; ++t) {
				double diff = y_hat[static_cast<size_t>(t)] - samples[i].y[static_cast<size_t>(t)];
				val_sq += diff * diff;
				y_min = std::min(y_min, samples[i].y[static_cast<size_t>(t)]);
				y_max = std::max(y_max, samples[i].y[static_cast<size_t>(t)]);
			}
		}
		model.val_rmse = std::sqrt(val_sq / static_cast<double>(n_val * n_tgt));
		y_range = std::max(1e-12, y_max - y_min);
	}

	res.n_train     = static_cast<int>(n_train);
	res.n_val       = static_cast<int>(n_val);
	res.train_rmse  = model.train_rmse;
	res.val_rmse    = (n_val > 0) ? model.val_rmse : 0.0;
	res.val_score   = std::max(0.0, 1.0 - res.val_rmse / y_range);
	res.model       = std::move(model);

	log << "[curvefit] train_rmse=" << res.train_rmse
		<< "  val_rmse=" << res.val_rmse
		<< "  val_score=" << res.val_score << "\n";

	// Write model JSON
	if (!cfg.write_model.empty()) {
		std::ofstream f(cfg.write_model);
		if (f) {
			f << "{\"model_type\":\"" << res.model.model_type << "\","
			  << "\"n_features\":" << res.model.n_features << ","
			  << "\"n_targets\":" << res.model.n_targets << ","
			  << "\"basis_terms\":" << res.model.basis_terms << ","
			  << "\"train_rmse\":" << res.train_rmse << ","
			  << "\"val_rmse\":" << res.val_rmse << ","
			  << "\"val_score\":" << res.val_score << "}\n";
			res.model_path = cfg.write_model;
			log << "[curvefit] model written -> " << cfg.write_model << "\n";
		}
	}

	// Write coefficients TSV
	if (!cfg.write_coefficients.empty()) {
		std::ofstream f(cfg.write_coefficients);
		if (f) {
			f << "target\tbasis_term\tcoefficient\n";
			for (int t = 0; t < n_tgt; ++t)
				for (int b = 0; b < n_basis; ++b)
					f << t << "\t" << b << "\t"
					  << res.model.coefficients[static_cast<size_t>(t * n_basis + b)] << "\n";
			res.coefficients_path = cfg.write_coefficients;
			log << "[curvefit] coefficients written -> " << cfg.write_coefficients << "\n";
		}
	}

	// Write validation report
	if (!cfg.write_report.empty()) {
		std::ofstream f(cfg.write_report);
		if (f) {
			f << "# CurveFit Validation Report\n\n"
			  << "| Field | Value |\n|---|---|\n"
			  << "| model | " << res.model.model_type << " |\n"
			  << "| n_train | " << res.n_train << " |\n"
			  << "| n_val | " << res.n_val << " |\n"
			  << "| train_rmse | " << res.train_rmse << " |\n"
			  << "| val_rmse | " << res.val_rmse << " |\n"
			  << "| val_score | " << res.val_score << " |\n";
			res.report_path = cfg.write_report;
			log << "[curvefit] report written -> " << cfg.write_report << "\n";
		}
	}

	res.ok = true;
	return res;
}

} // namespace presolve
} // namespace vsepr
