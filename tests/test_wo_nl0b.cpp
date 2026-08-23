// =============================================================================
// tests/test_wo_nl0b.cpp   WO-NL0B  (Group 86)
// =============================================================================
// NL0B-01  script_type = "newleaf_type2_md"      ->  is_type2_md()=true
// NL0B-02  script_type = "newleaf_type2_crystal" ->  is_type2_crystal()=true
// NL0B-03  is_type2() is true for both type2 variants
// NL0B-04  [run] mode = "crystal_relax" parsed correctly
// NL0B-05  [run] mode = "md" round-trip (standard MD script)
// NL0B-06  [objects.crystal] lattice_type / species parsed
// NL0B-07  [objects.crystal] supercell nx/ny/nz parsed
// NL0B-08  [objects.crystal] relax / relax_max_steps parsed
// NL0B-09  [visual] gl_spin + gl_crystal_grid output_type parsed
// NL0B-10  Full Type 2 crystal script round-trip (reference template)
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

static void NL0B_01() {
    auto doc = parse(
        "[project]\n"
        "name        = \"h2o_md\"\n"
        "script_type = \"newleaf_type2_md\"\n"
    );
    assert(doc.project.is_type2_md());
    assert(!doc.project.is_type2_crystal());
    assert(doc.project.is_type2());
    std::puts("  NL0B-01 PASS  script_type=newleaf_type2_md -> is_type2_md()=true");
}

static void NL0B_02() {
    auto doc = parse(
        "[project]\n"
        "name        = \"nacl_crystal\"\n"
        "script_type = \"newleaf_type2_crystal\"\n"
    );
    assert(doc.project.is_type2_crystal());
    assert(!doc.project.is_type2_md());
    assert(doc.project.is_type2());
    std::puts("  NL0B-02 PASS  script_type=newleaf_type2_crystal -> is_type2_crystal()=true");
}

static void NL0B_03() {
    auto md  = parse("[project]\nscript_type = \"newleaf_type2_md\"\n");
    auto cry = parse("[project]\nscript_type = \"newleaf_type2_crystal\"\n");
    auto std = parse("[project]\nscript_type = \"standard\"\n");
    assert( md.project.is_type2());
    assert(cry.project.is_type2());
    assert(!std.project.is_type2());
    std::puts("  NL0B-03 PASS  is_type2() true for both type2 variants, false for standard");
}

static void NL0B_04() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type2_crystal\"\n"
        "[run]\n"
        "mode      = \"crystal_relax\"\n"
        "max_steps = 1000\n"
        "converge  = true\n"
    );
    assert(doc.run.mode      == "crystal_relax");
    assert(doc.run.max_steps == 1000);
    assert(doc.run.converge  == true);
    std::puts("  NL0B-04 PASS  [run] mode=crystal_relax parsed correctly");
}

static void NL0B_05() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type2_md\"\n"
        "[run]\n"
        "mode      = \"md\"\n"
        "max_steps = 5000\n"
        "dt_fs     = 0.5\n"
        "converge  = false\n"
    );
    assert(doc.run.mode      == "md");
    assert(doc.run.max_steps == 5000);
    assert(doc.run.dt_fs     == 0.5);
    assert(doc.run.converge  == false);
    std::puts("  NL0B-05 PASS  [run] mode=md round-trip (standard MD script)");
}

static void NL0B_06() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type2_crystal\"\n"
        "[objects.crystal]\n"
        "lattice = \"FCC\"\n"
        "species = [\"Ni\"]\n"
        "a       = 3.52\n"
    );
    assert(doc.crystal.lattice_type == vsim::LatticeType::FCC);
    assert(!doc.crystal.species.empty());
    assert(doc.crystal.species[0] == "Ni");
    assert(doc.crystal.a_A == 3.52);
    std::puts("  NL0B-06 PASS  [objects.crystal] lattice_type/species/a parsed");
}

static void NL0B_07() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type2_crystal\"\n"
        "[objects.crystal]\n"
        "lattice = \"BCC\"\n"
        "species = [\"Fe\"]\n"
        "a  = 2.87\n"
        "nx = 4\n"
        "ny = 4\n"
        "nz = 4\n"
    );
    assert(doc.crystal.nx == 4);
    assert(doc.crystal.ny == 4);
    assert(doc.crystal.nz == 4);
    std::puts("  NL0B-07 PASS  [objects.crystal] supercell nx/ny/nz parsed");
}

static void NL0B_08() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type2_crystal\"\n"
        "[objects.crystal]\n"
        "lattice         = \"FCC\"\n"
        "species         = [\"Cu\"]\n"
        "a               = 3.61\n"
        "relax           = true\n"
        "relax_max_steps = 300\n"
    );
    assert(doc.crystal.relax           == true);
    assert(doc.crystal.relax_max_steps == 300);
    std::puts("  NL0B-08 PASS  [objects.crystal] relax/relax_max_steps parsed");
}

static void NL0B_09() {
    auto doc = parse(
        "[project]\nscript_type = \"newleaf_type2_crystal\"\n"
        "[visual]\n"
        "output_type      = \"gl_crystal_grid\"\n"
        "gl_spin          = true\n"
        "gl_spin_axis     = \"y\"\n"
        "gl_spin_deg_per_s = 20.0\n"
    );
    assert(doc.visual.output_type       == "gl_crystal_grid");
    assert(doc.visual.gl_spin           == true);
    assert(doc.visual.gl_spin_axis      == "y");
    assert(doc.visual.gl_spin_deg_per_s == 20.0);
    assert(doc.visual.gl_auto_orbit     == false); // spin disables orbit
    std::puts("  NL0B-09 PASS  [visual] gl_spin + gl_crystal_grid output_type parsed");
}

static void NL0B_10() {
    // Full Type 2 crystal reference template (WO-NL0B specification)
    auto doc = parse(
        "[project]\n"
        "name        = \"nacl_crystal_relax\"\n"
        "version     = \"v5.0.0\"\n"
        "script_type = \"newleaf_type2_crystal\"\n"
        "seed_base   = 42\n"
        "\n"
        "[objects.crystal]\n"
        "lattice  = \"Rocksalt\"\n"
        "species  = [\"Na\", \"Cl\"]\n"
        "a        = 5.64\n"
        "nx = 2\n"
        "ny = 2\n"
        "nz = 2\n"
        "relax           = true\n"
        "relax_max_steps = 500\n"
        "relax_method    = \"fire\"\n"
        "\n"
        "[run]\n"
        "mode      = \"crystal_relax\"\n"
        "max_steps = 500\n"
        "converge  = true\n"
        "\n"
        "[export]\n"
        "write_xyz  = true\n"
        "output_dir = \"out/nacl_crystal\"\n"
        "\n"
        "[visual]\n"
        "output_type      = \"gl_crystal_grid\"\n"
        "gl_spin          = true\n"
        "gl_spin_deg_per_s = 25.0\n"
    );
    assert(doc.project.is_type2_crystal());
    assert(doc.project.seed_base        == 42);
    assert(doc.crystal.a_A              == 5.64);
    assert(doc.crystal.nx               == 2);
    assert(doc.crystal.relax            == true);
    assert(doc.crystal.relax_max_steps  == 500);
    assert(doc.run.mode                 == "crystal_relax");
    assert(doc.visual.output_type       == "gl_crystal_grid");
    assert(doc.visual.gl_spin           == true);
    assert(doc.visual.gl_spin_deg_per_s == 25.0);
    std::puts("  NL0B-10 PASS  full Type-2 crystal script round-trip (reference template)");
}

// ---------------------------------------------------------------------------

int main() {
    std::puts("=== WO-NL0B  Type-2 MD / Crystal Scripts  (Group 86) ===");
    NL0B_01();
    NL0B_02();
    NL0B_03();
    NL0B_04();
    NL0B_05();
    NL0B_06();
    NL0B_07();
    NL0B_08();
    NL0B_09();
    NL0B_10();
    std::puts("=== ALL PASS ===");
    return 0;
}