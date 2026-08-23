#pragma once
// =============================================================================
// curvefit_presolve.hpp  —  Multivariable Surrogate Curve-Fit Presolver
//                           (WO-XSUITE-02B)
// =============================================================================
//
// Builds a fast surrogate model y^ = F(z; θ) from EigenMining + batch
// presolve archives.  Supports poly / log / eigen-hybrid bases.
//
// Loss: J(θ) = (1/n)Σ(y_i - y^_i)² + μ‖θ‖² + νΩ_physics
// Physics penalty: no negative diffusivity / conductivity, no energy blow-up.
//
// =============================================================================

#include <string>
#include <vector>
#include <ostream>
#include <iostream>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// CurvefitConfig  —  mirrors [curvefit] .X block
// ---------------------------------------------------------------------------

struct CurvefitConfig {
	std::string source;              // eigenmine_modes.ndjson input
	std::string target;              // presolve_batch.ndjson input
	std::string model           = "poly_log_eigen_hybrid";
	int         max_order       = 3;
	double      regularization  = 1.0e-4;
	double      train_fraction  = 0.80;
	double      val_fraction    = 0.20;
	std::string write_model;         // curvefit_model.json
	std::string write_coefficients;  // curvefit_coefficients.tsv
	std::string write_report;        // curvefit_validation.md
};

// ---------------------------------------------------------------------------
// CurvefitSample  —  one (z, y) observation
// Feature vector z = [T, ρ, P, N, Δt, φ, η, E0, λ_k...]
// Target  vector y = [E_drift, D_eff, k_eff, σ_macro, P_react, t_settle]
// ---------------------------------------------------------------------------

struct CurvefitSample {
	std::vector<double> z;  // feature vector
	std::vector<double> y;  // target vector
};

// ---------------------------------------------------------------------------
// CurvefitModel  —  fitted model ready for evaluation
// ---------------------------------------------------------------------------

struct CurvefitModel {
	std::string model_type;
	int         n_features   = 0;
	int         n_targets    = 0;
	int         basis_terms  = 0;
	std::vector<double> coefficients;   // flattened [n_targets × basis_terms]
	double      train_rmse   = 0.0;
	double      val_rmse     = 0.0;
	double      physics_penalty = 0.0;  // Ω_physics at solution

	// Evaluate model at a feature point
	std::vector<double> predict(const std::vector<double>& z) const;
};

// ---------------------------------------------------------------------------
// CurvefitResult
// ---------------------------------------------------------------------------

struct CurvefitResult {
	bool           ok = false;
	std::string    error;
	int            n_train     = 0;
	int            n_val       = 0;
	double         train_rmse  = 0.0;
	double         val_rmse    = 0.0;
	double         val_score   = 0.0;   // 1 - (val_rmse / y_range), in [0,1]
	CurvefitModel  model;
	std::string    model_path;
	std::string    coefficients_path;
	std::string    report_path;
};

// ---------------------------------------------------------------------------
// Physics penalty helpers
// ---------------------------------------------------------------------------

// Returns Ω_physics = w1·max(0,-D_eff) + w2·max(0,-k_eff) + w3·max(0,E_drift-E_max)
double curvefit_physics_penalty(
	double D_eff,
	double k_eff,
	double E_drift,
	double E_max    = 1.0e6,
	double w1 = 1.0,
	double w2 = 1.0,
	double w3 = 1.0);

// ---------------------------------------------------------------------------
// curvefit_run  —  fit model from samples
// ---------------------------------------------------------------------------

CurvefitResult curvefit_run(
	const CurvefitConfig&              cfg,
	const std::vector<CurvefitSample>& samples,
	std::ostream&                      log = std::cout);

} // namespace presolve
} // namespace vsepr
