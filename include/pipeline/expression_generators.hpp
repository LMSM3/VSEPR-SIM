#pragma once
/**
 * Necessary includes are missing.
 * 
 * expression_generators.hpp — Symbolic Expression Builders for Pipeline Stages
 * ==============================================================================
 *
 * Pure functions. Each returns a SymbolicTrace for one derived metric.
 * Call sites are in pipeline_stages.hpp (stage_analysis, stage_fingerprint,
 * stage_cluster). No side effects. No rendering.
 *
 * Naming convention: expr::<metric_name>(inputs..., result)
 *
 * VSEPR-SIM  |  beta-7  |  WO-56C
 */

#include "include/pipeline/pipeline_trace.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace vsepr::pipeline::expr {

// ============================================================================
// Helpers
// ============================================================================

namespace detail {

inline std::string fmt(double v, int prec = 6) {
	if (!std::isfinite(v)) return "nan";
	std::ostringstream s;
	s << std::fixed << std::setprecision(prec) << v;
	return s.str();
}

inline std::string fmt3(double v) { return fmt(v, 3); }

} // namespace detail

// ============================================================================
// stage_analysis generators
// ============================================================================

/**
 * energy_per_bead
 *   E_bead = E_final / N_beads
 */
inline SymbolicTrace energy_per_bead(
	double final_energy,
	int    n_beads,
	double result)
{
	SymbolicTrace tr;
	tr.metric_name          = "energy_per_bead";
	tr.symbolic_expression  = "E_bead = E_final / N_beads";

	{
		std::ostringstream s;
		s << "E_bead = " << detail::fmt3(final_energy) << " / " << n_beads;
		tr.substituted_expression = s.str();
	}
	{
		std::ostringstream s;
		s << "E_bead = " << detail::fmt3(result);
		tr.result_expression = s.str();
	}

	tr.units           = "energy/bead";
	tr.interpretation  = "Mean energy per structural bead.";
	tr.source_stage    = "stage_analysis";
	return tr;
}

/**
 * convergence_quality
 *   Q_conv = clamp(1 / (1 + rms * 100))    if converged
 *   Q_conv = 0.0                             otherwise
 */
inline SymbolicTrace convergence_quality(
	double rms_force,
	double result)
{
	SymbolicTrace tr;
	tr.metric_name         = "convergence_quality";
	tr.symbolic_expression = "Q_conv = clamp(1 / (1 + rms * 100))";

	{
		std::ostringstream s;
		s << "Q_conv = clamp(1 / (1 + " << detail::fmt(rms_force) << " * 100))";
		tr.substituted_expression = s.str();
	}
	{
		std::ostringstream s;
		s << "Q_conv = " << detail::fmt3(result);
		tr.result_expression = s.str();
	}

	tr.units          = "dimensionless";
	tr.interpretation = "Convergence quality: 1.0 = converged perfectly, decays with rms_force.";
	tr.source_stage   = "stage_analysis";
	return tr;
}

/**
 * packing_quality
 *   rho_norm = clamp(avg_rho / 15)
 *   C_norm   = clamp(avg_C   / 50)
 *   Q_pack   = 0.5 * (rho_norm + C_norm)
 */
inline SymbolicTrace packing_quality(
	double avg_rho,
	double avg_C,
	double rho_norm,
	double C_norm,
	double result)
{
	SymbolicTrace tr;
	tr.metric_name         = "packing_quality";
	tr.symbolic_expression =
		"rho_norm = clamp(avg_rho / 15)  |  C_norm = clamp(avg_C / 50)  |  Q_pack = 0.5*(rho_norm + C_norm)";

	{
		std::ostringstream s;
		s << "rho_norm = clamp(" << detail::fmt3(avg_rho) << " / 15) = " << detail::fmt3(rho_norm)
		  << "  |  C_norm = clamp(" << detail::fmt3(avg_C) << " / 50) = " << detail::fmt3(C_norm)
		  << "  |  Q_pack = 0.5*(" << detail::fmt3(rho_norm) << " + " << detail::fmt3(C_norm) << ")";
		tr.substituted_expression = s.str();
	}
	{
		std::ostringstream s;
		s << "Q_pack = " << detail::fmt3(result);
		tr.result_expression = s.str();
	}

	tr.units          = "dimensionless";
	tr.interpretation = "Packing quality derived from density and coordination normalised to FCC reference.";
	tr.source_stage   = "stage_analysis";
	return tr;
}

/**
 * defect_indicator
 *   D = n_l3_domains / N_beads
 */
inline SymbolicTrace defect_indicator(
	int    n_l3_domains,
	int    n_beads,
	double result)
{
	SymbolicTrace tr;
	tr.metric_name         = "defect_indicator";
	tr.symbolic_expression = "D = n_l3_domains / N_beads";

	{
		std::ostringstream s;
		s << "D = " << n_l3_domains << " / " << n_beads;
		tr.substituted_expression = s.str();
	}
	{
		std::ostringstream s;
		s << "D = " << detail::fmt3(result);
		tr.result_expression = s.str();
	}

	tr.units          = "fraction";
	tr.interpretation = "Fraction of beads in L3 (high-defect) domains. Higher = more defective.";
	tr.source_stage   = "stage_analysis";
	return tr;
}

/**
 * stability_score
 *   S = clamp(0.5*Q_conv + 0.3*Q_pack + 0.2*clamp(1 - 10*D))
 */
inline SymbolicTrace stability_score(
	double convergence_quality,
	double packing_quality,
	double defect_indicator,
	double result)
{
	SymbolicTrace tr;
	tr.metric_name         = "stability_score";
	tr.symbolic_expression =
		"S = clamp(0.5*Q_conv + 0.3*Q_pack + 0.2*clamp(1 - 10*D))";

	{
		std::ostringstream s;
		s << std::fixed << std::setprecision(6);
		double defect_term = std::max(0.0, std::min(1.0, 1.0 - defect_indicator * 10.0));
		s << "S = clamp(0.5*" << convergence_quality
		  << " + 0.3*" << packing_quality
		  << " + 0.2*clamp(1 - 10*" << defect_indicator << "))"
		  << "  =  clamp(0.5*" << detail::fmt3(convergence_quality)
		  << " + 0.3*" << detail::fmt3(packing_quality)
		  << " + 0.2*" << detail::fmt3(defect_term) << ")";
		tr.substituted_expression = s.str();
	}
	{
		std::ostringstream s;
		s << "S = " << detail::fmt3(result);
		tr.result_expression = s.str();
	}

	tr.units          = "dimensionless";
	tr.interpretation = "Combined stability score from convergence, packing, and defect suppression.";
	tr.source_stage   = "stage_analysis";
	return tr;
}

/**
 * feature_vector_norm
 *   ||f|| = sqrt(sum(f_i^2))
 */
inline SymbolicTrace feature_vector_norm(
	const std::string& symbol,
	double             norm)
{
	SymbolicTrace tr;
	tr.metric_name         = "feature_vector_norm";
	tr.symbolic_expression = "||f|| = sqrt(sum(f_i^2))";

	{
		std::ostringstream s;
		s << "||f[" << symbol << "]|| = " << detail::fmt3(norm);
		tr.substituted_expression = s.str();
	}
	{
		std::ostringstream s;
		s << "||f|| = " << detail::fmt3(norm);
		tr.result_expression = s.str();
	}

	tr.units          = "feature-space units";
	tr.interpretation = "Euclidean norm of the 8-component formation feature vector.";
	tr.source_stage   = "stage_fingerprint";
	return tr;
}

} // namespace vsepr::pipeline::expr
