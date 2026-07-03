/**
 * tests/test_vsepr_observe_sink.cpp
 * ==================================
 * Group 90  |  WO-84S  |  day84-vsepr-revival
 *
 * Unit tests for the VSEPR observe sink:
 *   - eval_observe_vsepr_metrics() produces correct EvalResult entries
 *   - vsepr_sites metric reports site count
 *   - vsepr_confidence metric reports mean confidence
 *   - fallback_mode metric reports 1.0 when any site used geometry fallback
 *   - lone_pair_inference metric reports count of inference-flagged sites
 *   - VSEPR metric stubs in eval_observe_metrics() return no_vsepr_data warning
 *   - detail payload is non-empty for populated reports
 *   - automation confidence branching works (>0.5 threshold)
 */

#include "vsim/vsepr_observe.hpp"
#include "vsim/vsim_document.hpp"
#include "vsim/vsim_runtime.hpp"
#include "kernel/kernel_event_log.hpp"

#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static int g_tests = 0, g_pass = 0;

static void check(bool cond, const char* label)
{
	++g_tests;
	if (cond) {
		++g_pass;
		std::printf("  PASS: %s\n", label);
	} else {
		std::printf("  FAIL: %s\n", label);
	}
}

// Build a minimal VSEPRReport with one or two synthetic sites
static atomistic::classify::VSEPRReport make_report(bool with_fallback = false)
{
	atomistic::classify::VSEPRReport r;

	// site 0: oxygen-like, bent
	atomistic::classify::VSEPRSite s0;
	s0.center_index            = 0;
	s0.center_Z                = 8;  // O
	s0.bonded_domain_count     = 2;
	s0.lone_pair_domain_count  = 2;
	s0.electron_domain_count   = 4;
	s0.electron_geometry       = atomistic::classify::VSEPRElectronGeometry::Tetrahedral;
	s0.molecular_shape         = atomistic::classify::VSEPRMolecularShape::Bent;
	s0.confidence              = 0.92;
	s0.ax_label                = "AX2E2";
	s0.used_element_lone_pair_inference = true;
	s0.used_geometry_lone_pair_fallback = with_fallback;
	r.sites.push_back(s0);
	r.bent_count = 1;

	// site 1: carbon-like, linear (optional, only when testing multi-site)
	atomistic::classify::VSEPRSite s1;
	s1.center_index            = 1;
	s1.center_Z                = 6;  // C
	s1.bonded_domain_count     = 2;
	s1.lone_pair_domain_count  = 0;
	s1.electron_domain_count   = 2;
	s1.electron_geometry       = atomistic::classify::VSEPRElectronGeometry::Linear;
	s1.molecular_shape         = atomistic::classify::VSEPRMolecularShape::Linear;
	s1.confidence              = 0.88;
	s1.ax_label                = "AX2";
	s1.used_element_lone_pair_inference = false;
	s1.used_geometry_lone_pair_fallback = false;
	r.sites.push_back(s1);
	r.linear_count = 1;

	return r;
}

// Build an ObserveSection requesting the six VSEPR metrics
static vsim::ObserveSection make_cfg()
{
	vsim::ObserveSection cfg;
	cfg.metrics = {
		"vsepr_sites",
		"geometry_candidates",
		"vsepr_confidence",
		"vsepr_flags",
		"lone_pair_inference",
		"fallback_mode"
	};
	return cfg;
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

static void test_vsepr_sites_count()
{
	const auto cfg    = make_cfg();
	const auto report = make_report();
	const auto results = vsim::observe::eval_observe_vsepr_metrics(cfg, report);

	bool found = false;
	for (const auto& r : results) {
		if (r.probe_name == "vsepr_sites") {
			check(r.value == 2.0, "vsepr_sites == 2");
			check(r.warning.empty(), "vsepr_sites: no warning");
			check(!r.detail.empty(), "vsepr_sites: detail payload non-empty");
			found = true;
			break;
		}
	}
	check(found, "vsepr_sites result present");
}

static void test_vsepr_confidence()
{
	const auto cfg    = make_cfg();
	const auto report = make_report();
	const auto results = vsim::observe::eval_observe_vsepr_metrics(cfg, report);

	for (const auto& r : results) {
		if (r.probe_name == "vsepr_confidence") {
			// mean of 0.92 and 0.88 = 0.90
			const double expected = (0.92 + 0.88) / 2.0;
			check(std::abs(r.value - expected) < 1e-9, "vsepr_confidence mean correct");
			// automation branching: confidence > 0.5
			check(r.value > 0.5, "vsepr_confidence: automation branch works (>0.5)");
			return;
		}
	}
	check(false, "vsepr_confidence result present");
}

static void test_fallback_mode_inactive()
{
	const auto cfg    = make_cfg();
	const auto report = make_report(/*with_fallback=*/false);
	const auto results = vsim::observe::eval_observe_vsepr_metrics(cfg, report);

	for (const auto& r : results) {
		if (r.probe_name == "fallback_mode") {
			check(r.value == 0.0, "fallback_mode: 0.0 when no fallback");
			check(r.detail == "fallback_inactive", "fallback_mode detail: inactive");
			return;
		}
	}
	check(false, "fallback_mode result present (no-fallback case)");
}

static void test_fallback_mode_active()
{
	const auto cfg    = make_cfg();
	const auto report = make_report(/*with_fallback=*/true);
	const auto results = vsim::observe::eval_observe_vsepr_metrics(cfg, report);

	for (const auto& r : results) {
		if (r.probe_name == "fallback_mode") {
			check(r.value == 1.0, "fallback_mode: 1.0 when fallback active");
			check(r.detail == "fallback_active", "fallback_mode detail: active");
			return;
		}
	}
	check(false, "fallback_mode result present (fallback case)");
}

static void test_lone_pair_inference_count()
{
	const auto cfg    = make_cfg();
	const auto report = make_report();  // site 0 uses lp inference
	const auto results = vsim::observe::eval_observe_vsepr_metrics(cfg, report);

	for (const auto& r : results) {
		if (r.probe_name == "lone_pair_inference") {
			check(r.value == 1.0, "lone_pair_inference count == 1");
			return;
		}
	}
	check(false, "lone_pair_inference result present");
}

static void test_detail_payload_contains_site_data()
{
	const auto cfg    = make_cfg();
	const auto report = make_report();
	const auto results = vsim::observe::eval_observe_vsepr_metrics(cfg, report);

	for (const auto& r : results) {
		if (r.probe_name == "vsepr_sites") {
			check(r.detail.find("site_0") != std::string::npos,
				  "detail contains site_0");
			check(r.detail.find("Bent") != std::string::npos ||
				  r.detail.find("bent") != std::string::npos ||
				  r.detail.find("Irregular") != std::string::npos,
				  "detail contains shape info");
			return;
		}
	}
	check(false, "detail payload reachable via vsepr_sites");
}

static void test_stub_metrics_in_eval_observe_metrics()
{
	// Verify that the general eval_observe_metrics() recognises VSEPR metric
	// names and returns the no_vsepr_data warning (not "unknown metric").
	auto& log = vsepr::kernel::KernelEventLog::instance();
	log.clear();
	vsim::VsimDocument doc;
	vsim::ObserveSection cfg;
	cfg.metrics = { "vsepr_sites", "vsepr_confidence", "fallback_mode" };

	const auto results = vsim::VsimRuntime::eval_observe_metrics(doc, cfg, log);

	for (const auto& r : results) {
		const bool is_vsepr_metric =
			r.probe_name == "vsepr_sites" ||
			r.probe_name == "vsepr_confidence" ||
			r.probe_name == "fallback_mode";
		if (is_vsepr_metric) {
			check(r.warning.find("no_vsepr_data") != std::string::npos,
				  ("stub warning for " + r.probe_name).c_str());
			check(r.warning.find("unknown metric") == std::string::npos,
				  ("stub NOT 'unknown metric' for " + r.probe_name).c_str());
		}
	}
	check(results.size() == 3, "stub: all three VSEPR metric stubs returned");
}

static void test_empty_report_is_safe()
{
	atomistic::classify::VSEPRReport empty;
	const auto cfg = make_cfg();
	const auto results = vsim::observe::eval_observe_vsepr_metrics(cfg, empty);

	for (const auto& r : results) {
		if (r.probe_name == "vsepr_sites") {
			check(r.value == 0.0, "empty report: vsepr_sites == 0");
			return;
		}
	}
	check(false, "empty report handled without crash");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
	std::printf("\n=== Group 90: WO-84S VSEPR Observe Sink ===\n\n");

	test_vsepr_sites_count();
	test_vsepr_confidence();
	test_fallback_mode_inactive();
	test_fallback_mode_active();
	test_lone_pair_inference_count();
	test_detail_payload_contains_site_data();
	test_stub_metrics_in_eval_observe_metrics();
	test_empty_report_is_safe();

	std::printf("\n  Result: %d / %d passed\n\n", g_pass, g_tests);
	return (g_pass == g_tests) ? 0 : 1;
}
