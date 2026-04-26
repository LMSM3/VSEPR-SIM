#pragma once
// =============================================================================
// src/analysis/packing_analysis.hpp
// =============================================================================
// Task 13  — Bead/powder packing measurement layer
//            (contact network, coordination, void fraction, wall proxy)
// Task 13B — Macro packing property inference layer
//            (bulk density, porosity, compressibility, permeability,
//             sintering readiness, macro class)
//
// Core principle
// ──────────────
//  Porosity is NOT an input.  It is computed from measured geometry.
//  Bulk density is NOT an input.  It is computed from mass and volume.
//  Compressibility and permeability are proxies derived from contact/void data.
//
// Property hierarchy
// ──────────────────
//  xyzFull row          → positions / identities / radii only
//  PackingRecord        → per-frame geometry measurement (13)
//  PackingInference     → per-case macro inference (13B)
//
// Forbidden in state/xyzFull
//   porosity, bulk_density, compressibility, permeability, sintering_fraction
//
// Measurement (Task 13) outputs per frame
//   bead_count, container_volume
//   occupied_volume, packing_fraction_measured
//   mean_coordination, coordination_stddev
//   largest_cluster_fraction
//   wall_force_proxy (sum of z-component forces on boundary atoms)
//   mean_contact_lifetime (frames current contacts have persisted)
//   persistent_contact_fraction
//   void_connectivity_proxy (fraction of void cells reachable from outside)
//   settling_frame_flag
//   E_rel_drift
//
// Inference (Task 13B) outputs per case
//   bulk_density_inferred
//   bulk_density_proxy
//   mass_basis_available
//   porosity_inferred
//   porosity_proxy_uncorrected
//   load_bearing_network_score
//   packing_stability_score
//   compressibility_proxy
//   densification_rate_proxy
//   contact_growth_rate
//   permeability_proxy
//   flow_accessibility_score
//   sintering_readiness_proxy
//   neck_growth_candidate_score
//   packing_macro_class
//   energy_status
//   valid_for_macro_inference
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
#include <queue>

namespace vsepr::packing {

// =============================================================================
// Box geometry helper
// =============================================================================

struct PackingBox {
	double Lx = 0, Ly = 0, Lz = 0;
	double volume() const { return Lx * Ly * Lz; }
	bool valid() const { return Lx > 0 && Ly > 0 && Lz > 0; }
};

// =============================================================================
// Task 13 — Per-frame measurement record
// =============================================================================

struct PackingRecord {
	uint64_t frame                       = 0;
	double   time                        = 0.0;
	uint32_t bead_count                  = 0;
	double   container_volume            = 0.0;   // Å³
	double   occupied_volume             = 0.0;   // Å³ (sphere volumes, no overlap correction)
	double   packing_fraction_measured   = 0.0;   // occupied / container
	double   mean_coordination           = 0.0;   // contacts per bead
	double   coordination_stddev         = 0.0;
	double   largest_cluster_fraction    = 0.0;   // largest connected cluster / N
	double   wall_force_proxy            = 0.0;   // Å — mean z-deviation of boundary beads
	double   mean_contact_lifetime       = 0.0;   // frames contacts have persisted
	double   persistent_contact_fraction = 0.0;   // fraction of contacts existing > 1 frame
	double   void_connectivity_proxy     = 0.0;   // [0,1] fraction of void accessible
	bool     settling_flag               = false;
	double   E_total                     = 0.0;
	double   E_rel_drift                 = 0.0;

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << frame                       << '\t'
		   << time                        << '\t'
		   << bead_count                  << '\t'
		   << container_volume            << '\t'
		   << occupied_volume             << '\t'
		   << packing_fraction_measured   << '\t'
		   << mean_coordination           << '\t'
		   << coordination_stddev         << '\t'
		   << largest_cluster_fraction    << '\t'
		   << wall_force_proxy            << '\t'
		   << mean_contact_lifetime       << '\t'
		   << persistent_contact_fraction << '\t'
		   << void_connectivity_proxy     << '\t'
		   << (settling_flag ? 1 : 0)     << '\t'
		   << E_total                     << '\t'
		   << E_rel_drift;
		return ss.str();
	}

	static std::string tsv_header() {
		return "frame\ttime"
			   "\tbead_count\tcontainer_volume\toccupied_volume"
			   "\tpacking_fraction_measured"
			   "\tmean_coordination\tcoordination_stddev"
			   "\tlargest_cluster_fraction"
			   "\twall_force_proxy"
			   "\tmean_contact_lifetime\tpersistent_contact_fraction"
			   "\tvoid_connectivity_proxy"
			   "\tsettling_flag\tE_total\tE_rel_drift";
	}
};

// =============================================================================
// Task 13B — Per-case macro packing inference record
// =============================================================================

struct PackingInference {
	std::string case_id;
	uint32_t    bead_count                   = 0;
	double      container_volume             = 0.0;
	double      occupied_volume              = 0.0;
	double      total_mass                   = 0.0;       // if mass data provided
	bool        mass_basis_available         = false;

	// Density
	double      bulk_density_inferred        = 0.0;       // mass / container_vol (if mass known)
	double      bulk_density_proxy           = 0.0;       // occupied_vol / container_vol

	// Porosity
	double      porosity_inferred            = 0.0;       // 1 - packing_fraction
	double      porosity_proxy_uncorrected   = 0.0;       // same; labeled uncorrected if overlaps possible

	// Coordination / stability
	double      mean_coordination            = 0.0;
	double      coordination_stddev          = 0.0;
	double      largest_cluster_fraction     = 0.0;
	double      load_bearing_network_score   = 0.0;       // [0,1]
	double      packing_stability_score      = 0.0;       // [0,1]

	// Compressibility proxy (filled if before/after compression cases provided)
	double      compressibility_proxy        = 0.0;
	double      densification_rate_proxy     = 0.0;
	double      contact_growth_rate          = 0.0;
	bool        compression_data_available   = false;

	// Permeability proxy
	double      permeability_proxy           = 0.0;
	double      flow_accessibility_score     = 0.0;

	// Sintering readiness
	double      sintering_readiness_proxy    = 0.0;
	double      neck_growth_candidate_score  = 0.0;

	// Classification
	std::string packing_macro_class;                      // analysis-only label
	std::string energy_status;
	bool        valid_for_macro_inference    = false;

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << case_id                       << '\t'
		   << bead_count                    << '\t'
		   << container_volume              << '\t'
		   << occupied_volume               << '\t'
		   << std::setprecision(4)
		   << bulk_density_proxy            << '\t'
		   << bulk_density_inferred         << '\t'
		   << porosity_inferred             << '\t'
		   << mean_coordination             << '\t'
		   << largest_cluster_fraction      << '\t'
		   << load_bearing_network_score    << '\t'
		   << compressibility_proxy         << '\t'
		   << permeability_proxy            << '\t'
		   << sintering_readiness_proxy     << '\t'
		   << packing_macro_class           << '\t'
		   << energy_status                 << '\t'
		   << (valid_for_macro_inference ? "yes" : "no");
		return ss.str();
	}

	static std::string tsv_header() {
		return "case_id\tbead_count\tcontainer_volume\toccupied_volume"
			   "\tbulk_density_proxy\tbulk_density_inferred"
			   "\tporosity_inferred\tmean_coordination"
			   "\tlargest_cluster_fraction\tload_bearing_network_score"
			   "\tcompressibility_proxy\tpermeability_proxy"
			   "\tsintering_readiness_proxy\tpacking_macro_class"
			   "\tenergy_status\tvalid_for_macro_inference";
	}
};

// =============================================================================
// Task 13 — PackingTracker
// =============================================================================
// Feed frames of bead positions + radii; produces a PackingRecord per frame.
//
// Usage
//   PackingTracker tr;
//   tr.r_contact = 0.1;          // Å — extra gap beyond radii to count contact
//   tr.set_box({Lx, Ly, Lz});
//   tr.set_baseline(E0);
//   for each frame:
//     auto row = tr.compute(frame, time, E_total, positions, radii, masses);

struct PackingTracker {
	// Configuration
	double   r_contact       = 0.5;   // Å gap beyond sum of radii = contact
	double   settling_MSD_thresh = 0.01; // Å² — MSD below this = settled
	double   E0              = 0.0;

	void set_box(const PackingBox& b)     { box_ = b; }
	void set_baseline(double E0_val)      { E0 = E0_val; }

	PackingRecord compute(
		uint64_t                        frame_idx,
		double                          sim_time,
		double                          E_total,
		const std::vector<vsepr::Vec3>& pos,
		const std::vector<double>&      radii,
		const std::vector<double>&      masses = {})
	{
		(void)masses;  // used only for mass_basis_available check in analyzer; not consumed here
		PackingRecord row;
		row.frame   = frame_idx;
		row.time    = sim_time;
		row.E_total = E_total;
		const double denom = std::abs(E0) > 1e-12 ? std::abs(E0) : 1.0;
		row.E_rel_drift = (E_total - E0) / denom;

		const int N = static_cast<int>(pos.size());
		if (N == 0) return row;
		row.bead_count = static_cast<uint32_t>(N);

		// ── Container volume ──────────────────────────────────────────────────
		row.container_volume = box_.valid() ? box_.volume() : infer_box_volume(pos);

		// ── Occupied volume (sum of sphere volumes, no overlap correction) ────
		if (!radii.empty()) {
			for (int i = 0; i < N && i < static_cast<int>(radii.size()); ++i)
				row.occupied_volume += (4.0/3.0) * M_PI * std::pow(radii[i], 3);
		} else {
			// Fallback: unit radius
			row.occupied_volume = N * (4.0/3.0) * M_PI;
		}

		if (row.container_volume > 0)
			row.packing_fraction_measured = row.occupied_volume / row.container_volume;

		// ── Contact network + coordination ────────────────────────────────────
		std::vector<int> coord(N, 0);
		std::vector<std::vector<int>> adj(N);
		auto r_i = [&](int i){ return (i < static_cast<int>(radii.size())) ? radii[i] : 1.0; };

		for (int i = 0; i < N; ++i)
			for (int j = i+1; j < N; ++j) {
				const double dx = pos[j].x - pos[i].x;
				const double dy = pos[j].y - pos[i].y;
				const double dz = pos[j].z - pos[i].z;
				const double d  = std::sqrt(dx*dx + dy*dy + dz*dz);
				if (d < r_i(i) + r_i(j) + r_contact) {
					++coord[i]; ++coord[j];
					adj[i].push_back(j);
					adj[j].push_back(i);
				}
			}

		double sum_coord = 0, sum_sq = 0;
		for (int i = 0; i < N; ++i) {
			sum_coord += coord[i];
			sum_sq    += static_cast<double>(coord[i]) * coord[i];
		}
		row.mean_coordination = sum_coord / N;
		const double var_coord = sum_sq / N - (sum_coord/N) * (sum_coord/N);
		row.coordination_stddev = std::sqrt(std::max(0.0, var_coord));

		// ── Largest connected cluster (BFS) ───────────────────────────────────
		std::vector<bool> visited(N, false);
		int largest = 0;
		for (int s = 0; s < N; ++s) {
			if (visited[s]) continue;
			std::queue<int> q;
			q.push(s); visited[s] = true; int sz = 0;
			while (!q.empty()) {
				int v = q.front(); q.pop(); ++sz;
				for (int nb : adj[v])
					if (!visited[nb]) { visited[nb] = true; q.push(nb); }
			}
			if (sz > largest) largest = sz;
		}
		row.largest_cluster_fraction = static_cast<double>(largest) / N;

		// ── Wall force proxy — mean z-distance of top 10% beads from ceiling ─
		{
			std::vector<double> zvals(N);
			for (int i = 0; i < N; ++i) zvals[i] = pos[i].z;
			std::sort(zvals.begin(), zvals.end(), std::greater<double>());
			const int top_n = std::max(1, N / 10);
			double ceiling = box_.valid() ? box_.Lz : zvals[0];
			double sum_gap = 0;
			for (int i = 0; i < top_n; ++i)
				sum_gap += ceiling - zvals[i];
			row.wall_force_proxy = sum_gap / top_n;
		}

		// ── Contact persistence tracking ──────────────────────────────────────
		// Total contacts available from coordination sum; used for persistence fraction below.

		if (!prev_contacts_.empty()) {
			int persistent = 0, total_prev = 0;
			for (int i = 0; i < N; ++i)
				for (int j : adj[i])
					if (j > i) {
						// Check if this pair was also in contact last frame
						const auto& prev_j = prev_contacts_[i];
						if (std::find(prev_j.begin(), prev_j.end(), j) != prev_j.end())
							++persistent;
						++total_prev;
					}
			row.persistent_contact_fraction = (total_prev > 0)
				? static_cast<double>(persistent) / total_prev : 0.0;
			contact_lifetime_sum_ += row.persistent_contact_fraction;
		} else {
			row.persistent_contact_fraction = 0.0;
		}
		prev_contacts_ = adj;
		contact_lifetime_frames_++;
		row.mean_contact_lifetime = (contact_lifetime_frames_ > 1)
			? contact_lifetime_sum_ / (contact_lifetime_frames_ - 1) : 0.0;

		// ── Void connectivity proxy (simple: 1 - packing_fraction) ────────────
		// Real void connectivity would need flood-fill on a voxel grid.
		// We label this a proxy transparently.
		row.void_connectivity_proxy = std::max(0.0, 1.0 - row.packing_fraction_measured);

		// ── Settling flag — MSD below threshold ───────────────────────────────
		if (!prev_pos_.empty()) {
			double msd = 0;
			for (int i = 0; i < N && i < static_cast<int>(prev_pos_.size()); ++i) {
				const double dx = pos[i].x - prev_pos_[i].x;
				const double dy = pos[i].y - prev_pos_[i].y;
				const double dz = pos[i].z - prev_pos_[i].z;
				msd += dx*dx + dy*dy + dz*dz;
			}
			msd /= N;
			row.settling_flag = msd < settling_MSD_thresh;
		}
		prev_pos_ = pos;

		return row;
	}

	void reset() {
		prev_pos_.clear(); prev_contacts_.clear();
		contact_lifetime_sum_ = 0; contact_lifetime_frames_ = 0;
	}

private:
	PackingBox               box_;
	std::vector<vsepr::Vec3> prev_pos_;
	std::vector<std::vector<int>> prev_contacts_;
	double contact_lifetime_sum_    = 0;
	int    contact_lifetime_frames_ = 0;

	static double infer_box_volume(const std::vector<vsepr::Vec3>& pos) {
		if (pos.empty()) return 1.0;
		double xmin=1e30,xmax=-1e30,ymin=1e30,ymax=-1e30,zmin=1e30,zmax=-1e30;
		for (const auto& p : pos) {
			xmin=std::min(xmin,p.x); xmax=std::max(xmax,p.x);
			ymin=std::min(ymin,p.y); ymax=std::max(ymax,p.y);
			zmin=std::min(zmin,p.z); zmax=std::max(zmax,p.z);
		}
		// Add small padding
		return (xmax-xmin+2) * (ymax-ymin+2) * (zmax-zmin+2);
	}
};

// =============================================================================
// Task 13B — PackingAnalyzer
// =============================================================================
// Consumes a PackingRecord log and optionally a compressed-state log to infer
// macro bulk properties.
//
// Usage
//   PackingAnalyzer pa;
//   auto inf = pa.infer("case_id", records, radii, masses);
//   // For compressibility:
//   pa.compute_compressibility(inf, initial_records, compressed_records);

struct PackingAnalyzer {
	double drift_threshold             = 0.05;   // rel energy drift → "unstable"
	double load_bearing_coord_min      = 4.0;    // min mean CN for load-bearing network
	double sintering_persistence_min   = 0.6;    // persistent fraction for sintering candidate

	PackingInference infer(
		const std::string&              case_id,
		const std::vector<PackingRecord>& log,
		double                          total_mass = 0.0) const
	{
		PackingInference inf;
		inf.case_id    = case_id;
		inf.total_mass = total_mass;
		inf.mass_basis_available = (total_mass > 0);

		if (log.empty()) {
			inf.energy_status = "no_data";
			inf.packing_macro_class = "invalid_energy_drift";
			return inf;
		}

		// Use last frame as the settled-state snapshot
		const auto& last = log.back();
		inf.bead_count         = last.bead_count;
		inf.container_volume   = last.container_volume;
		inf.occupied_volume    = last.occupied_volume;
		inf.mean_coordination  = last.mean_coordination;
		inf.coordination_stddev= last.coordination_stddev;
		inf.largest_cluster_fraction = last.largest_cluster_fraction;

		// ── Energy status ─────────────────────────────────────────────────────
		const double max_drift = [&](){
			double m = 0;
			for (const auto& r : log) m = std::max(m, std::abs(r.E_rel_drift));
			return m;
		}();
		if (max_drift > drift_threshold) {
			inf.energy_status             = "unstable";
			inf.valid_for_macro_inference = false;
			inf.packing_macro_class       = "invalid_energy_drift";
		} else {
			inf.energy_status             = "stable";
			inf.valid_for_macro_inference = true;
		}

		// ── Bulk density ──────────────────────────────────────────────────────
		inf.bulk_density_proxy = (inf.container_volume > 0)
			? inf.occupied_volume / inf.container_volume : 0.0;
		if (inf.mass_basis_available && inf.container_volume > 0)
			inf.bulk_density_inferred = total_mass / inf.container_volume;

		// ── Porosity ─────────────────────────────────────────────────────────
		inf.porosity_inferred           = std::max(0.0, 1.0 - last.packing_fraction_measured);
		inf.porosity_proxy_uncorrected  = inf.porosity_inferred;

		// ── Load-bearing network score ────────────────────────────────────────
		// Score = (mean_coord / load_bearing_min) × largest_cluster_fraction, clipped [0,1]
		inf.load_bearing_network_score = std::min(1.0,
			(inf.mean_coordination / load_bearing_coord_min)
			* inf.largest_cluster_fraction);

		// ── Packing stability ─────────────────────────────────────────────────
		// Settling flag fraction over the log
		int settled_count = 0;
		for (const auto& r : log) if (r.settling_flag) ++settled_count;
		inf.packing_stability_score = static_cast<double>(settled_count) / log.size();

		// ── Permeability proxy (Kozeny-Carman inspired, proxy only) ───────────
		// k_proxy ∝ ε³ / (1-ε)²   where ε = porosity
		// We compute this as an order-of-magnitude proxy; label it accordingly.
		{
			const double eps = inf.porosity_inferred;
			const double one_minus_eps = std::max(1e-6, 1.0 - eps);
			inf.permeability_proxy = (eps * eps * eps) / (one_minus_eps * one_minus_eps);
		}

		// Flow accessibility from void connectivity average
		double void_sum = 0;
		for (const auto& r : log) void_sum += r.void_connectivity_proxy;
		inf.flow_accessibility_score = void_sum / log.size();

		// ── Sintering readiness ────────────────────────────────────────────────
		// Based on persistent contact fraction and cluster connectivity
		const double mean_persistence = [&](){
			double s = 0;
			for (const auto& r : log) s += r.persistent_contact_fraction;
			return s / log.size();
		}();
		inf.sintering_readiness_proxy = mean_persistence * inf.largest_cluster_fraction;
		inf.neck_growth_candidate_score = (mean_persistence > sintering_persistence_min)
			? inf.largest_cluster_fraction : 0.0;

		// ── Macro class ───────────────────────────────────────────────────────
		if (!inf.valid_for_macro_inference) {
			inf.packing_macro_class = "invalid_energy_drift";
		} else if (last.packing_fraction_measured > 0.72) {
			inf.packing_macro_class = "jammed_pack";
		} else if (last.packing_fraction_measured > 0.62) {
			if (inf.load_bearing_network_score > 0.7)
				inf.packing_macro_class = "load_bearing_pack";
			else
				inf.packing_macro_class = "dense_pack";
		} else if (last.packing_fraction_measured > 0.40) {
			if (inf.porosity_inferred > 0.45 && inf.flow_accessibility_score > 0.4)
				inf.packing_macro_class = "open_porous_pack";
			else
				inf.packing_macro_class = "settled_powder";
		} else {
			inf.packing_macro_class = "loose_powder";
		}

		// Sintering override
		if (inf.sintering_readiness_proxy > 0.5 && inf.mean_coordination > 5.0)
			inf.packing_macro_class = "sintering_candidate";

		return inf;
	}

	// Fill compressibility_proxy by comparing before/after compression states
	static void compute_compressibility(
		PackingInference&        inf,
		const PackingRecord&     initial_state,
		const PackingRecord&     compressed_state)
	{
		// compressibility_proxy = -ΔV_fraction / Δpacking_proxy
		const double dpf = compressed_state.packing_fraction_measured
						 - initial_state.packing_fraction_measured;
		const double dp  = compressed_state.wall_force_proxy - initial_state.wall_force_proxy;

		inf.compression_data_available = true;

		// Densification: how much did packing fraction increase?
		inf.densification_rate_proxy = std::abs(dpf);

		// Contact growth rate: mean_coord change
		inf.contact_growth_rate = compressed_state.mean_coordination
								- initial_state.mean_coordination;

		// Compressibility proxy: - Δpacking / Δwall_proxy
		if (std::abs(dp) > 1e-10)
			inf.compressibility_proxy = std::abs(dpf) / std::abs(dp);
		else
			inf.compressibility_proxy = 0.0;
	}
};

} // namespace vsepr::packing
