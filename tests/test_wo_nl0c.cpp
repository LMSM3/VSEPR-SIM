// =============================================================================
// tests/test_wo_nl0c.cpp   WO-NL0C  (Group 84)
// =============================================================================
// NL0C-01  script_type = "newleaf_type3_replay"  ->  is_replay() == true
// NL0C-02  default script_type ("standard")       ->  is_replay() == false
// NL0C-03  [visual] replay_source / replay_format parsed correctly
// NL0C-04  replay_format defaults to "auto"
// NL0C-05  [visual] gl_spin parsed; forces gl_auto_orbit = false
// NL0C-06  gl_spin_axis and gl_spin_deg_per_s parsed correctly
// NL0C-07  gl_spin default = false, orbit default = true
// NL0C-08  [open] replay_path / source_format parsed correctly
// NL0C-09  has_replay_source() / effective_source() logic
// NL0C-10  Full replay script round-trip  (reference template from WO-NL0C)
// =============================================================================

#include <cassert>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_parser.hpp"

using namespace vsim;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static VsimDocument parse(const char* script) {
    return VsimParser::parse_string(script, "<test>");
}

// ---------------------------------------------------------------------------

static void NL0C_01() {
    auto doc = parse(
        "[project]\n"
        "name        = \"demo_replay\"\n"
        "script_type = \"newleaf_type3_replay\"\n"
    );
    assert(doc.project.is_replay());
    assert(!doc.project.is_interactive());
    std::puts("  NL0C-01 PASS  script_type=newleaf_type3_replay -> is_replay()=true");
}

static void NL0C_02() {
    auto doc = parse(
        "[project]\n"
        "name = \"standard_run\"\n"
    );
    assert(!doc.project.is_replay());
    assert(doc.project.script_type == "standard");
    std::puts("  NL0C-02 PASS  default script_type -> is_replay()=false");
}

static void NL0C_03() {
    auto doc = parse(
        "[visual]\n"
        "output_type   = \"gl_live_60fps\"\n"
        "replay_source = \"out/nacl/nacl.xyzf\"\n"
        "replay_format = \"xyzf\"\n"
    );
    assert(doc.visual.replay_source == "out/nacl/nacl.xyzf");
    assert(doc.visual.replay_format == "xyzf");
    assert(doc.visual.is_replay_mode());
    std::puts("  NL0C-03 PASS  [visual] replay_source/replay_format parsed");
}

static void NL0C_04() {
    VisualSection vis;
    assert(vis.replay_format == "auto");
    assert(!vis.is_replay_mode());    // empty replay_source -> not replay mode
    std::puts("  NL0C-04 PASS  replay_format default=\"auto\", is_replay_mode()=false");
}

static void NL0C_05() {
    auto doc = parse(
        "[visual]\n"
        "output_type  = \"gl_live_60fps\"\n"
        "gl_auto_orbit = true\n"
        "gl_spin       = true\n"
    );
    // gl_spin = true must clear gl_auto_orbit at parse time
    assert(doc.visual.gl_spin == true);
    assert(doc.visual.gl_auto_orbit == false);
    std::puts("  NL0C-05 PASS  gl_spin=true forces gl_auto_orbit=false");
}

static void NL0C_06() {
    auto doc = parse(
        "[visual]\n"
        "gl_spin            = true\n"
        "gl_spin_axis       = \"z\"\n"
        "gl_spin_deg_per_s  = -90.0\n"
    );
    assert(doc.visual.gl_spin == true);
    assert(doc.visual.gl_spin_axis == "z");
    assert(doc.visual.gl_spin_deg_per_s < -89.0f && doc.visual.gl_spin_deg_per_s > -91.0f);
    std::puts("  NL0C-06 PASS  gl_spin_axis=\"z\" gl_spin_deg_per_s=-90.0 parsed");
}

static void NL0C_07() {
    VisualSection vis;
    assert(vis.gl_spin == false);
    assert(vis.gl_auto_orbit == true);
    assert(vis.gl_spin_axis == "y");
    assert(vis.gl_spin_deg_per_s > 29.0f && vis.gl_spin_deg_per_s < 31.0f);
    std::puts("  NL0C-07 PASS  gl_spin default=false, gl_auto_orbit default=true, axis=\"y\", rate=30");
}

static void NL0C_08() {
    auto doc = parse(
        "[open]\n"
        "enabled       = true\n"
        "replay_path   = \"out/nacl/nacl.xyzf\"\n"
        "source_format = \"xyzf\"\n"
    );
    assert(doc.open.enabled == true);
    assert(doc.open.replay_path   == "out/nacl/nacl.xyzf");
    assert(doc.open.source_format == "xyzf");
    std::puts("  NL0C-08 PASS  [open] replay_path / source_format parsed");
}

static void NL0C_09() {
    // Case A: only replay_path set -> effective_source = replay_path
    {
        OpenSection s;
        s.replay_path = "out/run.xyzf";
        assert(s.has_replay_source());
        assert(s.effective_source() == "out/run.xyzf");
    }
    // Case B: only file set -> effective_source = file
    {
        OpenSection s;
        s.file = "out/run.xyz";
        assert(!s.has_replay_source());
        assert(s.effective_source() == "out/run.xyz");
    }
    // Case C: both set -> replay_path wins
    {
        OpenSection s;
        s.file        = "out/run.xyz";
        s.replay_path = "out/run.dynx";
        assert(s.has_replay_source());
        assert(s.effective_source() == "out/run.dynx");
    }
    std::puts("  NL0C-09 PASS  has_replay_source() / effective_source() logic correct");
}

static void NL0C_10() {
    // Full replay script round-trip using the WO-NL0C reference template
    const char* script =
        "[project]\n"
        "name        = \"demo_replay\"\n"
        "version     = \"v5.0.0\"\n"
        "script_type = \"newleaf_type3_replay\"\n"
        "\n"
        "[open]\n"
        "enabled       = true\n"
        "mode          = \"advanced\"\n"
        "replay_path   = \"out/nacl_crystal/nacl_crystal.xyzf\"\n"
        "source_format = \"xyzf\"\n"
        "\n"
        "[visual]\n"
        "output_type       = \"gl_live_60fps\"\n"
        "replay_source     = \"out/nacl_crystal/nacl_crystal.xyzf\"\n"
        "replay_format     = \"xyzf\"\n"
        "gl_spin           = true\n"
        "gl_spin_axis      = \"y\"\n"
        "gl_spin_deg_per_s = 45.0\n"
        "gl_show_axes      = true\n"
        "gl_window_width   = 1280\n"
        "gl_window_height  = 800\n";

    auto doc = parse(script);

    // Project
    assert(doc.project.is_replay());
    assert(doc.project.name    == "demo_replay");
    assert(doc.project.version == "v5.0.0");

    // Open
    assert(doc.open.enabled);
    assert(doc.open.mode          == "advanced");
    assert(doc.open.replay_path   == "out/nacl_crystal/nacl_crystal.xyzf");
    assert(doc.open.source_format == "xyzf");
    assert(doc.open.has_replay_source());
    assert(doc.open.effective_source() == "out/nacl_crystal/nacl_crystal.xyzf");

    // Visual
    assert(doc.visual.output_type      == "gl_live_60fps");
    assert(doc.visual.replay_source    == "out/nacl_crystal/nacl_crystal.xyzf");
    assert(doc.visual.replay_format    == "xyzf");
    assert(doc.visual.gl_spin          == true);
    assert(doc.visual.gl_auto_orbit    == false);  // cleared by gl_spin
    assert(doc.visual.gl_spin_axis     == "y");
    assert(doc.visual.gl_spin_deg_per_s > 44.0f && doc.visual.gl_spin_deg_per_s < 46.0f);
    assert(doc.visual.gl_show_axes     == true);
    assert(doc.visual.gl_window_width  == 1280);
    assert(doc.visual.gl_window_height == 800);
    assert(doc.visual.is_replay_mode());
    assert(doc.visual.is_gl_mode());

    std::puts("  NL0C-10 PASS  full replay script round-trip (reference template)");
}

// ---------------------------------------------------------------------------

int main() {
    std::puts("=== WO-NL0C  Type-3 Demo Replay  (Group 84) ===");
    NL0C_01();
    NL0C_02();
    NL0C_03();
    NL0C_04();
    NL0C_05();
    NL0C_06();
    NL0C_07();
    NL0C_08();
    NL0C_09();
    NL0C_10();
    std::puts("=== ALL PASS ===");
    return 0;
}