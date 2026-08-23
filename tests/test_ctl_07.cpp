/**
 * test_ctl_07.cpp  —  Group 64: CTL-07 Deterministic plan hash + artifact manifest
 * ==================================================================================
 * Tests: ctl_plan.hpp
 *
 *   CTL-07-01  ExecPlan::compile produces non-empty plan_hash
 *   CTL-07-02  Same script + same seed + same kernel → same plan_hash
 *   CTL-07-03  Different script → different plan_hash
 *   CTL-07-04  Different seed → different plan_hash
 *   CTL-07-05  to_json() contains plan_version, script_hash, plan_hash, commands array
 *   CTL-07-06  artifact_manifest_json() lists dynx.export entries
 *   CTL-07-07  gate_manifest_json() lists gate names
 *   CTL-07-08  ExecPlan::compile correctly extracts gate names from graph
 *   CTL-07-09  script_hash is stable (same text → same hash)
 *   CTL-07-10  to_json() contains every command op from the graph
 */

#include "vsim/ctl/ctl_parser.hpp"
#include "vsim/ctl/ctl_plan.hpp"
#include <iostream>
#include <string>

using namespace vsim::ctl;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

static ExecPlan compile(const std::string& script,
						 const std::string& kv = "dev",
						 uint64_t seed = 0)
{
	auto g = CtlParser::parse(script).graph;
	return ExecPlan::compile(g, kv, seed);
}

const char* SCRIPT_A = R"(
kernel.channel.reset()
kernel.channel.enable(name = "coulomb", weight = 1.0)
runtime.run_case("gate_coulomb")
gate.begin("force_channel_coulomb")
gate.end()
artifact.dynx.enable(profile = "smf_gate")
artifact.dynx.export("out/gates/coulomb/run.dynx")
)";

const char* SCRIPT_B = R"(
kernel.channel.reset()
kernel.channel.enable(name = "bond", weight = 1.0)
runtime.run_case("gate_bond")
gate.begin("force_channel_bond")
gate.end()
artifact.dynx.enable(profile = "smf_gate")
artifact.dynx.export("out/gates/bond/run.dynx")
)";

void test_CTL_07_01() {
	auto p = compile(SCRIPT_A, "v5.1.4", 9101);
	CHECK("CTL-07-01", !p.plan_hash.empty(),    "plan_hash non-empty");
	CHECK("CTL-07-01", !p.script_hash.empty(),  "script_hash non-empty");
}

void test_CTL_07_02() {
	auto p1 = compile(SCRIPT_A, "v5.1.4", 9101);
	auto p2 = compile(SCRIPT_A, "v5.1.4", 9101);
	CHECK("CTL-07-02", p1.plan_hash == p2.plan_hash, "same inputs → same plan_hash");
}

void test_CTL_07_03() {
	auto p1 = compile(SCRIPT_A, "v5.1.4", 9101);
	auto p2 = compile(SCRIPT_B, "v5.1.4", 9101);
	CHECK("CTL-07-03", p1.plan_hash != p2.plan_hash, "different script → different hash");
}

void test_CTL_07_04() {
	auto p1 = compile(SCRIPT_A, "v5.1.4", 9101);
	auto p2 = compile(SCRIPT_A, "v5.1.4", 9999);
	CHECK("CTL-07-04", p1.plan_hash != p2.plan_hash, "different seed → different hash");
}

void test_CTL_07_05() {
	auto p   = compile(SCRIPT_A, "v5.1.4", 9101);
	auto json = p.to_json();
	CHECK("CTL-07-05", json.find("\"plan_version\"")  != std::string::npos, "plan_version in JSON");
	CHECK("CTL-07-05", json.find("\"script_hash\"")   != std::string::npos, "script_hash in JSON");
	CHECK("CTL-07-05", json.find("\"plan_hash\"")     != std::string::npos, "plan_hash in JSON");
	CHECK("CTL-07-05", json.find("\"commands\"")      != std::string::npos, "commands array in JSON");
	CHECK("CTL-07-05", json.find("vsim_ctl_v1")       != std::string::npos, "correct plan_version value");
}

void test_CTL_07_06() {
	auto p    = compile(SCRIPT_A, "v5.1.4", 9101);
	auto mfst = p.artifact_manifest_json();
	CHECK("CTL-07-06", mfst.find("dynx")             != std::string::npos, "dynx in artifact manifest");
	CHECK("CTL-07-06", mfst.find("dynx.export")      != std::string::npos, "dynx.export op in manifest");
	CHECK("CTL-07-06", mfst.find("\"artifacts\"")     != std::string::npos, "artifacts key in manifest JSON");
}

void test_CTL_07_07() {
	auto p    = compile(SCRIPT_A, "v5.1.4", 9101);
	auto gmfst = p.gate_manifest_json();
	CHECK("CTL-07-07", gmfst.find("force_channel_coulomb") != std::string::npos, "gate name in manifest");
	CHECK("CTL-07-07", gmfst.find("\"gates\"")              != std::string::npos, "gates key in manifest JSON");
}

void test_CTL_07_08() {
	auto p = compile(SCRIPT_A, "v5.1.4", 9101);
	CHECK("CTL-07-08", p.gate_names.size() == 1,                        "one gate extracted");
	CHECK("CTL-07-08", p.gate_names[0] == "force_channel_coulomb",      "gate name correct");
}

void test_CTL_07_09() {
	auto p1 = compile(SCRIPT_A, "v5.1.4", 0);
	auto p2 = compile(SCRIPT_A, "v5.1.4", 0);
	CHECK("CTL-07-09", p1.script_hash == p2.script_hash, "script_hash stable");
}

void test_CTL_07_10() {
	auto p    = compile(SCRIPT_A, "v5.1.4", 9101);
	auto json = p.to_json();
	CHECK("CTL-07-10", json.find("channel.reset")  != std::string::npos, "channel.reset in JSON");
	CHECK("CTL-07-10", json.find("channel.enable") != std::string::npos, "channel.enable in JSON");
	CHECK("CTL-07-10", json.find("run_case")       != std::string::npos, "run_case in JSON");
	CHECK("CTL-07-10", json.find("dynx.export")    != std::string::npos, "dynx.export in JSON");
}

int main() {
	std::cout << "\n=== Group 64 — CTL-07 Deterministic plan hash + artifact manifest ===\n\n";
	test_CTL_07_01();
	test_CTL_07_02();
	test_CTL_07_03();
	test_CTL_07_04();
	test_CTL_07_05();
	test_CTL_07_06();
	test_CTL_07_07();
	test_CTL_07_08();
	test_CTL_07_09();
	test_CTL_07_10();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
