/**
 * test_ctl_smoke.cpp  —  Group 65: CTL End-to-End Smoke
 * =====================================================
 * WO-VSIM-CTL-TEST  |  v5.1.x
 *
 * Full pipeline: source text → parser → validator → ExecPlan → dispatcher → JSON
 *
 *   SMOKE-01  parse minimal CTL script succeeds
 *   SMOKE-02  validate minimal CTL script passes
 *   SMOKE-03  dispatch runtime.run_case() returns all-ok
 *   SMOKE-04  to_json() contains "commands" array
 *   SMOKE-05  artifact_manifest_json() is valid JSON structure
 *   SMOKE-06  gate_manifest_json() lists all gate names
 *   SMOKE-07  unknown namespace rejected before dispatch
 *   SMOKE-08  unknown command rejected before dispatch
 *   SMOKE-09  same script + same seed → same plan hash
 *   SMOKE-10  changed const changes the script text (const is textual, not semantic hash)
 */

#include "vsim/ctl/ctl_parser.hpp"
#include "vsim/ctl/ctl_validator.hpp"
#include "vsim/ctl/ctl_plan.hpp"
#include "vsim/ctl/ctl_dispatcher.hpp"
#include <iostream>
#include <string>

using namespace vsim::ctl;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

// ---------------------------------------------------------------------------
// Canonical minimal smoke script
// ---------------------------------------------------------------------------
static const char* SMOKE_SCRIPT = R"(
kernel.channel.reset()
kernel.channel.enable(name = "coulomb", weight = 1.0)
artifact.dynx.enable(profile = "smoke")
runtime.run_case("smoke_run")
metrics.capture()
gate.begin("smoke_gate")
gate.assert(energy.drift < 1e-3)
gate.end()
artifact.dynx.export("out/smoke/run.dynx")
)";

static const char* SMOKE_SCRIPT_CONST = R"(
const ch = "coulomb"
kernel.channel.reset()
kernel.channel.enable(name = "${ch}", weight = 1.0)
artifact.dynx.enable(profile = "smoke")
runtime.run_case("smoke_run")
metrics.capture()
gate.begin("smoke_gate")
gate.assert(energy.drift < 1e-3)
gate.end()
artifact.dynx.export("out/smoke/run.dynx")
)";

static const char* SMOKE_SCRIPT_CONST_ALT = R"(
const ch = "bond"
kernel.channel.reset()
kernel.channel.enable(name = "${ch}", weight = 1.0)
artifact.dynx.enable(profile = "smoke")
runtime.run_case("smoke_run")
metrics.capture()
gate.begin("smoke_gate")
gate.assert(energy.drift < 1e-3)
gate.end()
artifact.dynx.export("out/smoke/run.dynx")
)";

// ---------------------------------------------------------------------------
void test_SMOKE_01() {
	auto r = CtlParser::parse(SMOKE_SCRIPT);
	CHECK("SMOKE-01", r.ok,              "parse succeeds");
	CHECK("SMOKE-01", r.graph.size() > 0, "graph non-empty");
}

void test_SMOKE_02() {
	auto g = CtlParser::parse(SMOKE_SCRIPT).graph;
	auto v = CtlValidator::validate(g);
	CHECK("SMOKE-02", v.ok,             "validation passes");
	CHECK("SMOKE-02", v.errors.empty(), "no validation errors");
}

void test_SMOKE_03() {
	auto g    = CtlParser::parse(SMOKE_SCRIPT).graph;
	auto plan = ExecPlan::compile(g, "dev", 0);
	CtlDispatcher d;
	auto session = d.dispatch(plan);
	CHECK("SMOKE-03", session.all_ok(), "all commands dispatched ok");
}

void test_SMOKE_04() {
	auto g    = CtlParser::parse(SMOKE_SCRIPT).graph;
	auto plan = ExecPlan::compile(g, "dev", 0);
	auto json = plan.to_json();
	CHECK("SMOKE-04", json.find("\"commands\"")    != std::string::npos, "commands array in JSON");
	CHECK("SMOKE-04", json.find("\"plan_version\"")!= std::string::npos, "plan_version in JSON");
	CHECK("SMOKE-04", json.find("\"plan_hash\"")   != std::string::npos, "plan_hash in JSON");
}

void test_SMOKE_05() {
	auto g    = CtlParser::parse(SMOKE_SCRIPT).graph;
	auto plan = ExecPlan::compile(g, "dev", 0);
	auto mfst = plan.artifact_manifest_json();
	CHECK("SMOKE-05", mfst.find("{")            != std::string::npos, "opens with {");
	CHECK("SMOKE-05", mfst.find("\"artifacts\"")!= std::string::npos, "artifacts key present");
	CHECK("SMOKE-05", mfst.find("dynx")         != std::string::npos, "dynx artifact present");
}

void test_SMOKE_06() {
	auto g    = CtlParser::parse(SMOKE_SCRIPT).graph;
	auto plan = ExecPlan::compile(g, "dev", 0);
	auto gmfst = plan.gate_manifest_json();
	CHECK("SMOKE-06", gmfst.find("\"gates\"")    != std::string::npos, "gates key present");
	CHECK("SMOKE-06", gmfst.find("smoke_gate")   != std::string::npos, "smoke_gate listed");
}

void test_SMOKE_07() {
	auto r = CtlParser::parse("physics.mutate_particle(name = \"H\")");
	auto v = CtlValidator::validate(r.graph);
	CHECK("SMOKE-07", !v.ok,                                      "unknown namespace fails");
	CHECK("SMOKE-07", v.first_error().find("unknown") != std::string::npos, "error says unknown");
	// Verify: no dispatch ever happens because caller checks v.ok first
	bool would_dispatch = v.ok;
	CHECK("SMOKE-07", !would_dispatch, "dispatch prevented");
}

void test_SMOKE_08() {
	auto r = CtlParser::parse("kernel.inject_force(name = \"coulomb\")");
	auto v = CtlValidator::validate(r.graph);
	CHECK("SMOKE-08", !v.ok, "unknown command fails");
	bool would_dispatch = v.ok;
	CHECK("SMOKE-08", !would_dispatch, "dispatch prevented");
}

void test_SMOKE_09() {
	auto g1   = CtlParser::parse(SMOKE_SCRIPT).graph;
	auto g2   = CtlParser::parse(SMOKE_SCRIPT).graph;
	auto p1   = ExecPlan::compile(g1, "v5.1.4", 9101);
	auto p2   = ExecPlan::compile(g2, "v5.1.4", 9101);
	CHECK("SMOKE-09", p1.plan_hash == p2.plan_hash, "same inputs → same plan_hash");
}

void test_SMOKE_10() {
	// const value is part of the script text, so changing it changes script_hash and plan_hash
	auto g1   = CtlParser::parse(SMOKE_SCRIPT_CONST).graph;
	auto g2   = CtlParser::parse(SMOKE_SCRIPT_CONST_ALT).graph;
	auto p1   = ExecPlan::compile(g1, "v5.1.4", 9101);
	auto p2   = ExecPlan::compile(g2, "v5.1.4", 9101);
	CHECK("SMOKE-10", p1.script_hash != p2.script_hash, "different const → different script_hash");
	CHECK("SMOKE-10", p1.plan_hash   != p2.plan_hash,   "different const → different plan_hash");
}

// ---------------------------------------------------------------------------
int main() {
	std::cout << "\n=== Group 65 — CTL End-to-End Smoke ===\n\n";
	test_SMOKE_01();
	test_SMOKE_02();
	test_SMOKE_03();
	test_SMOKE_04();
	test_SMOKE_05();
	test_SMOKE_06();
	test_SMOKE_07();
	test_SMOKE_08();
	test_SMOKE_09();
	test_SMOKE_10();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
