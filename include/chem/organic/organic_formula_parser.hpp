/**
 * organic_formula_parser.hpp - Organic syntax -> canonical molecular formula
 *
 * Translates human-authored organic shorthand into standard molecular formulas.
 * Three input modes are supported:
 *
 *   1. Amino acid one-letter sequences  (e.g. "ACDEFG")
 *      -> summed residue formula + one H2O for the free terminus pair
 *
 *   2. Trivial / IUPAC common names     (e.g. "acetic acid", "ethanol", "glycine")
 *      -> direct lookup from the built-in name table
 *
 *   3. Condensed organic formulas       (e.g. "CH3COOH", "C2H5OH")
 *      -> normalised into Hill-order canonical form (C first, H second, rest alpha)
 *
 * The parser is domain-agnostic; callers decide what to do with the result.
 * VSIM integration: vsim_parser.cpp calls expand_organic_formula() when a
 * [system] block contains domain = "peptide" and a sequence key.
 *
 * Design rule: no heap allocation beyond std::string / std::map.
 * No external dependencies beyond the C++23 standard library.
 */

#pragma once

#include <map>
#include <optional>
#include <string>

namespace vsepr::chem::organic {

// ============================================================================
// Result type
// ============================================================================

struct FormulaResult {
	std::string formula;           // canonical Hill-order formula, e.g. "C10H17N3O6S"
	std::string domain;            // "peptide" | "small_molecule" | "unknown"
	int         residue_count = 0; // > 0 for peptide sequences; 0 for others
	bool        ok = false;

	std::string error;             // non-empty when ok == false
};

// ============================================================================
// Public API
// ============================================================================

/**
 * Attempt to parse organic shorthand and return a canonical formula.
 *
 * Input is tried in order:
 *   1. Peptide sequence (all-uppercase single letters, subset of ACDEFGHIKLMNPQRSTVWY)
 *   2. Trivial name lookup (case-insensitive, spaces/hyphens normalised)
 *   3. Condensed formula normalisation
 *
 * Returns FormulaResult::ok == false if none of the three strategies succeed.
 */
FormulaResult expand_organic_formula(const std::string& input);

/**
 * Attempt to parse a single-letter amino acid sequence specifically.
 * Returns std::nullopt if the input is not a valid sequence.
 */
std::optional<FormulaResult> parse_amino_acid_sequence(const std::string& seq);

/**
 * Look up a trivial organic name in the built-in table.
 * The lookup is case-insensitive and normalises spaces/hyphens.
 * Returns std::nullopt if the name is not in the table.
 */
std::optional<FormulaResult> lookup_trivial_name(const std::string& name);

/**
 * Normalise a condensed organic formula into Hill order.
 * Returns std::nullopt if the string cannot be parsed as an atom count list.
 */
std::optional<FormulaResult> normalise_condensed_formula(const std::string& formula);

// ============================================================================
// Residue data access (used by tests and the peptide formation layer)
// ============================================================================

struct ResidueFormula {
	char        one_letter;
	const char* three_letter;
	const char* name;
	// Residue formula (not free amino acid — already minus H2O for chain linkage)
	int C, H, N, O, S;
};

/**
 * Return the residue formula for a single amino acid letter, or nullptr.
 * The residue formula assumes the amino acid is peptide-linked (water already removed).
 * To get the free amino acid formula, add H2O (H:+2, O:+1).
 */
const ResidueFormula* residue_formula(char one_letter);

} // namespace vsepr::chem::organic
