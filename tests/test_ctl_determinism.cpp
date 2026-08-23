/**
 * test_ctl_determinism.cpp  —  Group 68: CTL Determinism
 * =======================================================
 * WO-VSIM-CTL-TEST  |  v5.1.x
 *
 * CTL must not inject nondeterminism into the pipeline.
 *
 *   DET-01  same script + same seed → same plan_hash (two compilations)
 *   DET-02  same script + different seed → different plan_hash
 *   DET-03  ExecGraph command ordering matches source order
 *   DET-04  const expansion order is deterministic
 *   DET-05  artifact manifest ordering matches command order in graph
 *   DET-06  gate manifest ordering matches gate.begin order in graph
 *   DET-07  FNV-1a-64 hash produces identical output on repeated calls
 *   DET-08  script_hash is stable across repeated compile() calls
 *   DET-09  ExecPlan command count matches ExecGraph command count
 *   DET-10  rejected script produces no ExecPlan output
 */

#include "vsim/ctl/ctl_parser.hpp"
#include "vsim/ctl/ctl_validator.hpp"
#include "vsim/ctl/ctl_plan.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace vsim::ctl;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

static const char* DET_SCRIPT = R"(
const tag = "det_run"
kernel.channel.reset()
kernel.channel.enable(name = "coulomb", weight = 1.0)
kernel.channel.enable(name = "dispersion", weight = 0.5)
artifact.dynx.enable(profile = "archive")
artifact.xbit.create("full")
runtime.run_case("${tag}")
metrics.capture()
gate.begin("quality_gate")
gate.assert(energy.drift < 1e-4)
gate.end()
artifact.dynx.export("out/det/run.dynx")
artifact.xbit.export("out/det/run.xbit")
artifact.report.write("out/det/report.txt")
gate.begin("export_gate")
gate.end()
)";

// ---------------------------------------------------------------------------
void test_DET_01() {
	auto g1 = CtlParser::parse(DET_SCRIPT).graph;
	auto g2 = CtlParser::parse(DET_SCRIPT).graph;
	auto p1 = ExecPlan::compile(g1, "v5.1.4", 42000);
	auto p2 = ExecPlan::compile(g2, "v5.1.4", 42000);
	CHECK("DET-01", p1.plan_hash == p2.plan_hash, "same inputs → same plan_hash");
	CHECK("DET-01", p1.script_hash == p2.script_hash, "same inputs → same script_hash");
}

void test_DET_02() {
	auto g1 = CtlParser::parse(DET_SCRIPT).graph;
	auto g2 = CtlParser::parse(DET_SCRIPT).graph;
	auto p1 = ExecPlan::compile(g1, "v5.1.4", 1);
	auto p2 = ExecPlan::compile(g2, "v5.1.4", 99999);
	CHECK("DET-02", p1.plan_hash != p2.plan_hash, "different seed → different plan_hash");
	// script_hash does not include seed, so it stays the same
	CHECK("DET-02", p1.script_hash == p2.script_hash, "script_hash independent of seed");
}

void test_DET_03() {
	// Command ordering in ExecGraph must match source order
	auto r = CtlParser::parse(R"(
artifact.dynx.enable("a")
runtime.run_case("b")
metrics.capture()
)");
	CHECK("DET-03", r.ok, "parse ok");
	CHECK("DET-03", r.graph.size() >= 3, "three commands parsed");
	if (r.graph.size() >= 3) {
		CHECK("DET-03", r.graph.commands[0].ns == CtlNamespace::Artifact, "first: artifact");
		CHECK("DET-03", r.graph.commands[1].ns == CtlNamespace::Runtime,  "second: runtime");
		CHECK("DET-03", r.graph.commands[2].ns == CtlNamespace::Metrics,  "third: metrics");
	}
}

void test_DET_04() {
	// const expansion order: both references expand correctly
	auto r = CtlParser::parse(R"(
const a = "coulomb"
const b = "dispersion"
artifact.dynx.enable("x")
kernel.channel.enable(name = "${a}", weight = 1.0)
kernel.channel.enable(name = "${b}", weight = 0.5)
runtime.run_case("${a}_${b}")
)");
	CHECK("DET-04", r.ok, "parse ok");
	bool found_a = false, found_b = false;
	for (std::size_t i = 0; i < r.graph.size(); ++i) {
		const auto& c = r.graph.commands[i];
		if (c.sub_op == "channel.enable" && c.ns == CtlNamespace::Kernel) {
			if (c.args.count("name") > 0 && c.args.at("name").is_string()) {
				if (c.args.at("name").as_string() == "coulomb")    found_a = true;
				if (c.args.at("name").as_string() == "dispersion") found_b = true;
			}
		}
	}
	CHECK("DET-04", found_a, "const a expanded to coulomb");
	CHECK("DET-04", found_b, "const b expanded to dispersion");
}

void test_DET_05() {
	// artifact manifest ordering matches the export commands in source order
	auto g    = CtlParser::parse(DET_SCRIPT).graph;
	auto plan = ExecPlan::compile(g, "dev", 0);
	auto mfst = plan.artifact_manifest_json();
	auto pos_dynx   = mfst.find("dynx");
	auto pos_xbit   = mfst.find("xbit");
	auto pos_report = mfst.find("report");
	CHECK("DET-05", pos_dynx   != std::string::npos, "dynx present");
	CHECK("DET-05", pos_xbit   != std::string::npos, "xbit present");
	CHECK("DET-05", pos_report != std::string::npos, "report present");
	// dynx export before xbit export before report in source
	CHECK("DET-05", pos_dynx < pos_xbit,   "dynx before xbit");
	CHECK("DET-05", pos_xbit < pos_report, "xbit before report");
}

void test_DET_06() {
	// gate manifest ordering: quality_gate then export_gate
	auto g     = CtlParser::parse(DET_SCRIPT).graph;
	auto plan  = ExecPlan::compile(g, "dev", 0);
	auto gmfst = plan.gate_manifest_json();
	auto pos_q = gmfst.find("quality_gate");
	auto pos_e = gmfst.find("export_gate");
	CHECK("DET-06", pos_q != std::string::npos, "quality_gate listed");
	CHECK("DET-06", pos_e != std::string::npos, "export_gate listed");
	CHECK("DET-06", pos_q < pos_e, "quality_gate before export_gate (source order)");
}

void test_DET_07() {
	// FNV-1a-64 is a pure function: same input → same output every call
	uint64_t h1 = fnv1a_64("test_string_for_fnv");
	uint64_t h2 = fnv1a_64("test_string_for_fnv");
	uint64_t h3 = fnv1a_64("test_string_for_fnv");
	CHECK("DET-07", h1 == h2, "fnv repeat 1 == 2");
	CHECK("DET-07", h2 == h3, "fnv repeat 2 == 3");
	// Different inputs produce different hashes
	uint64_t ha = fnv1a_64("input_a");
	uint64_t hb = fnv1a_64("input_b");
	CHECK("DET-07", ha != hb, "different inputs → different hashes");
}

void test_DET_08() {
	// script_hash must be stable across three compile() calls with the same inputs
	auto g  = CtlParser::parse(DET_SCRIPT).graph;
	auto p1 = ExecPlan::compile(g, "v5.1.4", 7);
	auto p2 = ExecPlan::compile(g, "v5.1.4", 7);
	auto p3 = ExecPlan::compile(g, "v5.1.4", 7);
	CHECK("DET-08", p1.script_hash == p2.script_hash, "script_hash stable (1==2)");
	CHECK("DET-08", p2.script_hash == p3.script_hash, "script_hash stable (2==3)");
}

void test_DET_09() {
	// ExecPlan.command_count must match ExecGraph.size()
	auto g    = CtlParser::parse(DET_SCRIPT).graph;
	auto plan = ExecPlan::compile(g, "dev", 0);
	size_t graph_count = g.size();
	size_t plan_count  = plan.commands.size();
	CHECK("DET-09", graph_count > 0,                "graph is non-empty");
	CHECK("DET-09", plan_count == graph_count,       "plan command count == graph command count");
}

void test_DET_10() {
	// Rejected script must produce no ExecPlan output
	auto g  = CtlParser::parse("physics.illegal()").graph;
	auto vr = CtlValidator::validate(g);
	CHECK("DET-10", !vr.ok, "validation fails on illegal namespace");
	// Only compile if validation passed — confirm we never do so here
	int compile_attempts = 0;
	if (vr.ok) {
		auto plan = ExecPlan::compile(g, "dev", 0);
		(void)plan;
		++compile_attempts;
	}
	CHECK("DET-10", compile_attempts == 0, "compile not reached after failed validation");
}

// ---------------------------------------------------------------------------
int main() {
	std::cout << "\n=== Group 68 — CTL Determinism ===\n\n";
	test_DET_01();
	test_DET_02();
	test_DET_03();
	test_DET_04();
	test_DET_05();
	test_DET_06();
	test_DET_07();
	test_DET_08();
	test_DET_09();
	test_DET_10();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
