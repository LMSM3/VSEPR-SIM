/**
 * test_ctl_01.cpp  —  Group 58: CTL-01 Typed command namespace registry
 * ======================================================================
 * Tests: ctl_types.hpp
 *
 *   CTL-01-01  parse_namespace recognises all six namespaces
 *   CTL-01-02  parse_namespace returns Unknown for garbage input
 *   CTL-01-03  is_known_op accepts valid ops per namespace
 *   CTL-01-04  is_known_op rejects unknown ops
 *   CTL-01-05  is_known_kernel_channel accepts valid channels
 *   CTL-01-06  is_known_kernel_channel rejects unknown channels
 *   CTL-01-07  CtlArg value_string produces correct representations
 *   CTL-01-08  CtlCommand has_arg / arg accessors work correctly
 */

#include "vsim/ctl/ctl_types.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace vsim::ctl;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

// ---------------------------------------------------------------------------

void test_CTL_01_01() {
	CHECK("CTL-01-01", parse_namespace("runtime")  == CtlNamespace::Runtime,  "runtime");
	CHECK("CTL-01-01", parse_namespace("kernel")   == CtlNamespace::Kernel,   "kernel");
	CHECK("CTL-01-01", parse_namespace("artifact") == CtlNamespace::Artifact, "artifact");
	CHECK("CTL-01-01", parse_namespace("metrics")  == CtlNamespace::Metrics,  "metrics");
	CHECK("CTL-01-01", parse_namespace("gate")     == CtlNamespace::Gate,     "gate");
	CHECK("CTL-01-01", parse_namespace("const")    == CtlNamespace::Control,  "const→control");
}

void test_CTL_01_02() {
	CHECK("CTL-01-02", parse_namespace("physics") == CtlNamespace::Unknown, "unknown namespace");
	CHECK("CTL-01-02", parse_namespace("")         == CtlNamespace::Unknown, "empty string");
	CHECK("CTL-01-02", parse_namespace("RUNTIME")  == CtlNamespace::Unknown, "case-sensitive");
}

void test_CTL_01_03() {
	CHECK("CTL-01-03", is_known_op(CtlNamespace::Runtime,  "run_case"),       "runtime.run_case");
	CHECK("CTL-01-03", is_known_op(CtlNamespace::Kernel,   "channel.enable"), "kernel.channel.enable");
	CHECK("CTL-01-03", is_known_op(CtlNamespace::Artifact, "dynx.export"),    "artifact.dynx.export");
	CHECK("CTL-01-03", is_known_op(CtlNamespace::Metrics,  "capture"),        "metrics.capture");
	CHECK("CTL-01-03", is_known_op(CtlNamespace::Gate,     "begin"),          "gate.begin");
}

void test_CTL_01_04() {
	CHECK("CTL-01-04", !is_known_op(CtlNamespace::Runtime,  "inject_particle"), "runtime.inject_particle unknown");
	CHECK("CTL-01-04", !is_known_op(CtlNamespace::Kernel,   "mutate_force"),    "kernel.mutate_force unknown");
	CHECK("CTL-01-04", !is_known_op(CtlNamespace::Artifact, "xyzwrite"),        "artifact.xyzwrite unknown");
}

void test_CTL_01_05() {
	CHECK("CTL-01-05", is_known_kernel_channel("coulomb"),    "coulomb");
	CHECK("CTL-01-05", is_known_kernel_channel("repulsion"),  "repulsion");
	CHECK("CTL-01-05", is_known_kernel_channel("dispersion"), "dispersion");
	CHECK("CTL-01-05", is_known_kernel_channel("bond"),       "bond");
	CHECK("CTL-01-05", is_known_kernel_channel("field"),      "field");
	CHECK("CTL-01-05", is_known_kernel_channel("state"),      "state");
}

void test_CTL_01_06() {
	CHECK("CTL-01-06", !is_known_kernel_channel("gravity"),   "gravity not a channel");
	CHECK("CTL-01-06", !is_known_kernel_channel(""),          "empty not a channel");
	CHECK("CTL-01-06", !is_known_kernel_channel("COULOMB"),   "case-sensitive check");
}

void test_CTL_01_07() {
	CtlArg a1; a1.key = "s"; a1.value = std::string("hello");
	CtlArg a2; a2.key = "d"; a2.value = 3.14;
	CtlArg a3; a3.key = "b"; a3.value = true;
	CtlArg a4; a4.key = "i"; a4.value = int64_t(42);

	CHECK("CTL-01-07", a1.value_string().find("hello") != std::string::npos, "string value_string");
	CHECK("CTL-01-07", a2.value_string().find("3.14")  != std::string::npos, "double value_string");
	CHECK("CTL-01-07", a3.value_string() == "true",                           "bool true value_string");
	CHECK("CTL-01-07", a4.value_string().find("42")    != std::string::npos, "int value_string");
}

void test_CTL_01_08() {
	CtlCommand cmd;
	cmd.ns     = CtlNamespace::Kernel;
	cmd.op     = "kernel.channel.enable";
	cmd.sub_op = "channel.enable";
	CtlArg a; a.key = "name"; a.value = std::string("coulomb");
	cmd.args["name"] = a;

	CHECK("CTL-01-08", cmd.has_arg("name"),                         "has_arg returns true");
	CHECK("CTL-01-08", !cmd.has_arg("weight"),                      "has_arg returns false for missing");
	CHECK("CTL-01-08", cmd.arg("name").as_string() == "coulomb",    "arg accessor");
	CHECK("CTL-01-08", cmd.first_string() == "coulomb",             "first_string");
}

// ---------------------------------------------------------------------------

int main() {
	std::cout << "\n=== Group 58 — CTL-01 Typed command namespace registry ===\n\n";
	test_CTL_01_01();
	test_CTL_01_02();
	test_CTL_01_03();
	test_CTL_01_04();
	test_CTL_01_05();
	test_CTL_01_06();
	test_CTL_01_07();
	test_CTL_01_08();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
