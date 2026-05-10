/**
 * src/batch/batch_aggregator.cpp
 * ================================
 * WO-VSIM-62C — Static-Axis Post-Run Aggregation Implementation
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_aggregator.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace vsim {
namespace batch {

// ── stat helpers ──────────────────────────────────────────────────────────────

static double stat_mean(const std::vector<double>& v) {
	if (v.empty()) return 0.0;
	return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

static double stat_std(const std::vector<double>& v) {
	if (v.size() < 2) return 0.0;
	double m = stat_mean(v);
	double sq = 0.0;
	for (double x : v) sq += (x - m) * (x - m);
	return std::sqrt(sq / (v.size() - 1));
}

static double stat_median(std::vector<double> v) {
	if (v.empty()) return 0.0;
	std::sort(v.begin(), v.end());
	size_t n = v.size();
	return (n % 2 == 0) ? (v[n/2-1] + v[n/2]) / 2.0 : v[n/2];
}

static std::map<std::string, double> compute_stats(
	const std::vector<double>& vals,
	const std::vector<std::string>& requested)
{
	std::map<std::string, double> out;
	double m = stat_mean(vals);
	double s = stat_std(vals);
	for (const auto& stat : requested) {
		if (stat == "mean")   out["mean"]   = m;
		if (stat == "std")    out["std"]    = s;
		if (stat == "cv")     out["cv"]     = (m != 0.0) ? s / std::abs(m) : 0.0;
		if (stat == "min")    out["min"]    = vals.empty() ? 0.0 : *std::min_element(vals.begin(), vals.end());
		if (stat == "max")    out["max"]    = vals.empty() ? 0.0 : *std::max_element(vals.begin(), vals.end());
		if (stat == "median") out["median"] = stat_median(vals);
	}
	return out;
}

// ── group key ────────────────────────────────────────────────────────────────

static std::map<std::string, std::string> make_group_key(
	const BatchRunRecord&            rec,
	const std::vector<std::string>&  group_by)
{
	std::map<std::string, std::string> key;
	for (const auto& g : group_by) {
		auto it = rec.axis_values.find(g);
		key[g] = (it != rec.axis_values.end()) ? it->second : "";
	}
	return key;
}

// ── aggregate_runs ────────────────────────────────────────────────────────────

BatchAggregateResult aggregate_runs(
	const std::vector<BatchRunRecord>&  records,
	const BatchAggregateSection&        agg)
{
	BatchAggregateResult result;
	if (!agg.enabled || records.empty()) return result;

	// Collect unique groups
	std::vector<std::map<std::string, std::string>> group_keys_seen;
	auto find_or_add = [&](const std::map<std::string, std::string>& k) -> size_t {
		for (size_t i = 0; i < group_keys_seen.size(); ++i)
			if (group_keys_seen[i] == k) return i;
		group_keys_seen.push_back(k);
		result.groups.push_back({});
		result.groups.back().group_keys = k;
		return group_keys_seen.size() - 1;
	};

	// Accumulate values per group per metric
	std::vector<std::map<std::string, std::vector<double>>> group_vals;

	for (const auto& rec : records) {
		auto gk = make_group_key(rec, agg.group_by);
		size_t gi = find_or_add(gk);
		if (group_vals.size() <= gi) group_vals.resize(gi + 1);

		result.groups[gi].run_count++;
		for (const auto& [metric, val] : rec.metrics)
			group_vals[gi][metric].push_back(val);
	}

	const std::vector<std::string> stats = {"mean", "std", "cv", "min", "max", "median"};

	for (size_t gi = 0; gi < result.groups.size(); ++gi) {
		for (auto& [metric, vals] : group_vals[gi])
			result.groups[gi].results[metric] = compute_stats(vals, stats);
	}

	return result;
}

// ── write_batch_summary ───────────────────────────────────────────────────────

void write_batch_summary(const std::vector<BatchRunRecord>& records,
						  const std::string&                  path)
{
	if (records.empty()) return;
	std::ofstream f(path);
	if (!f.is_open())
		throw std::runtime_error("write_batch_summary: cannot write " + path);

	// Collect all axis and metric keys
	std::vector<std::string> axis_keys, metric_keys;
	for (const auto& [k, _] : records.front().axis_values)  axis_keys.push_back(k);
	for (const auto& [k, _] : records.front().metrics)     metric_keys.push_back(k);

	f << "run_id\tcase_name\tverify_status\tfailure_modes";
	for (const auto& k : axis_keys)   f << "\t" << k;
	for (const auto& k : metric_keys) f << "\t" << k;
	f << "\n";

	for (const auto& rec : records) {
		std::string fm;
		for (size_t i = 0; i < rec.failure_modes.size(); ++i) {
			if (i) fm += ",";
			fm += rec.failure_modes[i];
		}
		f << rec.run_id << "\t" << rec.case_name << "\t"
		  << rec.verify_status << "\t" << fm;
		for (const auto& k : axis_keys) {
			auto it = rec.axis_values.find(k);
			f << "\t" << (it != rec.axis_values.end() ? it->second : "");
		}
		for (const auto& k : metric_keys) {
			auto it = rec.metrics.find(k);
			f << "\t" << (it != rec.metrics.end() ? std::to_string(it->second) : "");
		}
		f << "\n";
	}
}

// ── write_ranked_candidates ───────────────────────────────────────────────────

void write_ranked_candidates(const std::vector<BatchRunRecord>& records,
							  const std::string&                  rank_by,
							  const std::string&                  path)
{
	std::vector<const BatchRunRecord*> eligible;
	for (const auto& rec : records) {
		if (rec.verify_status == "PASS" || rec.verify_status == "WARN")
			eligible.push_back(&rec);
	}

	std::sort(eligible.begin(), eligible.end(),
		[&](const BatchRunRecord* a, const BatchRunRecord* b) {
			auto ai = a->metrics.find(rank_by);
			auto bi = b->metrics.find(rank_by);
			double av = (ai != a->metrics.end()) ? ai->second : 0.0;
			double bv = (bi != b->metrics.end()) ? bi->second : 0.0;
			return av > bv; // descending
		});

	std::ofstream f(path);
	if (!f.is_open())
		throw std::runtime_error("write_ranked_candidates: cannot write " + path);

	f << "rank\trun_id\tcase_name\tverify_status\t" << rank_by << "\n";
	int rank = 1;
	for (const auto* rec : eligible) {
		auto it = rec->metrics.find(rank_by);
		double val = (it != rec->metrics.end()) ? it->second : 0.0;
		f << rank++ << "\t" << rec->run_id << "\t" << rec->case_name
		  << "\t" << rec->verify_status << "\t" << val << "\n";
	}
}

// ── write_aggregate_tsv ───────────────────────────────────────────────────────

void write_aggregate_tsv(const BatchAggregateResult& result, const std::string& path)
{
	std::ofstream f(path);
	if (!f.is_open())
		throw std::runtime_error("write_aggregate_tsv: cannot write " + path);

	if (result.groups.empty()) return;

	// Header: group key columns + metric.stat columns
	const auto& first = result.groups.front();
	for (auto& [k, _] : first.group_keys) f << k << "\t";
	f << "run_count";
	for (auto& [metric, stats] : first.results)
		for (auto& [stat, _] : stats)
			f << "\t" << metric << "." << stat;
	f << "\n";

	for (const auto& grp : result.groups) {
		for (auto& [k, v] : grp.group_keys) f << v << "\t";
		f << grp.run_count;
		for (auto& [metric, stats] : grp.results)
			for (auto& [stat, val] : stats)
				f << "\t" << val;
		f << "\n";
	}
}

// ── write_aggregate_json ──────────────────────────────────────────────────────

void write_aggregate_json(const BatchAggregateResult& result, const std::string& path)
{
	std::ofstream f(path);
	if (!f.is_open())
		throw std::runtime_error("write_aggregate_json: cannot write " + path);

	f << "{\n  \"groups\": [\n";
	for (size_t gi = 0; gi < result.groups.size(); ++gi) {
		const auto& grp = result.groups[gi];
		f << "    {\n";
		f << "      \"group_keys\": {";
		bool first = true;
		for (auto& [k, v] : grp.group_keys) {
			if (!first) f << ", "; first = false;
			f << "\"" << k << "\": \"" << v << "\"";
		}
		f << "},\n";
		f << "      \"run_count\": " << grp.run_count << ",\n";
		f << "      \"results\": {\n";
		bool fm = true;
		for (auto& [metric, stats] : grp.results) {
			if (!fm) f << ",\n"; fm = false;
			f << "        \"" << metric << "\": {";
			bool fs = true;
			for (auto& [stat, val] : stats) {
				if (!fs) f << ", "; fs = false;
				f << "\"" << stat << "\": " << val;
			}
			f << "}";
		}
		f << "\n      }\n    }";
		if (gi + 1 < result.groups.size()) f << ",";
		f << "\n";
	}
	f << "  ]\n}\n";
}

} // namespace batch
} // namespace vsim
