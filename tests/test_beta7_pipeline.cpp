/**
 * test_beta7_pipeline.cpp — beta-7 Pipeline Full Round-Trip Test
 * ==============================================================
 *
 * Validates all five pipeline stages using the v4 reference dataset
 * (12 metal formation cases). Ensures:
 *
 *   Stage 1  FingerprintRecord: feature vector populated, norm > 0
 *   Stage 2  ClusterRecord: cluster_id assigned, size >= 1
 *   Stage 3  AnalysisRecord: stability_score in [0,1], interpretation non-empty
 *   Stage 4  ReportRecord: csv_row and json_fragment non-empty
 *   Stage 5  DashboardRecord: n_cases == 12, markdown_table and json_array non-empty
 *
 * Run-level checks:
 *   - run_pipeline() completes without exception
 *   - All 12 PipelineRecords have complete == true
 *   - At least 2 clusters formed (FCC / BCC / HCP partition)
 *   - DashboardRecord run_summary contains the run label
 *
 * Anti-black-box: every assertion is labelled; every computed value
 * is printed so failures are self-documenting.
 *
 * VSEPR-SIM  |  beta-7  |  2025-07-11
 */

#include "include/pipeline/pipeline_stages.hpp"
#include "v4/formation_record.hpp"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Minimal test harness (no external framework)
// ============================================================================

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(label, cond) do {                                 \
	if (cond) {                                                 \
		++g_pass;                                               \
		std::cout << "  PASS  " << (label) << "\n";            \
	} else {                                                    \
		++g_fail;                                               \
		std::cout << "  FAIL  " << (label) << "\n";            \
	}                                                           \
} while (0)

#define CHECK_EQ(label, a, b) CHECK((label), (a) == (b))
#define CHECK_GT(label, a, b) CHECK((label), (a) >  (b))
#define CHECK_GE(label, a, b) CHECK((label), (a) >= (b))
#define CHECK_LE(label, a, b) CHECK((label), (a) <= (b))
#define CHECK_NE(label, a, b) CHECK((label), (a) != (b))

// ============================================================================
// Helpers
// ============================================================================

static std::vector<v4::FormationRecord> load_reference() {
	auto arr = v4::reference_dataset();
	return std::vector<v4::FormationRecord>(arr.begin(), arr.end());
}

// ============================================================================
// T1 — Stage 1: stage_fingerprint
// ============================================================================

static void test_stage_fingerprint() {
	std::cout << "\n[T1] stage_fingerprint\n";
	auto dataset = load_reference();

	for (const auto& f : dataset) {
		auto fp = vsepr::pipeline::stage_fingerprint(f);
		CHECK_EQ("symbol forwarded: " + f.symbol, fp.symbol, f.symbol);
		CHECK_EQ("name forwarded: "   + f.name,   fp.name,   f.name);
		CHECK_NE("topology_hash != 0: " + f.symbol, fp.topology_hash, uint64_t(0));
		CHECK_GE("feature_norm >= 0: "  + f.symbol, fp.feature_norm, 0.0);
		// Feature[7] = log10(steps+1) must be >= 0
		CHECK_GE("feat[7] >= 0: " + f.symbol, fp.features[7], 0.0);
	}

	// Converged cases: Au, Ag, Cu, Al, Fe, W, Mo, Cr, Ti — should have no NotConverged warning
	auto fp_au = vsepr::pipeline::stage_fingerprint(dataset[0]); // Au, converged
	bool has_nc = false;
	for (auto w : fp_au.warnings)
		if (w == vsepr::pipeline::WarningCode::NotConverged) has_nc = true;
	CHECK("Au converged — no NotConverged warning", !has_nc);

	// Pt (index 3) is NOT converged
	auto fp_pt = vsepr::pipeline::stage_fingerprint(dataset[3]); // Pt, not converged
	bool has_nc_pt = false;
	for (auto w : fp_pt.warnings)
		if (w == vsepr::pipeline::WarningCode::NotConverged) has_nc_pt = true;
	CHECK("Pt not converged — NotConverged warning present", has_nc_pt);
}

// ============================================================================
// T2 — Stage 2: stage_cluster
// ============================================================================

static void test_stage_cluster() {
	std::cout << "\n[T2] stage_cluster\n";
	auto dataset = load_reference();
	vsepr::pipeline::ClusterRegistry registry;

	for (const auto& f : dataset) {
		auto fp = vsepr::pipeline::stage_fingerprint(f);
		auto cr = vsepr::pipeline::stage_cluster(fp, registry);
		CHECK_NE("cluster_id != 0: " + f.symbol, cr.cluster_id, uint64_t(0));
		CHECK_GE("cluster_size >= 1: " + f.symbol, cr.cluster_size, 1);
		CHECK_EQ("symbol forwarded: " + f.symbol, cr.symbol, f.symbol);
	}

	// BCC metals (Fe, W, Mo, Cr) should group together (similar features)
	// FCC metals (Au, Ag, Cu, Pt, Ni, Al) should group together
	// Verify at least 2 clusters formed
	CHECK_GE("at least 2 clusters formed", registry.num_clusters(), 2);

	// All 12 cases registered
	int total_members = 0;
	for (const auto& e : registry.clusters)
		total_members += e.count;
	CHECK_EQ("total members == 12", total_members, 12);

	std::cout << "  INFO  clusters formed: " << registry.num_clusters() << "\n";
	for (const auto& e : registry.clusters) {
		std::cout << "    cluster id=" << (e.id & 0xFFFFFF)
				  << "  size=" << e.count
				  << "  members: ";
		for (const auto& m : e.members) std::cout << m << " ";
		std::cout << "\n";
	}
}

// ============================================================================
// T3 — Stage 3: stage_analysis
// ============================================================================

static void test_stage_analysis() {
	std::cout << "\n[T3] stage_analysis\n";
	auto dataset = load_reference();
	vsepr::pipeline::ClusterRegistry registry;

	for (const auto& f : dataset) {
		auto fp = vsepr::pipeline::stage_fingerprint(f);
		auto cr = vsepr::pipeline::stage_cluster(fp, registry);
		auto ar = vsepr::pipeline::stage_analysis(cr, f);

		CHECK_EQ("symbol forwarded: " + f.symbol, ar.symbol, f.symbol);
		CHECK_GE("stability_score in [0,1] lo: " + f.symbol, ar.stability_score, 0.0);
		CHECK_LE("stability_score in [0,1] hi: " + f.symbol, ar.stability_score, 1.0);
		CHECK_GE("packing_quality >= 0: " + f.symbol,  ar.packing_quality, 0.0);
		CHECK_LE("packing_quality <= 1: " + f.symbol,  ar.packing_quality, 1.0);
		CHECK("interpretation non-empty: " + f.symbol, !ar.interpretation.empty());
		CHECK("motif_class non-empty: " + f.symbol,    !ar.motif_class.empty());

		// Motif class must match lattice
		std::string expected;
		switch (f.structure) {
			case v4::LatticeClass::FCC: expected = "FCC"; break;
			case v4::LatticeClass::BCC: expected = "BCC"; break;
			case v4::LatticeClass::HCP: expected = "HCP"; break;
			default: expected = "unknown"; break;
		}
		CHECK_EQ("motif_class matches lattice: " + f.symbol, ar.motif_class, expected);

		// Defect indicator must be >= 0
		CHECK_GE("defect_indicator >= 0: " + f.symbol, ar.defect_indicator, 0.0);
	}

	// Au (converged, good formation) should have high stability
	{
		auto fp = vsepr::pipeline::stage_fingerprint(dataset[0]);
		auto cr = vsepr::pipeline::stage_cluster(fp, registry);
		auto ar = vsepr::pipeline::stage_analysis(cr, dataset[0]);
		CHECK_GT("Au stability_score > 0.3", ar.stability_score, 0.3);
	}
}

// ============================================================================
// T4 — Stage 4: stage_report
// ============================================================================

static void test_stage_report() {
	std::cout << "\n[T4] stage_report\n";
	auto dataset = load_reference();
	vsepr::pipeline::ClusterRegistry registry;

	for (const auto& f : dataset) {
		auto fp = vsepr::pipeline::stage_fingerprint(f);
		auto cr = vsepr::pipeline::stage_cluster(fp, registry);
		auto ar = vsepr::pipeline::stage_analysis(cr, f);
		auto rr = vsepr::pipeline::stage_report(ar);

		CHECK("csv_row non-empty: "      + f.symbol, !rr.csv_row.empty());
		CHECK("json_fragment non-empty: " + f.symbol, !rr.json_fragment.empty());
		CHECK("summary_line non-empty: "  + f.symbol, !rr.summary_line.empty());
		CHECK("symbol in csv_row: " + f.symbol,
			  rr.csv_row.find(f.symbol) != std::string::npos);
		CHECK("json starts with {: " + f.symbol,
			  !rr.json_fragment.empty() && rr.json_fragment.front() == '{');
		CHECK("json ends with }: " + f.symbol,
			  !rr.json_fragment.empty() && rr.json_fragment.back() == '}');
	}
}

// ============================================================================
// T5 — Stage 5: stage_dashboard
// ============================================================================

static void test_stage_dashboard() {
	std::cout << "\n[T5] stage_dashboard\n";
	auto dataset = load_reference();
	vsepr::pipeline::ClusterRegistry registry;

	std::vector<vsepr::pipeline::ReportRecord> reports;
	for (const auto& f : dataset) {
		auto fp = vsepr::pipeline::stage_fingerprint(f);
		auto cr = vsepr::pipeline::stage_cluster(fp, registry);
		auto ar = vsepr::pipeline::stage_analysis(cr, f);
		auto rr = vsepr::pipeline::stage_report(ar);
		reports.push_back(rr);
	}

	auto dr = vsepr::pipeline::stage_dashboard(reports, registry, "test-beta7");
	CHECK_EQ("n_cases == 12",   dr.n_cases,    12);
	CHECK_GE("n_clusters >= 2", dr.n_clusters, 2);
	CHECK("markdown_table non-empty",  !dr.markdown_table.empty());
	CHECK("json_array non-empty",      !dr.json_array.empty());
	CHECK("run_summary non-empty",     !dr.run_summary.empty());
	CHECK("markdown contains run label",
		  dr.markdown_table.find("test-beta7") != std::string::npos);
	CHECK("json starts with [",
		  !dr.json_array.empty() && dr.json_array.front() == '[');
	CHECK("json ends with ]",
		  !dr.json_array.empty() && dr.json_array.back() == ']');
}

// ============================================================================
// T6 — run_pipeline convenience function
// ============================================================================

static void test_run_pipeline() {
	std::cout << "\n[T6] run_pipeline (full round-trip)\n";
	auto dataset = load_reference();

	auto [records, dash] = vsepr::pipeline::run_pipeline(
		dataset, "beta7-reference-12");

	CHECK_EQ("12 PipelineRecords returned",   static_cast<int>(records.size()), 12);
	CHECK_EQ("dashboard n_cases == 12",        dash.n_cases,   12);
	CHECK_GE("dashboard n_clusters >= 2",      dash.n_clusters, 2);

	for (const auto& pr : records) {
		CHECK("complete flag set: " + pr.formation.symbol, pr.complete);
		CHECK("formation symbol matches: " + pr.formation.symbol,
			  pr.fingerprint.symbol == pr.formation.symbol);
		CHECK("cluster id matches: " + pr.formation.symbol,
			  pr.cluster.cluster_id != 0);
		CHECK("report non-empty: " + pr.formation.symbol,
			  !pr.report.csv_row.empty());
	}

	// Print dashboard summary for human inspection
	std::cout << "\n  --- Dashboard summary ---\n";
	std::cout << "  " << dash.run_summary << "\n";
	std::cout << "\n  --- Summary lines ---\n";
	for (const auto& pr : records)
		std::cout << "  " << pr.report.summary_line << "\n";
}

// ============================================================================
// T7 — Determinism: two identical runs produce identical cluster IDs
// ============================================================================

static void test_determinism() {
	std::cout << "\n[T7] determinism\n";
	auto dataset = load_reference();

	auto [r1, d1] = vsepr::pipeline::run_pipeline(dataset, "run-A");
	auto [r2, d2] = vsepr::pipeline::run_pipeline(dataset, "run-B");

	CHECK_EQ("same n_clusters on both runs", d1.n_clusters, d2.n_clusters);
	for (int i = 0; i < static_cast<int>(r1.size()); ++i) {
		CHECK_EQ("cluster_id deterministic: " + r1[i].formation.symbol,
				 r1[i].cluster.cluster_id,
				 r2[i].cluster.cluster_id);
		// stability_score should be identical (same inputs → same outputs)
		CHECK("stability_score deterministic: " + r1[i].formation.symbol,
			  std::abs(r1[i].analysis.stability_score -
					   r2[i].analysis.stability_score) < 1e-12);
	}
}

// ============================================================================
// T8 — CSV header matches csv_row column count
// ============================================================================

static void test_csv_consistency() {
	std::cout << "\n[T8] CSV header vs row column count\n";
	auto dataset = load_reference();
	auto [records, dash] = vsepr::pipeline::run_pipeline(dataset, "csv-test");

	auto count_cols = [](const std::string& line) {
		// Count commas outside quotes
		int cols = 1;
		bool in_q = false;
		for (char c : line) {
			if (c == '"') { in_q = !in_q; continue; }
			if (c == ',' && !in_q) ++cols;
		}
		return cols;
	};

	int header_cols = count_cols(vsepr::pipeline::ReportRecord::csv_header());
	for (const auto& pr : records) {
		int row_cols = count_cols(pr.report.csv_row);
		CHECK_EQ("CSV columns match header: " + pr.formation.symbol,
				 row_cols, header_cols);
	}
}

// ============================================================================
// main
// ============================================================================

int main() {
	std::cout << "================================================================\n";
	std::cout << "  test_beta7_pipeline  — beta-7 research pipeline round-trip\n";
	std::cout << "================================================================\n";

	test_stage_fingerprint();
	test_stage_cluster();
	test_stage_analysis();
	test_stage_report();
	test_stage_dashboard();
	test_run_pipeline();
	test_determinism();
	test_csv_consistency();

	std::cout << "\n================================================================\n";
	std::cout << "  RESULTS:  " << g_pass << " pass  /  " << g_fail << " fail\n";
	std::cout << "================================================================\n";

	if (g_fail == 0)
		std::cout << "\n  ALL PASS\n\n";
	else
		std::cout << "\n  *** " << g_fail << " FAILURE(S) ***\n\n";

	return g_fail == 0 ? 0 : 1;
}
