/**
 * test_ctl_06.cpp  —  Group 63: CTL-06 Metrics/gate assertion system
 * ===================================================================
 * Tests: ctl_metrics.hpp + ctl_gate.hpp + dispatcher integration
 *
 *   CTL-06-01  MetricsStore throws on read before capture
 *   CTL-06-02  MetricsStore readable after capture()
 *   CTL-06-03  assert_expr evaluates numeric < correctly
 *   CTL-06-04  assert_expr evaluates bool == correctly
 *   CTL-06-05  assert_expr fails on wrong value
 *   CTL-06-06  GateState: begin → assert → end produces correct result
 *   CTL-06-07  GateState: explicit_fail marks gate failed
 *   CTL-06-08  GateRegistry accumulates multiple gates
 *   CTL-06-09  dispatcher: metrics.assert before run fails
 *   CTL-06-10  dispatcher: gate with populated metrics assertions
 */

#include "vsim/ctl/ctl_metrics.hpp"
#include "vsim/ctl/ctl_gate.hpp"
#include "vsim/ctl/ctl_parser.hpp"
#include "vsim/ctl/ctl_validator.hpp"
#include "vsim/ctl/ctl_plan.hpp"
#include "vsim/ctl/ctl_dispatcher.hpp"
#include <iostream>
#include <stdexcept>

using namespace vsim::ctl;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

void test_CTL_06_01() {
	MetricsStore m;
	bool threw = false;
	try { m.read_energy(); } catch (const CtlMetricsError&) { threw = true; }
	CHECK("CTL-06-01", threw, "read before capture throws CtlMetricsError");
}

void test_CTL_06_02() {
	MetricsStore m;
	m.set_energy_drift(1e-7);
	m.capture();
	CHECK("CTL-06-02", m.captured,                      "captured flag set");
	CHECK("CTL-06-02", m.read_energy().drift == 1e-7,   "energy.drift readable after capture");
}

void test_CTL_06_03() {
	MetricsStore m;
	m.set_energy_drift(5e-8);
	m.capture();
	auto r = m.assert_expr("energy.drift < 1e-6");
	CHECK("CTL-06-03", r.passed, "drift < 1e-6 passes");
	auto r2 = m.assert_expr("energy.drift < 1e-9");
	CHECK("CTL-06-03", !r2.passed, "drift < 1e-9 fails");
}

void test_CTL_06_04() {
	MetricsStore m;
	m.set_force_disabled_zero(true);
	m.capture();
	auto r = m.assert_expr("force.disabled_channels_zero == true");
	CHECK("CTL-06-04", r.passed, "bool == true passes");
	auto r2 = m.assert_expr("force.disabled_channels_zero == false");
	CHECK("CTL-06-04", !r2.passed, "bool == false fails when true");
}

void test_CTL_06_05() {
	MetricsStore m;
	m.set_force_channel_sum_error(1e-5);
	m.capture();
	auto r = m.assert_expr("force.channel_sum_error < 1e-9");
	CHECK("CTL-06-05", !r.passed, "sum_error 1e-5 < 1e-9 correctly fails");
	CHECK("CTL-06-05", !r.reason.empty(), "reason provided on failure");
}

void test_CTL_06_06() {
	MetricsStore m;
	m.set_energy_drift(1e-8);
	m.set_force_channel_sum_error(1e-10);
	m.set_force_disabled_zero(true);
	m.capture();

	GateState gs;
	gs.begin("coulomb_gate");
	gs.gate_assert("energy.drift < 1e-6",                       m);
	gs.gate_assert("force.channel_sum_error < 1e-9",            m);
	gs.gate_assert("force.disabled_channels_zero == true",       m);
	auto result = gs.end();
	CHECK("CTL-06-06", result.passed(),              "gate passes");
	CHECK("CTL-06-06", result.pass_count() == 3,     "3 assertions passed");
	CHECK("CTL-06-06", result.fail_count() == 0,     "0 assertions failed");
}

void test_CTL_06_07() {
	GateState gs;
	gs.begin("fail_gate");
	gs.explicit_fail("forced failure");
	auto result = gs.end();
	CHECK("CTL-06-07", result.failed(),           "gate failed");
	CHECK("CTL-06-07", result.explicit_fail,      "explicit_fail flag set");
}

void test_CTL_06_08() {
	GateRegistry reg;
	{
		GateState gs;
		gs.begin("gate_a");
		auto r = gs.end();
		reg.add(r);
	}
	{
		GateState gs;
		gs.begin("gate_b");
		gs.explicit_fail("deliberate");
		auto r = gs.end();
		reg.add(r);
	}
	CHECK("CTL-06-08", reg.total()  == 2, "2 gates registered");
	CHECK("CTL-06-08", reg.passed() == 1, "1 gate passed");
	CHECK("CTL-06-08", reg.failed() == 1, "1 gate failed");
	CHECK("CTL-06-08", !reg.all_passed(), "not all passed");
}

// Hooks that populate metrics on run
struct MetricHooks : CtlRuntimeHooks {
	bool run_case(const std::string&) override { return true; }
	void on_run_complete(MetricsStore& m) override {
		m.set_energy_drift(1e-8);
		m.set_force_channel_sum_error(1e-11);
		m.set_force_disabled_zero(true);
		m.set_energy_total(-400.0);
		// has_nan should be false because total_eV is set
	}
};

void test_CTL_06_09() {
	// metrics.assert before run — validator catches this
	const char* script = R"(
metrics.assert(energy.drift < 1e-6)
runtime.run_case("test")
)";
	auto g = CtlParser::parse(script).graph;
	auto v = CtlValidator::validate(g);
	CHECK("CTL-06-09", !v.ok, "metrics.assert before run fails validation");
}

void test_CTL_06_10() {
	const char* script = R"(
kernel.channel.reset()
kernel.channel.enable(name = "coulomb", weight = 1.0)
runtime.run_case("coulomb_gate")
gate.begin("coulomb_gate")
gate.assert(energy.drift < 1e-6)
gate.assert(force.disabled_channels_zero == true)
gate.end()
)";
	auto g   = CtlParser::parse(script).graph;
	auto v   = CtlValidator::validate(g);
	CHECK("CTL-06-10", v.ok, "script validates");

	MetricHooks hooks;
	auto plan    = ExecPlan::compile(g, "dev", 0);
	CtlDispatcher d(&hooks);
	auto session = d.dispatch(plan);

	CHECK("CTL-06-10", session.gates.total()  == 1, "1 gate");
	CHECK("CTL-06-10", session.gates.passed() == 1, "gate passed");
}

int main() {
	std::cout << "\n=== Group 63 — CTL-06 Metrics/gate assertion system ===\n\n";
	test_CTL_06_01();
	test_CTL_06_02();
	test_CTL_06_03();
	test_CTL_06_04();
	test_CTL_06_05();
	test_CTL_06_06();
	test_CTL_06_07();
	test_CTL_06_08();
	test_CTL_06_09();
	test_CTL_06_10();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
