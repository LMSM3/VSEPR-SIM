/**
 * test_ctl_hooks.cpp  —  Group 66: CTL Runtime Hook Integration
 * =============================================================
 * WO-VSIM-CTL-TEST  |  v5.1.x
 *
 * Recording hooks verify that every dispatched command routes to the
 * correct hook method with the correct arguments.
 *
 *   HOOK-01  runtime.run_case() calls hook exactly once
 *   HOOK-02  runtime.step(n) calls hook with correct n
 *   HOOK-03  runtime.reset(scope) calls hook with correct scope
 *   HOOK-04  kernel.channel.enable() calls hook with channel name + weight
 *   HOOK-05  kernel.channel.disable() calls hook with channel name
 *   HOOK-06  kernel.channel.reset() calls hook
 *   HOOK-07  artifact.dynx.enable() calls artifact hook
 *   HOOK-08  artifact.xbit.create() then xbit.export() calls export hook
 *   HOOK-09  metrics.capture() sets session.metrics.captured = true
 *   HOOK-10  gate begin→assert→end lifecycle produces a GateResult
 */

#include "vsim/ctl/ctl_parser.hpp"
#include "vsim/ctl/ctl_validator.hpp"
#include "vsim/ctl/ctl_plan.hpp"
#include "vsim/ctl/ctl_dispatcher.hpp"
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

// ---------------------------------------------------------------------------
// Recording hook implementation — method names match CtlRuntimeHooks exactly
// ---------------------------------------------------------------------------
struct RecordHooks : CtlRuntimeHooks {
	struct Record {
		std::string op;
		std::string arg1;
		std::string arg2;
		double      darg = 0.0;
		int64_t     iarg = 0;
	};
	std::vector<Record> calls;

	bool run_case(const std::string& n) override
		{ calls.push_back({"run_case", n}); return true; }
	bool step(int64_t n) override
		{ calls.push_back({"step", "", "", 0.0, n}); return true; }
	bool reset(const std::string& s) override
		{ calls.push_back({"reset", s}); return true; }
	bool relax() override
		{ calls.push_back({"relax"}); return true; }
	bool checkpoint() override
		{ calls.push_back({"checkpoint"}); return true; }
	bool channel_enable(const std::string& ch, double w) override
		{ calls.push_back({"channel_enable", ch, "", w}); return true; }
	bool channel_disable(const std::string& ch) override
		{ calls.push_back({"channel_disable", ch}); return true; }
	bool channel_reset() override
		{ calls.push_back({"channel_reset"}); return true; }
	bool trace_enable(const std::string& p) override
		{ calls.push_back({"trace_enable", p}); return true; }
	bool dynx_enable(const std::string& p) override
		{ calls.push_back({"dynx_enable", p}); return true; }
	bool dynx_export(const std::string& p) override
		{ calls.push_back({"dynx_export", p}); return true; }
	bool xbit_create(const std::string& m) override
		{ calls.push_back({"xbit_create", m}); return true; }
	bool xbit_export(const std::string& p) override
		{ calls.push_back({"xbit_export", p}); return true; }

	const Record* find(const std::string& op) const {
		for (auto& r : calls) if (r.op == op) return &r;
		return nullptr;
	}
	int count(const std::string& op) const {
		int n = 0;
		for (auto& r : calls) if (r.op == op) ++n;
		return n;
	}
};

// Validate + compile + dispatch
static DispatchSession run_script(const std::string& src, RecordHooks& hooks) {
	auto pr = CtlParser::parse(src);
	auto vr = CtlValidator::validate(pr.graph);
	if (!vr.ok) {
		// Return empty session — test will fail on all_ok()
		return DispatchSession{};
	}
	auto plan = ExecPlan::compile(pr.graph, "dev", 0);
	CtlDispatcher d(&hooks);
	return d.dispatch(plan);
}

// ---------------------------------------------------------------------------
void test_HOOK_01() {
	RecordHooks h;
	auto s = run_script(R"(
artifact.dynx.enable("default")
runtime.run_case("alpha_run")
)", h);
	CHECK("HOOK-01", s.all_ok(), "session ok");
	CHECK("HOOK-01", h.count("run_case") == 1, "run_case hook called once");
	auto* r = h.find("run_case");
	CHECK("HOOK-01", r && r->arg1 == "alpha_run", "run_case arg correct");
}

void test_HOOK_02() {
	RecordHooks h;
	auto s = run_script(R"(
artifact.dynx.enable("default")
runtime.run_case("x")
runtime.step(250)
)", h);
	CHECK("HOOK-02", s.all_ok(), "session ok");
	CHECK("HOOK-02", h.count("step") == 1, "step hook called once");
	auto* r = h.find("step");
	CHECK("HOOK-02", r && r->iarg == 250, "step n = 250");
}

void test_HOOK_03() {
	RecordHooks h;
	auto s = run_script(R"(
artifact.dynx.enable("default")
runtime.run_case("x")
runtime.reset("velocities")
)", h);
	CHECK("HOOK-03", s.all_ok(), "session ok");
	CHECK("HOOK-03", h.count("reset") == 1, "reset hook called once");
	auto* r = h.find("reset");
	CHECK("HOOK-03", r && r->arg1 == "velocities", "scope = velocities");
}

void test_HOOK_04() {
	RecordHooks h;
	auto s = run_script(R"(
kernel.channel.enable(name = "coulomb", weight = 0.75)
artifact.dynx.enable("default")
runtime.run_case("x")
)", h);
	CHECK("HOOK-04", s.all_ok(), "session ok");
	CHECK("HOOK-04", h.count("channel_enable") == 1, "channel_enable called once");
	auto* r = h.find("channel_enable");
	CHECK("HOOK-04", r && r->arg1 == "coulomb", "channel name ok");
	CHECK("HOOK-04", r && r->darg == 0.75,      "weight ok");
}

void test_HOOK_05() {
	RecordHooks h;
	auto s = run_script(R"(
kernel.channel.disable(name = "dispersion")
artifact.dynx.enable("default")
runtime.run_case("x")
)", h);
	CHECK("HOOK-05", s.all_ok(), "session ok");
	CHECK("HOOK-05", h.count("channel_disable") == 1, "channel_disable called once");
	auto* r = h.find("channel_disable");
	CHECK("HOOK-05", r && r->arg1 == "dispersion", "channel name ok");
}

void test_HOOK_06() {
	RecordHooks h;
	auto s = run_script(R"(
kernel.channel.reset()
artifact.dynx.enable("default")
runtime.run_case("x")
)", h);
	CHECK("HOOK-06", s.all_ok(), "session ok");
	CHECK("HOOK-06", h.count("channel_reset") == 1, "channel_reset called once");
}

void test_HOOK_07() {
	RecordHooks h;
	auto s = run_script(R"(
artifact.dynx.enable(profile = "full_archive")
runtime.run_case("x")
)", h);
	CHECK("HOOK-07", s.all_ok(), "session ok");
	CHECK("HOOK-07", h.count("dynx_enable") == 1, "dynx_enable called once");
	auto* r = h.find("dynx_enable");
	CHECK("HOOK-07", r && r->arg1 == "full_archive", "profile arg ok");
}

void test_HOOK_08() {
	RecordHooks h;
	auto s = run_script(R"(
artifact.xbit.create("full")
artifact.dynx.enable("default")
runtime.run_case("x")
artifact.xbit.export("out/run.xbit")
)", h);
	CHECK("HOOK-08", s.all_ok(), "session ok");
	CHECK("HOOK-08", h.count("xbit_create") == 1, "xbit_create called once");
	CHECK("HOOK-08", h.count("xbit_export") == 1, "xbit_export called once");
	auto* r = h.find("xbit_export");
	CHECK("HOOK-08", r && r->arg1 == "out/run.xbit", "export path ok");
}

void test_HOOK_09() {
	RecordHooks h;
	auto s = run_script(R"(
artifact.dynx.enable("default")
runtime.run_case("x")
metrics.capture()
)", h);
	CHECK("HOOK-09", s.all_ok(), "session ok");
	// After run_case the dispatcher calls metrics.capture() internally;
	// explicit metrics.capture() also sets captured
	CHECK("HOOK-09", s.metrics.captured, "metrics.captured is true");
}

void test_HOOK_10() {
	RecordHooks h;
	auto s = run_script(R"(
artifact.dynx.enable("default")
runtime.run_case("x")
metrics.capture()
gate.begin("verify_gate")
gate.assert(energy.drift < 1.0)
gate.end()
)", h);
	CHECK("HOOK-10", s.all_ok(), "session ok");
	CHECK("HOOK-10", s.gates.total() == 1, "one gate result");
	if (s.gates.total() == 1) {
		CHECK("HOOK-10", s.gates.results[0].name == "verify_gate", "gate name ok");
		// Gate assertion on NaN metrics evaluates false → gate may fail, but lifecycle completed
		CHECK("HOOK-10", !s.gates.results[0].open(), "gate is closed (lifecycle completed)");
		CHECK("HOOK-10", !s.gates.results[0].explicit_fail, "no explicit fail() was called");
	}
}

// ---------------------------------------------------------------------------
int main() {
	std::cout << "\n=== Group 66 — CTL Runtime Hook Integration ===\n\n";
	test_HOOK_01();
	test_HOOK_02();
	test_HOOK_03();
	test_HOOK_04();
	test_HOOK_05();
	test_HOOK_06();
	test_HOOK_07();
	test_HOOK_08();
	test_HOOK_09();
	test_HOOK_10();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
