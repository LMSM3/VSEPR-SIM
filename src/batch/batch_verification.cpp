/**
 * src/batch/batch_verification.cpp
 * ==================================
 * WO-VSEPR-SIM-62B — Batch Verification Aggregation Kernel
 *
 * Reads verify_report.json outputs from per-run folders, classifies
 * failure modes, groups results by declared axes, evaluates pass-rate
 * gates, and writes batch-level summary artefacts.
 *
 * Doctrine: this module NEVER recomputes verification checks.
 * It trusts the per-run verify_report.json written by WO-VSEPR-SIM-62A.
 *
 * WO-VSEPR-SIM-62B  |  beta-12
 */

#include "src/batch/batch_verification_kernel.hpp"
#include "include/batch/failure_modes.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ─── minimal JSON value extraction (no external dep) ─────────────────────────
static bool json_bool(const std::string& json, const std::string& key, bool def = false) {
	auto pos = json.find("\"" + key + "\"");
	if (pos == std::string::npos) return def;
	auto colon = json.find(':', pos);
	if (colon == std::string::npos) return def;
	auto vstart = json.find_first_not_of(" \t\r\n", colon + 1);
	if (vstart == std::string::npos) return def;
	return json.compare(vstart, 4, "true") == 0;
}

static std::string json_str(const std::string& json, const std::string& key,
							 const std::string& def = "") {
	auto pos = json.find("\"" + key + "\"");
	if (pos == std::string::npos) return def;
	auto colon = json.find(':', pos);
	if (colon == std::string::npos) return def;
	auto q1 = json.find('"', colon + 1);
	if (q1 == std::string::npos) return def;
	auto q2 = json.find('"', q1 + 1);
	if (q2 == std::string::npos) return def;
	return json.substr(q1 + 1, q2 - q1 - 1);
}

static double json_double(const std::string& json, const std::string& key, double def = 0.0) {
	auto pos = json.find("\"" + key + "\"");
	if (pos == std::string::npos) return def;
	auto colon = json.find(':', pos);
	if (colon == std::string::npos) return def;
	auto vstart = json.find_first_not_of(" \t\r\n", colon + 1);
	if (vstart == std::string::npos) return def;
	try { return std::stod(json.substr(vstart)); } catch (...) { return def; }
}

// ─────────────────────────────────────────────────────────────────────────────

namespace vsim {
namespace batch {

// ── load_verification_record ──────────────────────────────────────────────────

VerificationRunRecord load_verification_record(const std::string& run_id,
											   const std::string& run_dir) {
	VerificationRunRecord rec;
	rec.run_id    = run_id;
	rec.case_name = run_id;

	std::string path = run_dir + "/verify_report.json";
	std::ifstream f(path);
	if (!f.good()) {
		rec.status       = "MISSING";
		rec.overall_pass = false;
		rec.failure_modes.push_back(to_string(BatchFailureMode::FAIL_OUTPUT_MISSING));
		return rec;
	}

	std::string json((std::istreambuf_iterator<char>(f)),
					  std::istreambuf_iterator<char>());

	if (json.empty()) {
		rec.status       = "ERROR";
		rec.overall_pass = false;
		rec.failure_modes.push_back(to_string(BatchFailureMode::FAIL_RUNTIME_CRASH));
		return rec;
	}

	rec.overall_pass           = json_bool  (json, "overall_pass");
	rec.status                 = json_str   (json, "status",   rec.overall_pass ? "PASS" : "FAIL");
	rec.structure_pass         = json_bool  (json, "structure_pass");
	rec.rdf_pass               = json_bool  (json, "rdf_pass");
	rec.mass_pass              = json_bool  (json, "mass_pass");
	rec.msd_pass               = json_bool  (json, "msd_pass");
	rec.rdf_first_peak_error_A = json_double(json, "rdf_first_peak_error_A");
	rec.rdf_second_peak_error_A= json_double(json, "rdf_second_peak_error_A");
	rec.mass_relative_error    = json_double(json, "mass_relative_error");
	rec.structure_nn_error_A   = json_double(json, "structure_nn_error_A");
	rec.msd_bound_error        = json_double(json, "msd_bound_error");

	return rec;
}

// ── classify_failure_modes ───────────────────────────────────────────────────

void classify_failure_modes(VerificationRunRecord& rec) {
	if (rec.status == "MISSING") {
		rec.failure_modes.push_back(to_string(BatchFailureMode::FAIL_OUTPUT_MISSING));
		return;
	}
	if (rec.status == "ERROR") {
		rec.failure_modes.push_back(to_string(BatchFailureMode::FAIL_RUNTIME_CRASH));
		return;
	}
	if (!rec.mass_pass)
		rec.failure_modes.push_back(to_string(BatchFailureMode::FAIL_MASS_CONSERVATION));
	if (!rec.structure_pass)
		rec.failure_modes.push_back(to_string(BatchFailureMode::FAIL_STRUCTURE_COORDINATION));
	if (!rec.rdf_pass)
		rec.failure_modes.push_back(to_string(BatchFailureMode::FAIL_RDF_REFERENCE));
	if (!rec.msd_pass && rec.msd_bound_error > 0.0)
		rec.failure_modes.push_back(to_string(BatchFailureMode::FAIL_MSD_SOLID_BOUND));
}

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string group_key(const VerificationRunRecord&    rec,
							  const std::vector<std::string>& group_by) {
	std::string key;
	for (const auto& ax : group_by) {
		auto it = rec.axis_values.find(ax);
		key += ax + "=";
		key += (it != rec.axis_values.end()) ? it->second : "?";
		key += " / ";
	}
	if (key.size() >= 3) key.erase(key.size() - 3); // trim trailing " / "
	return key;
}

static double safe_rate(int num, int den) {
	return (den > 0) ? static_cast<double>(num) / den : 0.0;
}

// ── aggregate_verification ────────────────────────────────────────────────────

BatchVerificationSummary aggregate_verification(
	const std::vector<VerificationRunRecord>& records,
	const BatchAggregateVerifySection&        policy)
{
	BatchVerificationSummary sum;
	sum.run_records = records;
	sum.total_runs  = static_cast<int>(records.size());

	int s_pass = 0, r_pass = 0, m_pass = 0, d_pass = 0;

	// Tally top-level counts and per-check pass counts
	for (const auto& rec : records) {
		if      (rec.status == "PASS")    { ++sum.passed_runs;  ++s_pass; ++r_pass; ++m_pass; ++d_pass; }
		else if (rec.status == "WARN")    { ++sum.warned_runs;  }
		else if (rec.status == "FAIL")    { ++sum.failed_runs;  }
		else if (rec.status == "SKIP")    { ++sum.skipped_runs; }
		else if (rec.status == "MISSING") { ++sum.missing_runs; ++sum.failed_runs; }
		else if (rec.status == "ERROR")   { ++sum.error_runs;   ++sum.failed_runs; }

		// Per-check: include WARN in pass tallies (doc §4)
		bool counted = (rec.status == "PASS" || rec.status == "WARN");
		if (rec.structure_pass && counted) ++s_pass;
		if (rec.rdf_pass       && counted) ++r_pass;
		if (rec.mass_pass      && counted) ++m_pass;
		if (rec.msd_pass       && counted) ++d_pass;

		for (const auto& fm : rec.failure_modes)
			++sum.failure_mode_counts[fm];
	}

	// PASS + WARN are both counted as "passing" for rate calculation
	int eff_pass = sum.passed_runs + sum.warned_runs;
	sum.overall_pass_rate   = safe_rate(eff_pass, sum.total_runs);
	sum.structure_pass_rate = safe_rate(s_pass,   sum.total_runs);
	sum.rdf_pass_rate       = safe_rate(r_pass,   sum.total_runs);
	sum.mass_pass_rate      = safe_rate(m_pass,   sum.total_runs);
	sum.msd_pass_rate       = safe_rate(d_pass,   sum.total_runs);

	// Grouped summaries (only when group_by is specified)
	if (!policy.group_by.empty()) {
		std::map<std::string, GroupedVerificationSummary> groups;

		for (const auto& rec : records) {
			const std::string gk = group_key(rec, policy.group_by);
			auto& g = groups[gk];

			if (g.group_keys.empty()) {
				for (const auto& ax : policy.group_by) {
					auto it = rec.axis_values.find(ax);
					g.group_keys[ax] = (it != rec.axis_values.end()) ? it->second : "?";
				}
			}

			++g.runs;
			bool w = (rec.status == "WARN");
			bool p = (rec.status == "PASS");
			bool f = (!p && !w);
			if (p) ++g.passed;
			if (f) ++g.failed;
			if (w) ++g.warned;

			if (rec.structure_pass) ++g.passed; // track below via rates
			if (rec.rdf_pass)       {}
			if (rec.mass_pass)      {}
			if (rec.msd_pass)       {}

			for (const auto& fm : rec.failure_modes)
				++g.failure_modes[fm];

			// Accumulate for mean errors (computed after loop)
			g.mean_errors["rdf_first_peak_error_A"]  += rec.rdf_first_peak_error_A;
			g.mean_errors["rdf_second_peak_error_A"] += rec.rdf_second_peak_error_A;
			g.mean_errors["mass_relative_error"]     += rec.mass_relative_error;
			g.mean_errors["structure_nn_error_A"]    += rec.structure_nn_error_A;
			g.mean_errors["msd_bound_error"]         += rec.msd_bound_error;
		}

		// Second pass per group: compute pass rates and mean errors
		for (const auto& rec : records) {
			const std::string gk = group_key(rec, policy.group_by);
			auto& g = groups[gk];
			bool counted = (rec.status == "PASS" || rec.status == "WARN");
			if (counted) {
				if (rec.structure_pass) g.structure_pass_rate += 1.0;
				if (rec.rdf_pass)       g.rdf_pass_rate       += 1.0;
				if (rec.mass_pass)      g.mass_pass_rate      += 1.0;
				if (rec.msd_pass)       g.msd_pass_rate       += 1.0;
			}
		}

		for (auto& [k, g] : groups) {
			int eff = g.passed + g.warned;
			g.pass_rate           = safe_rate(eff,                          g.runs);
			g.structure_pass_rate = safe_rate((int)g.structure_pass_rate,   g.runs);
			g.rdf_pass_rate       = safe_rate((int)g.rdf_pass_rate,         g.runs);
			g.mass_pass_rate      = safe_rate((int)g.mass_pass_rate,        g.runs);
			g.msd_pass_rate       = safe_rate((int)g.msd_pass_rate,         g.runs);
			for (auto& [ek, ev] : g.mean_errors)
				ev = (g.runs > 0) ? ev / g.runs : 0.0;
			sum.grouped.push_back(g);
		}
	}

	return sum;
}

// ── evaluate_gates ────────────────────────────────────────────────────────────

void evaluate_gates(BatchVerificationSummary&              summary,
					const BatchAggregateVerifyGatesSection& gates)
{
	summary.batch_empirical_ready = true;
	summary.gate_failure_reason.clear();

	auto check = [&](double rate, double min_rate, const std::string& label) {
		if (min_rate > 0.0 && rate < min_rate) {
			summary.batch_empirical_ready = false;
			if (!summary.gate_failure_reason.empty())
				summary.gate_failure_reason += "; ";
			std::ostringstream oss;
			oss << label << " " << std::fixed << std::setprecision(4) << rate
				<< " below minimum " << min_rate;
			summary.gate_failure_reason += oss.str();
		}
	};

	check(summary.overall_pass_rate,   gates.min_overall_pass_rate,   "overall_pass_rate");
	check(summary.mass_pass_rate,      gates.min_mass_pass_rate,      "mass_pass_rate");
	check(summary.structure_pass_rate, gates.min_structure_pass_rate, "structure_pass_rate");
	check(summary.rdf_pass_rate,       gates.min_rdf_pass_rate,       "rdf_pass_rate");
	check(summary.msd_pass_rate,       gates.min_msd_pass_rate,       "msd_pass_rate");
}

// ── write_batch_verify_summary ────────────────────────────────────────────────

void write_batch_verify_summary(const BatchVerificationSummary& summary,
								const std::string& path) {
	std::ofstream f(path);
	if (!f.good()) return;
	f << "group\truns\tpassed\tfailed\twarned\tpass_rate\t"
		 "structure_pass_rate\trdf_pass_rate\tmass_pass_rate\tmsd_pass_rate\n";

	auto fmt = [](double v) -> std::string {
		std::ostringstream s; s << std::fixed << std::setprecision(4) << v; return s.str();
	};

	if (summary.grouped.empty()) {
		f << "all\t" << summary.total_runs << "\t" << summary.passed_runs << "\t"
		  << summary.failed_runs << "\t" << summary.warned_runs << "\t"
		  << fmt(summary.overall_pass_rate) << "\t"
		  << fmt(summary.structure_pass_rate) << "\t"
		  << fmt(summary.rdf_pass_rate) << "\t"
		  << fmt(summary.mass_pass_rate) << "\t"
		  << fmt(summary.msd_pass_rate) << "\n";
	} else {
		for (const auto& g : summary.grouped) {
			std::string gk;
			for (const auto& [k, v] : g.group_keys) gk += k + "=" + v + " / ";
			if (gk.size() >= 3) gk.erase(gk.size() - 3);
			f << gk << "\t" << g.runs << "\t" << g.passed << "\t"
			  << g.failed << "\t" << g.warned << "\t"
			  << fmt(g.pass_rate) << "\t"
			  << fmt(g.structure_pass_rate) << "\t"
			  << fmt(g.rdf_pass_rate) << "\t"
			  << fmt(g.mass_pass_rate) << "\t"
			  << fmt(g.msd_pass_rate) << "\n";
		}
	}
}

// ── write_batch_verify_matrix ─────────────────────────────────────────────────

void write_batch_verify_matrix(const BatchVerificationSummary& summary,
							   const std::string& path) {
	std::ofstream f(path);
	if (!f.good()) return;
	f << "group\tstructure\trdf\tmass\tmsd\n";

	auto fmt = [](double v) { std::ostringstream s; s << std::fixed << std::setprecision(4) << v; return s.str(); };

	auto write_row = [&](const std::string& g, double st, double rf, double ms, double md) {
		f << g << "\t" << fmt(st) << "\t" << fmt(rf) << "\t" << fmt(ms) << "\t" << fmt(md) << "\n";
	};

	if (summary.grouped.empty()) {
		write_row("all", summary.structure_pass_rate, summary.rdf_pass_rate,
				  summary.mass_pass_rate, summary.msd_pass_rate);
	} else {
		for (const auto& g : summary.grouped) {
			std::string gk;
			for (const auto& [k, v] : g.group_keys) gk += k + "=" + v + " / ";
			if (gk.size() >= 3) gk.erase(gk.size() - 3);
			write_row(gk, g.structure_pass_rate, g.rdf_pass_rate,
					  g.mass_pass_rate, g.msd_pass_rate);
		}
	}
}

// ── write_batch_failure_modes ─────────────────────────────────────────────────

void write_batch_failure_modes(const BatchVerificationSummary& summary,
							   const std::string& path) {
	std::ofstream f(path);
	if (!f.good()) return;
	f << "failure_mode\tcount\tfraction\n";

	// Sort descending by count
	std::vector<std::pair<std::string,int>> sorted(summary.failure_mode_counts.begin(),
												   summary.failure_mode_counts.end());
	std::sort(sorted.begin(), sorted.end(),
			  [](const auto& a, const auto& b){ return a.second > b.second; });

	double total = static_cast<double>(summary.total_runs);
	for (const auto& [code, cnt] : sorted) {
		double frac = (total > 0) ? cnt / total : 0.0;
		f << code << "\t" << cnt << "\t"
		  << std::fixed << std::setprecision(4) << frac << "\n";
	}
}

// ── write_batch_empirical_report ──────────────────────────────────────────────

void write_batch_empirical_report(const BatchVerificationSummary& summary,
								  const std::string& study_name,
								  const std::string& path) {
	std::ofstream f(path);
	if (!f.good()) return;

	auto fmt4 = [](double v) { std::ostringstream s; s << std::fixed << std::setprecision(4) << v; return s.str(); };
	auto pct  = [&](double v) { std::ostringstream s; s << std::fixed << std::setprecision(1) << v * 100.0 << "%"; return s.str(); };

	f << "# Batch Empirical Report — " << study_name << "\n\n";

	f << "## Summary\n\n";
	f << summary.total_runs << " runs completed. "
	  << (summary.passed_runs + summary.warned_runs) << " passed ("
	  << pct(summary.overall_pass_rate) << ").\n\n";

	f << "## Gate result\n\n";
	f << "batch_empirical_ready: " << (summary.batch_empirical_ready ? "true" : "false") << "\n";
	if (!summary.gate_failure_reason.empty())
		f << summary.gate_failure_reason << "\n";
	f << "\n";

	f << "## Pass rates by group\n\n";
	f << "| group | runs | passed | failed | warned | pass_rate | structure | rdf | mass | msd |\n";
	f << "|-------|------|--------|--------|--------|-----------|-----------|-----|------|-----|\n";
	if (summary.grouped.empty()) {
		f << "| all | " << summary.total_runs << " | " << summary.passed_runs << " | "
		  << summary.failed_runs << " | " << summary.warned_runs << " | "
		  << fmt4(summary.overall_pass_rate) << " | "
		  << fmt4(summary.structure_pass_rate) << " | "
		  << fmt4(summary.rdf_pass_rate) << " | "
		  << fmt4(summary.mass_pass_rate) << " | "
		  << fmt4(summary.msd_pass_rate) << " |\n";
	} else {
		for (const auto& g : summary.grouped) {
			std::string gk;
			for (const auto& [k, v] : g.group_keys) gk += k + "=" + v + " / ";
			if (gk.size() >= 3) gk.erase(gk.size() - 3);
			f << "| " << gk << " | " << g.runs << " | " << g.passed << " | "
			  << g.failed << " | " << g.warned << " | "
			  << fmt4(g.pass_rate) << " | "
			  << fmt4(g.structure_pass_rate) << " | "
			  << fmt4(g.rdf_pass_rate) << " | "
			  << fmt4(g.mass_pass_rate) << " | "
			  << fmt4(g.msd_pass_rate) << " |\n";
		}
	}
	f << "\n";

	f << "## Check matrix\n\n";
	f << "| group | structure | rdf | mass | msd |\n";
	f << "|-------|-----------|-----|------|-----|\n";
	if (summary.grouped.empty()) {
		f << "| all | " << fmt4(summary.structure_pass_rate) << " | "
		  << fmt4(summary.rdf_pass_rate) << " | "
		  << fmt4(summary.mass_pass_rate) << " | "
		  << fmt4(summary.msd_pass_rate) << " |\n";
	} else {
		for (const auto& g : summary.grouped) {
			std::string gk;
			for (const auto& [k, v] : g.group_keys) gk += k + "=" + v + " / ";
			if (gk.size() >= 3) gk.erase(gk.size() - 3);
			f << "| " << gk << " | "
			  << fmt4(g.structure_pass_rate) << " | "
			  << fmt4(g.rdf_pass_rate) << " | "
			  << fmt4(g.mass_pass_rate) << " | "
			  << fmt4(g.msd_pass_rate) << " |\n";
		}
	}
	f << "\n";

	f << "## Failure modes\n\n";
	f << "| failure_mode | count | fraction |\n";
	f << "|-------------|-------|----------|\n";
	std::vector<std::pair<std::string,int>> sorted(summary.failure_mode_counts.begin(),
												   summary.failure_mode_counts.end());
	std::sort(sorted.begin(), sorted.end(),
			  [](const auto& a, const auto& b){ return a.second > b.second; });
	for (const auto& [code, cnt] : sorted) {
		double frac = (summary.total_runs > 0) ? (double)cnt / summary.total_runs : 0.0;
		f << "| " << code << " | " << cnt << " | " << fmt4(frac) << " |\n";
	}
	f << "\n";
}

} // namespace batch
} // namespace vsim
