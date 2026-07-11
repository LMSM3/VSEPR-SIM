#pragma once
/**
 * pipeline_record.hpp — beta-7 Research Pipeline Record Types
 * ============================================================
 *
 * Connective data layer for the beta-7 pipeline:
 *
 *   FormationOutput (v4::FormationRecord)
 *       ↓  stage_fingerprint()
 *   FingerprintRecord
 *       ↓  stage_cluster()
 *   ClusterRecord
 *       ↓  stage_analysis()
 *   AnalysisRecord
 *       ↓  stage_report()
 *   ReportRecord
 *       ↓  stage_dashboard()
 *   DashboardRecord
 *
 * Design rules:
 *   - All records are plain aggregates. No hidden state.
 *   - Records carry forward what they received (provenance chain).
 *   - Analysis results are NOT stored back in FormationRecord.
 *   - Cluster IDs are deterministic hashes, not sequential ints.
 *   - Every field is public and inspectable.
 *
 * Fingerprint note (beta-7):
 *   FingerprintRecord uses scalar formation metrics extracted from
 *   v4::FormationRecord. Geometry-based fingerprinting via
 *   atomistic::classify::ProtoFingerprint (requires atomistic::State)
 *   is the planned upgrade path for beta-8 when full trajectory
 *   replay is wired in.
 *
 * VSEPR-SIM  |  beta-7  |  2025-07-11
 */

#include "v4/formation_record.hpp"
#include "include/pipeline/pipeline_trace.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace vsepr::pipeline {

// ============================================================================
// Validity warning type
// ============================================================================

enum class WarningCode : uint8_t {
	None              = 0,
	NotConverged      = 1,   // FIRE did not converge
	LowPopulation     = 2,   // fewer than 2 samples in cluster
	EnergyNaN         = 3,   // final_energy is NaN
	FingerprintDegenerate = 4, // fingerprint vector is near-zero
	ReportEmpty       = 5,   // no data to report
	DashboardEmpty    = 6,   // nothing to export
};

inline const char* warning_name(WarningCode w) {
	switch (w) {
		case WarningCode::None:                   return "ok";
		case WarningCode::NotConverged:            return "not_converged";
		case WarningCode::LowPopulation:           return "low_population";
		case WarningCode::EnergyNaN:               return "energy_nan";
		case WarningCode::FingerprintDegenerate:   return "fingerprint_degenerate";
		case WarningCode::ReportEmpty:             return "report_empty";
		case WarningCode::DashboardEmpty:          return "dashboard_empty";
	}
	return "unknown";
}

// ============================================================================
// Stage 1 — FingerprintRecord
// ============================================================================

/**
 * FingerprintRecord — scalar formation fingerprint (beta-7 version).
 *
 * Encodes one formation case as a fixed-length feature vector
 * extracted deterministically from v4::FormationRecord.
 *
 * Feature layout (8 components):
 *   [0]  final_energy       (kcal/mol or eV — caller's units)
 *   [1]  rms_force          (convergence quality)
 *   [2]  avg_eta            (mean packing fraction proxy)
 *   [3]  avg_rho            (mean density proxy)
 *   [4]  avg_C              (mean coordination proxy)
 *   [5]  macro_rigidity     (inferred macro-scale rigidity)
 *   [6]  macro_ductility    (inferred macro-scale ductility)
 *   [7]  log10(steps+1)     (convergence cost, log-scale)
 *
 * NaN components are replaced with 0.0 and flagged.
 * The topology_hash encodes lattice class + bead count for grouping.
 */
struct FingerprintRecord {
	// Source provenance
	std::string symbol;
	std::string name;

	// Feature vector (FEATURE_DIM = 8)
	static constexpr int FEATURE_DIM = 8;
	std::array<double, FEATURE_DIM> features{};

	// Topology hash — groups same lattice + bead-count families
	// Deterministic: hash(lattice_class_byte, n_beads)
	uint64_t topology_hash{0};

	// Norm of the feature vector (computed at construction)
	double feature_norm{0.0};

	// Distance to another fingerprint (Euclidean in feature space)
	double distance(const FingerprintRecord& other) const {
		double sum = 0.0;
		for (int i = 0; i < FEATURE_DIM; ++i) {
			double d = features[i] - other.features[i];
			sum += d * d;
		}
		return std::sqrt(sum);
	}

	// Validity warnings accumulated during this stage
	std::vector<WarningCode> warnings;

	// Symbolic trace and animation cues accumulated by stage_fingerprint
	StageTraceBundle trace;
};

// ============================================================================
// Stage 2 — ClusterRecord
// ============================================================================

/**
 * ClusterRegistry — shared mutable registry of known clusters.
 *
 * Assigns a cluster ID to each FingerprintRecord using threshold-based
 * nearest-centroid logic. Deterministic within a single run (insertion
 * order is canonical). Thread-unsafe by design — single-threaded pipeline.
 */
struct ClusterRegistry {
	struct Entry {
		uint64_t          id;              // deterministic hash of first member
		FingerprintRecord centroid;        // running centroid (mean features)
		std::vector<std::string> members;  // symbols in this cluster
		int               count{0};
	};

	double epsilon{0.20};   // Euclidean distance threshold in feature space
	std::vector<Entry> clusters;

	// Assign fp to a cluster; return cluster ID
	uint64_t assign(const FingerprintRecord& fp) {
		// Find nearest existing cluster within epsilon
		int    best_idx  = -1;
		double best_dist = epsilon;
		for (int i = 0; i < static_cast<int>(clusters.size()); ++i) {
			double d = fp.distance(clusters[i].centroid);
			if (d < best_dist) {
				best_dist = d;
				best_idx  = i;
			}
		}

		if (best_idx >= 0) {
			// Update centroid (online mean)
			Entry& e = clusters[best_idx];
			for (int f = 0; f < FingerprintRecord::FEATURE_DIM; ++f) {
				e.centroid.features[f] =
					(e.centroid.features[f] * e.count + fp.features[f])
					/ (e.count + 1);
			}
			++e.count;
			e.members.push_back(fp.symbol);
			return e.id;
		}

		// New cluster — ID = hash of symbol + topology_hash
		Entry e;
		e.id = _make_id(fp);
		e.centroid = fp;
		e.count = 1;
		e.members.push_back(fp.symbol);
		clusters.push_back(std::move(e));
		return clusters.back().id;
	}

	int num_clusters() const { return static_cast<int>(clusters.size()); }

	const Entry* find(uint64_t id) const {
		for (const auto& e : clusters)
			if (e.id == id) return &e;
		return nullptr;
	}

private:
	static uint64_t _make_id(const FingerprintRecord& fp) {
		// FNV-1a over symbol bytes + topology_hash bytes
		uint64_t h = 14695981039346656037ULL;
		for (unsigned char c : fp.symbol)
			h = (h ^ c) * 1099511628211ULL;
		for (int b = 0; b < 8; ++b) {
			unsigned char byte = (fp.topology_hash >> (b * 8)) & 0xFF;
			h = (h ^ byte) * 1099511628211ULL;
		}
		return h;
	}
};

/**
 * ClusterRecord — one formation case after cluster assignment.
 */
struct ClusterRecord {
	// Forwarded from FingerprintRecord
	std::string      symbol;
	std::string      name;
	FingerprintRecord fingerprint;

	// Cluster assignment
	uint64_t         cluster_id{0};
	int              cluster_size{0};    // snapshot of registry count at time of assignment
	std::string      cluster_label;      // e.g. "BCC-dense", "FCC-soft", etc. (set in analysis)

	std::vector<WarningCode> warnings;

	// Animation cues accumulated by stage_cluster
	StageTraceBundle trace;
};

// ============================================================================
// Stage 3 — AnalysisRecord
// ============================================================================

/**
 * AnalysisRecord — per-case interpreted analysis layer.
 *
 * Interprets ClusterRecord + original FormationRecord to produce
 * analysis-layer outputs. Nothing here is stored back into xyzFull.
 *
 * Analysis dimensions (beta-7):
 *   energy_per_bead        final_energy / n_beads
 *   convergence_quality    1.0 if converged, else 1 - tanh(rms_force)
 *   motif_class            inferred from lattice class
 *   packing_quality        derived from avg_rho and avg_C
 *   stability_score        weighted combination of energy + convergence
 *   defect_indicator       n_l3_domains / n_beads  (proxy)
 */
struct AnalysisRecord {
	// Forwarded provenance
	std::string       symbol;
	std::string       name;
	uint64_t          cluster_id{0};
	std::string       cluster_label;

	// Computed analysis fields
	double energy_per_bead{0.0};
	double convergence_quality{0.0};
	std::string motif_class;           // "FCC" / "BCC" / "HCP" / "unknown"
	double packing_quality{0.0};       // [0,1] — higher is denser/more ordered
	double stability_score{0.0};       // [0,1] — higher is more stable
	double defect_indicator{0.0};      // n_l3_domains / n_beads
	std::string interpretation;        // human-readable one-liner

	// Validity warnings
	std::vector<WarningCode> warnings;

	// Symbolic trace and animation cues accumulated by stage_analysis
	StageTraceBundle trace;
};

// ============================================================================
// Stage 4 — ReportRecord
// ============================================================================

/**
 * ReportRecord — tables, CSV row, JSON blob.
 *
 * All output is text. No binary blobs. Every field inspectable.
 */
struct ReportRecord {
	std::string symbol;
	std::string name;

	// CSV row (header provided separately)
	std::string csv_row;

	// JSON object for this case (no array wrapper — caller aggregates)
	std::string json_fragment;

	// Human-readable summary line
	std::string summary_line;

	// Validity warnings forwarded from prior stages
	std::vector<WarningCode> warnings;

	// Trace bundle forwarded from AnalysisRecord
	StageTraceBundle trace;

	// Markdown block containing the full symbolic calculation trace
	// Populated by stage_report from ar.trace.expressions.
	std::string symbolic_markdown;

	static std::string csv_header() {
		return "symbol,name,cluster_id,cluster_label,"
			   "energy_per_bead,convergence_quality,"
			   "packing_quality,stability_score,defect_indicator,"
			   "motif_class,interpretation,warnings";
	}
};

// ============================================================================
// Stage 5 — DashboardRecord
// ============================================================================

/**
 * DashboardRecord — aggregated export artifact for one run.
 *
 * beta-7: text-only output (Markdown summary table + JSON array).
 * SVG/PNG rendering is planned for beta-8 when the glass pipeline
 * bridge is wired in.
 */
struct DashboardRecord {
	std::string run_label;            // e.g. "beta-7-reference-12"
	int         n_cases{0};
	int         n_clusters{0};
	int         n_warnings{0};

	// Full Markdown table of all cases
	std::string markdown_table;

	// JSON array of all report fragments
	std::string json_array;

	// One-line run summary
	std::string run_summary;

	// Aggregated animation cues collected from all ReportRecords
	StageTraceBundle trace;
};

// ============================================================================
// Composite PipelineRecord — one case fully processed
// ============================================================================

/**
 * PipelineRecord — full provenance chain for one formation case.
 *
 * Preserves every intermediate stage record so any stage can be
 * re-inspected, re-run, or audited after the fact.
 */
struct PipelineRecord {
	v4::FormationRecord  formation;    // Stage 0: source
	FingerprintRecord    fingerprint;  // Stage 1
	ClusterRecord        cluster;      // Stage 2
	AnalysisRecord       analysis;     // Stage 3
	ReportRecord         report;       // Stage 4
	// Stage 5 (DashboardRecord) is assembled across all PipelineRecords
	// by run_pipeline() — it is a run-level record, not a per-case record.

	bool complete{false};  // set true when all stages ran without fatal error
};

} // namespace vsepr::pipeline
