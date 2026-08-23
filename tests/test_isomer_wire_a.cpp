/**
 * tests/test_isomer_wire_a.cpp
 * ==============================
 * Group 56 — Isomer Pipeline Wiring A (WO-VSIM-ISOMER-WIRE-A)
 *
 * Acceptance tests:
 *
 *   ISO-A-01  generator produces >=1 candidate for a known formula (CoA4B2)
 *   ISO-A-02  candidate_count matches candidates.size()
 *   ISO-A-03  deterministic seed: same formula + seed → same candidate list
 *   ISO-A-04  disabled generator returns ok=false with clear error
 *   ISO-A-05  empty formula returns ok=false with clear error
 *   ISO-A-06  manifest_lines produces header row + N data rows
 */

#include "vsim/intent/isomer_bridge.hpp"
#include "vsim/vsim_document.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

static vsim::IsomerGeneratorSection make_gen(const std::string& formula,
											  bool enabled = true)
{
	vsim::IsomerGeneratorSection g;
	g.enabled    = enabled;
	g.formula    = formula;
	g.max_bond_order = 3;
	g.deduplicate = "canonical_graph_hash";
	return g;
}

static bool test_ISO_A_01() {
	auto g = make_gen("CoA4B2");
	auto r = vsim::IsomerBridge::generate(g, 0);
	if (!r.ok) {
		std::printf("FAIL ISO-A-01: generate failed: %s\n", r.error.c_str());
		return false;
	}
	if (r.candidate_count < 1) {
		std::puts("FAIL ISO-A-01: expected >=1 candidate");
		return false;
	}
	std::printf("PASS ISO-A-01: generator produced %d candidates\n", r.candidate_count);
	return true;
}

static bool test_ISO_A_02() {
	auto g = make_gen("CoA4B2");
	auto r = vsim::IsomerBridge::generate(g, 0);
	if (!r.ok) {
		std::printf("FAIL ISO-A-02: %s\n", r.error.c_str());
		return false;
	}
	if (r.candidate_count != static_cast<int>(r.candidates.size())) {
		std::printf("FAIL ISO-A-02: candidate_count=%d but candidates.size()=%zu\n",
			r.candidate_count, r.candidates.size());
		return false;
	}
	std::puts("PASS ISO-A-02: candidate_count matches candidates.size()");
	return true;
}

static bool test_ISO_A_03() {
	auto g = make_gen("PtCl2N2");
	auto r1 = vsim::IsomerBridge::generate(g, 42u);
	auto r2 = vsim::IsomerBridge::generate(g, 42u);

	if (!r1.ok || !r2.ok) {
		std::puts("FAIL ISO-A-03: one run failed");
		return false;
	}
	if (r1.candidate_count != r2.candidate_count) {
		std::printf("FAIL ISO-A-03: seed=42 produced %d vs %d candidates\n",
			r1.candidate_count, r2.candidate_count);
		return false;
	}
	for (int i = 0; i < r1.candidate_count; ++i) {
		if (r1.candidates[i].descriptor != r2.candidates[i].descriptor) {
			std::printf("FAIL ISO-A-03: descriptor mismatch at index %d\n", i);
			return false;
		}
	}
	// Also verify different seeds produce potentially different orderings
	// (or at least the same count — seed just controls order, not count)
	auto r3 = vsim::IsomerBridge::generate(g, 99u);
	if (r3.candidate_count != r1.candidate_count) {
		std::printf("FAIL ISO-A-03: seed=99 gave different count %d vs %d\n",
			r3.candidate_count, r1.candidate_count);
		return false;
	}

	std::puts("PASS ISO-A-03: same formula+seed → same candidate list (deterministic)");
	return true;
}

static bool test_ISO_A_04() {
	auto g = make_gen("NaCl", /*enabled=*/false);
	auto r = vsim::IsomerBridge::generate(g, 0);
	if (r.ok) {
		std::puts("FAIL ISO-A-04: disabled generator should return ok=false");
		return false;
	}
	if (r.error.empty()) {
		std::puts("FAIL ISO-A-04: error message is empty");
		return false;
	}
	std::printf("PASS ISO-A-04: disabled generator fails clearly: \"%s\"\n", r.error.c_str());
	return true;
}

static bool test_ISO_A_05() {
	vsim::IsomerGeneratorSection g;
	g.enabled = true;
	g.formula = "";   // empty
	auto r = vsim::IsomerBridge::generate(g, 0);
	if (r.ok) {
		std::puts("FAIL ISO-A-05: empty formula should return ok=false");
		return false;
	}
	std::printf("PASS ISO-A-05: empty formula fails clearly: \"%s\"\n", r.error.c_str());
	return true;
}

static bool test_ISO_A_06() {
	auto g = make_gen("CoA4B2");
	auto r = vsim::IsomerBridge::generate(g, 0);
	if (!r.ok) {
		std::printf("FAIL ISO-A-06: generate failed: %s\n", r.error.c_str());
		return false;
	}
	auto lines = vsim::IsomerBridge::manifest_lines(r);
	// First line must be the header
	if (lines.empty() || lines[0].find("index") == std::string::npos) {
		std::puts("FAIL ISO-A-06: missing header row");
		return false;
	}
	// Total lines = 1 header + candidate_count
	if (static_cast<int>(lines.size()) != 1 + r.candidate_count) {
		std::printf("FAIL ISO-A-06: lines=%zu expected %d\n",
			lines.size(), 1 + r.candidate_count);
		return false;
	}
	std::printf("PASS ISO-A-06: manifest has header + %d data rows\n", r.candidate_count);
	return true;
}

// ============================================================================

int main() {
	std::puts("\n=== Group 56 — Isomer Pipeline Wiring A ===\n");

	int pass = 0, fail = 0;
	auto run = [&](bool (*fn)()) {
		if (fn()) ++pass; else ++fail;
	};

	run(test_ISO_A_01);
	run(test_ISO_A_02);
	run(test_ISO_A_03);
	run(test_ISO_A_04);
	run(test_ISO_A_05);
	run(test_ISO_A_06);

	std::printf("\n  Results: %d passed  %d failed\n\n", pass, fail);
	return (fail == 0) ? 0 : 1;
}
