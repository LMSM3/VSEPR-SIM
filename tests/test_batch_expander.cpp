/**
 * tests/test_batch_expander.cpp
 * ===============================
 * WO-VSIM-62C — Group 42: Batch Expander Tests
 *
 * Tests E1–E10 from spec §10.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_parser.hpp"
#include "include/batch/batch_expander.hpp"
#include "include/batch/seed_resolver.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace vsim::batch;
using vsim::FormationStageKind;
using vsim::SeedSection;

static int failures = 0;

#define EXPECT_TRUE(cond) \
	do { \
		if (!(cond)) { \
			std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__ << "] " #cond "\n"; \
			++failures; \
		} else { \
			std::cout << "  PASS: " #cond "\n"; \
		} \
	} while(0)

#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))
#define EXPECT_STR(a, b) EXPECT_TRUE(std::string(a) == std::string(b))

// ── helpers ───────────────────────────────────────────────────────────────────

static BatchDocument make_2axis_doc() {
	const std::string src = R"(
[study]
name = "expand_test"

[batch.base]
mode = "inline"

[batch.design]
replicates_per_case = 1
seed_policy = "split"

[seed]
foundation = 1000

[[batch.axis]]
name   = "temperature"
target = "environment.temperature"
kind   = "static"
values = [300, 500]

[[batch.axis]]
name   = "pressure"
target = "environment.pressure_GPa"
kind   = "static"
values = [0.0, 1.0, 2.0]
)";
	return BatchParser::parse_string(src);
}

// ── E1: 2-axis factorial → 6 specs ───────────────────────────────────────────

static void test_E1() {
	std::cout << "E1: 2-axis factorial (2×3) → 6 specs\n";
	auto doc = make_2axis_doc();
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);
	EXPECT_TRUE(specs.size() == 6);
	EXPECT_STR(specs[0].run_id, "run_0001");
	EXPECT_STR(specs[5].run_id, "run_0006");
}

// ── E2: 1 axis × 3 replicates → 3 specs ─────────────────────────────────────

static void test_E2() {
	std::cout << "E2: 1 axis × 3 replicates → 3 specs\n";
	const std::string src = R"(
[study]
name = "rep_test"

[batch.base]
mode = "inline"

[batch.design]
replicates_per_case = 3
seed_policy = "split"

[seed]
foundation = 2000

[[batch.axis]]
name   = "temperature"
target = "environment.temperature"
kind   = "static"
values = [300]
)";
	auto doc = BatchParser::parse_string(src);
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);
	EXPECT_TRUE(specs.size() == 3);
	EXPECT_TRUE(specs[0].replicate == 0);
	EXPECT_TRUE(specs[1].replicate == 1);
	EXPECT_TRUE(specs[2].replicate == 2);
}

// ── E3: seed_policy = "split" → seeds differ by 1 ────────────────────────────

static void test_E3() {
	std::cout << "E3: seed_policy=split → seeds differ by 1\n";
	const std::string src = R"(
[study]
name = "split_test"

[batch.base]
mode = "inline"

[batch.design]
replicates_per_case = 3
seed_policy = "split"

[seed]
foundation = 5000

[[batch.axis]]
name   = "t"
target = "environment.temperature"
kind   = "static"
values = [300]
)";
	auto doc = BatchParser::parse_string(src);
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);
	EXPECT_TRUE(specs.size() == 3);
	uint64_t d0 = specs[0].seed_defect;
	uint64_t d1 = specs[1].seed_defect;
	uint64_t d2 = specs[2].seed_defect;
	EXPECT_TRUE(d1 == d0 + 1);
	EXPECT_TRUE(d2 == d0 + 2);
}

// ── E4: seed_policy = "shift" → seeds differ by 1000 ─────────────────────────

static void test_E4() {
	std::cout << "E4: seed_policy=shift → seeds differ by 1000\n";
	const std::string src = R"(
[study]
name = "shift_test"

[batch.base]
mode = "inline"

[batch.design]
replicates_per_case = 3
seed_policy = "shift"

[seed]
foundation = 5000

[[batch.axis]]
name   = "t"
target = "environment.temperature"
kind   = "static"
values = [300]
)";
	auto doc = BatchParser::parse_string(src);
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);
	EXPECT_TRUE(specs.size() == 3);
	uint64_t d0 = specs[0].seed_defect;
	uint64_t d1 = specs[1].seed_defect;
	EXPECT_TRUE(d1 == d0 + 1000);
}

// ── E5: SeedResolver derives sub-seeds correctly ─────────────────────────────

static void test_E5() {
	std::cout << "E5: SeedResolver foundation → derived sub-seeds\n";
	SeedSection declared;
	declared.foundation = 6100;
	auto resolved = SeedResolver::resolve(declared);
	EXPECT_TRUE(resolved.resolved_defect    == 6100 + 3000);
	EXPECT_TRUE(resolved.resolved_formation == 6100 + 6000);
	EXPECT_TRUE(resolved.resolved_thermal   == 6100 + 8000);
	EXPECT_TRUE(resolved.resolved_placement == 6100 + 11000);
}

// ── E6: SeedResolver explicit non-zero → preserved ───────────────────────────

static void test_E6() {
	std::cout << "E6: SeedResolver explicit non-zero → preserved\n";
	SeedSection declared;
	declared.foundation = 1;
	declared.defect     = 9999;
	declared.formation  = 8888;
	declared.thermal    = 7777;
	declared.placement  = 6666;
	auto resolved = SeedResolver::resolve(declared);
	EXPECT_TRUE(resolved.resolved_defect    == 9999);
	EXPECT_TRUE(resolved.resolved_formation == 8888);
	EXPECT_TRUE(resolved.resolved_thermal   == 7777);
	EXPECT_TRUE(resolved.resolved_placement == 6666);
}

// ── E7: [batch.expand] cases × axes ──────────────────────────────────────────

static void test_E7() {
	std::cout << "E7: expand cases × axes\n";
	const std::string src = R"(
[study]
name = "cross_test"

[batch.base]
mode = "inline"

[batch.design]
replicates_per_case = 2
seed_policy = "split"

[seed]
foundation = 100

[batch.expand]
cases = true

[[batch.axis]]
name   = "t"
target = "environment.temperature"
kind   = "static"
values = [300, 500]

[[batch.case]]
name = "case_a"

[[batch.case]]
name = "case_b"
)";
	auto doc = BatchParser::parse_string(src);
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);
	// 2 cases × 2 axis values × 2 replicates = 8
	EXPECT_TRUE(specs.size() == 8);
}

// ── E8: Case override wins over inline base ───────────────────────────────────

static void test_E8() {
	std::cout << "E8: case override wins over inline base\n";
	const std::string src = R"(
[study]
name = "override_test"

[batch.base]
mode = "inline"

[batch.design]
replicates_per_case = 1

[batch.expand]
cases = true

[[batch.axis]]
name   = "t"
target = "environment.temperature"
kind   = "static"
values = [300]

[[batch.case]]
name = "low_frac"
material.solute_fraction = 0.01
)";
	auto doc = BatchParser::parse_string(src);
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);
	EXPECT_TRUE(specs.size() == 1);
	EXPECT_STR(specs[0].axis_values.at("material.solute_fraction"), "0.01");
}

// ── E9: Axis value wins over case override ────────────────────────────────────

static void test_E9() {
	std::cout << "E9: axis value wins over case override\n";
	// Axis targets environment.temperature; case also tries to set the same path
	const std::string src = R"(
[study]
name = "axis_wins_test"

[batch.base]
mode = "inline"

[batch.design]
replicates_per_case = 1

[batch.expand]
cases = true

[[batch.axis]]
name   = "t"
target = "environment.temperature"
kind   = "static"
values = [700]

[[batch.case]]
name = "hot_case"
environment.temperature = 100
)";
	auto doc = BatchParser::parse_string(src);
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);
	EXPECT_TRUE(specs.size() == 1);
	// Axis value (700) must win over case override (100)
	EXPECT_STR(specs[0].axis_values.at("environment.temperature"), "700");
}

// ── E10: batch_plan.tsv written correctly ────────────────────────────────────

static void test_E10() {
	std::cout << "E10: batch_plan.tsv written correctly\n";
	auto doc = make_2axis_doc();
	std::vector<std::string> warnings;
	auto specs = BatchExpander::expand(doc, warnings);

	const std::string tmp = "test_batch_plan_E10.tsv";
	BatchExpander::write_batch_plan(specs, doc, tmp);

	std::ifstream f(tmp);
	EXPECT_TRUE(f.good());
	std::string header;
	std::getline(f, header);
	EXPECT_TRUE(header.find("run_id") != std::string::npos);
	EXPECT_TRUE(header.find("temperature") != std::string::npos);
	EXPECT_TRUE(header.find("seed_foundation") != std::string::npos);
	f.close();
	std::remove(tmp.c_str());
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
	std::cout << "=== Group 42: Batch Expander Tests ===\n\n";
	test_E1();
	test_E2();
	test_E3();
	test_E4();
	test_E5();
	test_E6();
	test_E7();
	test_E8();
	test_E9();
	test_E10();

	std::cout << "\n";
	if (failures == 0)
		std::cout << "All Group 42 tests PASSED.\n";
	else
		std::cout << failures << " test(s) FAILED.\n";
	return failures ? 1 : 0;
}
