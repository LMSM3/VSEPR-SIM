/**
 * tests/test_observe_metrics.cpp  -  Unit tests for eval_observe_metrics()
 * ==========================================================================
 *
 * Group 81  |  WO-75A  |  v5.0.0-main
 *
 * Tests that eval_observe_metrics() returns correct values and warnings
 * for all supported metric names, using a pre-populated KernelEventLog.
 */

#include "vsim/vsim_runtime.hpp"
#include "vsim/vsim_document.hpp"
#include "kernel/kernel_event_log.hpp"
#include "kernel/kernel_event.hpp"
#include "tests/test_fixtures.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static void populate_log(
	vsepr::kernel::KernelEventLog& log,
	int n_formation  = 4,
	int n_reaction   = 3,
	int n_transport  = 2,
	int n_defect     = 1)
{
	using K = vsepr::kernel::KernelEventKind;
	int id = 0;

	for (int i = 0; i < n_formation; ++i) {
		vsepr::kernel::KernelEvent ev(K::Formation, "CH4", static_cast<uint64_t>(++id));
		ev.result_value = -100.0 - i * 5.0;
		ev.result_unit  = "kcal/mol";
		ev.is_valid     = true;
		log.record(ev);
	}
	for (int i = 0; i < n_reaction; ++i) {
		vsepr::kernel::KernelEvent ev(K::Reaction, "CH4", static_cast<uint64_t>(++id));
		ev.result_value = (i % 2 == 0) ? -20.0 : 10.0;  // some exothermic
		ev.result_unit  = "kcal/mol";
		ev.is_valid     = true;
		log.record(ev);
	}
	for (int i = 0; i < n_transport; ++i) {
		vsepr::kernel::KernelEvent ev(K::Transport, "CH4", static_cast<uint64_t>(++id));
		ev.result_value = 0.0;
		ev.is_valid     = true;
		log.record(ev);
	}
	for (int i = 0; i < n_defect; ++i) {
		vsepr::kernel::KernelEvent ev(K::Defect, "CH4", static_cast<uint64_t>(++id));
		ev.result_value = 0.0;
		ev.is_valid     = true;
		log.record(ev);
	}
}

static vsim::EvalResult find_result(
	const std::vector<vsim::EvalResult>& results,
	const std::string& name)
{
	for (const auto& r : results)
		if (r.probe_name == name) return r;
	vsim::EvalResult missing;
	missing.probe_name = name;
	missing.warning    = "not_found";
	return missing;
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

static int g_tests = 0, g_pass = 0;

#define EXPECT(cond, msg) do { \
	++g_tests; \
	if (cond) { ++g_pass; std::printf("  [PASS] %s\n", msg); } \
	else       { std::printf("  [FAIL] %s\n", msg); } \
} while(0)

int main()
{
	std::printf("\n=== test_observe_metrics  (Group 81) ===\n\n");

	auto& log = vsepr::kernel::KernelEventLog::instance();
	log.clear();
	populate_log(log, 4, 3, 2, 1);

	auto doc = vsim_test::make_observe_doc({
		"reaction_events", "chemical_state", "exothermic_count",
		"avg_delta_E", "formation", "transport", "defect",
		"bond_angles", "spectral_response", "interference"
	});

	auto results = vsim::VsimRuntime::eval_observe_metrics(doc, doc.observe, log, false);

	// Basic count checks
	EXPECT(find_result(results, "reaction_events").value  == 3.0, "reaction_events == 3");
	EXPECT(find_result(results, "formation").value        == 4.0, "formation == 4");
	EXPECT(find_result(results, "transport").value        == 2.0, "transport == 2");
	EXPECT(find_result(results, "defect").value           == 1.0, "defect == 1");

	// Exothermic: reactions where result < 0  (index 0, 2 -> 2 of 3)
	EXPECT(find_result(results, "exothermic_count").value == 2.0, "exothermic_count == 2");

	// avg_delta_E: (-20 + 10 - 20) / 3 = -10
	{
		double avg = find_result(results, "avg_delta_E").value;
		EXPECT(avg < -5.0 && avg > -20.0, "avg_delta_E in expected range");
	}

	// bond_angles for CH4 should be ~109.47 (no warning)
	{
		auto r = find_result(results, "bond_angles");
		EXPECT(r.warning.empty(), "bond_angles: no warning");
		EXPECT(r.value > 100.0 && r.value < 120.0, "bond_angles ~109 deg");
	}

	// spectral_response for CH4 -> optical gap ~12.6 eV
	{
		auto r = find_result(results, "spectral_response");
		EXPECT(r.warning.empty(), "spectral_response: no warning");
		EXPECT(r.value > 5.0 && r.value < 20.0, "spectral_response in eV range");
	}

	// interference: score in [0, 1]
	{
		auto r = find_result(results, "interference");
		EXPECT(r.warning.empty(), "interference: no warning");
		EXPECT(r.value >= 0.0 && r.value <= 1.0, "interference score in [0,1]");
	}

	// Unknown metric should produce a warning
	{
		auto doc2 = vsim_test::make_observe_doc({ "nonexistent_metric" });
		auto r2   = vsim::VsimRuntime::eval_observe_metrics(doc2, doc2.observe, log, false);
		EXPECT(!r2.empty() && !r2[0].warning.empty(), "unknown metric produces warning");
	}

	std::printf("\n--- %d / %d tests passed ---\n\n", g_pass, g_tests);
	return (g_pass == g_tests) ? 0 : 1;
}
