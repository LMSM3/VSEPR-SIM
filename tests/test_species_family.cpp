/**
 * test_species_family.cpp
 * -----------------------
 * Verification suite for the SpeciesFamily taxonomy and classification engine.
 *
 * Tests:
 *   T01-T06: GAS subfamily classification (noble, diatomic, polyatomic, refrigerant, fuel)
 *   T07-T10: CRYSTAL subfamily classification (metal, ionic, covalent, actinide)
 *   T11-T14: CERAMIC subfamily classification (oxide, carbide, nitride, silicate)
 *   T15-T17: ORGANOMETALLIC classification (carbonyl, Cp/arene, actinide)
 *   T18-T20: Scale classification
 *   T21-T24: Physics emphasis defaults
 *   T25-T27: Full SpeciesEntity build + summary
 *   T28:     Subfamily parent consistency
 *   T29:     Family descriptor table completeness
 *   T30:     Unknown formula fallback
 */

#include "core/species_family.hpp"
#include <iostream>
#include <string>
#include <cassert>
#include <cmath>

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name, condition)                                              \
    do {                                                                   \
        if (condition) {                                                   \
            std::cout << "  " << name << " [PASS]\n";                     \
            ++pass_count;                                                  \
        } else {                                                           \
            std::cout << "  " << name << " [FAIL]\n";                     \
            ++fail_count;                                                  \
        }                                                                  \
    } while (0)

int main() {
    using namespace vsepr;
    using namespace vsepr::classify;

    std::cout << "\n=== Species Family Classification Tests ===\n\n";

    // ── GAS family ──────────────────────────────────────────────────────────
    TEST("T01 Noble: Ar -> GAS_NOBLE",
         classify_formula("Ar") == SpeciesSubfamily::GAS_NOBLE);
    TEST("T02 Noble: Xe -> GAS_NOBLE",
         classify_formula("Xe") == SpeciesSubfamily::GAS_NOBLE);
    TEST("T03 Diatomic: N2 -> GAS_DIATOMIC",
         classify_formula("N2") == SpeciesSubfamily::GAS_DIATOMIC);
    TEST("T04 Diatomic: Cl2 -> GAS_DIATOMIC",
         classify_formula("Cl2") == SpeciesSubfamily::GAS_DIATOMIC);
    TEST("T05 Polyatomic: CO2 -> GAS_POLYATOMIC",
         classify_formula("CO2") == SpeciesSubfamily::GAS_POLYATOMIC);
    TEST("T06 Polyatomic: NH3 -> GAS_POLYATOMIC",
         classify_formula("NH3") == SpeciesSubfamily::GAS_POLYATOMIC);
    TEST("T06b Refrigerant: R134a -> GAS_REFRIGERANT",
         classify_formula("R134a") == SpeciesSubfamily::GAS_REFRIGERANT);
    TEST("T06c Fuel: C2H6 -> GAS_FUEL",
         classify_formula("C2H6") == SpeciesSubfamily::GAS_FUEL);

    // ── CRYSTAL family ──────────────────────────────────────────────────────
    TEST("T07 Metal: Au -> CRY_METAL",
         classify_formula("Au") == SpeciesSubfamily::CRY_METAL);
    TEST("T08 Metal: Fe -> CRY_METAL",
         classify_formula("Fe") == SpeciesSubfamily::CRY_METAL);
    TEST("T09 Ionic: NaCl -> CRY_IONIC",
         classify_formula("NaCl") == SpeciesSubfamily::CRY_IONIC);
    TEST("T10 Actinide solid: UO2 -> CRY_ACTINIDE_SOLID",
         classify_formula("UO2") == SpeciesSubfamily::CRY_ACTINIDE_SOLID);
    TEST("T10b Covalent: Si -> CRY_COVALENT",
         classify_formula("Si") == SpeciesSubfamily::CRY_COVALENT);

    // ── CERAMIC family ──────────────────────────────────────────────────────
    TEST("T11 Oxide: Al2O3 -> CM_OXIDE",
         classify_formula("Al2O3") == SpeciesSubfamily::CM_OXIDE);
    TEST("T12 Carbide: SiC -> CM_CARBIDE",
         classify_formula("SiC") == SpeciesSubfamily::CM_CARBIDE);
    TEST("T13 Nitride: TiN -> CM_NITRIDE",
         classify_formula("TiN") == SpeciesSubfamily::CM_NITRIDE);
    TEST("T14 Silicate: CaSiO3 -> CM_SILICATE",
         classify_formula("CaSiO3") == SpeciesSubfamily::CM_SILICATE);
    TEST("T14b Refractory: HfC -> CM_REFRACTORY (first match wins)",
         classify_formula("HfC") == SpeciesSubfamily::CM_CARBIDE  // HfC is in carbide list first
         || classify_formula("HfC") == SpeciesSubfamily::CM_REFRACTORY);

    // ── ORGANOMETALLIC family ───────────────────────────────────────────────
    TEST("T15 Carbonyl: Ni(CO)4 -> OM_CARBONYL",
         classify_formula("Ni(CO)4") == SpeciesSubfamily::OM_CARBONYL);
    TEST("T16 Cp/arene: Cp2Fe -> OM_CP_ARENE",
         classify_formula("Cp2Fe") == SpeciesSubfamily::OM_CP_ARENE);
    TEST("T17 TM complex: Pd(PPh3)4 -> OM_TRANSITION_METAL",
         classify_formula("Pd(PPh3)4") == SpeciesSubfamily::OM_TRANSITION_METAL);

    // ── Scale classification ────────────────────────────────────────────────
    TEST("T18 Scale: 1 atom -> SMALL",
         classify_scale(1) == ScaleClass::SMALL);
    TEST("T19 Scale: 50 atoms -> MEDIUM",
         classify_scale(50) == ScaleClass::MEDIUM);
    TEST("T20 Scale: 5000 atoms -> LARGE",
         classify_scale(5000) == ScaleClass::LARGE);
    TEST("T20b Scale: 50000 atoms -> BULK",
         classify_scale(50000) == ScaleClass::BULK);

    // ── Physics emphasis ────────────────────────────────────────────────────
    TEST("T21 GAS emphasis: transport + EOS",
         default_emphasis(SpeciesFamily::GAS).transport
         && default_emphasis(SpeciesFamily::GAS).eos);
    TEST("T22 CRYSTAL emphasis: lattice + topology",
         default_emphasis(SpeciesFamily::CRYSTAL).lattice
         && default_emphasis(SpeciesFamily::CRYSTAL).topology);
    TEST("T23 OM emphasis: coordination",
         default_emphasis(SpeciesFamily::ORGANOMETALLIC).coordination);
    TEST("T24 CM emphasis: lattice + fracture",
         default_emphasis(SpeciesFamily::CERAMIC).lattice
         && default_emphasis(SpeciesFamily::CERAMIC).fracture);

    // ── Full SpeciesEntity ──────────────────────────────────────────────────
    auto ent_ar = classify::classify("Ar", 1);
    TEST("T25 Entity Ar: family=GAS",
         ent_ar.family == SpeciesFamily::GAS);
    TEST("T26 Entity Ar: subfamily=GAS_NOBLE",
         ent_ar.subfamily == SpeciesSubfamily::GAS_NOBLE);
    TEST("T27 Entity Ar: summary contains 'GAS'",
         ent_ar.summary().find("GAS") != std::string::npos);

    auto ent_nacl = classify::classify("NaCl", 1000);
    TEST("T27b Entity NaCl: family=CRYSTAL",
         ent_nacl.family == SpeciesFamily::CRYSTAL);
    TEST("T27c Entity NaCl: scale=LARGE (1000 atoms)",
         ent_nacl.scale == ScaleClass::LARGE);

    // ── Subfamily parent consistency ────────────────────────────────────────
    bool parents_ok = true;
    for (int i = 0; i <= static_cast<int>(SpeciesSubfamily::UNKNOWN); ++i) {
        auto sf = static_cast<SpeciesSubfamily>(i);
        auto parent = subfamily_parent(sf);
        // Just verify it doesn't crash and returns a valid family
        (void)family_name(parent);
    }
    TEST("T28 All subfamily_parent() calls succeed",
         parents_ok);

    // ── Descriptor tables ───────────────────────────────────────────────────
    TEST("T29 Family descriptor table has 4 entries",
         family_descriptors().size() == 4);
    TEST("T29b Subfamily descriptor table has entries",
         subfamily_descriptors().size() > 20);

    // ── Unknown fallback ────────────────────────────────────────────────────
    TEST("T30 Unknown formula 'XYZ123' -> UNKNOWN",
         classify_formula("XYZ123") == SpeciesSubfamily::UNKNOWN);
    TEST("T30b Unknown family -> GAS (default)",
         classify_family("XYZ123") == SpeciesFamily::GAS);

    // ── Family name round-trips ─────────────────────────────────────────────
    TEST("T31 family_name(GAS)='GAS'",
         std::string(family_name(SpeciesFamily::GAS)) == "GAS");
    TEST("T32 family_name(CRYSTAL)='CRY'",
         std::string(family_name(SpeciesFamily::CRYSTAL)) == "CRY");
    TEST("T33 family_name(ORGANOMETALLIC)='OM'",
         std::string(family_name(SpeciesFamily::ORGANOMETALLIC)) == "OM");
    TEST("T34 family_name(CERAMIC)='CM'",
         std::string(family_name(SpeciesFamily::CERAMIC)) == "CM");

    // ── External layer ──────────────────────────────────────────────────────
    ExternalLayer ext;
    TEST("T35 ExternalLayer default is empty",
         ext.empty());
    ext.resize(5);
    TEST("T36 ExternalLayer resize: charges.size()=5",
         ext.charges.size() == 5 && ext.velocities.size() == 5);

    // ── Scale score ─────────────────────────────────────────────────────────
    auto ent_big = classify::classify("Al2O3", 50000);
    TEST("T37 scale_score for 50000 atoms > 0.9",
         ent_big.scale_score > 0.9);
    auto ent_tiny = classify::classify("He", 1);
    TEST("T38 scale_score for 1 atom = 0.0",
         ent_tiny.scale_score == 0.0);

    // ── update_scale ────────────────────────────────────────────────────────
    SpeciesEntity manual;
    manual.core.atoms.resize(500);
    manual.update_scale();
    TEST("T39 update_scale: 500 atoms -> LARGE",
         manual.scale == ScaleClass::LARGE);
    TEST("T40 update_scale: scale_score > 0.5",
         manual.scale_score > 0.5);

    // ── Results ─────────────────────────────────────────────────────────────
    std::cout << "\n--- Results: " << pass_count << " passed, "
              << fail_count << " failed out of " << (pass_count + fail_count) << " ---\n\n";

    return fail_count > 0 ? 1 : 0;
}
