#pragma once
// =============================================================================
// src/analysis/diffusion_analysis.hpp
// =============================================================================
// Task 12  — Diffusion measurement layer (MSD, jumps, site residence, tortuosity)
// Task 12B — Macro transport inference layer (D_eff, anisotropy, mobility,
//             transport class, activation trend)
//
// Core principle
// ──────────────
//  The diffusion coefficient is NOT an input.
//  It is dragged out of the trajectory via MSD slope, as nature intended.
//
// Property hierarchy
// ──────────────────
//  xyzFull row          → positions / velocities / identities only
//  DiffusionRecord      → per-frame trajectory measurement (12)
//  TransportInference   → per-case macro inference (12B)
//
// Forbidden in state/xyzFull
//   diffusion_coefficient, mobility, transport_class, activation_energy
//
// Measurement (Task 12) outputs per frame
//   MSD_total, MSD_x, MSD_y, MSD_z
//   jump_count_cumulative, residence_time_current_site
//   path_length_cumulative, net_displacement
//   tortuosity_proxy (path/net ratio)
//   neighbor_exchange_count
//   motion_region (surface | interior)
//   KE_proxy (mean KE per atom)
//   energy_status
//
// Inference (Task 12B) outputs per case
//   diffusion_dimension
//   MSD_slope, D_eff_analysis_only
//   D_x, D_y, D_z, anisotropy_ratio
//   jump_rate, mean_residence_time
//   path_tortuosity
//   mobility_proxy, trap_strength_proxy, site_escape_frequency
//   transport_class
//   D_fit_quality
//   energy_status, valid_for_macro_inference
//
// Anti-black-box: every field public, every formula inline.
// =============================================================================

#include "core/math_vec3.hpp"
#include "core/stats/online_stats.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <limits>

namespace vsepr::diffusion {

// =============================================================================
// Task 12 — Per-frame measurement record
// =============================================================================

struct DiffusionRecord {
	uint64_t frame                    = 0;
	double   time                     = 0.0;
	double   MSD_total                = 0.0;   // Å²
	double   MSD_x                    = 0.0;
	double   MSD_y                    = 0.0;
	double   MSD_z                    = 0.0;
	double   net_displacement         = 0.0;   // Å  |r(t) - r(0)|
	double   path_length              = 0.0;   // Å  Σ|r(t)-r(t-1)| accumulated
	double   tortuosity_proxy         = 0.0;   // path / net_displacement (≥1)
	int      jump_count               = 0;     // cumulative site-to-site jumps
	double   residence_time           = 0.0;   // frames in current site
	int      neighbor_exchange_count  = 0;
	double   KE_proxy                 = 0.0;   // mean KE per atom (kJ/mol units)
	double   E_total                  = 0.0;
	double   E_rel_drift              = 0.0;
	std::string motion_region;                 // "surface" | "interior" | "mixed"

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << frame                   << '\t'
		   << time                    << '\t'
		   << MSD_total               << '\t'
		   << MSD_x                   << '\t'
		   << MSD_y                   << '\t'
		   << MSD_z                   << '\t'
		   << net_displacement        << '\t'
		   << path_length             << '\t'
		   << tortuosity_proxy        << '\t'
		   << jump_count              << '\t'
		   << residence_time          << '\t'
		   << neighbor_exchange_count << '\t'
		   << KE_proxy                << '\t'
		   << E_total                 << '\t'
		   << E_rel_drift             << '\t'
		   << motion_region;
		return ss.str();
	}

	static std::string tsv_header() {
		return "frame\ttime"
			   "\tMSD_total\tMSD_x\tMSD_y\tMSD_z"
			   "\tnet_displacement\tpath_length\ttortuosity_proxy"
			   "\tjump_count\tresidence_time\tneighbor_exchange_count"
			   "\tKE_proxy\tE_total\tE_rel_drift\tmotion_region";
	}
};

// =============================================================================
// Task 12B — Per-case macro transport inference record
// =============================================================================

struct TransportInference {
	std::string case_id;
	int         diffusion_dimension       = 3;
	double      MSD_slope                 = 0.0;   // Å²/fs from linear fit
	double      D_eff_analysis_only       = 0.0;   // Å²/fs  = MSD_slope / (2*dim)
	double      D_x                       = 0.0;
	double      D_y                       = 0.0;
	double      D_z                       = 0.0;
	double      anisotropy_ratio          = 1.0;   // max(D) / min_nonzero(D)
	int         jump_count                = 0;
	double      jump_rate                 = 0.0;   // jumps / fs
	double      mean_residence_time       = 0.0;   // fs per visited site
	double      path_tortuosity           = 0.0;
	double      mobility_proxy            = 0.0;   // D_eff / KE_proxy (dimensionless ratio)
	double      trap_strength_proxy       = 0.0;   // 1 / site_escape_frequency
	double      site_escape_frequency     = 0.0;   // jumps / frames_in_sites
	std::string transport_class;                   // analysis-only label
	std::string anisotropy_class;
	double      D_fit_quality             = 0.0;   // R² of MSD linear fit [0,1]
	double      D_fit_window_start        = 0.0;   // time window used for fit
	double      D_fit_window_end          = 0.0;
	std::string energy_status;                     // "stable" | "drifting" | "unstable"
	bool        valid_for_macro_inference = false;

	// Defect-relative ratios (filled in comparison step, not here)
	double      D_ratio_vs_ideal          = 1.0;   // D_this / D_ideal

	// Activation trend (multi-T runs)
	double      activation_trend_proxy    = 0.0;   // slope of ln(D) vs 1/KE
	std::string activation_class;                  // "thermal" | "weak" | "athermal" | "insufficient_data"

	// Formation-context annotation (analysis-only; never stored in State)
	double      ref_energy_per_atom       = 0.0;   // U/atom of ideal reference crystal [kcal/mol]
	double      energy_per_atom           = 0.0;   // U/atom of this case at t=0 [kcal/mol]
	double      formation_energy_proxy    = 0.0;   // energy_per_atom - ref_energy_per_atom (ΔU proxy)
	std::string formation_context;                 // "ideal_reference" | "defect_modified" | "thermally_activated" | "surface_case" | "unknown"

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << case_id                   << '\t'
		   << diffusion_dimension       << '\t'
		   << MSD_slope                 << '\t'
		   << D_eff_analysis_only       << '\t'
		   << D_x                       << '\t'
		   << D_y                       << '\t'
		   << D_z                       << '\t'
		   << anisotropy_ratio          << '\t'
		   << jump_count                << '\t'
		   << jump_rate                 << '\t'
		   << mean_residence_time       << '\t'
		   << path_tortuosity           << '\t'
		   << mobility_proxy            << '\t'
		   << trap_strength_proxy       << '\t'
		   << site_escape_frequency     << '\t'
		   << transport_class           << '\t'
		   << anisotropy_class          << '\t'
		   << D_ratio_vs_ideal          << '\t'
		   << D_fit_quality             << '\t'
		   << energy_status             << '\t'
		   << (valid_for_macro_inference ? "yes" : "no") << '\t'
		   << std::setprecision(4)
		   << ref_energy_per_atom       << '\t'
		   << energy_per_atom           << '\t'
		   << formation_energy_proxy    << '\t'
		   << formation_context;
		return ss.str();
	}

	static std::string tsv_header() {
		return "case_id\tdiffusion_dimension"
			   "\tMSD_slope\tD_eff_analysis_only"
			   "\tD_x\tD_y\tD_z\tanisotropy_ratio"
			   "\tjump_count\tjump_rate\tmean_residence_time"
			   "\tpath_tortuosity\tmobility_proxy\ttrap_strength_proxy"
			   "\tsite_escape_frequency\ttransport_class\tanisotropy_class"
			   "\tD_ratio_vs_ideal\tD_fit_quality"
			   "\tenergy_status\tvalid_for_macro_inference"
			   "\tref_energy_per_atom\tenergy_per_atom"
			   "\tformation_energy_proxy\tformation_context";
	}
};

// =============================================================================
// Helpers — linear least-squares slope fit  (Σ(x-x̄)(y-ȳ) / Σ(x-x̄)²)
// =============================================================================

struct LinFitResult {
	double slope     = 0.0;
	double intercept = 0.0;
	double r2        = 0.0;   // coefficient of determination [0,1]
	bool   valid     = false; // false if fewer than 2 points or zero variance in x
};

inline LinFitResult linear_fit(
	const std::vector<double>& x,
	const std::vector<double>& y)
{
	const int n = static_cast<int>(std::min(x.size(), y.size()));
	if (n < 2) return {};

	double sx = 0, sy = 0;
	for (int i = 0; i < n; ++i) { sx += x[i]; sy += y[i]; }
	const double mx = sx / n, my = sy / n;

	double sxx = 0, sxy = 0, syy = 0;
	for (int i = 0; i < n; ++i) {
		const double dx = x[i] - mx, dy = y[i] - my;
		sxx += dx * dx; sxy += dx * dy; syy += dy * dy;
	}

	if (sxx < 1e-30) return {};

	LinFitResult r;
	r.slope     = sxy / sxx;
	r.intercept = my - r.slope * mx;
	r.r2        = (syy > 1e-30) ? (sxy * sxy) / (sxx * syy) : 1.0;
	r.valid     = true;
	return r;
}

// =============================================================================
// Task 12 — DiffusionTracker
// =============================================================================
// Feed frames of positions + velocities; get a DiffusionRecord per frame.
//
// Usage
//   DiffusionTracker tr;
//   tr.set_reference(initial_positions);
//   tr.set_surface_z(z_threshold);     // optional — enables surface/interior
//   tr.set_jump_radius(r_site);        // optional — enables jump counting
//   tr.set_baseline(E0);
//   for each frame:
//     auto row = tr.compute(frame, time, E_total, positions, velocities);

struct DiffusionTracker {
	// Configuration (all public)
	double r_jump         = 3.5;   // Å — displacement > r_jump = site transition
	double surface_z_min  = 1e30;  // Å — z above which = "surface"
	double E0             = 0.0;

	void set_reference(const std::vector<vsepr::Vec3>& ref) {
		ref_pos_   = ref;
		prev_pos_  = ref;
		// Build site IDs: integer grid index (1 site per atom at t=0)
		site_id_.assign(ref.size(), 0);
		for (std::size_t i = 0; i < ref.size(); ++i)
			site_id_[i] = static_cast<int>(i);
		path_length_.assign(ref.size(), 0.0);
		visit_count_.assign(ref.size(), 0);
		has_ref_ = true;
	}

	void set_baseline(double E_total_0) { E0 = E_total_0; }

	DiffusionRecord compute(
		uint64_t                        frame_idx,
		double                          sim_time,
		double                          E_total,
		const std::vector<vsepr::Vec3>& pos,
		const std::vector<vsepr::Vec3>& vel)
	{
		DiffusionRecord row;
		row.frame   = frame_idx;
		row.time    = sim_time;
		row.E_total = E_total;
		const double denom = std::abs(E0) > 1e-12 ? std::abs(E0) : 1.0;
		row.E_rel_drift = (E_total - E0) / denom;

		if (!has_ref_ || pos.empty()) return row;

		const int N = static_cast<int>(std::min(pos.size(), ref_pos_.size()));

		// ── MSD per axis ─────────────────────────────────────────────────────
		double sum_x2 = 0, sum_y2 = 0, sum_z2 = 0;
		for (int i = 0; i < N; ++i) {
			const double dx = pos[i].x - ref_pos_[i].x;
			const double dy = pos[i].y - ref_pos_[i].y;
			const double dz = pos[i].z - ref_pos_[i].z;
			sum_x2 += dx * dx;
			sum_y2 += dy * dy;
			sum_z2 += dz * dz;
		}
		row.MSD_x     = sum_x2 / N;
		row.MSD_y     = sum_y2 / N;
		row.MSD_z     = sum_z2 / N;
		row.MSD_total = row.MSD_x + row.MSD_y + row.MSD_z;

		// ── Net displacement (centroid tracer) ────────────────────────────────
		// Mean displacement of all atoms from their starting positions
		double sum_d = 0;
		for (int i = 0; i < N; ++i)
			sum_d += (pos[i] - ref_pos_[i]).norm();
		row.net_displacement = sum_d / N;

		// ── Path length accumulation ──────────────────────────────────────────
		double total_path = 0;
		int    jumps      = 0;
		int    n_exc      = 0;
		for (int i = 0; i < N; ++i) {
			const double step = (pos[i] - prev_pos_[i]).norm();
			path_length_[i] += step;
			total_path       += path_length_[i];
			// Jump detection: if step this frame > r_jump threshold
			if (step > r_jump) {
				++jumps;
				++visit_count_[i];
			}
		}
		jump_count_cum_ += jumps;
		row.jump_count       = jump_count_cum_;
		row.path_length      = total_path / N;
		row.neighbor_exchange_count = n_exc;

		// ── Tortuosity ────────────────────────────────────────────────────────
		row.tortuosity_proxy = (row.net_displacement > 1e-6)
			? row.path_length / row.net_displacement : 1.0;

		// ── Residence time ───────────────────────────────────────────────────
		// Frames since last jump on atom 0 (tracer proxy)
		if (jumps > 0 && N > 0) residence_frames_ = 0;
		else                     ++residence_frames_;
		row.residence_time = static_cast<double>(residence_frames_);

		// ── KE proxy (mean KE per atom, raw velocity units) ──────────────────
		if (!vel.empty()) {
			double ke = 0;
			const int Nv = static_cast<int>(std::min(vel.size(), pos.size()));
			for (int i = 0; i < Nv; ++i)
				ke += vel[i].x*vel[i].x + vel[i].y*vel[i].y + vel[i].z*vel[i].z;
			row.KE_proxy = ke / Nv;
		}

		// ── Surface/interior classification ──────────────────────────────────
		int surf_count = 0;
		for (int i = 0; i < N; ++i)
			if (pos[i].z >= surface_z_min) ++surf_count;
		if (surf_count == 0)         row.motion_region = "interior";
		else if (surf_count == N)    row.motion_region = "surface";
		else                         row.motion_region = "mixed";

		prev_pos_ = std::vector<vsepr::Vec3>(pos.begin(), pos.begin() + N);
		return row;
	}

	void reset() {
		ref_pos_.clear(); prev_pos_.clear();
		site_id_.clear(); path_length_.clear(); visit_count_.clear();
		jump_count_cum_ = 0; residence_frames_ = 0; has_ref_ = false;
	}

private:
	std::vector<vsepr::Vec3> ref_pos_;
	std::vector<vsepr::Vec3> prev_pos_;
	std::vector<int>          site_id_;
	std::vector<double>       path_length_;
	std::vector<int>          visit_count_;
	int      jump_count_cum_   = 0;
	int      residence_frames_ = 0;
	bool     has_ref_          = false;
};

// =============================================================================
// Task 12B — TransportAnalyzer
// =============================================================================
// Consumes a DiffusionRecord log and produces a TransportInference.
//
// Usage
//   TransportAnalyzer ta;
//   ta.dt = 0.02;
//   ta.fit_fraction = 0.5;    // use last 50% of trajectory for MSD slope
//   auto inf = ta.infer("case_id", records);

struct TransportAnalyzer {
	double dt               = 0.02;   // fs per step
	double fit_fraction     = 0.5;    // fraction of tail to use for linear fit
	double drift_threshold  = 0.05;   // rel energy drift → "unstable"
	double jump_threshold_A = 3.5;    // Å — same as DiffusionTracker
	int    dim              = 3;      // default: 3D bulk

	TransportInference infer(
		const std::string&               case_id,
		const std::vector<DiffusionRecord>& log) const
	{
		TransportInference inf;
		inf.case_id = case_id;
		if (log.empty()) {
			inf.energy_status = "no_data";
			inf.transport_class = "invalid_energy_drift";
			return inf;
		}

		// ── Energy status ─────────────────────────────────────────────────────
		const double max_drift = [&](){
			double m = 0;
			for (const auto& r : log) m = std::max(m, std::abs(r.E_rel_drift));
			return m;
		}();
		if (max_drift > drift_threshold) {
			inf.energy_status             = "unstable";
			inf.valid_for_macro_inference = false;
			inf.transport_class           = "invalid_energy_drift";
			// Still compute D from trajectory — mark as suspicious
		} else {
			inf.energy_status = "stable";
			inf.valid_for_macro_inference = true;
		}

		// ── MSD linear fit on tail fraction ──────────────────────────────────
		const int N = static_cast<int>(log.size());
		const int fit_start = std::max(0, static_cast<int>(N * (1.0 - fit_fraction)));
		std::vector<double> t_fit, msd_total, msd_x, msd_y, msd_z;
		t_fit.reserve(N - fit_start);
		for (int i = fit_start; i < N; ++i) {
			t_fit.push_back(log[i].time);
			msd_total.push_back(log[i].MSD_total);
			msd_x.push_back(log[i].MSD_x);
			msd_y.push_back(log[i].MSD_y);
			msd_z.push_back(log[i].MSD_z);
		}

		const auto fit_total = linear_fit(t_fit, msd_total);
		const auto fit_x     = linear_fit(t_fit, msd_x);
		const auto fit_y     = linear_fit(t_fit, msd_y);
		const auto fit_z     = linear_fit(t_fit, msd_z);

		inf.diffusion_dimension  = dim;
		inf.D_fit_window_start   = t_fit.empty() ? 0 : t_fit.front();
		inf.D_fit_window_end     = t_fit.empty() ? 0 : t_fit.back();
		inf.D_fit_quality        = fit_total.valid ? fit_total.r2 : 0.0;

		if (fit_total.valid) {
			// MSD = 2*dim*D*t  →  D = slope / (2*dim)
			inf.MSD_slope           = fit_total.slope;
			inf.D_eff_analysis_only = fit_total.slope / (2.0 * dim);
		}
		if (fit_x.valid) inf.D_x = fit_x.slope / 2.0;
		if (fit_y.valid) inf.D_y = fit_y.slope / 2.0;
		if (fit_z.valid) inf.D_z = fit_z.slope / 2.0;

		// ── Anisotropy ────────────────────────────────────────────────────────
		const double dmax = std::max({inf.D_x, inf.D_y, inf.D_z});
		double dmin = std::numeric_limits<double>::max();
		for (double d : {inf.D_x, inf.D_y, inf.D_z})
			if (d > 1e-12 && d < dmin) dmin = d;
		if (dmin > 1e-12 && dmax > 1e-12)
			inf.anisotropy_ratio = dmax / dmin;
		else
			inf.anisotropy_ratio = 1.0;

		if      (inf.anisotropy_ratio > 5.0)  inf.anisotropy_class = "channel_like_transport";
		else if (inf.anisotropy_ratio > 2.0)  inf.anisotropy_class = "anisotropic_transport";
		else if (inf.anisotropy_ratio > 1.2)  inf.anisotropy_class = "weakly_anisotropic";
		else                                  inf.anisotropy_class = "isotropic_transport";

		// ── Jump / mobility metrics ───────────────────────────────────────────
		inf.jump_count = log.back().jump_count;
		const double total_time = log.back().time - log.front().time;
		inf.jump_rate = (total_time > 0) ? inf.jump_count / total_time : 0.0;

		// Mean path tortuosity over all frames
		double tor_sum = 0;
		for (const auto& r : log) tor_sum += r.tortuosity_proxy;
		inf.path_tortuosity = tor_sum / N;

		// Site escape frequency — jumps per total frames × N atoms
		inf.site_escape_frequency = (N > 0) ? static_cast<double>(inf.jump_count) / N : 0.0;
		inf.trap_strength_proxy   = (inf.site_escape_frequency > 1e-12) ?
			1.0 / inf.site_escape_frequency : 9999.0;  // sentinel: no escapes observed

		// Mean residence time
		double res_sum = 0;
		for (const auto& r : log) res_sum += r.residence_time;
		inf.mean_residence_time = res_sum / N;

		// Mobility proxy: D_eff / KE_proxy (thermal activity)
		const double mean_ke = [&](){
			double s = 0;
			for (const auto& r : log) s += r.KE_proxy;
			return (N > 0) ? s / N : 1.0;
		}();
		inf.mobility_proxy = (mean_ke > 1e-12) ? inf.D_eff_analysis_only / mean_ke : 0.0;

		// ── Transport class ───────────────────────────────────────────────────
		if (!inf.valid_for_macro_inference) {
			inf.transport_class = "invalid_energy_drift";
		} else if (inf.D_eff_analysis_only < 1e-10 && inf.jump_count < 2) {
			inf.transport_class = "no_transport";
		} else if (inf.D_eff_analysis_only < 1e-6 && inf.path_tortuosity < 1.05) {
			inf.transport_class = "localized_vibration";
		} else if (inf.jump_count > 0 && inf.D_fit_quality < 0.6) {
			inf.transport_class = "hopping_transport";
		} else if (inf.D_fit_quality >= 0.6 && inf.D_eff_analysis_only > 1e-6) {
			if      (inf.anisotropy_ratio > 5.0)  inf.transport_class = "anisotropic_diffusion";
			else                                  inf.transport_class = "bulk_diffusion";
		} else {
			inf.transport_class = "localized_vibration";
		}

		return inf;
	}
};

// =============================================================================
// Task 12B — Defect transport comparison
// =============================================================================
// Compute D_ratio_vs_ideal for a set of inferences; fills D_ratio_vs_ideal.

inline void annotate_defect_ratios(
	TransportInference&       target,
	const TransportInference& ideal_ref)
{
	const double D_ref = (std::abs(ideal_ref.D_eff_analysis_only) > 1e-20)
		? ideal_ref.D_eff_analysis_only : 1e-20;
	target.D_ratio_vs_ideal = target.D_eff_analysis_only / D_ref;

	// Upgrade transport class based on ratio
	if (target.valid_for_macro_inference && ideal_ref.valid_for_macro_inference) {
		if (target.D_ratio_vs_ideal > 2.0)
			target.transport_class = "vacancy_assisted_transport";
		else if (target.D_ratio_vs_ideal > 1.5 && target.jump_count > ideal_ref.jump_count)
			target.transport_class = "interstitial_assisted_transport";
	}
}

// =============================================================================
// Task 12B — Activation trend (multi-temperature sweep)
// =============================================================================
// Fit ln(D_eff) vs 1/KE_proxy over a set of inferences at different activities.

struct ActivationTrend {
	double      slope_proxy            = 0.0;  // d ln(D) / d(1/KE)
	double      activation_energy_proxy= 0.0;  // |slope_proxy|  (proxy, not true Ea)
	double      r2                     = 0.0;
	std::string activation_class;              // "thermal" | "weak" | "athermal" | "insufficient_data"

	static ActivationTrend fit(
		const std::vector<TransportInference>& cases,
		const std::vector<double>&             mean_KE_per_case)
	{
		ActivationTrend tr;
		if (cases.size() < 2 || cases.size() != mean_KE_per_case.size()) {
			tr.activation_class = "insufficient_data";
			return tr;
		}

		// Only use valid, non-zero D cases
		std::vector<double> inv_KE, ln_D;
		for (std::size_t i = 0; i < cases.size(); ++i) {
			if (!cases[i].valid_for_macro_inference) continue;
			if (cases[i].D_eff_analysis_only <= 0)   continue;
			if (mean_KE_per_case[i] <= 0)             continue;
			inv_KE.push_back(1.0 / mean_KE_per_case[i]);
			ln_D.push_back(std::log(cases[i].D_eff_analysis_only));
		}

		if (static_cast<int>(inv_KE.size()) < 2) {
			tr.activation_class = "insufficient_data";
			return tr;
		}

		const auto fit = linear_fit(inv_KE, ln_D);
		if (!fit.valid) {
			tr.activation_class = "insufficient_data";
			return tr;
		}

		tr.slope_proxy             = fit.slope;
		tr.activation_energy_proxy = std::abs(fit.slope);
		tr.r2                      = fit.r2;

		if      (fit.r2 > 0.8 && fit.slope < -0.01) tr.activation_class = "thermal";
		else if (fit.r2 > 0.5 && fit.slope < 0)     tr.activation_class = "weak_thermal";
		else if (std::abs(fit.slope) < 0.005)        tr.activation_class = "athermal";
		else                                         tr.activation_class = "weak_thermal";

		return tr;
	}
};

// =============================================================================
// Formation-context annotation
// =============================================================================
// Stamps formation-energy-proxies onto TransportInference records.
// Inputs: the ideal reference inference (for ref_energy_per_atom),
//         the initial total energy of this case, and its atom count.
// The formation_energy_proxy = energy_per_atom - ref_energy_per_atom
// is a ΔU proxy only — not a true DFT formation enthalpy.
// It is labelled "proxy" because we cannot claim Arrhenius without a
// physically calibrated temperature model.

inline void annotate_formation_context(
	TransportInference&       target,
	double                    initial_E_total,   // U at t=0 for this case [kcal/mol]
	uint32_t                  N_atoms,
	const TransportInference& ideal_ref,
	const std::string&        role)             // "ideal_reference" | "vacancy" | "interstitial" | ...
{
	target.energy_per_atom     = (N_atoms > 0) ? initial_E_total / N_atoms : 0.0;
	target.ref_energy_per_atom = ideal_ref.ref_energy_per_atom;
	target.formation_energy_proxy = target.energy_per_atom - target.ref_energy_per_atom;

	// Classify formation context
	if (role == "ideal" || role == "fcc" || role == "bcc")
		target.formation_context = "ideal_reference";
	else if (role == "vacancy" || role == "interstitial" || role == "mixed_defect")
		target.formation_context = "defect_modified";
	else if (role == "thermal_lo" || role == "thermal_hi")
		target.formation_context = "thermally_activated";
	else if (role == "surface" || role == "channel_1d")
		target.formation_context = "surface_case";
	else
		target.formation_context = "unknown";
}

// Convenience overload: stamp ideal record as its own reference
inline void annotate_formation_context_ideal(
	TransportInference& ideal,
	double              initial_E_total,
	uint32_t            N_atoms)
{
	ideal.energy_per_atom       = (N_atoms > 0) ? initial_E_total / N_atoms : 0.0;
	ideal.ref_energy_per_atom   = ideal.energy_per_atom;
	ideal.formation_energy_proxy = 0.0;
	ideal.formation_context     = "ideal_reference";
}

} // namespace vsepr::diffusion
