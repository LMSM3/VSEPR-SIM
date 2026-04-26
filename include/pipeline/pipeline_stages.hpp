#pragma once
/**
 * pipeline_stages.hpp — beta-7 Pipeline Stage Functions
 * ======================================================
 *
 * Inline implementations of all five transformation stages:
 *
 *   stage_fingerprint(FormationRecord)    → FingerprintRecord
 *   stage_cluster(FingerprintRecord, &registry) → ClusterRecord
 *   stage_analysis(ClusterRecord, FormationRecord) → AnalysisRecord
 *   stage_report(AnalysisRecord)          → ReportRecord
 *   stage_dashboard(vector<ReportRecord>, &registry, label) → DashboardRecord
 *
 *   run_pipeline(vector<FormationRecord>, label) → pair<vector<PipelineRecord>, DashboardRecord>
 *
 * Design rules:
 *   - No side-effects on FormationRecord or State.
 *   - All functions are pure transforms (inputs → output).
 *   - Warnings are accumulated, never thrown.
 *   - NaN inputs are tolerated: replaced with 0.0 and flagged.
 *
 * VSEPR-SIM  |  beta-7  |  2025-07-11
 */

#include "include/pipeline/pipeline_record.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace vsepr::pipeline {

// ============================================================================
// Helpers
// ============================================================================

namespace detail {

inline double safe(double v) {
	return std::isfinite(v) ? v : 0.0;
}

inline double clamp01(double v) {
	return std::max(0.0, std::min(1.0, v));
}

// Topology hash: FNV-1a over lattice_class byte + n_beads bytes
inline uint64_t topology_hash(v4::LatticeClass lc, int n_beads) {
	uint64_t h = 14695981039346656037ULL;
	auto mix = [&](uint64_t x) {
		for (int b = 0; b < 8; ++b)
			h = (h ^ ((x >> (b * 8)) & 0xFF)) * 1099511628211ULL;
	};
	mix(static_cast<uint64_t>(lc));
	mix(static_cast<uint64_t>(n_beads));
	return h;
}

// Format a double as fixed-precision string (JSON-safe)
inline std::string jd(double v, int prec = 6) {
	if (!std::isfinite(v)) return "null";
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(prec) << v;
	return ss.str();
}

inline std::string jq(const std::string& s) {
	// Minimal JSON string quoting — no control characters in our strings
	return "\"" + s + "\"";
}

} // namespace detail

// ============================================================================
// Stage 1 — Fingerprint
// ============================================================================

/**
 * stage_fingerprint
 *
 * Extracts a fixed-length feature vector from a v4::FormationRecord.
 * All NaN fields are replaced with 0.0 (flagged via WarningCode).
 *
 * Feature layout  (FingerprintRecord::FEATURE_DIM == 8):
 *   [0] final_energy
 *   [1] rms_force
 *   [2] avg_eta
 *   [3] avg_rho
 *   [4] avg_C
 *   [5] macro_rigidity
 *   [6] macro_ductility
 *   [7] log10(steps + 1)
 *
 * Normalization: none at this stage — ClusterRegistry epsilon is set
 * in feature-space units. Callers may normalize before passing to
 * stage_cluster if the registry uses normalized features.
 */
inline FingerprintRecord stage_fingerprint(const v4::FormationRecord& f) {
	FingerprintRecord fp;
	fp.symbol = f.symbol;
	fp.name   = f.name;
	fp.topology_hash = detail::topology_hash(f.structure, f.n_beads);

	bool any_nan = false;

	auto fill = [&](int idx, double v) {
		if (!std::isfinite(v)) { fp.features[idx] = 0.0; any_nan = true; }
		else                    fp.features[idx] = v;
	};

	fill(0, f.final_energy);
	fill(1, f.rms_force);
	fill(2, f.avg_eta);
	fill(3, f.avg_rho);
	fill(4, f.avg_C);
	fill(5, f.macro_rigidity);
	fill(6, f.macro_ductility);
	fill(7, std::log10(static_cast<double>(f.steps) + 1.0));

	if (!f.converged)
		fp.warnings.push_back(WarningCode::NotConverged);
	if (any_nan)
		fp.warnings.push_back(WarningCode::EnergyNaN);

	// Compute norm
	double sum = 0.0;
	for (double x : fp.features) sum += x * x;
	fp.feature_norm = std::sqrt(sum);

	if (fp.feature_norm < 1e-12)
		fp.warnings.push_back(WarningCode::FingerprintDegenerate);

	return fp;
}

// ============================================================================
// Stage 2 — Cluster
// ============================================================================

/**
 * stage_cluster
 *
 * Assigns a FingerprintRecord to a cluster in the shared registry.
 * Returns a ClusterRecord with the cluster ID and a size snapshot.
 *
 * Cluster label heuristic (beta-7):
 *   topology_hash encodes lattice class → used to generate a readable label.
 *   Full label assignment from analysis context happens in stage_analysis.
 */
inline ClusterRecord stage_cluster(const FingerprintRecord& fp,
								   ClusterRegistry& registry) {
	ClusterRecord cr;
	cr.symbol      = fp.symbol;
	cr.name        = fp.name;
	cr.fingerprint = fp;
	cr.warnings    = fp.warnings;

	cr.cluster_id = registry.assign(fp);

	const ClusterRegistry::Entry* entry = registry.find(cr.cluster_id);
	cr.cluster_size  = entry ? entry->count : 1;
	cr.cluster_label = "";  // filled by stage_analysis

	if (cr.cluster_size < 2)
		cr.warnings.push_back(WarningCode::LowPopulation);

	return cr;
}

// ============================================================================
// Stage 3 — Analysis
// ============================================================================

/**
 * stage_analysis
 *
 * Interprets a ClusterRecord and its source FormationRecord to produce
 * the analysis layer. All conclusions are kept in AnalysisRecord — none
 * are written back into xyzFull or FormationRecord.
 */
inline AnalysisRecord stage_analysis(const ClusterRecord& cr,
									 const v4::FormationRecord& f) {
	AnalysisRecord ar;
	ar.symbol      = cr.symbol;
	ar.name        = cr.name;
	ar.cluster_id  = cr.cluster_id;
	ar.warnings    = cr.warnings;

	// energy_per_bead
	ar.energy_per_bead = (f.n_beads > 0 && std::isfinite(f.final_energy))
					   ? (f.final_energy / f.n_beads)
					   : 0.0;

	// convergence_quality: 1.0 = converged perfectly, decays on rms_force
	if (f.converged) {
		double rms = detail::safe(f.rms_force);
		ar.convergence_quality = detail::clamp01(1.0 / (1.0 + rms * 100.0));
	} else {
		ar.convergence_quality = 0.0;
	}

	// motif_class from lattice
	switch (f.structure) {
		case v4::LatticeClass::FCC:     ar.motif_class = "FCC"; break;
		case v4::LatticeClass::BCC:     ar.motif_class = "BCC"; break;
		case v4::LatticeClass::HCP:     ar.motif_class = "HCP"; break;
		default:                        ar.motif_class = "unknown"; break;
	}

	// packing_quality: avg_rho and avg_C; both normalised heuristically
	{
		double rho_norm = detail::clamp01(detail::safe(f.avg_rho) / 15.0); // 15 is rough FCC max
		double C_norm   = detail::clamp01(detail::safe(f.avg_C)   / 50.0); // 50 is rough FCC max
		ar.packing_quality = 0.5 * (rho_norm + C_norm);
	}

	// defect_indicator: l3 domains / n_beads
	ar.defect_indicator = (f.n_beads > 0)
						? static_cast<double>(f.n_l3_domains) / f.n_beads
						: 0.0;

	// stability_score: weighted combo
	ar.stability_score = detail::clamp01(
		0.5 * ar.convergence_quality +
		0.3 * ar.packing_quality +
		0.2 * detail::clamp01(1.0 - ar.defect_indicator * 10.0));

	// cluster_label: lattice class + density tier
	{
		std::string tier = (ar.packing_quality > 0.5) ? "dense" : "sparse";
		ar.cluster_label = ar.motif_class + "-" + tier;
	}
	const_cast<ClusterRecord&>(cr).cluster_label = ar.cluster_label; // backfill label

	// interpretation
	{
		std::ostringstream s;
		s << std::fixed << std::setprecision(3);
		s << ar.motif_class
		  << "  E/bead=" << ar.energy_per_bead
		  << "  conv=" << ar.convergence_quality
		  << "  pack=" << ar.packing_quality
		  << "  stab=" << ar.stability_score;
		if (ar.defect_indicator > 0.01)
			s << "  defect=" << ar.defect_indicator;
		ar.interpretation = s.str();
	}

	return ar;
}

// ============================================================================
// Stage 4 — Report
// ============================================================================

/**
 * stage_report
 *
 * Converts an AnalysisRecord into a ReportRecord (CSV row + JSON fragment
 * + human-readable summary line).
 */
inline ReportRecord stage_report(const AnalysisRecord& ar) {
	ReportRecord rr;
	rr.symbol   = ar.symbol;
	rr.name     = ar.name;
	rr.warnings = ar.warnings;

	if (ar.symbol.empty()) {
		rr.warnings.push_back(WarningCode::ReportEmpty);
		return rr;
	}

	// Collect warning names
	std::string warn_str;
	for (auto w : ar.warnings) {
		if (!warn_str.empty()) warn_str += '|';
		warn_str += warning_name(w);
	}
	if (warn_str.empty()) warn_str = "ok";

	// CSV row
	{
		std::ostringstream s;
		s << std::fixed << std::setprecision(6);
		s << ar.symbol          << ','
		  << ar.name            << ','
		  << ar.cluster_id      << ','
		  << ar.cluster_label   << ','
		  << ar.energy_per_bead << ','
		  << ar.convergence_quality << ','
		  << ar.packing_quality << ','
		  << ar.stability_score << ','
		  << ar.defect_indicator << ','
		  << ar.motif_class     << ','
		  << "\"" << ar.interpretation << "\","
		  << warn_str;
		rr.csv_row = s.str();
	}

	// JSON fragment
	{
		std::ostringstream s;
		s << "{"
		  << "\"symbol\":"              << detail::jq(ar.symbol)          << ","
		  << "\"name\":"                << detail::jq(ar.name)            << ","
		  << "\"cluster_id\":"          << ar.cluster_id                  << ","
		  << "\"cluster_label\":"       << detail::jq(ar.cluster_label)   << ","
		  << "\"energy_per_bead\":"     << detail::jd(ar.energy_per_bead) << ","
		  << "\"convergence_quality\":" << detail::jd(ar.convergence_quality) << ","
		  << "\"packing_quality\":"     << detail::jd(ar.packing_quality) << ","
		  << "\"stability_score\":"     << detail::jd(ar.stability_score) << ","
		  << "\"defect_indicator\":"    << detail::jd(ar.defect_indicator) << ","
		  << "\"motif_class\":"         << detail::jq(ar.motif_class)     << ","
		  << "\"warnings\":"            << detail::jq(warn_str)
		  << "}";
		rr.json_fragment = s.str();
	}

	// Summary line
	{
		std::ostringstream s;
		s << std::left  << std::setw(4) << ar.symbol
		  << std::left  << std::setw(14) << ar.name
		  << std::right << std::fixed << std::setprecision(3)
		  << "  stab=" << std::setw(5) << ar.stability_score
		  << "  pack=" << std::setw(5) << ar.packing_quality
		  << "  " << ar.cluster_label
		  << (warn_str == "ok" ? "" : "  [" + warn_str + "]");
		rr.summary_line = s.str();
	}

	return rr;
}

// ============================================================================
// Stage 5 — Dashboard
// ============================================================================

/**
 * stage_dashboard
 *
 * Aggregates all ReportRecords and the cluster registry into a single
 * DashboardRecord (Markdown table + JSON array + run summary).
 *
 * beta-7: text output only. SVG/PNG export is planned for beta-8.
 */
inline DashboardRecord stage_dashboard(const std::vector<ReportRecord>& reports,
									   const ClusterRegistry& registry,
									   const std::string& run_label) {
	DashboardRecord dr;
	dr.run_label  = run_label;
	dr.n_cases    = static_cast<int>(reports.size());
	dr.n_clusters = registry.num_clusters();

	if (reports.empty()) {
		dr.run_summary = "no cases";
		return dr;
	}

	// Count total warnings
	for (const auto& r : reports)
		dr.n_warnings += static_cast<int>(r.warnings.size());

	// Markdown table
	{
		std::ostringstream md;
		md << "# VSEPR-SIM Pipeline Report\n\n";
		md << "**Run:** " << run_label << "  "
		   << "**Cases:** " << dr.n_cases << "  "
		   << "**Clusters:** " << dr.n_clusters << "  "
		   << "**Warnings:** " << dr.n_warnings << "\n\n";
		md << "| Symbol | Name | Cluster | Label | Stab | Pack | Conv | Motif | Warn |\n";
		md << "|--------|------|---------|-------|------|------|------|-------|------|\n";
		for (const auto& r : reports) {
			// Parse CSV row for the structured fields
			std::string sym   = r.symbol;
			std::string nm    = r.name;
			std::string warn  = r.warnings.empty() ? "ok" : "";
			for (auto w : r.warnings) {
				if (!warn.empty()) warn += '|';
				warn += warning_name(w);
			}
			if (warn.empty()) warn = "ok";

			// Re-read from json_fragment would be cleanest but we keep it simple
			// by re-running a mini extraction from the analysis (we have csv_row)
			// Just emit the summary line as a table row using pipe-separated values
			// from csv_row
			// csv: symbol,name,cluster_id,cluster_label,epb,cq,pq,ss,di,motif,interp,warn
			std::vector<std::string> cols;
			{
				std::istringstream ss(r.csv_row);
				std::string tok;
				// Simple split — interpretation field is quoted and may contain commas;
				// we only need the first 12 columns
				bool in_quote = false;
				std::string cur;
				for (char c : r.csv_row) {
					if (c == '"') { in_quote = !in_quote; continue; }
					if (c == ',' && !in_quote) {
						cols.push_back(cur); cur.clear(); continue;
					}
					cur += c;
				}
				cols.push_back(cur);
			}
			auto col = [&](int i) -> std::string {
				return (i < static_cast<int>(cols.size())) ? cols[i] : "";
			};

			// Truncate cluster_id to last 6 hex digits for readability
			std::string cid = col(2);
			if (cid.size() > 6) {
				// Convert decimal string → hex suffix
				try {
					uint64_t id_val = std::stoull(cid);
					std::ostringstream h;
					h << std::hex << (id_val & 0xFFFFFF);
					cid = h.str();
				} catch (...) { cid = cid.substr(cid.size() - 6); }
			}

			md << "| " << sym   << " | " << nm     << " | "
			   << cid           << " | " << col(3)  << " | "
			   << col(7)        << " | " << col(6)  << " | "
			   << col(5)        << " | " << col(9)  << " | "
			   << col(11)       << " |\n";
		}
		md << "\n";

		// Cluster summary
		md << "## Clusters\n\n";
		md << "| ID (hex) | Size | Members |\n";
		md << "|----------|------|---------|\n";
		for (const auto& e : registry.clusters) {
			std::ostringstream h;
			h << std::hex << (e.id & 0xFFFFFF);
			std::string members;
			for (const auto& m : e.members) {
				if (!members.empty()) members += ", ";
				members += m;
			}
			md << "| " << h.str() << " | " << e.count << " | " << members << " |\n";
		}

		dr.markdown_table = md.str();
	}

	// JSON array
	{
		std::ostringstream js;
		js << "[\n";
		for (int i = 0; i < static_cast<int>(reports.size()); ++i) {
			js << "  " << reports[i].json_fragment;
			if (i + 1 < static_cast<int>(reports.size())) js << ",";
			js << "\n";
		}
		js << "]";
		dr.json_array = js.str();
	}

	// Run summary
	{
		std::ostringstream s;
		s << run_label << "  cases=" << dr.n_cases
		  << "  clusters=" << dr.n_clusters
		  << "  warnings=" << dr.n_warnings;
		dr.run_summary = s.str();
	}

	return dr;
}

// ============================================================================
// run_pipeline — convenience: run all 5 stages over a batch of formations
// ============================================================================

/**
 * run_pipeline
 *
 * Runs all five transformation stages over a vector of FormationRecords.
 * Returns the per-case PipelineRecords and the aggregated DashboardRecord.
 *
 * @param formations   Input formation cases (source: v4::reference_dataset() etc.)
 * @param run_label    Label for this run (used in dashboard header)
 * @param epsilon      Cluster distance threshold (default 0.20 in feature space)
 *
 * @return { vector<PipelineRecord>, DashboardRecord }
 */
inline std::pair<std::vector<PipelineRecord>, DashboardRecord>
run_pipeline(const std::vector<v4::FormationRecord>& formations,
			 const std::string& run_label = "beta-7-run",
			 double epsilon = 0.20) {
	ClusterRegistry registry;
	registry.epsilon = epsilon;

	std::vector<PipelineRecord> records;
	records.reserve(formations.size());

	std::vector<ReportRecord> all_reports;
	all_reports.reserve(formations.size());

	for (const auto& f : formations) {
		PipelineRecord pr;
		pr.formation   = f;
		pr.fingerprint = stage_fingerprint(f);
		pr.cluster     = stage_cluster(pr.fingerprint, registry);
		pr.analysis    = stage_analysis(pr.cluster, f);
		pr.report      = stage_report(pr.analysis);
		pr.complete    = !pr.report.csv_row.empty();

		all_reports.push_back(pr.report);
		records.push_back(std::move(pr));
	}

	DashboardRecord dash = stage_dashboard(all_reports, registry, run_label);
	return { std::move(records), std::move(dash) };
}

} // namespace vsepr::pipeline
