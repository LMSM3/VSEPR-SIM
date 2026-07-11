#pragma once
/**
 * include/vsim/vsepr_observe.hpp
 * ===============================
 * WO-84S: VSEPR Observe Sink
 *
 * Provides eval_observe_vsepr_metrics() which converts a live VSEPRReport
 * (and optional OrganicCandidate) into structured EvalResult entries that
 * the [observe] metric system can consume and automation scripts can branch on.
 *
 * This header is intentionally separated from vsim_runtime.hpp to avoid
 * pulling atomistic State headers into the heavier runtime header chain.
 *
 * Metrics exposed:
 *   vsepr_sites          - number of classified VSEPR centres
 *   geometry_candidates  - total geometry candidate count across all sites
 *   vsepr_confidence     - mean confidence across all classified sites
 *   vsepr_flags          - number of sites carrying any flag
 *   lone_pair_inference  - number of sites that used lone-pair inference
 *   fallback_mode        - 1.0 if any site used geometry-only fallback, else 0.0
 *
 * Each EvalResult also populates the detail field with a compact JSON-like
 * string describing the per-site data, suitable for automation parsing.
 *
 * Usage (in classify path):
 *   #include "vsim/vsepr_observe.hpp"
 *   auto results = vsim::observe::eval_observe_vsepr_metrics(cfg, vreport, &cand);
 */

#include "vsim_document.hpp"
#include "vsim_runtime.hpp"

#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/organic_candidate.hpp"

#include <sstream>
#include <string>
#include <vector>
#include <cmath>

namespace vsim {
namespace observe {

// ---------------------------------------------------------------------------
// Minimal per-site descriptor for the detail payload
// ---------------------------------------------------------------------------

struct VSEPRObserveSite {
	std::string center_id;          // e.g. "site_0"
	int         center_Z      = 0;
	std::size_t bonded_atoms  = 0;
	std::size_t lone_pairs    = 0;
	std::size_t steric_number = 0;  // bonded + lone pairs
	std::string electron_geometry;
	std::string observed_geometry;
	double      confidence    = 0.0;
	std::vector<std::string> flags;
};

// ---------------------------------------------------------------------------
// Build VSEPRObserveSite list from a VSEPRReport
// ---------------------------------------------------------------------------

inline std::vector<VSEPRObserveSite>
vsepr_observe_sites(const atomistic::classify::VSEPRReport& report)
{
	std::vector<VSEPRObserveSite> out;
	for (const auto& s : report.sites) {
		if (s.bonded_domain_count == 0) continue;  // bare atom
		VSEPRObserveSite os;
		os.center_id        = "site_" + std::to_string(s.center_index);
		os.center_Z         = s.center_Z;
		os.bonded_atoms     = s.bonded_domain_count;
		os.lone_pairs       = s.lone_pair_domain_count;
		os.steric_number    = s.electron_domain_count;
		os.electron_geometry = atomistic::classify::to_string(s.electron_geometry);
		os.observed_geometry = atomistic::classify::to_string(s.molecular_shape);
		os.confidence       = s.confidence;

		if (s.used_element_lone_pair_inference)
			os.flags.emplace_back("lone_pairs_inferred");
		if (s.used_geometry_lone_pair_fallback)
			os.flags.emplace_back("geometry_fallback");
		if (s.is_hypervalent)
			os.flags.emplace_back("hypervalent");

		out.push_back(std::move(os));
	}
	return out;
}

// ---------------------------------------------------------------------------
// Serialize site list to compact detail string
// ---------------------------------------------------------------------------

inline std::string serialise_sites(const std::vector<VSEPRObserveSite>& sites)
{
	std::ostringstream oss;
	oss << "[";
	bool first = true;
	for (const auto& s : sites) {
		if (!first) oss << ",";
		first = false;
		oss << "{center:" << s.center_id
			<< ",Z:" << s.center_Z
			<< ",bonded:" << s.bonded_atoms
			<< ",lp:" << s.lone_pairs
			<< ",sn:" << s.steric_number
			<< ",eg:" << s.electron_geometry
			<< ",og:" << s.observed_geometry
			<< ",conf:" << s.confidence;
		if (!s.flags.empty()) {
			oss << ",flags:[";
			for (std::size_t i = 0; i < s.flags.size(); ++i) {
				if (i) oss << ",";
				oss << s.flags[i];
			}
			oss << "]";
		}
		oss << "}";
	}
	oss << "]";
	return oss.str();
}

// ---------------------------------------------------------------------------
// eval_observe_vsepr_metrics
// ---------------------------------------------------------------------------

/**
 * Convert a VSEPRReport (and optional OrganicCandidate) into EvalResult entries.
 *
 * Only metrics listed in cfg.metrics are produced.  Unrecognised metric names
 * are silently skipped (they fall through to the caller's own handler).
 *
 * @param cfg      [observe] section from the parsed .vsim document
 * @param report   result of atomistic::classify::classify_vsepr_sites()
 * @param organic  optional OrganicCandidate; pass nullptr to skip organic fields
 */
inline std::vector<vsim::EvalResult>
eval_observe_vsepr_metrics(
		const ObserveSection&                          cfg,
		const atomistic::classify::VSEPRReport&        report,
		const atomistic::classify::OrganicCandidate*   organic = nullptr)
{
	std::vector<vsim::EvalResult> results;

	const auto obs_sites   = vsepr_observe_sites(report);
	const std::string detail_payload = serialise_sites(obs_sites);

	// Pre-compute aggregates
	const double site_count = static_cast<double>(obs_sites.size());

	double mean_conf = 0.0;
	double flag_count = 0.0;
	double lp_inferred_count = 0.0;
	double fallback_count = 0.0;
	double candidate_count = 0.0;  // steric number proxy

	for (const auto& s : obs_sites) {
		mean_conf += s.confidence;
		candidate_count += static_cast<double>(s.steric_number);
		for (const auto& f : s.flags) {
			flag_count += 1.0;
			if (f == "lone_pairs_inferred") lp_inferred_count += 1.0;
			if (f == "geometry_fallback")   fallback_count += 1.0;
		}
	}
	if (site_count > 0.0) mean_conf /= site_count;

	for (const auto& metric : cfg.metrics) {
		vsim::EvalResult r;
		r.probe_name = metric;
		r.field      = metric;
		r.window     = "static";

		if (metric == "vsepr_sites") {
			r.value  = site_count;
			r.detail = detail_payload;
		} else if (metric == "geometry_candidates") {
			r.value  = candidate_count;
			r.detail = detail_payload;
		} else if (metric == "vsepr_confidence") {
			r.value  = mean_conf;
			r.detail = detail_payload;
		} else if (metric == "vsepr_flags") {
			r.value  = flag_count;
			r.detail = detail_payload;
		} else if (metric == "lone_pair_inference") {
			r.value  = lp_inferred_count;
			r.detail = detail_payload;
		} else if (metric == "fallback_mode") {
			r.value  = (fallback_count > 0.0) ? 1.0 : 0.0;
			r.detail = fallback_count > 0.0 ? "fallback_active" : "fallback_inactive";
		} else {
			// Not a VSEPR metric -- skip; caller handles other metrics
			continue;
		}

		results.push_back(r);
	}

	return results;
}

} // namespace observe
} // namespace vsim
