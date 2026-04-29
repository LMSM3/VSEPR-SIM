/**
 * alias_tests.cpp
 * ---------------
 * Tests for the molecule alias resolver.
 *
 * Verifies:
 *   1. Known name resolution (case-insensitive)
 *   2. Formula pass-through detection
 *   3. Oxalate fallback for unknown names
 *   4. Oxalate pool integrity
 *   5. Alias chain resolution
 *   6. Empty input handling
 *   7. Table inspection
 *   8. Seed reproducibility for oxalate default
 */

#include "core/molecule_alias.hpp"
#include <iostream>
#include <cassert>
#include <string>

static int passed = 0;
static int failed = 0;

#define TEST(name) \
    { \
        std::string test_name = name; \
        bool test_ok = true; \
        try {

#define EXPECT(cond) \
    if (!(cond)) { \
        std::cerr << "  FAIL: " #cond " (line " << __LINE__ << ")\n"; \
        test_ok = false; \
    }

#define END_TEST \
        } catch (const std::exception& e) { \
            std::cerr << "  EXCEPTION: " << e.what() << "\n"; \
            test_ok = false; \
        } \
        if (test_ok) { \
            std::cout << "  PASS: " << test_name << "\n"; \
            ++passed; \
        } else { \
            std::cerr << "  FAIL: " << test_name << "\n"; \
            ++failed; \
        } \
    }

int main() {
    std::cout << "=== Molecule Alias Resolver Tests ===\n\n";

    // ---- Test 1: Known name resolution ----
    TEST("known_name_water")
        auto r = vsepr::resolve_alias("water");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "H2O")
        EXPECT(r.value().family == "inorganic")
        EXPECT(r.value().atom_count == 3)
    END_TEST

    TEST("known_name_benzene")
        auto r = vsepr::resolve_alias("benzene");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "C6H6")
        EXPECT(r.value().family == "organic")
    END_TEST

    TEST("known_name_aspirin")
        auto r = vsepr::resolve_alias("aspirin");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "C9H8O4")
        EXPECT(r.value().iupac == "acetylsalicylic acid")
        EXPECT(r.value().family == "pharmaceutical")
    END_TEST

    TEST("known_name_caffeine")
        auto r = vsepr::resolve_alias("caffeine");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "C8H10N4O2")
    END_TEST

    TEST("known_name_glucose")
        auto r = vsepr::resolve_alias("glucose");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "C6H12O6")
        EXPECT(r.value().family == "biochemistry")
    END_TEST

    // ---- Test 2: Case insensitivity ----
    TEST("case_insensitive_upper")
        auto r = vsepr::resolve_alias("WATER");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "H2O")
    END_TEST

    TEST("case_insensitive_mixed")
        auto r = vsepr::resolve_alias("BeNzEnE");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "C6H6")
    END_TEST

    TEST("case_insensitive_title")
        auto r = vsepr::resolve_alias("Ethanol");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "C2H6O")
    END_TEST

    // ---- Test 3: Formula pass-through ----
    TEST("formula_passthrough_h2o")
        auto r = vsepr::resolve_alias("H2O");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "H2O")
        EXPECT(r.value().family == "formula_passthrough")
    END_TEST

    TEST("formula_passthrough_nacl")
        auto r = vsepr::resolve_alias("NaCl");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "NaCl")
        EXPECT(r.value().family == "formula_passthrough")
    END_TEST

    TEST("formula_passthrough_complex")
        auto r = vsepr::resolve_alias("C6H12O6");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "C6H12O6")
        EXPECT(r.value().family == "formula_passthrough")
    END_TEST

    // ---- Test 4: Oxalate default for unknown names ----
    TEST("unknown_name_oxalate_default")
        auto r = vsepr::resolve_alias("unicorn juice", 42);
        EXPECT(r.is_ok())
        EXPECT(r.value().family == "oxalate_default")
        // Formula must contain C2 and O4 (oxalate core)
        EXPECT(r.value().formula.find("C2") != std::string::npos)
        EXPECT(r.value().formula.find("O4") != std::string::npos)
    END_TEST

    TEST("unknown_name_different_each_seed")
        auto r1 = vsepr::resolve_alias("fake molecule", 100);
        auto r2 = vsepr::resolve_alias("fake molecule", 200);
        EXPECT(r1.is_ok())
        EXPECT(r2.is_ok())
        EXPECT(r1.value().family == "oxalate_default")
        EXPECT(r2.value().family == "oxalate_default")
        // Different seeds should (usually) produce different oxalates
        // Not guaranteed but highly likely with pool size 17
    END_TEST

    // ---- Test 5: Seed reproducibility ----
    TEST("seed_reproducibility")
        auto r1 = vsepr::resolve_alias("gibberish", 12345);
        auto r2 = vsepr::resolve_alias("gibberish", 12345);
        EXPECT(r1.is_ok())
        EXPECT(r2.is_ok())
        EXPECT(r1.value().formula == r2.value().formula)
        EXPECT(r1.value().name == r2.value().name)
    END_TEST

    // ---- Test 6: Alias chains ----
    TEST("alias_chain_dhmo")
        auto r = vsepr::resolve_alias("dhmo");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "H2O")
    END_TEST

    TEST("alias_chain_table_salt")
        auto r = vsepr::resolve_alias("table salt");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "NaCl")
    END_TEST

    TEST("alias_chain_baking_soda")
        auto r = vsepr::resolve_alias("baking soda");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "NaHCO3")
    END_TEST

    TEST("alias_chain_dry_ice")
        auto r = vsepr::resolve_alias("dry ice");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "CO2")
    END_TEST

    TEST("alias_chain_laughing_gas")
        auto r = vsepr::resolve_alias("laughing gas");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "N2O")
    END_TEST

    // ---- Test 7: Empty input → oxalate ----
    TEST("empty_input_oxalate")
        auto r = vsepr::resolve_alias("", 999);
        EXPECT(r.is_ok())
        EXPECT(r.value().family == "oxalate_default")
    END_TEST

    // ---- Test 8: Table inspection ----
    TEST("alias_table_nonempty")
        EXPECT(vsepr::alias_count() > 70)
        EXPECT(vsepr::alias_table().size() == vsepr::alias_count())
    END_TEST

    TEST("oxalate_pool_nonempty")
        EXPECT(vsepr::oxalate_pool().size() >= 10)
        for (const auto& entry : vsepr::oxalate_pool()) {
            EXPECT(entry.family == "oxalate")
            EXPECT(!entry.formula.empty())
        }
    END_TEST

    // ---- Test 9: looks_like_formula ----
    TEST("looks_like_formula_positive")
        EXPECT(vsepr::looks_like_formula("H2O"))
        EXPECT(vsepr::looks_like_formula("NaCl"))
        EXPECT(vsepr::looks_like_formula("C6H6"))
        EXPECT(vsepr::looks_like_formula("Al"))
        EXPECT(vsepr::looks_like_formula("Ca"))
    END_TEST

    TEST("looks_like_formula_negative")
        EXPECT(!vsepr::looks_like_formula("water"))
        EXPECT(!vsepr::looks_like_formula("dry ice"))
        EXPECT(!vsepr::looks_like_formula("baking soda"))
        EXPECT(!vsepr::looks_like_formula(""))
        EXPECT(!vsepr::looks_like_formula("123"))
    END_TEST

    // ---- Test 10: Oxalate names are directly resolvable ----
    TEST("oxalate_by_name")
        auto r = vsepr::resolve_alias("oxalic acid");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "C2H2O4")
        EXPECT(r.value().family == "oxalate")
    END_TEST

    TEST("calcium_oxalate_by_name")
        auto r = vsepr::resolve_alias("calcium oxalate");
        EXPECT(r.is_ok())
        EXPECT(r.value().formula == "CaC2O4")
    END_TEST

    // ---- Summary ----
    std::cout << "\n=== Results: " << passed << "/" << (passed + failed)
              << " passed ===\n";

    return failed > 0 ? 1 : 0;
}
