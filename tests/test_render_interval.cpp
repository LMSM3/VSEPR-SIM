// =============================================================================
// tests/test_render_interval.cpp   WO-VSEPR-SIM-57D  (Group 35)
// =============================================================================
// RI1  render_interval default = 1 renders every step
// RI2  render_interval = 10 renders only on multiples of 10
// RI3  render_interval = 0 treated as 1 (guard path)
// RI4  render_interval parsed from [visual] section in .vsim script
// RI5  VisualExternalSection render_interval overrides VisualSection value
// RI6  VisualExternalSection render_interval = 0 falls back to VisualSection value
// =============================================================================

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_parser.hpp"

using namespace vsim;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<int> collect_render_steps(const VisualSection& vis, int total_steps) {
	std::vector<int> frames;
	for (int s = 0; s <= total_steps; ++s)
		if (vis.should_render(s)) frames.push_back(s);
	return frames;
}

// ---------------------------------------------------------------------------

static void RI1() {
	VisualSection vis;
	// default render_interval = 1
	assert(vis.render_interval == 1);
	auto frames = collect_render_steps(vis, 5);
	// every step: 0,1,2,3,4,5
	assert(frames.size() == 6);
	for (int i = 0; i <= 5; ++i) assert(frames[static_cast<size_t>(i)] == i);
	std::puts("  RI1 PASS  render_interval=1 -> every step rendered");
}

static void RI2() {
	VisualSection vis;
	vis.render_interval = 10;
	auto frames = collect_render_steps(vis, 50);
	// should be 0,10,20,30,40,50 => 6 frames
	assert(frames.size() == 6);
	assert(frames[0] == 0);
	assert(frames[1] == 10);
	assert(frames[5] == 50);
	// step 5 must not be in the list
	for (int f : frames) assert(f % 10 == 0);
	std::puts("  RI2 PASS  render_interval=10 -> multiples of 10 only");
}

static void RI3() {
	VisualSection vis;
	vis.render_interval = 0;   // invalid -> treated as 1
	auto frames = collect_render_steps(vis, 3);
	assert(frames.size() == 4);  // 0,1,2,3
	std::puts("  RI3 PASS  render_interval=0 guarded -> behaves as 1");
}

static void RI4() {
	const std::string script = R"(
[project]
name    = ri4_test
version = v5.0.0-beta.7

[visual]
output_type     = none
render_interval = 25
)";
	VsimDocument doc = VsimParser::parse_string(script);
	assert(doc.visual.render_interval == 25);
	// step 24 should NOT render, step 25 should
	assert(!doc.visual.should_render(24));
	assert( doc.visual.should_render(25));
	assert( doc.visual.should_render(0));
	std::puts("  RI4 PASS  render_interval=25 parsed from [visual] script");
}

static void RI5() {
	// VisualExternalSection render_interval overrides VisualSection
	VisualSection vis;
	vis.render_interval = 10;

	VisualExternalSection ext;
	ext.render_interval = 50;

	// step 10 renders according to vis but NOT according to ext
	assert( vis.should_render(10));
	assert(!ext.should_render(10, vis.render_interval));
	// step 50 renders on both
	assert( vis.should_render(50));
	assert( ext.should_render(50, vis.render_interval));
	std::puts("  RI5 PASS  VisualExternalSection.render_interval overrides VisualSection");
}

static void RI6() {
	// VisualExternalSection render_interval=0 falls back to VisualSection value
	VisualSection vis;
	vis.render_interval = 7;

	VisualExternalSection ext;
	ext.render_interval = 0;   // fallback path

	// should_render(7, 7) == true; should_render(3, 7) == false
	assert( ext.should_render(7, vis.render_interval));
	assert(!ext.should_render(3, vis.render_interval));
	assert( ext.should_render(0, vis.render_interval));
	std::puts("  RI6 PASS  VisualExternalSection.render_interval=0 falls back to visual interval");
}

// ---------------------------------------------------------------------------

int main() {
	std::puts("\n  Group 35 — WO-VSEPR-SIM-57D render_interval");
	std::puts("  ─────────────────────────────────────────────");
	RI1();
	RI2();
	RI3();
	RI4();
	RI5();
	RI6();
	std::puts("\n  [ALL PASS]  Group 35 — render_interval\n");
	return 0;
}
