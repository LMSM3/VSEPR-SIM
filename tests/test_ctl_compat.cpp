/**
 * test_ctl_compat.cpp  —  Group 69: CTL Existing Workflow Compatibility
 * ======================================================================
 * WO-VSIM-CTL-TEST  |  v5.1.x
 *
 * CTL headers must not pollute or break existing v5.1.x APIs.
 *
 *   COMPAT-01  ctl_types.hpp can be included alongside vsim_document.hpp
 *   COMPAT-02  CtlNamespace enum does not collide with existing VSIM enums
 *   COMPAT-03  MetricsStore does not conflict with existing naming conventions
 *   COMPAT-04  ExecGraph default state produces an empty graph
 *   COMPAT-05  CtlDispatcher with null hooks falls back to default no-op hooks
 *   COMPAT-06  CTL headers are pragma-once safe (double-include is silent)
 *   COMPAT-07  CtlParser::parse("") returns ok=true, empty graph
 *   COMPAT-08  CtlValidator::validate({}) returns ok=true on empty graph
 *   COMPAT-09  ExecPlan::compile({}, "dev", 0) does not crash on empty graph
 *   COMPAT-10  CtlDispatcher::dispatch(empty_plan) returns all-ok session
 */

// Double-include test: include all CTL headers twice
#include "vsim/ctl/ctl_types.hpp"
#include "vsim/ctl/ctl_parser.hpp"
#include "vsim/ctl/ctl_validator.hpp"
#include "vsim/ctl/ctl_metrics.hpp"
#include "vsim/ctl/ctl_gate.hpp"
#include "vsim/ctl/ctl_plan.hpp"
#include "vsim/ctl/ctl_dispatcher.hpp"

// COMPAT-01: include alongside vsim_document.hpp (no collision)
#include "vsim/vsim_document.hpp"

// Second include of all CTL headers — pragma once keeps this silent
#include "vsim/ctl/ctl_types.hpp"
#include "vsim/ctl/ctl_parser.hpp"
#include "vsim/ctl/ctl_validator.hpp"
#include "vsim/ctl/ctl_metrics.hpp"
#include "vsim/ctl/ctl_gate.hpp"
#include "vsim/ctl/ctl_plan.hpp"
#include "vsim/ctl/ctl_dispatcher.hpp"

#include <iostream>
#include <string>
#include <type_traits>

using namespace vsim::ctl;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

// ---------------------------------------------------------------------------
void test_COMPAT_01() {
	// If it compiled, COMPAT-01 is already satisfied
	CHECK("COMPAT-01", true, "ctl headers compile alongside vsim_document.hpp");
	// Also verify the document type is usable
	vsim::VsimDocument doc;
	CHECK("COMPAT-01", doc.source_path.empty(), "VsimDocument still default-constructible");
}

void test_COMPAT_02() {
	// CtlNamespace is a scoped enum — no pollution of global namespace
	auto ns = parse_namespace("runtime");
	CHECK("COMPAT-02", ns == CtlNamespace::Runtime, "CtlNamespace::Runtime parsed ok");
	// Verify the namespace name round-trips without colliding with anything
	std::string name = namespace_name(CtlNamespace::Runtime);
	CHECK("COMPAT-02", name == "runtime", "namespace_name round-trip ok");
}

void test_COMPAT_03() {
	// MetricsStore must be default-constructible and start uncaptured
	MetricsStore ms;
	CHECK("COMPAT-03", !ms.captured, "MetricsStore starts uncaptured");
	// Confirm the type exists and is in vsim::ctl (not leaking into global)
	CHECK("COMPAT-03", std::is_default_constructible_v<MetricsStore>, "MetricsStore default-constructible");
}

void test_COMPAT_04() {
	// ExecGraph is just std::vector<CtlCommand> — empty by default
	ExecGraph g;
	CHECK("COMPAT-04", g.empty(), "ExecGraph default is empty");
	CHECK("COMPAT-04", g.size() == 0, "ExecGraph size() == 0");
}

void test_COMPAT_05() {
	// CtlDispatcher(nullptr) falls back to internal default no-op hooks
	CtlDispatcher d(nullptr);
	ExecGraph g;
	auto plan = ExecPlan::compile(g, "dev", 0);
	auto s = d.dispatch(plan);
	CHECK("COMPAT-05", s.all_ok(), "null-hook dispatcher returns all-ok");
	CHECK("COMPAT-05", s.results.empty(), "empty plan → empty results");
}

void test_COMPAT_06() {
	// Double-include: already handled at the top of this file with two include blocks.
	// If this line is reached without link error or redefinition error, it passed.
	auto r = CtlParser::parse("runtime.run_case(\"x\")");
	CHECK("COMPAT-06", r.ok, "parser still usable after double-include");
}

void test_COMPAT_07() {
	auto r = CtlParser::parse("");
	CHECK("COMPAT-07", r.ok,        "empty script → ok=true");
	CHECK("COMPAT-07", r.graph.empty(), "empty script → empty graph");
}

void test_COMPAT_08() {
	ExecGraph empty;
	auto vr = CtlValidator::validate(empty);
	CHECK("COMPAT-08", vr.ok,           "empty graph → validation ok");
	CHECK("COMPAT-08", vr.errors.empty(), "no errors on empty graph");
}

void test_COMPAT_09() {
	ExecGraph empty;
	// Must not crash; plan is valid but has no commands
	auto plan = ExecPlan::compile(empty, "dev", 0);
	CHECK("COMPAT-09", plan.commands.empty(),       "empty plan has no commands");
	CHECK("COMPAT-09", !plan.plan_hash.empty(),     "plan_hash still produced");
	CHECK("COMPAT-09", !plan.script_hash.empty(),   "script_hash still produced");
}

void test_COMPAT_10() {
	ExecGraph empty;
	auto plan = ExecPlan::compile(empty, "dev", 0);
	CtlDispatcher d;
	auto s = d.dispatch(plan);
	CHECK("COMPAT-10", s.all_ok(),        "empty plan dispatch returns all-ok");
	CHECK("COMPAT-10", s.results.empty(), "no results for empty plan");
}

// ---------------------------------------------------------------------------
int main() {
	std::cout << "\n=== Group 69 — CTL Existing Workflow Compatibility ===\n\n";
	test_COMPAT_01();
	test_COMPAT_02();
	test_COMPAT_03();
	test_COMPAT_04();
	test_COMPAT_05();
	test_COMPAT_06();
	test_COMPAT_07();
	test_COMPAT_08();
	test_COMPAT_09();
	test_COMPAT_10();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
