/**
 * test_gallery_parse.cpp -- Gallery script parse smoke tests
 * ==========================================================================
 *
 * Parses all five scripts/gallery/*.vsim scripts inline and verifies:
 *   - No parse errors thrown
 *   - visual_workspace.enabled == true
 *   - view_directives populated with the expected kinds
 *   - options_raw correctly captured (JSON-colon syntax)
 *   - [room] section round-trips for reactor scripts
 *   - default_layout round-trips
 *
 * Tests:
 *   GP1  calibration_htgr: parses without error
 *   GP2  calibration_htgr: visual_workspace.enabled == true
 *   GP3  calibration_htgr: 3 view directives
 *   GP4  calibration_htgr: first kind == "calibration.helix"
 *   GP5  room_reactor: parses without error
 *   GP6  room_reactor: room.preset == "reactor"
 *   GP7  room_reactor: first kind == "room.heatfield"
 *   GP8  room_reactor: options_raw contains "subsample"
 *   GP9  helium_reactor: parses without error
 *   GP10 helium_reactor: room.preset == "reactor"
 *   GP11 helium_reactor: 4 view directives
 *   GP12 helium_reactor: kinds[0] == "room.heatfield", kinds[1] == "calibration.helix"
 *   GP13 helium_reactor: room.heatfield options_raw contains "subsample"
 *   GP14 nacl_pbc_supercell: parses without error
 *   GP15 nacl_pbc_supercell: 3 view directives
 *   GP16 nacl_pbc_supercell: default_layout == "tabs"
 *   GP17 graphite_events_live: parses without error
 *   GP18 graphite_events_live: default_layout == "detached"
 *   GP19 graphite_events_live: live == true
 *   GP20 graphite_events_live: 5 view directives
 *   GP21 graphite_events_live: overlay.cycle options_raw contains "sequence"
 */

#include "vsim/vsim_parser.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

using namespace vsim;

// -- Test infrastructure ------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

static void PASS(const char* name) { std::printf("  [PASS] %s\n", name); ++g_pass; }
static void FAIL(const char* name, const std::string& detail = "") {
	std::printf("  [FAIL] %s  %s\n", name, detail.c_str()); ++g_fail;
}

template<typename T>
static void CHECK_EQ(const char* test, const T& actual, const T& expected) {
	if (actual == expected) PASS(test);
	else FAIL(test, "(got \"" + std::to_string(actual) + "\")");
}

static void CHECK_STR(const char* test, const std::string& actual, const std::string& expected) {
	if (actual == expected) PASS(test);
	else FAIL(test, "(got \"" + actual + "\", expected \"" + expected + "\")");
}

static void CHECK_TRUE(const char* test, bool cond, const std::string& detail = "") {
	if (cond) PASS(test); else FAIL(test, detail);
}

static VsimDocument parse(const char* src) {
	return VsimParser::parse_string(src);
}

// -- Scripts ------------------------------------------------------------------

static const char* k_calibration_htgr = R"vsim(
[project]
name      = "calibration_helium_htgr"
seed_base = 3008

[material]
formula = "He"

[environment]
temperature = 1473.0
medium      = "helium_gas"

[run]
mode      = "relax"
max_steps = 500

[observe]
metrics       = ["energy_map", "regime_transition", "loop_boundary", "severity"]
every_n_steps = 1

[visual.workspace]
enabled = true

show "calibration.helix"    target = "run.history"
show "data.events.timeline" target = "kernel.events"
show "data.scalar.panel"    target = "run.summary"
)vsim";

static const char* k_room_reactor = R"vsim(
[project]
name = "room_reactor"

[material]
formula = "He"

[environment]
temperature = 600.0
medium      = "helium_gas"

[room]
preset          = "reactor"
n_steps         = 200
record_interval = 50

[visual.workspace]
enabled = true

show "room.heatfield"    target = "room.solver" options = { "subsample": 80000 }
show "data.scalar.panel" target = "room.solver"
)vsim";

static const char* k_helium_reactor = R"vsim(
[project]
name        = "gallery_helium_reactor"
seed_base   = 6001

[material]
formula = "He"
phase   = "gas"

[environment]
temperature = 873.0
medium      = "helium_gas"

[room]
preset          = "reactor"
n_steps         = 200
record_interval = 50

[run]
mode      = "relax"
max_steps = 500

[visual.workspace]
enabled        = true
default_layout = "tabs"
auto_open_tree = true

show "room.heatfield" target = "room.solver" options = {
	"colormap": "blue_white_red",
	"subsample": 80000,
	"show_obstacles": true,
	"show_bounding_box": true
}

show "calibration.helix" target = "run.history" options = {
	"turns": 3,
	"radius": 4.0,
	"pitch": 6.0,
	"severity_scale": 3.0
}

show "data.rdf"          target = "analysis.rdf"
show "data.scalar.panel" target = "room.solver"
)vsim";

static const char* k_nacl_pbc = R"vsim(
[project]
name = "gallery_nacl_pbc_supercell"
seed_base = 1011

[material]
formula   = "NaCl"
structure = "rocksalt"

[visual.workspace]
enabled        = true
default_layout = "tabs"
auto_open_tree = true

show "scene.pbc_supercell" target = "material.0" options = {
	"extent": [3, 3, 3],
	"show_central_box": true,
	"image_alpha": 0.55
}

show "data.history.timeseries" target = "run.history" options = {
	"series": ["energy.total", "rms_force"],
	"log_y": false
}

show "data.scalar.panel" target = "run.summary"
)vsim";

static const char* k_graphite = R"vsim(
[project]
name      = "gallery_graphite_events_live"
seed_base = 3008

[material]
formula   = "C"
structure = "graphite"
phase     = "solid"

[visual.workspace]
enabled        = true
default_layout = "detached"
auto_open_tree = true
live           = true

show "scene.cg_bead" target = "run.cg_state"

show "overlay.cycle" target = "run.history" options = {
	"sequence": ["density", "coordination", "memory", "orient_order"],
	"hold_s": 2.5,
	"auto_orbit": true
}

show "data.events.timeline"  target = "kernel.events"
show "data.events.bar_chart" target = "kernel.events"

show "live.runtime" target = "run.history" options = {
	"tail_length": 30,
	"metrics": ["energy.total", "rms_force"]
}
)vsim";

// -- Main ---------------------------------------------------------------------

int main() {
	std::printf("\nGallery Parse Smoke Test\n");
	std::printf("========================\n\n");

	// -------------------------------------------------------------------------
	// calibration_htgr
	// -------------------------------------------------------------------------
	{
		VsimDocument doc;
		bool ok = true;
		try { doc = parse(k_calibration_htgr); }
		catch (const std::exception& e) { ok = false; FAIL("GP1", e.what()); }
		if (ok) PASS("GP1  calibration_htgr: parses without error");
		CHECK_TRUE("GP2  calibration_htgr: visual_workspace.enabled",  doc.visual_workspace.enabled);
		CHECK_EQ(  "GP3  calibration_htgr: 3 view directives",         (int)doc.view_directives.size(), 3);
		if (!doc.view_directives.empty())
			CHECK_STR("GP4  calibration_htgr: first kind",             doc.view_directives[0].kind, "calibration.helix");
	}

	// -------------------------------------------------------------------------
	// room_reactor
	// -------------------------------------------------------------------------
	{
		VsimDocument doc;
		bool ok = true;
		try { doc = parse(k_room_reactor); }
		catch (const std::exception& e) { ok = false; FAIL("GP5", e.what()); }
		if (ok) PASS("GP5  room_reactor: parses without error");
		CHECK_STR("GP6  room_reactor: room.preset",                    doc.room.preset, "reactor");
		if (!doc.view_directives.empty())
			CHECK_STR("GP7  room_reactor: first kind",                 doc.view_directives[0].kind, "room.heatfield");
		bool has_sub = !doc.view_directives.empty() &&
					   doc.view_directives[0].options_raw.find("subsample") != std::string::npos;
		CHECK_TRUE("GP8  room_reactor: options_raw contains \"subsample\"", has_sub);
	}

	// -------------------------------------------------------------------------
	// helium_reactor
	// -------------------------------------------------------------------------
	{
		VsimDocument doc;
		bool ok = true;
		try { doc = parse(k_helium_reactor); }
		catch (const std::exception& e) { ok = false; FAIL("GP9", e.what()); }
		if (ok) PASS("GP9  helium_reactor: parses without error");
		CHECK_STR("GP10 helium_reactor: room.preset",                  doc.room.preset, "reactor");
		CHECK_EQ( "GP11 helium_reactor: 4 view directives",            (int)doc.view_directives.size(), 4);
		if (doc.view_directives.size() >= 2) {
			CHECK_STR("GP12a helium_reactor: kinds[0] == room.heatfield",
					  doc.view_directives[0].kind, "room.heatfield");
			CHECK_STR("GP12b helium_reactor: kinds[1] == calibration.helix",
					  doc.view_directives[1].kind, "calibration.helix");
		}
		bool has_sub = !doc.view_directives.empty() &&
					   doc.view_directives[0].options_raw.find("subsample") != std::string::npos;
		CHECK_TRUE("GP13 helium_reactor: heatfield options has \"subsample\"", has_sub);
	}

	// -------------------------------------------------------------------------
	// nacl_pbc_supercell
	// -------------------------------------------------------------------------
	{
		VsimDocument doc;
		bool ok = true;
		try { doc = parse(k_nacl_pbc); }
		catch (const std::exception& e) { ok = false; FAIL("GP14", e.what()); }
		if (ok) PASS("GP14 nacl_pbc_supercell: parses without error");
		CHECK_EQ( "GP15 nacl_pbc_supercell: 3 view directives",        (int)doc.view_directives.size(), 3);
		CHECK_STR("GP16 nacl_pbc_supercell: default_layout == tabs",   doc.visual_workspace.default_layout, "tabs");
	}

	// -------------------------------------------------------------------------
	// graphite_events_live
	// -------------------------------------------------------------------------
	{
		VsimDocument doc;
		bool ok = true;
		try { doc = parse(k_graphite); }
		catch (const std::exception& e) { ok = false; FAIL("GP17", e.what()); }
		if (ok) PASS("GP17 graphite_events_live: parses without error");
		CHECK_STR("GP18 graphite_events_live: default_layout == detached",
				  doc.visual_workspace.default_layout, "detached");
		CHECK_TRUE("GP19 graphite_events_live: live == true",          doc.visual_workspace.live);
		CHECK_EQ( "GP20 graphite_events_live: 5 view directives",      (int)doc.view_directives.size(), 5);
		// overlay.cycle is index 1 (after scene.cg_bead)
		bool has_seq = doc.view_directives.size() > 1 &&
					   doc.view_directives[1].options_raw.find("sequence") != std::string::npos;
		CHECK_TRUE("GP21 graphite_events_live: overlay.cycle options has \"sequence\"", has_seq);
	}

	// -------------------------------------------------------------------------
	std::printf("\n  Results: %d passed, %d failed\n\n", g_pass, g_fail);
	return (g_fail > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
