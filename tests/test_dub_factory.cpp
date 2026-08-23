/**
 * tests/test_dub_factory.cpp  —  Group 77: Default Usage Bundle (DUB) Factory
 * =============================================================================
 * WO-72E  |  V5.1.4
 *
 * Verifies make_default_usage_bundle() produces a correct five-slot manifest.
 *
 *   DUB-01  bundle_id and purpose are set from caller arguments
 *   DUB-02  exactly five slots are produced
 *   DUB-03  LOAD-01 slot exists with the correct script_path
 *   DUB-04  RUN-01 depends on LOAD-01
 *   DUB-05  VALIDATE-01 depends on RUN-01 and is ValidationCheck kind
 *   DUB-06  REPORT-01 depends on VALIDATE-01
 *   DUB-07  BENCH-01 is Benchmark kind and allow_failure == true
 *   DUB-08  summary_md_path, events_jsonl_path, bench_tsv_path use bundle_id stem
 *   DUB-09  run_seed propagated correctly
 *   DUB-10  find_slot() locates every slot by id
 */

#include "include/vsim/bundle/x_bundle.hpp"

#include <iostream>
#include <string>

using namespace vsim::bundle;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

int main() {
	const auto m = make_default_usage_bundle(
		"scripts/demos/water.vsim",
		"water_demo",
		42u,
		"v5.1.4"
	);

	// DUB-01: bundle_id and purpose set from caller
	CHECK("DUB-01", m.bundle_id == "water_demo" &&
					m.purpose   == "Default single-script execution bundle",
		  "bundle_id and purpose populated from caller arguments");

	// DUB-02: exactly five slots
	CHECK("DUB-02", m.slot_count() == 5,
		  "exactly five slots produced");

	// DUB-03: LOAD-01 has correct script_path
	{
		const auto* s = m.find_slot("LOAD-01");
		CHECK("DUB-03", s != nullptr && s->script_path == "scripts/demos/water.vsim",
			  "LOAD-01 exists with correct script_path");
	}

	// DUB-04: RUN-01 depends on LOAD-01
	{
		const auto* s = m.find_slot("RUN-01");
		bool dep = s != nullptr && !s->depends_on.empty() &&
				   s->depends_on[0] == "LOAD-01";
		CHECK("DUB-04", dep, "RUN-01 depends_on LOAD-01");
	}

	// DUB-05: VALIDATE-01 is ValidationCheck and depends on RUN-01
	{
		const auto* s = m.find_slot("VALIDATE-01");
		bool ok = s != nullptr &&
				  s->kind == XBundleSlotKind::ValidationCheck &&
				  !s->depends_on.empty() && s->depends_on[0] == "RUN-01";
		CHECK("DUB-05", ok,
			  "VALIDATE-01 is ValidationCheck kind and depends on RUN-01");
	}

	// DUB-06: REPORT-01 depends on VALIDATE-01
	{
		const auto* s = m.find_slot("REPORT-01");
		bool dep = s != nullptr && !s->depends_on.empty() &&
				   s->depends_on[0] == "VALIDATE-01";
		CHECK("DUB-06", dep, "REPORT-01 depends_on VALIDATE-01");
	}

	// DUB-07: BENCH-01 is Benchmark kind and allow_failure == true
	{
		const auto* s = m.find_slot("BENCH-01");
		bool ok = s != nullptr &&
				  s->kind == XBundleSlotKind::Benchmark &&
				  s->allow_failure == true;
		CHECK("DUB-07", ok, "BENCH-01 is Benchmark kind with allow_failure = true");
	}

	// DUB-08: output paths use bundle_id stem
	CHECK("DUB-08",
		  m.summary_md_path   == "water_demo_summary.md"   &&
		  m.events_jsonl_path == "water_demo_events.jsonl" &&
		  m.bench_tsv_path    == "water_demo_bench.tsv",
		  "output paths derive from bundle_id stem");

	// DUB-09: run_seed propagated
	CHECK("DUB-09", m.run_seed == 42u,
		  "run_seed propagated from caller argument");

	// DUB-10: find_slot() locates every standard slot
	{
		const char* ids[] = { "LOAD-01", "RUN-01", "VALIDATE-01", "REPORT-01", "BENCH-01" };
		bool all_found = true;
		for (const auto* id : ids)
			if (m.find_slot(id) == nullptr) { all_found = false; break; }
		CHECK("DUB-10", all_found, "find_slot() locates all five standard DUB slots");
	}

	// -------------------------------------------------------------------------
	std::cout << "\n" << PASS << "/" << (PASS + FAIL) << " tests passed";
	if (FAIL == 0) std::cout << "  (Group 77 OK)\n";
	else           std::cout << "  *** " << FAIL << " FAILURES ***\n";
	return FAIL == 0 ? 0 : 1;
}
