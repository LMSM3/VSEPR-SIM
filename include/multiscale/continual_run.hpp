#pragma once
/**
 * continual_run.hpp  —  WO-74A / WO-74D
 * =========================================
 * ContinualRunRecord: the central row type for the continual refinement loop.
 * uncertainty_weighted_update(): Bayesian prior update rule (WO-74D).
 *
 * Column names are canonical per WO-73A (naming_convention_map.csv).
 *
 * Schema columns match continual_run_schema.csv exactly.
 */

#include <string>
#include <cmath>

namespace vsepr {
namespace multiscale {

// ============================================================================
// ContinualRunRecord — one row in the continual run table
// ============================================================================

struct ContinualRunRecord {
	// Identity
	std::string run_id;
	std::string object_id;
	std::string family;
	std::string state;
	std::string casting;
	std::string source_type   = "simulation";
	int         iteration     = 1;

	// Simulation output (canonical names — WO-73A)
	double Lambda_nm          = 0.0;
	double E_value            = 0.0;
	double UFF                = 0.0;
	double CFF                = 0.0;

	// Scale tier classification (WO-73C)
	std::string primary_scale;
	int z_scale_1 = 0, z_scale_2 = 0, z_scale_3 = 0;
	int z_scale_4 = 0, z_scale_5 = 0, z_scale_6 = 0;
	double eta_scale = 0.0;

	// Prior (from state_prior_table.csv or previous iteration)
	double prior_lambda_nm    = 0.0;
	double prior_sigma_nm     = 1.0;
	double prior_energy       = 0.0;
	double prior_energy_sigma = 1.0;

	// Updated (filled by uncertainty_weighted_update)
	double updated_lambda_nm  = 0.0;
	double updated_sigma_nm   = 0.0;
	double updated_energy     = 0.0;

	// Gap classification (WO-74C)
	std::string gap_label         = "pending";
	std::string validation_status = "pending";

	// Next-run generation (WO-74E)
	std::string next_run_family;
	std::string next_run_state;

	std::string notes;
};

// ============================================================================
// WO-74D: uncertainty_weighted_update
//
// Bayesian update: fuse simulation value with prior using inverse-variance weights.
//
//   mu_new   = (mu_prior / sigma_prior^2 + mu_sim / sigma_sim^2)
//              / (1/sigma_prior^2 + 1/sigma_sim^2)
//
//   sigma_new = 1 / sqrt(1/sigma_prior^2 + 1/sigma_sim^2)
//
// @param mu_prior     Prior mean
// @param sigma_prior  Prior std-dev (must be > 0)
// @param mu_sim       Simulation estimate
// @param sigma_sim    Simulation uncertainty (must be > 0)
// @param mu_out       Updated mean (out)
// @param sigma_out    Updated std-dev (out)
// ============================================================================

inline void uncertainty_weighted_update(double  mu_prior,
										 double  sigma_prior,
										 double  mu_sim,
										 double  sigma_sim,
										 double& mu_out,
										 double& sigma_out)
{
	const double sp2 = sigma_prior * sigma_prior;
	const double ss2 = sigma_sim   * sigma_sim;

	// Guard against degenerate inputs
	const double inv_sp2 = (sp2 > 1e-20) ? 1.0 / sp2 : 1e20;
	const double inv_ss2 = (ss2 > 1e-20) ? 1.0 / ss2 : 1e20;

	const double inv_sum = inv_sp2 + inv_ss2;
	sigma_out = (inv_sum > 1e-20) ? 1.0 / std::sqrt(inv_sum) : 0.0;
	mu_out    = (mu_prior * inv_sp2 + mu_sim * inv_ss2) / inv_sum;
}

// ============================================================================
// Apply WO-74D update to a ContinualRunRecord in-place
// Requires: rec.Lambda_nm (sim), rec.prior_lambda_nm / prior_sigma_nm set
// Also fuses energy if sim sigma_energy > 0
// ============================================================================

inline void apply_prior_update(ContinualRunRecord& rec,
								double sigma_lambda_sim,
								double sigma_energy_sim = 1.0)
{
	uncertainty_weighted_update(rec.prior_lambda_nm, rec.prior_sigma_nm,
								rec.Lambda_nm, sigma_lambda_sim,
								rec.updated_lambda_nm, rec.updated_sigma_nm);

	double updated_e_sigma = 0.0;
	uncertainty_weighted_update(rec.prior_energy, rec.prior_energy_sigma,
								rec.E_value, sigma_energy_sim,
								rec.updated_energy, updated_e_sigma);
}

} // namespace multiscale
} // namespace vsepr
