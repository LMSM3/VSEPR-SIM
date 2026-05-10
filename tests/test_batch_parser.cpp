/**
 * tests/test_batch_parser.cpp
 * =============================
 * WO-VSIM-62C — Group 41: Batch Parser Tests
 *
 * Tests P1–P18 from spec §10.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_parser.hpp"
#include "include/batch/batch_expander.hpp"
#include "include/batch/seed_resolver.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace vsim::batch;
using vsim::FormationStageKind;

// ── helpers ───────────────────────────────────────────────────────────────────

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

// ── P1: Parse minimal inline study ───────────────────────────────────────────

static void test_P1() {
	std::cout << "P1: minimal inline study\n";
	const std::string src = R"(
[study]
name = "nacl_test"

[batch.base]
mode = "inline"

[batch.design]
type = "factorial"
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_STR(doc.study.name, "nacl_test");
	EXPECT_TRUE(doc.study.populated);
	EXPECT_STR(doc.base.mode, "inline");
	EXPECT_TRUE(doc.base.populated);
	EXPECT_STR(doc.design.type, "factorial");
}

// ── P2: Parse template study ─────────────────────────────────────────────────

static void test_P2() {
	std::cout << "P2: template study\n";
	const std::string src = R"(
[study]
name = "template_study"

[batch.base]
mode = "template"
script = "scripts/base.vsim"
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_STR(doc.base.mode, "template");
	EXPECT_STR(doc.base.script, "scripts/base.vsim");
}

// ── P3: Parse static batch axis ───────────────────────────────────────────────

static void test_P3() {
	std::cout << "P3: static batch axis\n";
	const std::string src = R"(
[study]
name = "ax_test"

[batch.base]
mode = "inline"

[[batch.axis]]
name   = "temperature"
target = "environment.temperature"
kind   = "static"
values = [300, 500, 700]
units  = "K"
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.axes.size() == 1);
	const auto& ax = doc.axes[0];
	EXPECT_STR(ax.name,   "temperature");
	EXPECT_STR(ax.target, "environment.temperature");
	EXPECT_STR(ax.kind,   "static");
	EXPECT_TRUE(ax.values.size() == 3);
	EXPECT_STR(ax.values[0], "300");
	EXPECT_STR(ax.units,  "K");
}

// ── P4: Parse stochastic axis ─────────────────────────────────────────────────

static void test_P4() {
	std::cout << "P4: stochastic axis\n";
	const std::string src = R"(
[study]
name = "stoch_test"

[batch.base]
mode = "inline"

[[batch.axis]]
name         = "defect_fraction"
target       = "material.solute_fraction"
kind         = "stochastic"
seed_source  = "defect"
distribution = "truncated_normal"
mean         = 0.04
std          = 0.01
min          = 0.00
max          = 0.10
n_samples    = 5
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.axes.size() == 1);
	const auto& ax = doc.axes[0];
	EXPECT_STR(ax.kind,         "stochastic");
	EXPECT_STR(ax.seed_source,  "defect");
	EXPECT_STR(ax.distribution, "truncated_normal");
	EXPECT_TRUE(ax.mean == 0.04);
	EXPECT_TRUE(ax.n_samples == 5);
}

// ── P5: Parse formation axis ──────────────────────────────────────────────────

static void test_P5() {
	std::cout << "P5: formation axis\n";
	const std::string src = R"(
[study]
name = "form_test"

[batch.base]
mode = "inline"

[formation.library.slow_q]
type = "quench"

[[formation.library.slow_q.stage]]
kind = "hold"
temperature_K = 1800.0
duration_ps   = 20.0

[[batch.axis]]
name   = "path"
target = "formation.path"
kind   = "formation"
values = ["slow_q"]
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.axes.size() == 1);
	EXPECT_STR(doc.axes[0].kind, "formation");
	EXPECT_STR(doc.axes[0].target, "formation.path");
	EXPECT_TRUE(doc.formation_library.count("slow_q") == 1);
}

// ── P6: Parse batch.case with dot-path overrides ─────────────────────────────

static void test_P6() {
	std::cout << "P6: batch.case dot-path overrides\n";
	const std::string src = R"(
[study]
name = "case_test"

[batch.base]
mode = "inline"

[[batch.case]]
name = "pure_al_reference"
material.formula = "Al"
material.solute_fraction = 0.0
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.cases.size() == 1);
	EXPECT_STR(doc.cases[0].name, "pure_al_reference");
	EXPECT_TRUE(doc.cases[0].overrides.count("material.formula") == 1);
	EXPECT_STR(doc.cases[0].overrides.at("material.formula"), "Al");
	EXPECT_STR(doc.cases[0].overrides.at("material.solute_fraction"), "0.0");
}

// ── P7: Parse [seed] all explicit ────────────────────────────────────────────

static void test_P7() {
	std::cout << "P7: seed all explicit\n";
	const std::string src = R"(
[study]
name = "seed_test"

[batch.base]
mode = "inline"

[seed]
foundation = 6100
defect     = 9100
formation  = 12000
thermal    = 14000
placement  = 17000
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.seed.populated);
	EXPECT_TRUE(doc.seed.foundation == 6100);
	EXPECT_TRUE(doc.seed.defect     == 9100);
	EXPECT_TRUE(doc.seed.formation  == 12000);
	EXPECT_TRUE(doc.seed.thermal    == 14000);
	EXPECT_TRUE(doc.seed.placement  == 17000);
}

// ── P8: Parse [seed] foundation only ─────────────────────────────────────────

static void test_P8() {
	std::cout << "P8: seed foundation only\n";
	const std::string src = R"(
[study]
name = "seed_foundation_test"

[batch.base]
mode = "inline"

[seed]
foundation = 6100
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.seed.foundation == 6100);
	EXPECT_TRUE(doc.seed.defect    == 0);   // to be derived
	EXPECT_TRUE(doc.seed.formation == 0);
	EXPECT_TRUE(doc.seed.thermal   == 0);
	EXPECT_TRUE(doc.seed.placement == 0);
}

// ── P9: Parse [formation.library.*] ──────────────────────────────────────────

static void test_P9() {
	std::cout << "P9: formation.library entry\n";
	const std::string src = R"(
[study]
name = "flib_test"

[batch.base]
mode = "inline"

[formation.library.slow_quench]
type         = "quench"
time_units   = "ps"
pressure_GPa = 0.0
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.formation_library.count("slow_quench") == 1);
	const auto& e = doc.formation_library.at("slow_quench");
	EXPECT_STR(e.type, "quench");
	EXPECT_STR(e.time_units, "ps");
	EXPECT_TRUE(e.pressure_GPa == 0.0);
}

// ── P10: Parse formation.library stage — hold ─────────────────────────────────

static void test_P10() {
	std::cout << "P10: formation stage hold\n";
	const std::string src = R"(
[study]
name = "stage_hold_test"

[batch.base]
mode = "inline"

[formation.library.path_a]
type = "custom"

[[formation.library.path_a.stage]]
kind          = "hold"
temperature_K = 1800.0
duration_ps   = 20.0
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.formation_library.count("path_a") == 1);
	const auto& e = doc.formation_library.at("path_a");
	EXPECT_TRUE(e.stages.size() == 1);
	EXPECT_TRUE(e.stages[0].kind == FormationStageKind::Hold);
	EXPECT_TRUE(e.stages[0].temperature_K == 1800.0);
	EXPECT_TRUE(e.stages[0].duration_ps   == 20.0);
}

// ── P11: Parse formation stage — ramp ────────────────────────────────────────

static void test_P11() {
	std::cout << "P11: formation stage ramp\n";
	const std::string src = R"(
[study]
name = "stage_ramp_test"

[batch.base]
mode = "inline"

[formation.library.path_b]
type = "custom"

[[formation.library.path_b.stage]]
kind               = "ramp"
from_temperature_K = 1800.0
to_temperature_K   = 300.0
duration_ps        = 50.0
profile            = "linear"
)";
	auto doc = BatchParser::parse_string(src);
	const auto& s = doc.formation_library.at("path_b").stages[0];
	EXPECT_TRUE(s.kind == FormationStageKind::Ramp);
	EXPECT_TRUE(s.from_temperature_K == 1800.0);
	EXPECT_TRUE(s.to_temperature_K   == 300.0);
	EXPECT_STR(s.profile, "linear");
}

// ── P12: Parse formation stage — relax ───────────────────────────────────────

static void test_P12() {
	std::cout << "P12: formation stage relax\n";
	const std::string src = R"(
[study]
name = "stage_relax_test"

[batch.base]
mode = "inline"

[formation.library.path_c]
type = "custom"

[[formation.library.path_c.stage]]
kind      = "relax"
max_steps = 3000
converge  = true
)";
	auto doc = BatchParser::parse_string(src);
	const auto& s = doc.formation_library.at("path_c").stages[0];
	EXPECT_TRUE(s.kind == FormationStageKind::Relax);
	EXPECT_TRUE(s.max_steps == 3000);
	EXPECT_TRUE(s.converge  == true);
}

// ── P13: Parse [batch.expand] ────────────────────────────────────────────────

static void test_P13() {
	std::cout << "P13: batch.expand\n";
	const std::string src = R"(
[study]
name = "expand_test"

[batch.base]
mode = "inline"

[batch.expand]
cases = true
axes  = ["temperature", "pressure"]
)";
	auto doc = BatchParser::parse_string(src);
	EXPECT_TRUE(doc.expand.populated);
	EXPECT_TRUE(doc.expand.cases == true);
	EXPECT_TRUE(doc.expand.axes.size() == 2);
	EXPECT_STR(doc.expand.axes[0], "temperature");
	EXPECT_STR(doc.expand.axes[1], "pressure");
}

// ── P14: Validate missing study.name ─────────────────────────────────────────

static void test_P14() {
	std::cout << "P14: validate missing study.name\n";
	const std::string src = R"(
[study]
type = "parameter_sweep"

[batch.base]
mode = "inline"
)";
	auto doc = BatchParser::parse_string(src);
	auto errors = BatchParser::validate(doc);
	bool found = false;
	for (const auto& e : errors)
		if (e.find("[study] name is required") != std::string::npos) { found = true; break; }
	EXPECT_TRUE(found);
}

// ── P15: Validate stochastic axis missing seed_source ────────────────────────

static void test_P15() {
	std::cout << "P15: validate stochastic axis missing seed_source\n";
	const std::string src = R"(
[study]
name = "stoch_missing_seed"

[batch.base]
mode = "inline"

[[batch.axis]]
name   = "x"
target = "material.solute_fraction"
kind   = "stochastic"
)";
	auto doc = BatchParser::parse_string(src);
	auto errors = BatchParser::validate(doc);
	bool found = false;
	for (const auto& e : errors)
		if (e.find("seed_source required when kind = stochastic") != std::string::npos) { found = true; break; }
	EXPECT_TRUE(found);
}

// ── P16: Validate formation axis value not in library ────────────────────────

static void test_P16() {
	std::cout << "P16: validate formation axis value not in library\n";
	const std::string src = R"(
[study]
name = "form_missing_lib"

[batch.base]
mode = "inline"

[[batch.axis]]
name   = "path"
target = "formation.path"
kind   = "formation"
values = ["nonexistent_protocol"]
)";
	auto doc = BatchParser::parse_string(src);
	auto errors = BatchParser::validate(doc);
	bool found = false;
	for (const auto& e : errors)
		if (e.find("not declared in [formation.library]") != std::string::npos) { found = true; break; }
	EXPECT_TRUE(found);
}

// ── P17: Validate aggregate.verify enabled with empty group_by ───────────────

static void test_P17() {
	std::cout << "P17: validate aggregate.verify enabled, group_by empty\n";
	const std::string src = R"(
[study]
name = "agg_test"

[batch.base]
mode = "inline"

[batch.aggregate.verify]
enabled = true
)";
	auto doc = BatchParser::parse_string(src);
	auto errors = BatchParser::validate(doc);
	bool found = false;
	for (const auto& e : errors)
		if (e.find("group_by required when enabled") != std::string::npos) { found = true; break; }
	EXPECT_TRUE(found);
}

// ── P18: Parse multiple axes, no errors ──────────────────────────────────────

static void test_P18() {
	std::cout << "P18: multi-axis parse, clean validate\n";
	const std::string src = R"(
[study]
name    = "alcu_sweep"
type    = "empirical_validation"
goal    = "verify AlCu structure"
version = "v5.0.0-beta.12"

[batch.base]
mode = "inline"

[batch.design]
type                = "factorial"
replicates_per_case = 2
seed_policy         = "split"
abort_on_fail       = false
checkpoint          = true

[seed]
foundation = 7000

[[batch.axis]]
name   = "temperature"
target = "environment.temperature"
kind   = "static"
values = [300, 500, 700]
units  = "K"

[[batch.axis]]
name   = "fraction"
target = "material.solute_fraction"
kind   = "static"
values = [0.01, 0.05, 0.10]
)";
	auto doc = BatchParser::parse_string(src);
	auto errors = BatchParser::validate(doc);
	EXPECT_TRUE(errors.empty());
	EXPECT_STR(doc.study.name,   "alcu_sweep");
	EXPECT_STR(doc.study.type,   "empirical_validation");
	EXPECT_TRUE(doc.axes.size()  == 2);
	EXPECT_TRUE(doc.design.replicates_per_case == 2);
	EXPECT_TRUE(doc.seed.foundation == 7000);

	// Also verify SeedResolver derives sub-seeds correctly
	auto resolved = SeedResolver::resolve(doc.seed);
	EXPECT_TRUE(resolved.resolved_defect    == 7000 + 3000);
	EXPECT_TRUE(resolved.resolved_formation == 7000 + 6000);
	EXPECT_TRUE(resolved.resolved_thermal   == 7000 + 8000);
	EXPECT_TRUE(resolved.resolved_placement == 7000 + 11000);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
	std::cout << "=== Group 41: Batch Parser Tests ===\n\n";
	test_P1();
	test_P2();
	test_P3();
	test_P4();
	test_P5();
	test_P6();
	test_P7();
	test_P8();
	test_P9();
	test_P10();
	test_P11();
	test_P12();
	test_P13();
	test_P14();
	test_P15();
	test_P16();
	test_P17();
	test_P18();

	std::cout << "\n";
	if (failures == 0)
		std::cout << "All Group 41 tests PASSED.\n";
	else
		std::cout << failures << " test(s) FAILED.\n";
	return failures ? 1 : 0;
}
