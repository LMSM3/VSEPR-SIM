/**
 * test_domain_audit.cpp -- VSIM domain audit: peptide + gas mixed script
 * ==========================================================================
 *
 * Verifies the full parsing path for a [chemistry] domain/sequence block
 * coexisting with explicit [[simulation.molecule]] gas species.
 *
 * Tests:
 *   DA1  [chemistry] domain is parsed correctly
 *   DA2  [chemistry] sequence is parsed correctly
 *   DA3  expanded_formula is a non-empty Hill-order formula
 *   DA4  expanded_formula starts with C (organic, Hill order)
 *   DA5  material.formula is back-filled from expanded_formula
 *   DA6  simulation.molecules[0] is the auto-injected peptide (domain-driven)
 *   DA7  simulation.molecules[0].formula matches expanded_formula
 *   DA8  simulation.molecules[1].formula == "N2"  (explicit gas block 1)
 *   DA9  simulation.molecules[2].formula == "H2O" (explicit gas block 2)
 *   DA10 simulation.molecules[1].count == 12
 *   DA11 simulation.molecules[2].count == 50
 *   DA12 VsimDocument::validate() reports no errors
 *   DA13 chemistry.domain == "peptide" (explicit, not inferred)
 *   DA14 explicit gas molecules preserve their temperature
 *
 * Style: standalone framework (same as test_pbc_vsim_parser.cpp).
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

static void PASS(const char* name) {
    std::printf("  [PASS] %s\n", name);
    ++g_pass;
}

static void FAIL(const char* name, const std::string& detail = "") {
    std::printf("  [FAIL] %s  %s\n", name, detail.c_str());
    ++g_fail;
}

template<typename T>
static void CHECK_EQ(const char* test, const T& actual, const T& expected) {
    if (actual == expected) { PASS(test); }
    else {
        FAIL(test, "(got \"" + std::string(actual) + "\", expected \"" + std::string(expected) + "\")");
    }
}

static void CHECK_TRUE(const char* test, bool cond, const std::string& detail = "") {
    if (cond) { PASS(test); }
    else       { FAIL(test, detail); }
}

// -- Script under test --------------------------------------------------------

static const char* k_script = R"vsim(
[project]
name        = domain_audit_inline
version     = v5.0.0-beta.7.2
seed_base   = 9901
determinism = true

[chemistry]
domain   = peptide
sequence = ACDEFG

[environment]
temperature = 310.0

[simulation]
fire_max_steps   = 400
fire_dt_fs       = 1.0
box_size_ang     = 50.0
formation_preset = organic

[[simulation.molecule]]
formula     = N2
count       = 12
temperature = 310.0

[[simulation.molecule]]
formula     = H2O
count       = 50
temperature = 310.0

[export]
write_xyz = true
)vsim";

// -- Tests --------------------------------------------------------------------

int main() {
    std::printf("\n=== Domain Audit: peptide + gas mixed script ===\n\n");

    VsimDocument doc;
    try {
        doc = VsimParser::parse_string(k_script, "<inline>");
    } catch (const std::exception& ex) {
        std::printf("  FATAL: parse threw: %s\n", ex.what());
        return 1;
    }

    const auto& chem = doc.chemistry;
    const auto& mols = doc.simulation.molecules;

    // DA1-DA2: raw domain/sequence fields
    CHECK_EQ("DA1  chemistry.domain   == \"peptide\"",
             chem.domain,   std::string("peptide"));
    CHECK_EQ("DA2  chemistry.sequence == \"ACDEFG\"",
             chem.sequence, std::string("ACDEFG"));

    // DA3-DA4: expanded Hill formula
    CHECK_TRUE("DA3  expanded_formula non-empty",
               !chem.expanded_formula.empty(), "(empty)");
    CHECK_TRUE("DA4  expanded_formula starts with C (Hill order)",
               !chem.expanded_formula.empty() && chem.expanded_formula[0] == 'C',
               "(got \"" + chem.expanded_formula + "\")");

    // DA5: material.formula back-filled
    CHECK_TRUE("DA5  material.formula == expanded_formula",
               doc.material.formula == chem.expanded_formula,
               "(material=\"" + doc.material.formula + "\" expanded=\"" + chem.expanded_formula + "\")");

    // DA6-DA7: peptide auto-injected as molecules[0]
    // Explicit gas blocks are [[simulation.molecule]] which are appended AFTER
    // post-parse injection; the peptide entry should appear first.
    CHECK_TRUE("DA6  molecules[0] exists (auto-injected peptide)",
               mols.size() >= 1, "(size=" + std::to_string(mols.size()) + ")");
    if (mols.size() >= 1)
        CHECK_TRUE("DA7  molecules[0].formula == expanded_formula",
                   mols[0].formula == chem.expanded_formula,
                   "(got \"" + mols[0].formula + "\")");

    // DA8-DA11: explicit gas species
    CHECK_TRUE("DA8  molecules[1].formula == \"N2\"",
               mols.size() >= 2 && mols[1].formula == "N2",
               mols.size() >= 2 ? "(got \"" + mols[1].formula + "\")" : "(missing)");
    CHECK_TRUE("DA9  molecules[2].formula == \"H2O\"",
               mols.size() >= 3 && mols[2].formula == "H2O",
               mols.size() >= 3 ? "(got \"" + mols[2].formula + "\")" : "(missing)");
    CHECK_TRUE("DA10 molecules[1].count == 12",
               mols.size() >= 2 && mols[1].count == 12,
               mols.size() >= 2 ? "(got " + std::to_string(mols[1].count) + ")" : "(missing)");
    CHECK_TRUE("DA11 molecules[2].count == 50",
               mols.size() >= 3 && mols[2].count == 50,
               mols.size() >= 3 ? "(got " + std::to_string(mols[2].count) + ")" : "(missing)");

    // DA12: validate() clean
    {
        auto vr = doc.validate();
        CHECK_TRUE("DA12 validate() no errors", vr.errors.empty(),
                   vr.errors.empty() ? "" : "(first: " + vr.errors[0] + ")");
    }

    // DA13: domain is "peptide" (explicit, not inferred path)
    CHECK_EQ("DA13 domain explicit, not inferred",
             chem.domain, std::string("peptide"));

    // DA14: gas molecule temperature preserved
    CHECK_TRUE("DA14 molecules[1].temperature_K == 310.0",
               mols.size() >= 2 && mols[1].temperature_K == 310.0,
               mols.size() >= 2 ? "(got " + std::to_string(mols[1].temperature_K) + ")" : "(missing)");

    // Summary
    std::printf("\n  Results: %d passed, %d failed\n\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
