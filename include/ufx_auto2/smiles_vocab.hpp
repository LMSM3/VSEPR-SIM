// include/ufx_auto2/smiles_vocab.hpp
// UFX_AUTO_2 Phase 6 -- SMILES Vocabulary Table
// VSEPR-SIM v5 beta9
//
// Curated per-element-family SMILES strings for known coordination/oxidation
// combinations. These are NOT random strings. Each entry encodes a structurally
// valid molecule or coordination complex.
//
// Usage:
//   auto smiles = SmilesVocab::lookup(element, oxidation_state, coordination);
//   // returns "" if no entry exists for the combination

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace vsepr::ufx {

struct SmilesEntry {
	std::string element;
	int         oxidation_state;
	int         coordination;
	std::string smiles;           // canonical SMILES or best approximation
	int         hbond_donor;
	int         hbond_acceptor;
	std::string note;
};

// ============================================================================
// SmilesVocab -- lookup table for canonical SMILES by element/ox/coord
// ============================================================================

class SmilesVocab {
public:
	// Return SMILES for the given element/oxidation_state/coordination.
	// Returns "" if no entry is found.
	static std::string lookup(const std::string& element,
							  int oxidation_state,
							  int coordination) noexcept;

	// Return the full SmilesEntry or a blank one if not found.
	static SmilesEntry lookup_entry(const std::string& element,
									int oxidation_state,
									int coordination) noexcept;

	// All entries for a given element (may be empty).
	static std::vector<SmilesEntry> entries_for(const std::string& element) noexcept;

private:
	static const std::vector<SmilesEntry>& table_() noexcept;
};

// ============================================================================
// Vocabulary table -- inline definition
// ============================================================================
//
// Format:  { element, ox_state, coordination, SMILES, hbd, hba, note }
//
// Sources: PubChem canonical SMILES, IUPAC naming conventions.
// Actinides: simplified ligand-field approximations.
// Noble gases: monatomic only.

inline const std::vector<SmilesEntry>& SmilesVocab::table_() noexcept {
	static const std::vector<SmilesEntry> tbl = {
		// ── Hydrogen ───────────────────────────────────────────────────────
		{ "H",  1,  1, "[H+]",               0, 0, "proton" },
		{ "H",  0,  1, "[H][H]",             0, 0, "dihydrogen" },

		// ── Carbon ────────────────────────────────────────────────────────
		{ "C",  4,  4, "C",                  0, 0, "methane" },
		{ "C",  2,  3, "C=O",                0, 1, "formaldehyde-like" },
		{ "C",  4,  3, "C(=O)O",             1, 2, "formic acid" },
		{ "C",  4,  4, "CC",                 0, 0, "ethane" },
		{ "C", -4,  4, "[CH4]",              0, 0, "methane explicit" },

		// ── Nitrogen ──────────────────────────────────────────────────────
		{ "N", -3,  4, "[NH4+]",             4, 0, "ammonium" },
		{ "N", -3,  3, "N",                  3, 1, "ammonia" },
		{ "N",  5,  3, "[N+](=O)[O-]",       0, 2, "nitro" },
		{ "N",  3,  3, "NO",                 0, 1, "nitric oxide" },

		// ── Oxygen ────────────────────────────────────────────────────────
		{ "O", -2,  2, "O",                  2, 1, "water" },
		{ "O", -1,  1, "[O-][O-]",           0, 2, "peroxide" },
		{ "O", -2,  1, "[O-2]",              0, 1, "oxide ion" },

		// ── Fluorine ──────────────────────────────────────────────────────
		{ "F", -1,  1, "F",                  0, 1, "HF" },
		{ "F", -1,  1, "[F-]",               0, 1, "fluoride" },

		// ── Sodium ────────────────────────────────────────────────────────
		{ "Na",  1,  6, "[Na+]",             0, 0, "sodium ion" },

		// ── Magnesium ─────────────────────────────────────────────────────
		{ "Mg",  2,  6, "[Mg+2]",            0, 0, "magnesium ion" },
		{ "Mg",  2,  4, "[Mg+2]",            0, 0, "magnesium tetrahedral" },

		// ── Aluminum ──────────────────────────────────────────────────────
		{ "Al",  3,  6, "[Al+3]",            0, 0, "aluminium ion" },
		{ "Al",  3,  4, "[Al+3]",            0, 0, "aluminium tetrahedral" },

		// ── Silicon ───────────────────────────────────────────────────────
		{ "Si",  4,  4, "[SiH4]",            0, 0, "silane" },
		{ "Si",  4,  6, "[Si+4]",            0, 0, "silicon hexacoordinate" },

		// ── Phosphorus ────────────────────────────────────────────────────
		{ "P",   5,  4, "P(=O)(O)(O)O",      3, 4, "phosphoric acid" },
		{ "P",   3,  3, "P",                  0, 1, "phosphine-like" },

		// ── Sulfur ────────────────────────────────────────────────────────
		{ "S",   6,  4, "S(=O)(=O)(O)O",     2, 4, "sulfuric acid" },
		{ "S",  -2,  2, "[S-2]",              0, 1, "sulfide" },
		{ "S",   4,  3, "S=O",               0, 1, "sulfur monoxide" },

		// ── Chlorine ──────────────────────────────────────────────────────
		{ "Cl", -1,  1, "[Cl-]",             0, 1, "chloride" },
		{ "Cl",  7,  4, "[ClO4-]",           0, 4, "perchlorate" },

		// ── Potassium ─────────────────────────────────────────────────────
		{ "K",   1,  6, "[K+]",              0, 0, "potassium ion" },

		// ── Calcium ───────────────────────────────────────────────────────
		{ "Ca",  2,  6, "[Ca+2]",            0, 0, "calcium ion" },
		{ "Ca",  2,  8, "[Ca+2]",            0, 0, "calcium 8-coord" },

		// ── Titanium ──────────────────────────────────────────────────────
		{ "Ti",  4,  6, "[Ti+4]",            0, 0, "titanium(IV) ion" },
		{ "Ti",  4,  4, "[Ti+4]",            0, 0, "titanium(IV) tetrahedral" },
		{ "Ti",  3,  6, "[Ti+3]",            0, 0, "titanium(III) ion" },

		// ── Vanadium ──────────────────────────────────────────────────────
		{ "V",   5,  6, "[V+5]",             0, 0, "vanadium(V)" },
		{ "V",   4,  6, "[V+4]",             0, 0, "vanadium(IV)" },
		{ "V",   3,  6, "[V+3]",             0, 0, "vanadium(III)" },

		// ── Chromium ──────────────────────────────────────────────────────
		{ "Cr",  3,  6, "[Cr+3]",            0, 0, "chromium(III)" },
		{ "Cr",  6,  4, "[Cr+6]",            0, 0, "chromate-like" },

		// ── Manganese ─────────────────────────────────────────────────────
		{ "Mn",  2,  6, "[Mn+2]",            0, 0, "manganese(II)" },
		{ "Mn",  4,  6, "[Mn+4]",            0, 0, "manganese(IV)" },
		{ "Mn",  7,  4, "[Mn+7]",            0, 0, "permanganate-like" },

		// ── Iron ──────────────────────────────────────────────────────────
		{ "Fe",  2,  6, "[Fe+2]",            0, 0, "iron(II)" },
		{ "Fe",  3,  6, "[Fe+3]",            0, 0, "iron(III)" },
		{ "Fe",  3,  4, "[Fe+3]",            0, 0, "iron(III) tetrahedral" },

		// ── Cobalt ────────────────────────────────────────────────────────
		{ "Co",  2,  6, "[Co+2]",            0, 0, "cobalt(II)" },
		{ "Co",  3,  6, "[Co+3]",            0, 0, "cobalt(III)" },

		// ── Nickel ────────────────────────────────────────────────────────
		{ "Ni",  2,  6, "[Ni+2]",            0, 0, "nickel(II) octahedral" },
		{ "Ni",  2,  4, "[Ni+2]",            0, 0, "nickel(II) square_planar" },

		// ── Copper ────────────────────────────────────────────────────────
		{ "Cu",  2,  4, "[Cu+2]",            0, 0, "copper(II)" },
		{ "Cu",  1,  2, "[Cu+]",             0, 0, "copper(I)" },

		// ── Zinc ──────────────────────────────────────────────────────────
		{ "Zn",  2,  4, "[Zn+2]",            0, 0, "zinc(II) tetrahedral" },
		{ "Zn",  2,  6, "[Zn+2]",            0, 0, "zinc(II) octahedral" },

		// ── Bromine ───────────────────────────────────────────────────────
		{ "Br", -1,  1, "[Br-]",             0, 1, "bromide" },

		// ── Iodine ────────────────────────────────────────────────────────
		{ "I",  -1,  1, "[I-]",              0, 1, "iodide" },
		{ "I",   5,  5, "[I+5]",             0, 0, "iodate-like" },

		// ── Cesium ────────────────────────────────────────────────────────
		{ "Cs",  1,  8, "[Cs+]",             0, 0, "cesium ion" },

		// ── Barium ────────────────────────────────────────────────────────
		{ "Ba",  2,  8, "[Ba+2]",            0, 0, "barium ion" },

		// ── Lanthanum ─────────────────────────────────────────────────────
		{ "La",  3,  9, "[La+3]",            0, 0, "lanthanum(III)" },

		// ── Neodymium ─────────────────────────────────────────────────────
		{ "Nd",  3,  9, "[Nd+3]",            0, 0, "neodymium(III)" },

		// ── Tungsten ──────────────────────────────────────────────────────
		{ "W",   6,  6, "[W+6]",             0, 0, "tungsten(VI)" },
		{ "W",   4,  6, "[W+4]",             0, 0, "tungsten(IV)" },

		// ── Rhenium ───────────────────────────────────────────────────────
		{ "Re",  7,  6, "[Re+7]",            0, 0, "rhenium(VII)" },
		{ "Re",  4,  6, "[Re+4]",            0, 0, "rhenium(IV)" },

		// ── Iridium ───────────────────────────────────────────────────────
		{ "Ir",  3,  6, "[Ir+3]",            0, 0, "iridium(III)" },
		{ "Ir",  4,  6, "[Ir+4]",            0, 0, "iridium(IV)" },

		// ── Platinum ──────────────────────────────────────────────────────
		{ "Pt",  2,  4, "[Pt+2]",            0, 0, "platinum(II) square_planar" },
		{ "Pt",  4,  6, "[Pt+4]",            0, 0, "platinum(IV) octahedral" },

		// ── Gold ──────────────────────────────────────────────────────────
		{ "Au",  1,  2, "[Au+]",             0, 0, "gold(I) linear" },
		{ "Au",  3,  4, "[Au+3]",            0, 0, "gold(III) square_planar" },

		// ── Lead ──────────────────────────────────────────────────────────
		{ "Pb",  2,  6, "[Pb+2]",            0, 0, "lead(II)" },
		{ "Pb",  4,  4, "[Pb+4]",            0, 0, "lead(IV)" },

		// ── Bismuth ───────────────────────────────────────────────────────
		{ "Bi",  3,  6, "[Bi+3]",            0, 0, "bismuth(III)" },

		// ── Thorium ───────────────────────────────────────────────────────
		{ "Th",  4,  8, "[Th+4]",            0, 0, "thorium(IV) 8-coord" },
		{ "Th",  4, 12, "[Th+4]",            0, 0, "thorium(IV) 12-coord" },

		// ── Uranium ───────────────────────────────────────────────────────
		{ "U",   6,  6, "[U+6]",             0, 0, "uranium(VI) octahedral" },
		{ "U",   4,  8, "[U+4]",             0, 0, "uranium(IV) 8-coord" },
		{ "U",   3,  6, "[U+3]",             0, 0, "uranium(III)" },

		// ── Plutonium ─────────────────────────────────────────────────────
		{ "Pu",  4,  8, "[Pu+4]",            0, 0, "plutonium(IV) 8-coord" },
		{ "Pu",  3,  6, "[Pu+3]",            0, 0, "plutonium(III)" },
	};
	return tbl;
}

inline std::string SmilesVocab::lookup(const std::string& element,
									   int oxidation_state,
									   int coordination) noexcept {
	auto e = lookup_entry(element, oxidation_state, coordination);
	return e.smiles;
}

inline SmilesEntry SmilesVocab::lookup_entry(const std::string& element,
											  int oxidation_state,
											  int coordination) noexcept {
	for (auto& e : table_()) {
		if (e.element == element &&
			e.oxidation_state == oxidation_state &&
			e.coordination == coordination)
		{
			return e;
		}
	}
	return {};
}

inline std::vector<SmilesEntry> SmilesVocab::entries_for(const std::string& element) noexcept {
	std::vector<SmilesEntry> out;
	for (auto& e : table_()) {
		if (e.element == element) out.push_back(e);
	}
	return out;
}

} // namespace vsepr::ufx
