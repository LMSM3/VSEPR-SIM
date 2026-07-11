/**
 * molecule_alias.cpp
 * ------------------
 * Common-name → canonical-formula resolver implementation.
 *
 * Contains the master alias table (~80+ entries), oxalate fallback pool,
 * case-insensitive matching, and formula pass-through detection.
 */

#include "core/molecule_alias.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <random>

namespace vsepr {

// ============================================================================
// Internal: lowercase helper
// ============================================================================

static std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// ============================================================================
// Oxalate Fallback Pool
// ============================================================================

static const std::vector<AliasEntry>& build_oxalate_pool() {
    static const std::vector<AliasEntry> pool = {
        // Core oxalate ion and oxalic acid
        {"oxalic acid",         "C2H2O4",       "ethanedioic acid",             "oxalate", 8},
        {"sodium oxalate",      "Na2C2O4",      "disodium ethanedioate",        "oxalate", 8},
        {"potassium oxalate",   "K2C2O4",       "dipotassium ethanedioate",     "oxalate", 8},
        {"calcium oxalate",     "CaC2O4",       "calcium ethanedioate",         "oxalate", 7},
        {"iron oxalate",        "FeC2O4",       "iron(II) ethanedioate",        "oxalate", 7},
        {"ammonium oxalate",    "N2H8C2O4",     "diammonium ethanedioate",      "oxalate", 16},
        {"magnesium oxalate",   "MgC2O4",       "magnesium ethanedioate",       "oxalate", 7},
        {"barium oxalate",      "BaC2O4",       "barium ethanedioate",          "oxalate", 7},
        {"copper oxalate",      "CuC2O4",       "copper(II) ethanedioate",      "oxalate", 7},
        {"lithium oxalate",     "Li2C2O4",      "dilithium ethanedioate",       "oxalate", 8},
        {"zinc oxalate",        "ZnC2O4",       "zinc ethanedioate",            "oxalate", 7},
        {"silver oxalate",      "Ag2C2O4",      "disilver ethanedioate",        "oxalate", 8},
        {"manganese oxalate",   "MnC2O4",       "manganese(II) ethanedioate",   "oxalate", 7},
        {"nickel oxalate",      "NiC2O4",       "nickel(II) ethanedioate",      "oxalate", 7},
        {"cobalt oxalate",      "CoC2O4",       "cobalt(II) ethanedioate",      "oxalate", 7},
        {"lead oxalate",        "PbC2O4",       "lead(II) ethanedioate",        "oxalate", 7},
        {"strontium oxalate",   "SrC2O4",       "strontium ethanedioate",       "oxalate", 7},
    };
    return pool;
}

const std::vector<AliasEntry>& oxalate_pool() {
    return build_oxalate_pool();
}

AliasEntry random_oxalate(uint64_t seed) {
    const auto& pool = build_oxalate_pool();

    if (seed == 0) {
        seed = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
    AliasEntry entry = pool[dist(rng)];
    entry.family = "oxalate_default";  // Mark as fallback
    return entry;
}

// ============================================================================
// Master Alias Table
// ============================================================================

static const std::vector<AliasEntry>& build_alias_table() {
    static const std::vector<AliasEntry> table = {
        // --- Inorganic: Water and simple hydrides ---
        {"water",               "H2O",          "dihydrogen monoxide",          "inorganic", 3},
        {"heavy water",         "D2O",          "deuterium oxide",              "inorganic", 3},
        {"hydrogen peroxide",   "H2O2",         "dihydrogen dioxide",           "inorganic", 4},
        {"ammonia",             "NH3",          "azane",                        "inorganic", 4},
        {"hydrogen sulfide",    "H2S",          "dihydrogen sulfide",           "inorganic", 3},
        {"hydrogen fluoride",   "HF",           "hydrogen fluoride",            "inorganic", 2},
        {"hydrogen chloride",   "HCl",          "hydrogen chloride",            "inorganic", 2},
        {"hydrogen bromide",    "HBr",          "hydrogen bromide",             "inorganic", 2},
        {"hydrazine",           "N2H4",         "diazane",                      "inorganic", 6},

        // --- Inorganic: Oxides ---
        {"carbon dioxide",      "CO2",          "carbon dioxide",               "inorganic", 3},
        {"carbon monoxide",     "CO",           "carbon monoxide",              "inorganic", 2},
        {"nitric oxide",        "NO",           "nitrogen monoxide",            "inorganic", 2},
        {"nitrogen dioxide",    "NO2",          "nitrogen dioxide",             "inorganic", 3},
        {"nitrous oxide",       "N2O",          "dinitrogen monoxide",          "inorganic", 3},
        {"sulfur dioxide",      "SO2",          "sulfur dioxide",               "inorganic", 3},
        {"sulfur trioxide",     "SO3",          "sulfur trioxide",              "inorganic", 4},
        {"phosphorus pentoxide","P4O10",        "diphosphorus pentoxide",       "inorganic", 14},

        // --- Inorganic: Salts and ionic ---
        {"salt",                "NaCl",         "sodium chloride",              "inorganic", 2},
        {"sodium chloride",     "NaCl",         "sodium chloride",              "inorganic", 2},
        {"potassium chloride",  "KCl",          "potassium chloride",           "inorganic", 2},
        {"calcium fluoride",    "CaF2",         "calcium fluoride",             "inorganic", 3},
        {"magnesium oxide",     "MgO",          "magnesium oxide",              "inorganic", 2},
        {"calcium carbonate",   "CaCO3",        "calcium carbonate",            "inorganic", 5},
        {"sodium bicarbonate",  "NaHCO3",       "sodium hydrogen carbonate",    "inorganic", 6},
        {"calcium hydroxide",   "CaO2H2",      "calcium hydroxide",            "inorganic", 5},
        {"sodium hydroxide",    "NaOH",         "sodium hydroxide",             "inorganic", 3},
        {"potassium hydroxide", "KOH",          "potassium hydroxide",          "inorganic", 3},

        // --- Inorganic: Acids ---
        {"sulfuric acid",       "H2SO4",        "sulfuric acid",                "inorganic", 7},
        {"nitric acid",         "HNO3",         "nitric acid",                  "inorganic", 5},
        {"hydrochloric acid",   "HCl",          "hydrogen chloride",            "inorganic", 2},
        {"phosphoric acid",     "H3PO4",        "phosphoric acid",              "inorganic", 8},

        // --- Organic: Alkanes ---
        {"methane",             "CH4",          "methane",                      "organic", 5},
        {"ethane",              "C2H6",         "ethane",                       "organic", 8},
        {"propane",             "C3H8",         "propane",                      "organic", 11},
        {"butane",              "C4H10",        "butane",                       "organic", 14},
        {"pentane",             "C5H12",        "pentane",                      "organic", 17},
        {"hexane",              "C6H14",        "hexane",                       "organic", 20},
        {"octane",              "C8H18",        "octane",                       "organic", 26},

        // --- Organic: Alkenes/Alkynes ---
        {"ethylene",            "C2H4",         "ethene",                       "organic", 6},
        {"propylene",           "C3H6",         "propene",                      "organic", 9},
        {"acetylene",           "C2H2",         "ethyne",                       "organic", 4},

        // --- Organic: Aromatics ---
        {"benzene",             "C6H6",         "benzene",                      "organic", 12},
        {"toluene",             "C7H8",         "methylbenzene",                "organic", 15},
        {"naphthalene",         "C10H8",        "naphthalene",                  "organic", 18},
        {"phenol",              "C6H6O",        "hydroxybenzene",               "organic", 13},

        // --- Organic: Alcohols ---
        {"methanol",            "CH4O",         "methanol",                     "organic", 6},
        {"ethanol",             "C2H6O",        "ethanol",                      "organic", 9},
        {"propanol",            "C3H8O",        "propan-1-ol",                  "organic", 12},
        {"isopropanol",         "C3H8O",        "propan-2-ol",                  "organic", 12},
        {"glycerol",            "C3H8O3",       "propane-1,2,3-triol",          "organic", 14},

        // --- Organic: Acids ---
        {"formic acid",         "CH2O2",        "methanoic acid",               "organic", 5},
        {"acetic acid",         "C2H4O2",       "ethanoic acid",                "organic", 8},
        {"citric acid",         "C6H8O7",       "2-hydroxypropane-1,2,3-tricarboxylic acid", "organic", 21},

        // --- Organic: Aldehydes/Ketones ---
        {"formaldehyde",        "CH2O",         "methanal",                     "organic", 4},
        {"acetaldehyde",        "C2H4O",        "ethanal",                      "organic", 7},
        {"acetone",             "C3H6O",        "propan-2-one",                 "organic", 10},

        // --- Organic: Esters ---
        {"ethyl acetate",       "C4H8O2",       "ethyl ethanoate",              "organic", 14},

        // --- Organic: Amines ---
        {"methylamine",         "CH5N",         "methanamine",                  "organic", 7},
        {"dimethylamine",       "C2H7N",        "N-methylmethanamine",          "organic", 10},
        {"aniline",             "C6H7N",        "phenylamine",                  "organic", 14},

        // --- Biochemistry ---
        {"glucose",             "C6H12O6",      "D-glucose",                    "biochemistry", 24},
        {"sucrose",             "C12H22O11",    "alpha-D-glucopyranosyl-(1->2)-beta-D-fructofuranoside", "biochemistry", 45},
        {"urea",                "CH4N2O",       "carbonyl diamide",             "biochemistry", 8},
        {"glycine",             "C2H5NO2",      "2-aminoacetic acid",           "biochemistry", 10},
        {"alanine",             "C3H7NO2",      "2-aminopropanoic acid",        "biochemistry", 13},
        {"adenine",             "C5H5N5",       "6-aminopurine",                "biochemistry", 15},
        {"thymine",             "C5H6N2O2",     "5-methyluracil",               "biochemistry", 15},
        {"caffeine",            "C8H10N4O2",    "1,3,7-trimethylxanthine",      "biochemistry", 24},

        // --- Pharmaceuticals ---
        {"aspirin",             "C9H8O4",       "acetylsalicylic acid",         "pharmaceutical", 21},
        {"ibuprofen",           "C13H18O2",     "2-(4-isobutylphenyl)propionic acid", "pharmaceutical", 33},
        {"paracetamol",         "C8H9NO2",      "N-(4-hydroxyphenyl)acetamide", "pharmaceutical", 20},
        {"acetaminophen",       "C8H9NO2",      "N-(4-hydroxyphenyl)acetamide", "pharmaceutical", 20},

        // --- Oxalate family (also in the fallback pool, but accessible by name) ---
        {"oxalic acid",         "C2H2O4",       "ethanedioic acid",             "oxalate", 8},
        {"sodium oxalate",      "Na2C2O4",      "disodium ethanedioate",        "oxalate", 8},
        {"potassium oxalate",   "K2C2O4",       "dipotassium ethanedioate",     "oxalate", 8},
        {"calcium oxalate",     "CaC2O4",       "calcium ethanedioate",         "oxalate", 7},
        {"iron oxalate",        "FeC2O4",       "iron(II) ethanedioate",        "oxalate", 7},

        // --- Alias chains (alternate names → same molecule) ---
        {"dihydrogen monoxide", "H2O",          "dihydrogen monoxide",          "inorganic", 3},
        {"dhmo",                "H2O",          "dihydrogen monoxide",          "inorganic", 3},
        {"table salt",          "NaCl",         "sodium chloride",              "inorganic", 2},
        {"baking soda",         "NaHCO3",       "sodium hydrogen carbonate",    "inorganic", 6},
        {"limestone",           "CaCO3",        "calcium carbonate",            "inorganic", 5},
        {"chalk",               "CaCO3",        "calcium carbonate",            "inorganic", 5},
        {"dry ice",             "CO2",          "carbon dioxide",               "inorganic", 3},
        {"laughing gas",        "N2O",          "dinitrogen monoxide",          "inorganic", 3},
        {"grain alcohol",       "C2H6O",        "ethanol",                      "organic", 9},
        {"wood alcohol",        "CH4O",         "methanol",                     "organic", 6},
        {"rubbing alcohol",     "C3H8O",        "propan-2-ol",                  "organic", 12},
        {"vinegar",             "C2H4O2",       "ethanoic acid",                "organic", 8},
        {"lye",                 "NaOH",         "sodium hydroxide",             "inorganic", 3},
        {"muriatic acid",       "HCl",          "hydrogen chloride",            "inorganic", 2},
        {"vitamin c",           "C6H8O6",       "L-ascorbic acid",              "biochemistry", 20},
        {"ascorbic acid",       "C6H8O6",       "L-ascorbic acid",              "biochemistry", 20},
    };
    return table;
}

// ============================================================================
// Public API
// ============================================================================

const std::vector<AliasEntry>& alias_table() {
    return build_alias_table();
}

size_t alias_count() {
    return build_alias_table().size();
}

bool looks_like_formula(const std::string& input) {
    if (input.empty()) return false;

    // Formulas start with an uppercase letter
    if (!std::isupper(static_cast<unsigned char>(input[0]))) return false;

    // If it contains spaces, hyphens, or all-lowercase runs > 2, it's a name
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == ' ' || c == '-') return false;
    }

    // Must contain only uppercase, lowercase, and digits
    for (char c : input) {
        if (!std::isalpha(static_cast<unsigned char>(c)) &&
            !std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

Result<AliasEntry> resolve_alias(const std::string& input, uint64_t seed) {
    if (input.empty()) {
        // Empty input → random oxalate
        AliasEntry entry = random_oxalate(seed);
        return Result<AliasEntry>::ok(entry);
    }

    // Step 1: Case-insensitive lookup in alias table
    std::string key = to_lower(input);
    const auto& table = build_alias_table();

    for (const auto& entry : table) {
        if (to_lower(entry.name) == key) {
            return Result<AliasEntry>::ok(entry);
        }
    }

    // Step 2: Check the oxalate pool by name
    const auto& pool = build_oxalate_pool();
    for (const auto& entry : pool) {
        if (to_lower(entry.name) == key) {
            return Result<AliasEntry>::ok(entry);
        }
    }

    // Step 3: If it looks like a formula, pass through as-is
    if (looks_like_formula(input)) {
        AliasEntry passthrough;
        passthrough.name = input;
        passthrough.formula = input;
        passthrough.iupac = "";
        passthrough.family = "formula_passthrough";
        passthrough.atom_count = 0;  // Unknown without parsing
        return Result<AliasEntry>::ok(passthrough);
    }

    // Step 4: Unrecognized name → random oxalate default
    AliasEntry fallback = random_oxalate(seed);
    return Result<AliasEntry>::ok(fallback);
}

} // namespace vsepr
