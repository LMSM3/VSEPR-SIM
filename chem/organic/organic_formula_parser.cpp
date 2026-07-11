/**
 * organic_formula_parser.cpp — implementation
 *
 * Residue formulas are the *condensed* (peptide-linked) form: the
 * free amino acid minus one water molecule (H2O = H:2, O:1).
 * When computing a whole chain, we add one H2O back for the
 * terminal NH2 and COOH that are not consumed by peptide bonds.
 *
 * Canonical Hill order: C first, H second, then all other elements
 * in alphabetical order.  Zero-count atoms are omitted.
 */

#include "chem/organic/organic_formula_parser.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace vsepr::chem::organic {

// ============================================================================
// Residue table — all 20 standard amino acids
// Residue formula = free amino acid - H2O
//   Ala free: C3H7NO2  →  residue: C3H5NO  (H:-2, O:-1)
// ============================================================================

static constexpr ResidueFormula k_residues[] = {
	// one  three   name                         C   H   N   O   S
	{ 'A', "Ala", "alanine",                     3,  5,  1,  1,  0 },
	{ 'C', "Cys", "cysteine",                    3,  5,  1,  1,  1 },
	{ 'D', "Asp", "aspartic acid",               4,  5,  1,  3,  0 },
	{ 'E', "Glu", "glutamic acid",               5,  7,  1,  3,  0 },
	{ 'F', "Phe", "phenylalanine",               9,  9,  1,  1,  0 },
	{ 'G', "Gly", "glycine",                     2,  3,  1,  1,  0 },
	{ 'H', "His", "histidine",                   6,  7,  3,  1,  0 },
	{ 'I', "Ile", "isoleucine",                  6, 11,  1,  1,  0 },
	{ 'K', "Lys", "lysine",                      6, 12,  2,  1,  0 },
	{ 'L', "Leu", "leucine",                     6, 11,  1,  1,  0 },
	{ 'M', "Met", "methionine",                  5,  9,  1,  1,  1 },
	{ 'N', "Asn", "asparagine",                  4,  6,  2,  2,  0 },
	{ 'P', "Pro", "proline",                     5,  7,  1,  1,  0 },
	{ 'Q', "Gln", "glutamine",                   5,  8,  2,  2,  0 },
	{ 'R', "Arg", "arginine",                    6, 12,  4,  1,  0 },
	{ 'S', "Ser", "serine",                      3,  5,  1,  2,  0 },
	{ 'T', "Thr", "threonine",                   4,  7,  1,  2,  0 },
	{ 'V', "Val", "valine",                      5,  9,  1,  1,  0 },
	{ 'W', "Trp", "tryptophan",                 11,  10, 2,  1,  0 },
	{ 'Y', "Tyr", "tyrosine",                    9,  9,  1,  2,  0 },
};

static constexpr int k_residue_count =
	static_cast<int>(sizeof(k_residues) / sizeof(k_residues[0]));

const ResidueFormula* residue_formula(char c) {
	char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	for (int i = 0; i < k_residue_count; ++i) {
		if (k_residues[i].one_letter == upper)
			return &k_residues[i];
	}
	return nullptr;
}

// ============================================================================
// Trivial name → formula table
// ============================================================================

struct TrivialEntry {
	const char* name;    // lowercase, spaces normalised
	const char* formula; // Hill order
	const char* domain;
};

static constexpr TrivialEntry k_trivial[] = {
	// Small molecules
	{ "water",               "H2O",      "small_molecule" },
	{ "methane",             "CH4",      "small_molecule" },
	{ "ethane",              "C2H6",     "small_molecule" },
	{ "propane",             "C3H8",     "small_molecule" },
	{ "ethanol",             "C2H6O",    "small_molecule" },
	{ "methanol",            "CH4O",     "small_molecule" },
	{ "isopropanol",         "C3H8O",    "small_molecule" },
	{ "acetone",             "C3H6O",    "small_molecule" },
	{ "acetic acid",         "C2H4O2",   "small_molecule" },
	{ "formic acid",         "CH2O2",    "small_molecule" },
	{ "propionic acid",      "C3H6O2",   "small_molecule" },
	{ "benzene",             "C6H6",     "small_molecule" },
	{ "toluene",             "C7H8",     "small_molecule" },
	{ "phenol",              "C6H6O",    "small_molecule" },
	{ "glucose",             "C6H12O6",  "small_molecule" },
	{ "fructose",            "C6H12O6",  "small_molecule" },
	{ "sucrose",             "C12H22O11","small_molecule" },
	{ "caffeine",            "C8H10N4O2","small_molecule" },
	{ "urea",                "CH4N2O",   "small_molecule" },
	{ "ammonia",             "H3N",      "small_molecule" },
	{ "ethylene",            "C2H4",     "small_molecule" },
	{ "acetylene",           "C2H2",     "small_molecule" },
	{ "formaldehyde",        "CH2O",     "small_molecule" },
	{ "acetaldehyde",        "C2H4O",    "small_molecule" },
	{ "dimethyl sulfoxide",  "C2H6OS",   "small_molecule" },
	{ "dmso",                "C2H6OS",   "small_molecule" },
	// Free amino acids (trivial name → free acid formula)
	{ "glycine",             "C2H5NO2",  "small_molecule" },
	{ "alanine",             "C3H7NO2",  "small_molecule" },
	{ "valine",              "C5H11NO2", "small_molecule" },
	{ "leucine",             "C6H13NO2", "small_molecule" },
	{ "isoleucine",          "C6H13NO2", "small_molecule" },
	{ "proline",             "C5H9NO2",  "small_molecule" },
	{ "phenylalanine",       "C9H11NO2", "small_molecule" },
	{ "tryptophan",          "C11H12N2O2","small_molecule"},
	{ "methionine",          "C5H11NO2S","small_molecule" },
	{ "serine",              "C3H7NO3",  "small_molecule" },
	{ "threonine",           "C4H9NO3",  "small_molecule" },
	{ "cysteine",            "C3H7NO2S", "small_molecule" },
	{ "tyrosine",            "C9H11NO3", "small_molecule" },
	{ "asparagine",          "C4H8N2O3", "small_molecule" },
	{ "glutamine",           "C5H10N2O3","small_molecule" },
	{ "aspartic acid",       "C4H7NO4",  "small_molecule" },
	{ "glutamic acid",       "C5H9NO4",  "small_molecule" },
	{ "lysine",              "C6H14N2O2","small_molecule" },
	{ "arginine",            "C6H14N4O2","small_molecule" },
	{ "histidine",           "C6H9N3O2", "small_molecule" },
};

static constexpr int k_trivial_count =
	static_cast<int>(sizeof(k_trivial) / sizeof(k_trivial[0]));

// ============================================================================
// Hill-order canonical formula builder
// ============================================================================

static std::string build_hill_formula(const std::map<std::string, int>& counts) {
	std::string out;
	auto emit = [&](const std::string& sym) {
		auto it = counts.find(sym);
		if (it == counts.end() || it->second == 0) return;
		out += sym;
		if (it->second > 1) out += std::to_string(it->second);
	};

	emit("C");
	emit("H");
	// remaining elements alphabetically
	for (const auto& [sym, n] : counts) {
		if (sym == "C" || sym == "H") continue;
		if (n > 0) { out += sym; if (n > 1) out += std::to_string(n); }
	}
	return out;
}

// ============================================================================
// Amino acid sequence parser
// ============================================================================

std::optional<FormulaResult> parse_amino_acid_sequence(const std::string& seq) {
	if (seq.empty()) return std::nullopt;

	// Must be all uppercase letters that are valid one-letter codes.
	// Lowercase or mixed-case inputs are intentionally rejected here so the
	// dispatcher can fall through to the trivial-name strategy.
	for (char c : seq) {
		if (!std::isupper(static_cast<unsigned char>(c))) return std::nullopt;
		if (!residue_formula(c)) return std::nullopt;
	}

	std::map<std::string, int> counts;
	for (char c : seq) {
		const ResidueFormula* r = residue_formula(c);
		counts["C"] += r->C;
		counts["H"] += r->H;
		counts["N"] += r->N;
		counts["O"] += r->O;
		if (r->S > 0) counts["S"] += r->S;
	}

	// Add terminal H2O (free N-terminus NH2 + C-terminus COOH not involved in bonds)
	counts["H"] += 2;
	counts["O"] += 1;

	FormulaResult res;
	res.formula       = build_hill_formula(counts);
	res.domain        = "peptide";
	res.residue_count = static_cast<int>(seq.size());
	res.ok            = true;
	return res;
}

// ============================================================================
// Trivial name lookup
// ============================================================================

static std::string normalise_name(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (char c : s) {
		if (c == '-' || c == '_') { out += ' '; continue; }
		out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	// collapse multiple spaces
	std::string out2;
	bool prev_space = false;
	for (char c : out) {
		if (c == ' ') { if (!prev_space) out2 += c; prev_space = true; }
		else          { out2 += c; prev_space = false; }
	}
	// trim trailing space
	while (!out2.empty() && out2.back() == ' ') out2.pop_back();
	return out2;
}

std::optional<FormulaResult> lookup_trivial_name(const std::string& name) {
	std::string key = normalise_name(name);
	for (int i = 0; i < k_trivial_count; ++i) {
		if (key == k_trivial[i].name) {
			FormulaResult r;
			r.formula = k_trivial[i].formula;
			r.domain  = k_trivial[i].domain;
			r.ok      = true;
			return r;
		}
	}
	return std::nullopt;
}

// ============================================================================
// Condensed formula normaliser
// ============================================================================

// Parse a condensed formula like "CH3COOH" or "C2H5OH" into atom counts,
// then re-emit in Hill order.
std::optional<FormulaResult> normalise_condensed_formula(const std::string& input) {
	std::map<std::string, int> counts;
	int i = 0;
	int n = static_cast<int>(input.size());

	while (i < n) {
		if (!std::isupper(static_cast<unsigned char>(input[i])))
			return std::nullopt; // not starting with capital letter

		std::string sym;
		sym += input[i++];
		// collect lowercase continuation
		while (i < n && std::islower(static_cast<unsigned char>(input[i])))
			sym += input[i++];

		// collect digit count
		int count = 0;
		while (i < n && std::isdigit(static_cast<unsigned char>(input[i]))) {
			count = count * 10 + (input[i] - '0');
			++i;
		}
		if (count == 0) count = 1;

		counts[sym] += count;
	}

	if (counts.empty()) return std::nullopt;

	FormulaResult r;
	r.formula = build_hill_formula(counts);
	r.domain  = "small_molecule";
	r.ok      = true;
	return r;
}

// ============================================================================
// Top-level dispatcher
// ============================================================================

FormulaResult expand_organic_formula(const std::string& input) {
	if (input.empty()) {
		FormulaResult bad;
		bad.error = "empty input";
		return bad;
	}

	// Strategy 1: amino acid sequence (all-caps, all valid codes)
	if (auto r = parse_amino_acid_sequence(input)) return *r;

	// Strategy 2: trivial name (lowercase or mixed, contains space/letters)
	if (auto r = lookup_trivial_name(input)) return *r;

	// Strategy 3: condensed formula
	if (auto r = normalise_condensed_formula(input)) return *r;

	FormulaResult bad;
	bad.error = "unrecognised organic input: " + input;
	return bad;
}

} // namespace vsepr::chem::organic
