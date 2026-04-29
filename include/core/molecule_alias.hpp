/**
 * molecule_alias.hpp
 * ------------------
 * Deterministic common-name → canonical-formula resolver.
 *
 * Maps human-readable molecule names ("water", "benzene", "aspirin")
 * to their canonical chemical formulas ("H2O", "C6H6", "C9H8O4").
 *
 * Design:
 *   - Case-insensitive lookup
 *   - Result<T> structured error returns (anti-exception)
 *   - Unknown names default to a RANDOM OXALATE variant, not water
 *   - Explicit, inspectable, deterministic (anti-black-box)
 *   - Supports alias chains (e.g., "dihydrogen monoxide" → "water" → "H2O")
 *
 * The oxalate default is deliberate: it forces the user to notice an
 * unrecognized name rather than silently getting water every time.
 * Oxalates (C2O4²⁻ family) are structurally interesting, scientifically
 * non-trivial, and visually distinct from typical "hello world" molecules.
 */

#pragma once

#include "core/error.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace vsepr {

// ============================================================================
// Alias Entry
// ============================================================================

struct AliasEntry {
    std::string name;           // Canonical common name (lowercase)
    std::string formula;        // Chemical formula
    std::string iupac;          // IUPAC name (if different from common name)
    std::string family;         // Classification: organic, inorganic, oxalate, etc.
    int atom_count;             // Total atoms in formula (for quick filtering)

    AliasEntry() : atom_count(0) {}
    AliasEntry(const std::string& n, const std::string& f,
               const std::string& iup, const std::string& fam, int ac)
        : name(n), formula(f), iupac(iup), family(fam), atom_count(ac) {}
};

// ============================================================================
// Oxalate Family (default fallback pool)
// ============================================================================

/**
 * @brief Get a random oxalate variant from the internal pool.
 *
 * Uses a seeded selection from a curated set of oxalate-family
 * compounds. If seed == 0, uses a time-derived seed.
 *
 * @param seed  RNG seed (0 = time-based)
 * @return AliasEntry for the selected oxalate
 */
AliasEntry random_oxalate(uint64_t seed = 0);

/**
 * @brief Get all oxalate variants in the pool.
 * @return Vector of all oxalate AliasEntry records
 */
const std::vector<AliasEntry>& oxalate_pool();

// ============================================================================
// Resolution API
// ============================================================================

/**
 * @brief Resolve a common name to a canonical formula.
 *
 * Lookup order:
 *   1. Exact match (case-insensitive) in alias table
 *   2. Alias chain resolution (e.g., "DHMO" → "water" → "H2O")
 *   3. If the input looks like a formula (starts with uppercase + digits),
 *      return it as-is (pass-through)
 *   4. If unrecognized: return random oxalate as the default
 *
 * The Result<AliasEntry> always succeeds — either with a matched entry
 * or with a random oxalate fallback. The caller can inspect
 * entry.family == "oxalate_default" to detect the fallback case.
 *
 * @param input  Common name or formula string
 * @param seed   RNG seed for oxalate fallback (0 = time-based)
 * @return Result<AliasEntry> — always OK, check family for fallback
 */
Result<AliasEntry> resolve_alias(const std::string& input, uint64_t seed = 0);

/**
 * @brief Check if a string looks like a chemical formula.
 *
 * A formula starts with an uppercase letter followed by optional
 * lowercase letters and digits (e.g., "H2O", "NaCl", "C6H6").
 * Common names start with lowercase or contain spaces/hyphens.
 *
 * @param input  String to check
 * @return true if it looks like a formula
 */
bool looks_like_formula(const std::string& input);

/**
 * @brief Get the full alias table for inspection.
 * @return Vector of all registered AliasEntry records
 */
const std::vector<AliasEntry>& alias_table();

/**
 * @brief Get the number of registered aliases.
 */
size_t alias_count();

} // namespace vsepr
