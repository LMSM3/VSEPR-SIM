#pragma once
/**
 * interaction_channel.hpp  -  SM-channel row, conservation gates, and kappa_total
 *
 * InteractionChannelRow
 *   Nine channel scores for a particle pair (i,j), inspired by Standard Model
 *   force carriers and transition types.  Only m_gamma (EM) and m_ann
 *   (annihilation) produce nonzero kappa contributions in this release;
 *   all others are gated but set to 0 until their kernels are implemented.
 *
 * ConservationGates
 *   Boolean gate stack:  G_E  G_p  G_q  G_l  G_b  G_c  G_s
 *   conservation_pass() = AND of all enabled gates.
 *
 * ChannelPolicy
 *   Weights and thresholds governing the kappa_total computation and
 *   the annihilation / resonance decision.
 *
 * kappa_total()
 *   Weighted combination:  w_Z*kappa_Z + w_q*kappa_q + w_s*kappa_s
 *                        + w_c*kappa_c + w_E*kappa_E
 *
 * v5.2.0  |  WO-SM-IDENTITY  |  v5.0.0-main
 */

#include "IDENTITY/identity_matrix.hpp"
#include "IDENTITY/particle_identity.hpp"
#include "PARTICLES/qcd_particle.hpp"

namespace vsepr {
namespace identity {

// ============================================================================
// Nine SM-inspired interaction channel scores for one pair (i,j)
// ============================================================================

struct InteractionChannelRow {
	// Nonzero in this release:
	double m_gamma {0.0};   // EM / photon-mediated channel score
	double m_ann   {0.0};   // annihilation candidate score

	// Gated but zero until kernels are implemented:
	double m_g     {0.0};   // strong / gluon-mediated  (Cornell fires when > threshold)
	double m_W     {0.0};   // weak charged current
	double m_Z     {0.0};   // weak neutral current
	double m_H     {0.0};   // Higgs / scalar coupling
	double m_pair  {0.0};   // pair production
	double m_decay {0.0};   // unstable decay
	double m_conf  {0.0};   // confinement / hadronization

	// Convenience: is the color / QCD channel active above threshold?
	bool color_channel_active(double threshold = 0.05) const {
		return m_g > threshold;
	}
};

// ============================================================================
// kappa sub-scores for a pair
// ============================================================================

struct KappaScores {
	double kappa_Z  {0.0};   // 2×2 matrix opposition
	double kappa_q  {0.0};   // charge complementarity
	double kappa_s  {0.0};   // spin compatibility
	double kappa_c  {0.0};   // color / anti-color compatibility
	double kappa_E  {0.0};   // energy-channel availability
};

// ============================================================================
// Channel policy weights and decision thresholds
// ============================================================================

struct ChannelPolicy {
	// kappa weights (must sum to 1 for normalized kappa_total)
	double w_Z  {0.40};
	double w_q  {0.30};
	double w_s  {0.10};
	double w_c  {0.10};
	double w_E  {0.10};

	// Annihilation gate thresholds
	double r_c          {0.20};    // max separation for annihilation (VSIM Å analog)
	double E_c          {0.50};    // min relative energy for annihilation
	double kappa_c      {0.70};    // min kappa_total for annihilation
	double L_ann        {0.50};    // min state loss L_ij = -delta_I for annihilation

	// Color channel activation threshold
	double m_g_threshold {0.05};

	// EM channel: charge scale (pairs with complementary non-zero charges score 1)
	double em_charge_scale {1.0};

	static ChannelPolicy defaults() { return {}; }
};

// ============================================================================
// Conservation gates  G = G_E AND G_p AND G_q AND G_l AND G_b AND G_c AND G_s
// ============================================================================

struct ConservationGates {
	bool G_E {true};   // energy conservation
	bool G_p {true};   // momentum conservation
	bool G_q {true};   // electric charge conservation
	bool G_l {true};   // lepton-family bookkeeping
	bool G_b {true};   // baryon-number bookkeeping
	bool G_c {true};   // color neutrality
	bool G_s {true};   // spin / angular-momentum compatibility

	bool conservation_pass() const {
		return G_E && G_p && G_q && G_l && G_b && G_c && G_s;
	}
};

// ============================================================================
// Compute conservation gates for a pair
// ============================================================================

inline ConservationGates evaluate_conservation(
	const ParticleIdentity& a,
	const ParticleIdentity& b,
	double E_rel,
	double p_balance_error,
	double gate_tol_q   = 1e-6,
	double gate_tol_p   = 1e-3,
	double gate_tol_E   = 1e-3)
{
	ConservationGates g;

	// G_E: relative energy must be positive (kinematically allowed)
	g.G_E = (E_rel >= 0.0) && (E_rel < 1e6);   // sanity upper bound

	// G_p: momentum balance check — caller supplies pre-computed |Δp|/|p_total|
	g.G_p = (p_balance_error < gate_tol_p);

	// G_q: total charge must be conserved (just checks it is finite / defined here;
	//       the actual product-side check lives in evaluate_pair after channel selection)
	double q_sum = a.charge + b.charge;
	g.G_q = std::isfinite(q_sum);   // refined per-channel in evaluate_pair

	// G_l: lepton families must be compatible (same or 0)
	g.G_l = (a.lepton_family == 0 || b.lepton_family == 0 ||
			 a.lepton_family == b.lepton_family);

	// G_b: baryon number is additive — must be tracked but not gated here
	//       (true unless both are fractional quarks without a companion)
	g.G_b = true;

	// G_c: color must be neutral in aggregate (quark + anticolor = neutral-capable)
	bool a_colored = (a.color != particles::ColorCharge::Neutral &&
					  a.color != particles::ColorCharge::White);
	bool b_colored = (b.color != particles::ColorCharge::Neutral &&
					  b.color != particles::ColorCharge::White);
	if (a_colored && b_colored) {
		g.G_c = (b.color == particles::complementary_color(a.color));
	} else {
		g.G_c = true;
	}

	// G_s: spin/angular momentum — half+half or int+int are compatible pairings
	g.G_s = (a.spin_proxy == b.spin_proxy) ||
			(a.spin_proxy == SpinProxy::Zero) ||
			(b.spin_proxy == SpinProxy::Zero);

	return g;
}

// ============================================================================
// kappa sub-score computation for a pair
// ============================================================================

inline KappaScores compute_kappa(const ParticleIdentity& a, const ParticleIdentity& b) {
	KappaScores k;

	// kappa_Z: 2×2 matrix opposition — 0=identical, 1=fully opposed
	k.kappa_Z = opposition(a.Z2, b.Z2);

	// kappa_q: charge complementarity — 1 when charges sum to 0, scale by magnitude
	double q_sum = a.charge + b.charge;
	double q_mag = std::abs(a.charge) + std::abs(b.charge);
	k.kappa_q = (q_mag > 1e-12) ? (1.0 - std::abs(q_sum) / q_mag) : 0.0;

	// kappa_s: spin compatibility (1 = same type, 0 = different)
	k.kappa_s = (a.spin_proxy == b.spin_proxy) ? 1.0 : 0.5;

	// kappa_c: color / anticolor compatibility
	bool a_colored = (a.color != particles::ColorCharge::Neutral &&
					  a.color != particles::ColorCharge::White);
	bool b_colored = (b.color != particles::ColorCharge::Neutral &&
					  b.color != particles::ColorCharge::White);
	if (a_colored && b_colored) {
		k.kappa_c = (b.color == particles::complementary_color(a.color)) ? 1.0 : 0.0;
	} else {
		k.kappa_c = 0.0;
	}

	// kappa_E: energy-channel availability — placeholder (1 = available)
	k.kappa_E = 1.0;

	return k;
}

// ============================================================================
// kappa_total  =  w_Z*kappa_Z + w_q*kappa_q + w_s*kappa_s + w_c*kappa_c + w_E*kappa_E
// ============================================================================

inline double kappa_total(const KappaScores& k, const ChannelPolicy& policy) {
	return policy.w_Z * k.kappa_Z
		 + policy.w_q * k.kappa_q
		 + policy.w_s * k.kappa_s
		 + policy.w_c * k.kappa_c
		 + policy.w_E * k.kappa_E;
}

// ============================================================================
// Channel row scoring — EM and annihilation only produce nonzero contributions
// ============================================================================

inline InteractionChannelRow score_channels(
	const ParticleIdentity& a,
	const ParticleIdentity& b,
	const KappaScores&      k,
	const ChannelPolicy&    policy,
	double                  r,
	double                  E_rel)
{
	InteractionChannelRow row;

	// m_gamma: EM channel — nonzero whenever either particle is charged
	double q_product = a.charge * b.charge;
	if (std::abs(a.charge) > 1e-9 || std::abs(b.charge) > 1e-9) {
		// Attractive if opposite signs, repulsive if same sign — store magnitude
		row.m_gamma = std::abs(q_product) / (policy.em_charge_scale + 1e-15);
		row.m_gamma = std::min(row.m_gamma, 1.0);
	}

	// m_g: strong — active only if both particles are color-charged
	bool a_col = (a.color != particles::ColorCharge::Neutral &&
				  a.color != particles::ColorCharge::White);
	bool b_col = (b.color != particles::ColorCharge::Neutral &&
				  b.color != particles::ColorCharge::White);
	row.m_g = (a_col && b_col) ? 1.0 : 0.0;

	// m_ann: annihilation channel score
	//   = kappa_total * charge-complementarity * (1 / (1 + r)) soft-range factor
	double kt = kappa_total(k, policy);
	double charge_comp = k.kappa_q;
	double range_factor = 1.0 / (1.0 + r);
	row.m_ann = kt * charge_comp * range_factor;
	row.m_ann = std::max(0.0, std::min(row.m_ann, 1.0));

	// m_W, m_Z, m_H, m_pair, m_decay, m_conf — gated, zero until kernels added
	row.m_W    = 0.0;
	row.m_Z    = 0.0;
	row.m_H    = 0.0;
	row.m_pair = 0.0;
	row.m_decay = 0.0;
	row.m_conf  = 0.0;

	return row;
}

} // namespace identity
} // namespace vsepr
