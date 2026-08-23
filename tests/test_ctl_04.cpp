/**
 * test_ctl_04.cpp  —  Group 61: CTL-04 Kernel channel command bindings
 * =====================================================================
 * Tests: kernel.channel.* and kernel.trace.* dispatch through hooks
 *
 *   CTL-04-01  kernel.channel.enable calls hook with correct channel + weight
 *   CTL-04-02  kernel.channel.disable calls hook with correct channel
 *   CTL-04-03  kernel.channel.reset calls hook
 *   CTL-04-04  kernel.trace.enable calls hook with profile
 *   CTL-04-05  kernel.set calls hook with key/value
 *   CTL-04-06  channel enable sequence: reset then enable then run_case
 *   CTL-04-07  invalid channel name blocked by validator (not hook)
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

// Recording hooks
struct RecordingHooks : CtlRuntimeHooks {
	struct Record { std::string op; std::string arg1; std::string arg2; double darg = 0.0; };
	std::vector<Record> calls;

	bool channel_enable(const std::string& ch, double w) override {
		calls.push_back({"channel_enable", ch, "", w}); return true;
	}
	bool channel_disable(const std::string& ch) override {
		calls.push_back({"channel_disable", ch, "", 0.0}); return true;
	}
	bool channel_reset() override {
		calls.push_back({"channel_reset", "", "", 0.0}); return true;
	}
	bool trace_enable(const std::string& profile) override {
		calls.push_back({"trace_enable", profile, "", 0.0}); return true;
	}
	bool kernel_set(const std::string& k, const std::string& v) override {
		calls.push_back({"kernel_set", k, v, 0.0}); return true;
	}
	bool run_case(const std::string& name) override {
		calls.push_back({"run_case", name, "", 0.0}); return true;
	}

	const Record* find(const std::string& op) const {
		for (auto& r : calls) if (r.op == op) return &r;
		return nullptr;
	}
};

static DispatchSession run_script(const std::string& script, RecordingHooks& hooks) {
	auto g    = CtlParser::parse(script).graph;
	auto plan = ExecPlan::compile(g, "dev", 0);
	CtlDispatcher d(&hooks);
	return d.dispatch(plan);
}

void test_CTL_04_01() {
	RecordingHooks h;
	run_script("kernel.channel.enable(name = \"coulomb\", weight = 2.0)", h);
	auto* r = h.find("channel_enable");
	CHECK("CTL-04-01", r != nullptr,             "channel_enable hook called");
	CHECK("CTL-04-01", r && r->arg1 == "coulomb","channel name correct");
	CHECK("CTL-04-01", r && r->darg  == 2.0,     "weight correct");
}

void test_CTL_04_02() {
	RecordingHooks h;
	run_script("kernel.channel.disable(name = \"dispersion\")", h);
	auto* r = h.find("channel_disable");
	CHECK("CTL-04-02", r != nullptr,               "channel_disable hook called");
	CHECK("CTL-04-02", r && r->arg1 == "dispersion","channel name correct");
}

void test_CTL_04_03() {
	RecordingHooks h;
	run_script("kernel.channel.reset()", h);
	CHECK("CTL-04-03", h.find("channel_reset") != nullptr, "channel_reset hook called");
}

void test_CTL_04_04() {
	RecordingHooks h;
	run_script("kernel.trace.enable(profile = \"force_debug\")", h);
	auto* r = h.find("trace_enable");
	CHECK("CTL-04-04", r != nullptr,                   "trace_enable hook called");
	CHECK("CTL-04-04", r && r->arg1 == "force_debug",  "profile arg correct");
}

void test_CTL_04_05() {
	RecordingHooks h;
	run_script("kernel.set(name = \"cutoff_A\", value = \"8.0\")", h);
	auto* r = h.find("kernel_set");
	CHECK("CTL-04-05", r != nullptr,              "kernel_set hook called");
	CHECK("CTL-04-05", r && r->arg1 == "cutoff_A","key correct");
	CHECK("CTL-04-05", r && r->arg2 == "8.0",     "value correct");
}

void test_CTL_04_06() {
	const char* script = R"(
kernel.channel.reset()
kernel.channel.enable(name = "bond", weight = 1.0)
runtime.run_case("bond_test")
)";
	RecordingHooks h;
	auto session = run_script(script, h);
	CHECK("CTL-04-06", session.all_ok(),                     "session all ok");
	CHECK("CTL-04-06", h.find("channel_reset")  != nullptr, "reset called");
	CHECK("CTL-04-06", h.find("channel_enable") != nullptr, "enable called");
	CHECK("CTL-04-06", h.find("run_case")       != nullptr, "run_case called");
}

void test_CTL_04_07() {
	auto g = CtlParser::parse("kernel.channel.enable(name = \"gravity\")").graph;
	auto v = CtlValidator::validate(g);
	CHECK("CTL-04-07", !v.ok, "invalid channel caught by validator");
	CHECK("CTL-04-07", v.first_error().find("gravity") != std::string::npos, "channel named in error");
}

int main() {
	std::cout << "\n=== Group 61 — CTL-04 Kernel channel command bindings ===\n\n";
	test_CTL_04_01();
	test_CTL_04_02();
	test_CTL_04_03();
	test_CTL_04_04();
	test_CTL_04_05();
	test_CTL_04_06();
	test_CTL_04_07();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
