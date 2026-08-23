/**
 * tests/test_wo93a_status_loop.cpp
 *
 * Verifies the WO-93A [visual] status-loop schema round-trip:
 *   - show_status_loop, status_loop_hz, hardware_monitor_hz parse correctly.
 *   - Defaults are sensible when the keys are absent.
 */

#include "vsim/vsim_parser.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
	void require(bool cond, const char* msg) {
		if (!cond) {
			std::fprintf(stderr, "FAIL: %s\n", msg);
			std::exit(1);
		}
	}
}

int main() {
	// 1) Explicit values.
	const std::string explicit_script = R"(
[visual]
output_type           = "terminal_chart"
show_status_loop      = true
status_loop_hz        = 60.0
hardware_monitor_hz   = 5.0
)";

	auto doc1 = vsim::VsimParser::parse_string(explicit_script, "wo93a_explicit.vsim");
	require(doc1.visual.show_status_loop == true,
			"explicit: show_status_loop should be true");
	require(doc1.visual.status_loop_hz == 60.0f,
			"explicit: status_loop_hz should be 60.0");
	require(doc1.visual.hardware_monitor_hz == 5.0f,
			"explicit: hardware_monitor_hz should be 5.0");

	// 2) Defaults when keys are absent.
	const std::string default_script = R"(
[visual]
output_type = "terminal_chart"
)";

	auto doc2 = vsim::VsimParser::parse_string(default_script, "wo93a_default.vsim");
	require(doc2.visual.show_status_loop == false,
			"default: show_status_loop should be false");
	require(doc2.visual.status_loop_hz == 30.0f,
			"default: status_loop_hz should be 30.0");
	require(doc2.visual.hardware_monitor_hz == 2.0f,
			"default: hardware_monitor_hz should be 2.0");

	std::printf("OK  WO-93A status-loop schema round-trip\n");
	return 0;
}
