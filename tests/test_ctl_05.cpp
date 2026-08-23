/**
 * test_ctl_05.cpp  —  Group 62: CTL-05 Artifact command bindings (XBIT/Dynx)
 * ============================================================================
 * Tests: artifact.xbit.* and artifact.dynx.* dispatch
 *
 *   CTL-05-01  artifact.dynx.enable hook called with profile
 *   CTL-05-02  artifact.dynx.include hook called with payload
 *   CTL-05-03  artifact.dynx.export hook called with path
 *   CTL-05-04  artifact.dynx.validate hook called
 *   CTL-05-05  artifact.xbit.create hook called with mode
 *   CTL-05-06  artifact.xbit.export hook called with path
 *   CTL-05-07  artifact.xbit.validate hook called
 *   CTL-05-08  dynx.export without prior enable fails validator
 *   CTL-05-09  full dynx sequence (enable→include×N→export) dispatches correctly
 */

#include "vsim/ctl/ctl_parser.hpp"
#include "vsim/ctl/ctl_validator.hpp"
#include "vsim/ctl/ctl_plan.hpp"
#include "vsim/ctl/ctl_dispatcher.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace vsim::ctl;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

struct ArtifactHooks : CtlRuntimeHooks {
	struct Rec { std::string op, arg; };
	std::vector<Rec> calls;

	bool dynx_enable(const std::string& p)  override { calls.push_back({"dynx_enable",   p}); return true; }
	bool dynx_include(const std::string& p) override { calls.push_back({"dynx_include",  p}); return true; }
	bool dynx_export(const std::string& p)  override { calls.push_back({"dynx_export",   p}); return true; }
	bool dynx_validate()                    override { calls.push_back({"dynx_validate",  ""}); return true; }
	bool xbit_create(const std::string& m)  override { calls.push_back({"xbit_create",   m}); return true; }
	bool xbit_export(const std::string& p)  override { calls.push_back({"xbit_export",   p}); return true; }
	bool xbit_validate()                    override { calls.push_back({"xbit_validate",  ""}); return true; }

	const Rec* find(const std::string& op) const {
		for (auto& r : calls) if (r.op == op) return &r;
		return nullptr;
	}
	int count(const std::string& op) const {
		int n = 0;
		for (auto& r : calls) if (r.op == op) ++n;
		return n;
	}
};

static DispatchSession run(const std::string& s, ArtifactHooks& h) {
	auto g    = CtlParser::parse(s).graph;
	auto plan = ExecPlan::compile(g, "dev", 0);
	CtlDispatcher d(&h);
	return d.dispatch(plan);
}

void test_CTL_05_01() {
	ArtifactHooks h;
	run("artifact.dynx.enable(profile = \"smf_gate\")", h);
	auto* r = h.find("dynx_enable");
	CHECK("CTL-05-01", r != nullptr,               "dynx_enable called");
	CHECK("CTL-05-01", r && r->arg == "smf_gate",  "profile arg correct");
}

void test_CTL_05_02() {
	ArtifactHooks h;
	run("artifact.dynx.enable(profile = \"debug\")\nartifact.dynx.include(\"positions\")", h);
	CHECK("CTL-05-02", h.find("dynx_include") != nullptr, "dynx_include called");
	CHECK("CTL-05-02", h.find("dynx_include")->arg == "positions", "payload correct");
}

void test_CTL_05_03() {
	ArtifactHooks h;
	run("artifact.dynx.enable(profile = \"debug\")\nartifact.dynx.export(\"out/run.dynx\")", h);
	auto* r = h.find("dynx_export");
	CHECK("CTL-05-03", r != nullptr,                  "dynx_export called");
	CHECK("CTL-05-03", r && r->arg == "out/run.dynx", "path correct");
}

void test_CTL_05_04() {
	ArtifactHooks h;
	run("artifact.dynx.enable(profile = \"debug\")\nartifact.dynx.validate()", h);
	CHECK("CTL-05-04", h.find("dynx_validate") != nullptr, "dynx_validate called");
}

void test_CTL_05_05() {
	ArtifactHooks h;
	run("artifact.xbit.create(mode = \"geometry_binding\")", h);
	auto* r = h.find("xbit_create");
	CHECK("CTL-05-05", r != nullptr,                         "xbit_create called");
	CHECK("CTL-05-05", r && r->arg == "geometry_binding",    "mode correct");
}

void test_CTL_05_06() {
	ArtifactHooks h;
	run("artifact.xbit.create(mode = \"geo\")\nartifact.xbit.export(\"out/run.xbit\")", h);
	auto* r = h.find("xbit_export");
	CHECK("CTL-05-06", r != nullptr,                   "xbit_export called");
	CHECK("CTL-05-06", r && r->arg == "out/run.xbit",  "path correct");
}

void test_CTL_05_07() {
	ArtifactHooks h;
	run("artifact.xbit.create(mode = \"geo\")\nartifact.xbit.validate()", h);
	CHECK("CTL-05-07", h.find("xbit_validate") != nullptr, "xbit_validate called");
}

void test_CTL_05_08() {
	auto g = CtlParser::parse("artifact.dynx.export(\"out/run.dynx\")").graph;
	auto v = CtlValidator::validate(g);
	CHECK("CTL-05-08", !v.ok, "export without enable fails validation");
}

void test_CTL_05_09() {
	const char* script = R"(
artifact.dynx.enable(profile = "smf_gate")
artifact.dynx.include("positions")
artifact.dynx.include("velocities")
artifact.dynx.include("force_channels")
artifact.dynx.include("energy_terms")
runtime.run_case("gate_coulomb")
artifact.dynx.validate()
artifact.dynx.export("out/gates/coulomb/run.dynx")
)";
	ArtifactHooks h;
	auto session = run(script, h);
	CHECK("CTL-05-09", session.all_ok(),         "all commands ok");
	CHECK("CTL-05-09", h.count("dynx_include") == 4, "4 dynx_include calls");
	CHECK("CTL-05-09", h.find("dynx_export") != nullptr, "dynx_export called");
}

int main() {
	std::cout << "\n=== Group 62 — CTL-05 Artifact command bindings ===\n\n";
	test_CTL_05_01();
	test_CTL_05_02();
	test_CTL_05_03();
	test_CTL_05_04();
	test_CTL_05_05();
	test_CTL_05_06();
	test_CTL_05_07();
	test_CTL_05_08();
	test_CTL_05_09();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
