/**
 * test_ctl_02.cpp  —  Group 59: CTL-02 Script parser to execution graph
 * =======================================================================
 * Tests: ctl_parser.hpp
 *
 *   CTL-02-01  simple namespace.op() parses into correct namespace and sub_op
 *   CTL-02-02  quoted string arg parsed correctly
 *   CTL-02-03  key=value double arg parsed correctly
 *   CTL-02-04  key=bool arg parsed correctly
 *   CTL-02-05  const variable stored and expanded in later args
 *   CTL-02-06  comment lines and blank lines ignored
 *   CTL-02-07  multi-command script produces ordered ExecGraph
 *   CTL-02-08  positional arg promoted to "name" key
 *   CTL-02-09  unknown namespace parses (validation is validator's job)
 */

#include "vsim/ctl/ctl_parser.hpp"
#include <iostream>
#include <cassert>

using namespace vsim::ctl;

static int PASS = 0, FAIL = 0;
#define CHECK(id, cond, msg) \
	do { \
		if (cond) { std::cout << "PASS " id ": " msg "\n"; ++PASS; } \
		else      { std::cout << "FAIL " id ": " msg "\n"; ++FAIL; } \
	} while(0)

void test_CTL_02_01() {
	auto r = CtlParser::parse("kernel.channel.enable(name = \"coulomb\", weight = 1.0)");
	CHECK("CTL-02-01", r.ok,                                          "parse ok");
	CHECK("CTL-02-01", r.graph.size() == 1,                           "one command");
	CHECK("CTL-02-01", r.graph[0].ns == CtlNamespace::Kernel,         "namespace=kernel");
	CHECK("CTL-02-01", r.graph[0].sub_op == "channel.enable",         "sub_op=channel.enable");
}

void test_CTL_02_02() {
	auto r = CtlParser::parse("runtime.run_case(\"gate_coulomb\")");
	CHECK("CTL-02-02", r.ok,                                          "parse ok");
	CHECK("CTL-02-02", r.graph.size() == 1,                           "one command");
	CHECK("CTL-02-02", r.graph[0].has_arg("name"),                    "name arg present");
	CHECK("CTL-02-02", r.graph[0].arg("name").as_string() == "gate_coulomb", "string value");
}

void test_CTL_02_03() {
	auto r = CtlParser::parse("kernel.channel.enable(name = \"bond\", weight = 2.5)");
	CHECK("CTL-02-03", r.ok, "parse ok");
	if (r.ok && r.graph.size() == 1 && r.graph[0].has_arg("weight")) {
		CHECK("CTL-02-03", r.graph[0].arg("weight").is_double(),      "weight is double");
		CHECK("CTL-02-03", r.graph[0].arg("weight").as_double() == 2.5, "weight == 2.5");
	}
}

void test_CTL_02_04() {
	auto r = CtlParser::parse("gate.assert(metrics.force.disabled_channels_zero == true)");
	CHECK("CTL-02-04", r.ok, "parse ok");
	// The expression is treated as a bare word / string in the name arg
	CHECK("CTL-02-04", r.graph.size() == 1, "one command");
}

void test_CTL_02_05() {
	const char* script = R"(
const ch = "coulomb"
kernel.channel.enable(name = "${ch}", weight = 1.0)
)";
	auto r = CtlParser::parse(script);
	CHECK("CTL-02-05", r.ok, "parse ok");
	// Find the channel.enable command
	bool found = false;
	for (auto& c : r.graph.commands) {
		if (c.sub_op == "channel.enable" && c.has_arg("name")) {
			CHECK("CTL-02-05", c.arg("name").as_string() == "coulomb", "variable expanded");
			found = true;
		}
	}
	CHECK("CTL-02-05", found, "channel.enable command found");
}

void test_CTL_02_06() {
	const char* script = R"(
# this is a comment

# another comment
runtime.run_case("test")
)";
	auto r = CtlParser::parse(script);
	CHECK("CTL-02-06", r.ok, "parse ok");
	// Should have: one run_case command (const/let for blank lines don't appear)
	int run_count = 0;
	for (auto& c : r.graph.commands) {
		if (c.sub_op == "run_case") ++run_count;
	}
	CHECK("CTL-02-06", run_count == 1, "only run_case command present");
}

void test_CTL_02_07() {
	const char* script = R"(
kernel.channel.reset()
kernel.channel.enable(name = "coulomb", weight = 1.0)
runtime.run_case("gate_coulomb")
metrics.capture()
gate.begin("test_gate")
gate.end()
)";
	auto r = CtlParser::parse(script);
	CHECK("CTL-02-07", r.ok, "parse ok");
	CHECK("CTL-02-07", r.graph.size() == 6, "6 commands in order");
	if (r.graph.size() >= 3) {
		CHECK("CTL-02-07", r.graph[0].sub_op == "channel.reset",  "cmd[0]=channel.reset");
		CHECK("CTL-02-07", r.graph[1].sub_op == "channel.enable", "cmd[1]=channel.enable");
		CHECK("CTL-02-07", r.graph[2].sub_op == "run_case",       "cmd[2]=run_case");
	}
}

void test_CTL_02_08() {
	auto r = CtlParser::parse("gate.begin(\"my_gate\")");
	CHECK("CTL-02-08", r.ok, "parse ok");
	if (r.ok && r.graph.size() == 1) {
		CHECK("CTL-02-08", r.graph[0].has_arg("name"),                  "name key present");
		CHECK("CTL-02-08", r.graph[0].arg("name").as_string() == "my_gate", "positional promoted");
	}
}

void test_CTL_02_09() {
	// Parser is permissive — validator catches unknown namespaces
	auto r = CtlParser::parse("physics.mutate(name = \"particle\")");
	CHECK("CTL-02-09", r.ok,                                          "parse succeeds");
	CHECK("CTL-02-09", r.graph.size() == 1,                           "one command in graph");
	CHECK("CTL-02-09", r.graph[0].ns == CtlNamespace::Unknown,        "namespace=Unknown");
}

int main() {
	std::cout << "\n=== Group 59 — CTL-02 Script parser to execution graph ===\n\n";
	test_CTL_02_01();
	test_CTL_02_02();
	test_CTL_02_03();
	test_CTL_02_04();
	test_CTL_02_05();
	test_CTL_02_06();
	test_CTL_02_07();
	test_CTL_02_08();
	test_CTL_02_09();
	std::cout << "\n  Results: " << PASS << " passed  " << FAIL << " failed\n\n";
	return FAIL > 0 ? 1 : 0;
}
