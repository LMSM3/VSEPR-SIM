#pragma once
/**
 * interaction_candidate.hpp  -  InteractionCandidate and evaluate_pair()
 *
 * evaluate_pair() is the core pair evaluator for the SM-channel identity layer.
 * It produces an InteractionCandidate record containing:
 *   - All kappa sub-scores and kappa_total
 *   - The nine-channel InteractionChannelRow
 *   - Conservation gate result
 *   - delta_I and state_loss
 *   - Boolean classification flags (annihilation_candidate, positive_information_event, ...)
 *
 * Annihilation gate (all must be true):
 *   r_ij  < policy.r_c
 *   E_rel > policy.E_c
 *   kappa_total > policy.kappa_c
 *   L_ij  > policy.L_ann       where L_ij = -delta_I
 *   conservation_pass == true
 *
 * Positive information event:
 *   delta_I > 0
 *   (short-lived structured state forming — resonance / pre-hadron / matrix alignment)
 *
 * Identity mutation (weak identity transition):
 *   channel.m_W > 0  (currently always 0 until W-kernel is implemented;
 *                     flag is set for forward-compatibility with logging)
 *
 * The function is header-only, stateless, allocation-free.
 *
 * v5.2.0  |  WO-SM-IDENTITY  |  v5.0.0-main
 */

#include "IDENTITY/interaction_channel.hpp"
#include "IDENTITY/particle_identity.hpp"

#include <cmath>

namespace vsepr {
namespace identity {

// ============================================================================
// InteractionCandidate  -  full evaluation record for one pair (i,j)
// ============================================================================

struct InteractionCandidate {
	// --- pair indexing -------------------------------------------------------
	int64_t i {-1};
	int64_t j {-1};

	// --- geometric / kinematic inputs (pre-computed by caller) ---------------
	double r               {0.0};   // separation |r_i - r_j|
	double E_rel           {0.0};   // relative kinetic energy
	double p_balance_error {0.0};   // |Δp|/|p_total| momentum balance check

	// --- kappa sub-scores ----------------------------------------------------
	KappaScores kappa {};

	// --- aggregated score ----------------------------------------------------
	double kappa_total_val {0.0};

	// --- channel row ---------------------------------------------------------
	InteractionChannelRow channels {};

	// --- conservation gates --------------------------------------------------
	ConservationGates gates {};
	bool conservation_pass {false};

	// --- identity-information tracking ---------------------------------------
	double delta_I     {0.0};   // I_out - I_in (negative = loss, positive = gain)
	double state_loss  {0.0};   // L_ij = -delta_I  (>= 0 by definition)

	// --- channel-level scores (per SM analogy) --------------------------------
	double annihilation_score        {0.0};
	double shower_score              {0.0};   // reserved for parton-branch kernel
	double weak_transition_score     {0.0};   // reserved for W-kernel
	double resonance_score           {0.0};

	// --- classification flags ------------------------------------------------
	bool annihilation_candidate      {false};
	bool annihilation_confirmed      {false};
	bool positive_information_event  {false};
	bool weak_identity_transition    {false};   // I_i^- != I_i^+
	bool color_exchange_active       {false};
	bool temporary_resonance         {false};

	// --- IKK hidden-active flags (IKK Doc I §3.3, Doc II §D.4) --------------
	// A particle is "hidden-active" when its net observable channel is zero
	// but its hidden intensity H_j > 0.  Classic example: neutral baryon
	// (net charge = 0, H_charge = 2/3 ≠ 0).  These flags track which
	// channel is in the hidden-active state for each member of the pair.
	bool a_hidden_active_charge {false};   // particle a: net q=0 but H_charge>0
	bool b_hidden_active_charge {false};   // particle b
	bool a_hidden_active_color  {false};   // particle a: color-neutral but H_color>0
	bool b_hidden_active_color  {false};   // particle b
	// Combined: at least one member of the pair is in the hidden-active state
	bool pair_has_hidden_charge_activity  {false};
};

// ============================================================================
// delta_I heuristic
//
// Computes the expected change in recoverable identity information for particle i
// after interacting with particle j, given the channel scores and kappa_total.
//
// Convention (State Loss Principle):
//   delta_I < 0  — identity information is lost (ordinary interaction loss)
//   delta_I = 0  — elastic / identity-preserving
//   delta_I > 0  — short-lived structured state forming (positive information event)
//
// This is a placeholder heuristic.  A proper kernel should derive delta_I from
// the branching ratios and energy-momentum of the selected channel.
// ============================================================================

inline double compute_delta_I(
	const ParticleIdentity&      a,
	const ParticleIdentity&      b,
	const InteractionChannelRow& ch,
	const KappaScores&           k,
	double                       r,
	double                       E_rel)
{
	// Base loss from EM scattering: small, proportional to channel strength
	double loss_em    = -0.05 * ch.m_gamma;

	// Annihilation: near-total loss for both particles
	double loss_ann   = -0.90 * ch.m_ann;

	// Color exchange: moderate loss, proportional to g channel
	double loss_color = -0.20 * ch.m_g * k.kappa_c;

	// Resonance / alignment gain: positive when Z-matrices are unusually compatible
	//   kappa_Z close to 1 = matrices are opposed = pre-annihilation ordering
	//   kappa_Z close to 0 = matrices aligned = possible resonance
	double alignment = 1.0 - k.kappa_Z;   // 1 = fully aligned
	double gain_resonance = (alignment > 0.80) ? +0.10 * alignment : 0.0;

	double dI = loss_em + loss_ann + loss_color + gain_resonance;

	// Clamp: identity cannot increase beyond its current value or drop below -I_a
	double floor = -(a.I_recoverable);
	double ceil  = +a.I_recoverable * 0.15;   // positive events are small
	return std::max(floor, std::min(ceil, dI));
}

// ============================================================================
// evaluate_pair()  -  main SM-channel pair evaluator
// ============================================================================

inline InteractionCandidate evaluate_pair(
	const ParticleIdentity& a,
	const ParticleIdentity& b,
	const ChannelPolicy&    policy,
	double                  r,
	double                  E_rel,
	double                  p_balance_error = 0.0)
{
	InteractionCandidate cand;
	cand.i               = a.id;
	cand.j               = b.id;
	cand.r               = r;
	cand.E_rel           = E_rel;
	cand.p_balance_error = p_balance_error;

	// 1. kappa sub-scores
	cand.kappa           = compute_kappa(a, b);
	cand.kappa_total_val = kappa_total(cand.kappa, policy);

	// 2. nine-channel scoring
	cand.channels = score_channels(a, b, cand.kappa, policy, r, E_rel);

	// 3. conservation gates
	cand.gates            = evaluate_conservation(a, b, E_rel, p_balance_error);
	cand.conservation_pass = cand.gates.conservation_pass();

	// 4. delta_I and state_loss
	cand.delta_I    = compute_delta_I(a, b, cand.channels, cand.kappa, r, E_rel);
	cand.state_loss = -cand.delta_I;   // L_ij = -delta_I, >= 0 for ordinary loss

	// 5. channel-level summary scores
	cand.annihilation_score    = cand.channels.m_ann;
	cand.shower_score          = cand.channels.m_g;   // gluon-emission proxy
	cand.weak_transition_score = cand.channels.m_W;
	cand.resonance_score       = (cand.delta_I > 0.0) ? cand.delta_I : 0.0;

	// 6. classification flags
	double L_ij = cand.state_loss;

	//   annihilation candidate: all five gates satisfied
	cand.annihilation_candidate =
		(r               < policy.r_c)     &&
		(E_rel           > policy.E_c)     &&
		(cand.kappa_total_val > policy.kappa_c) &&
		(L_ij            > policy.L_ann)   &&
		cand.conservation_pass;

	//   confirmed = candidate + charge sums to zero (tightest EM gate)
	cand.annihilation_confirmed =
		cand.annihilation_candidate &&
		(std::abs(a.charge + b.charge) < 1e-6);

	//   positive information event: delta_I > 0
	cand.positive_information_event = (cand.delta_I > 0.0);

	//   temporary resonance: positive event AND both particles are still intact
	cand.temporary_resonance =
		cand.positive_information_event &&
		(a.I_recoverable > 0.5) &&
		(b.I_recoverable > 0.5);

	//   weak identity transition: W channel active (zero for now — forward compat)
	cand.weak_identity_transition = (cand.channels.m_W > 0.0);

	//   color exchange: color channel active
	cand.color_exchange_active = cand.channels.color_channel_active(policy.m_g_threshold);

	// 7. IKK hidden-active flags (IKK Doc I §3.3 / Doc II §D.4)
	//    Detect net-zero observable channel with nonzero hidden intensity.
	cand.a_hidden_active_charge = a.hidden_active_charge();
	cand.b_hidden_active_charge = b.hidden_active_charge();

	//    Hidden-active color: color is White/Neutral but H_color > 0.
	//    Approximation: a hadron-scale White composite can have H_color > 0.
	auto color_neutral = [](const ParticleIdentity& p) {
		return (p.color == particles::ColorCharge::Neutral ||
				p.color == particles::ColorCharge::White);
	};
	cand.a_hidden_active_color = color_neutral(a) && (a.H_color > 1e-9);
	cand.b_hidden_active_color = color_neutral(b) && (b.H_color > 1e-9);

	cand.pair_has_hidden_charge_activity =
		cand.a_hidden_active_charge || cand.b_hidden_active_charge;

	return cand;
}

} // namespace identity
} // namespace vsepr
