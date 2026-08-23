// =============================================================================
// tests/test_wo_nl0a.cpp   WO-NL0A  (Group 85)
// =============================================================================
// NL0A-01  script_type = "newleaf_type1_discovery"  ->  is_discovery() == true
// NL0A-02  default script_type  ->  is_discovery() == false
// NL0A-03  [discovery] feasibility_filter parsed
// NL0A-04  [discovery] max_compounds parsed
// NL0A-05  [discovery] seed_formula parsed
// NL0A-06  [discovery] event_enumerate = false parsed
// NL0A-07  [discovery] output_xyzf / output_dynx / emit_mode parsed
// NL0A-08  [discovery] output_dir / display_mode parsed
// NL0A-09  DiscoverySection::is_active() / emits_dynx() helpers
// NL0A-10  Full discovery script round-trip (reference template)
// =============================================================================

#include <cassert>
#include <cstdio>
#include <string>
#include <sstream>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_parser.hpp"

using namespace vsim;

static VsimDocument parse(const char* script) {
    return VsimParser::parse_string(script, "<test>");
}

// ---------------------------------------------------------------------------

static void NL0A_01() {
    auto doc = parse(
        "[project]\n"
        "name        = \"rand_discovery\"\n"
        "script_type = \"newleaf_type1_discovery\"\n"
    );
    assert(doc.project.is_discovery());
    assert(!doc.project.is_replay());
    assert(!doc.project.is_type2());
    std::puts("  NL0A-01 PASS  script_type=newleaf_type1_discovery -> is_discovery()=true");
}

static void NL0A_02() {
    auto doc = parse(
        "[project]\n"
        "name = \"standard_run\"\n"
    );
    assert(!doc.project.is_discovery());
    std::puts("  NL0A-02 PASS  default script_type -> is_discovery()=false");
}

static void NL0A_03() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type1_discovery\"\n"
        "[discovery]\n"
        "feasibility_filter = \"stoich+charge+valence\"\n"
    );
    assert(doc.discovery.feasibility_filter == "stoich+charge+valence");
    std::puts("  NL0A-03 PASS  [discovery] feasibility_filter parsed");
}

static void NL0A_04() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type1_discovery\"\n"
        "[discovery]\n"
        "max_compounds = 500\n"
    );
    assert(doc.discovery.max_compounds == 500);
    std::puts("  NL0A-04 PASS  [discovery] max_compounds parsed");
}

static void NL0A_05() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type1_discovery\"\n"
        "[discovery]\n"
        "seed_formula = \"C6H6\"\n"
    );
    assert(doc.discovery.seed_formula == "C6H6");
    std::puts("  NL0A-05 PASS  [discovery] seed_formula parsed");
}

static void NL0A_06() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type1_discovery\"\n"
        "[discovery]\n"
        "event_enumerate = false\n"
    );
    assert(doc.discovery.event_enumerate == false);
    std::puts("  NL0A-06 PASS  [discovery] event_enumerate=false parsed");
}

static void NL0A_07() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type1_discovery\"\n"
        "[discovery]\n"
        "output_xyzf = true\n"
        "output_dynx = true\n"
        "emit_mode   = \"continuous\"\n"
    );
    assert(doc.discovery.output_xyzf == true);
    assert(doc.discovery.output_dynx == true);
    assert(doc.discovery.emit_mode   == "continuous");
    std::puts("  NL0A-07 PASS  [discovery] output_xyzf/output_dynx/emit_mode parsed");
}

static void NL0A_08() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type1_discovery\"\n"
        "[discovery]\n"
        "output_dir   = \"out/discovery_run\"\n"
        "display_mode = \"gl_live_60fps\"\n"
    );
    assert(doc.discovery.output_dir   == "out/discovery_run");
    assert(doc.discovery.display_mode == "gl_live_60fps");
    std::puts("  NL0A-08 PASS  [discovery] output_dir/display_mode parsed");
}

static void NL0A_09() {
    // Default DiscoverySection: max_compounds=0, seed_formula empty -> is_active()=false
    DiscoverySection d;
    assert(!d.is_active());
    assert(!d.emits_dynx());

    // max_compounds > 0 -> active
    d.max_compounds = 100;
    assert(d.is_active());

    // emit_mode != "off" -> emits_dynx
    d.emit_mode = "run";
    assert(d.emits_dynx());

    // output_dynx=true also triggers emits_dynx
    DiscoverySection d2;
    d2.output_dynx = true;
    assert(d2.emits_dynx());

    std::puts("  NL0A-09 PASS  is_active() / emits_dynx() helper logic correct");
}

static void NL0A_10() {
    // Full reference template from WO-NL0A specification
    auto doc = parse(
        "[project]\n"
        "name        = \"rand_discovery_run\"\n"
        "version     = \"v5.0.0\"\n"
        "script_type = \"newleaf_type1_discovery\"\n"
        "\n"
        "[discovery]\n"
        "feasibility_filter = \"stoich+charge\"\n"
        "max_compounds      = 0\n"
        "seed_formula       = \"\"\n"
        "event_enumerate    = true\n"
        "output_xyzf        = true\n"
        "output_dynx        = false\n"
        "emit_mode          = \"off\"\n"
        "output_dir         = \"out/discovery\"\n"
        "display_mode       = \"terminal_chart\"\n"
        "\n"
        "[visual]\n"
        "output_type = \"terminal_chart\"\n"
    );
    assert(doc.project.is_discovery());
    assert(doc.discovery.feasibility_filter == "stoich+charge");
    assert(doc.discovery.event_enumerate    == true);
    assert(doc.discovery.output_xyzf        == true);
    assert(doc.discovery.output_dir         == "out/discovery");
    assert(doc.discovery.display_mode       == "terminal_chart");
    assert(doc.visual.output_type           == "terminal_chart");
    std::puts("  NL0A-10 PASS  full discovery script round-trip (reference template)");
}

// ---------------------------------------------------------------------------

int main() {
    std::puts("=== WO-NL0A  Type-1 Random Discovery  (Group 85) ===");
    NL0A_01();
    NL0A_02();
    NL0A_03();
    NL0A_04();
    NL0A_05();
    NL0A_06();
    NL0A_07();
    NL0A_08();
    NL0A_09();
    NL0A_10();
    std::puts("=== ALL PASS ===");
    return 0;
}