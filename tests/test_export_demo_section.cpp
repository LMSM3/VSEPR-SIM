/**
 * tests/test_export_demo_section.cpp
 * =====================================
 * WO-OUTPUT-P2-A  |  Group 73  |  ExportDemoSection parse + defaults
 */

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_parser.hpp"

#include <cassert>

using vsim::ExportDemoSection;
using vsim::VsimDocument;
using vsim::VsimParser;
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static VsimDocument parse_script(const std::string& src) {
	return VsimParser::parse_string(src);
}

static void check(bool cond, const char* msg) {
	if (!cond) {
		std::cerr << "FAIL: " << msg << "\n";
		std::exit(1);
	}
	std::cout << "  PASS: " << msg << "\n";
}

// ---------------------------------------------------------------------------
// P2-A-01  defaults
// ---------------------------------------------------------------------------

static void test_defaults() {
	ExportDemoSection s;
	check(!s.enabled,                     "P2-A-01 enabled default false");
	check(s.demo_frames == 10,            "P2-A-01 demo_frames default 10");
	check(s.strategy == "uniform",        "P2-A-01 strategy default uniform");
	check(s.write_demo_dynx,              "P2-A-01 write_demo_dynx default true");
	check(s.write_demo_bundle,            "P2-A-01 write_demo_bundle default true");
	check(s.include_source,               "P2-A-01 include_source default true");
	check(s.include_manifest,             "P2-A-01 include_manifest default true");
	check(s.bundle_name.empty(),          "P2-A-01 bundle_name default empty");
}

// ---------------------------------------------------------------------------
// P2-A-02  parse all keys
// ---------------------------------------------------------------------------

static void test_parse_all_keys() {
	std::string src =
		"[simulation]\n"
		"name = test\n"
		"\n"
		"[export.demo]\n"
		"enabled           = true\n"
		"demo_frames       = 5\n"
		"strategy          = first\n"
		"write_demo_dynx   = false\n"
		"write_demo_bundle = false\n"
		"include_source    = false\n"
		"include_manifest  = false\n"
		"bundle_name       = my_demo.X\n";

	auto doc = parse_script(src);
	check(doc.export_demo.enabled,              "P2-A-02 enabled");
	check(doc.export_demo.demo_frames == 5,     "P2-A-02 demo_frames");
	check(doc.export_demo.strategy == "first",  "P2-A-02 strategy");
	check(!doc.export_demo.write_demo_dynx,     "P2-A-02 write_demo_dynx false");
	check(!doc.export_demo.write_demo_bundle,   "P2-A-02 write_demo_bundle false");
	check(!doc.export_demo.include_source,      "P2-A-02 include_source false");
	check(!doc.export_demo.include_manifest,    "P2-A-02 include_manifest false");
	check(doc.export_demo.bundle_name == "my_demo.X", "P2-A-02 bundle_name");
}

// ---------------------------------------------------------------------------
// P2-A-03  strategy variants parse
// ---------------------------------------------------------------------------

static void test_strategy_variants() {
	for (const char* strat : {"first", "last", "uniform", "event_gated"}) {
		std::string src =
			std::string("[simulation]\nname = t\n[export.demo]\nenabled = true\nstrategy = ") +
			strat + "\n";
		auto doc = parse_script(src);
		std::string label = std::string("P2-A-03 strategy=") + strat;
		check(doc.export_demo.strategy == strat, label.c_str());
	}
}

// ---------------------------------------------------------------------------
// P2-A-04  export.demo does not pollute export section
// ---------------------------------------------------------------------------

static void test_no_cross_pollution() {
	std::string src =
		"[simulation]\nname = t\n"
		"[export]\nwrite_xyz = false\n"
		"[export.demo]\nenabled = true\n";
	auto doc = parse_script(src);
	check(!doc.exports.write_xyz,       "P2-A-04 export.write_xyz remains false");
	check(doc.export_demo.enabled,      "P2-A-04 export_demo.enabled true");
}

// ---------------------------------------------------------------------------
// P2-A-05  export.demo independent of export.visual
// ---------------------------------------------------------------------------

static void test_no_visual_pollution() {
	std::string src =
		"[simulation]\nname = t\n"
		"[export.visual]\nwrite_svg_figures = true\n"
		"[export.demo]\nenabled = true\ndemo_frames = 3\n";
	auto doc = parse_script(src);
	check(doc.export_visual.write_svg_figures, "P2-A-05 visual flag intact");
	check(doc.export_demo.enabled,             "P2-A-05 demo enabled");
	check(doc.export_demo.demo_frames == 3,    "P2-A-05 demo_frames");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	std::cout << "=== Group 73: ExportDemoSection Parse + Defaults ===\n";
	test_defaults();
	test_parse_all_keys();
	test_strategy_variants();
	test_no_cross_pollution();
	test_no_visual_pollution();
	std::cout << "All Group 73 tests passed.\n";
	return 0;
}
