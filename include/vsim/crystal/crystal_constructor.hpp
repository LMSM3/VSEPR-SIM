/**
 * crystal_constructor.hpp  -  CrystalModule functional-constructor form
 *
 * WO-66P  |  Universal Translation + Crystal/PBC vsim Module Compression
 *            into Functional Programming
 * v5.1.4  |  v5.0.0-main
 *
 * Compresses the existing [pbc] / [cell] / [crystal] flat-key schema into a
 * single functional constructor expression usable in [objects]:
 *
 *   [objects]
 *   system.crystal = CrystalModule(
 *       lattice    = fcc,
 *       a          = 3.52,
 *       species    = [Ni],
 *       supercell  = [4, 4, 4],
 *       relax      = true
 *   )
 *
 * Goals:
 *   1. All crystal/PBC parameters expressible as a single constructor call.
 *   2. Backward compatibility: legacy [pbc] / [cell] sections continue to
 *      parse and populate the same CrystalConstructorSection fields.
 *   3. Universal translation: a CrystalConstructorSection may be emitted to
 *      .xyz, xyz-supercell, CIF-style comment block, or internal cell state.
 *
 * Translation targets (WO-66P §4):
 *   - internal formation state (positions, PBC box)
 *   - xyz header with CELL comment
 *   - CIF summary block (not full CIF; summary only)
 *   - JSON crystal manifest
 *
 * Diagnostic codes: VSIM-C010 … VSIM-C019
 */

#pragma once

#include <string>
#include <vector>
#include <array>

namespace vsim {

// ============================================================================
// LatticeType  —  enumerated Bravais / common lattice forms
// ============================================================================

enum class LatticeType : int {
	Unknown        = 0,
	// Cubic
	SC             = 1,   // simple cubic
	BCC            = 2,   // body-centred cubic
	FCC            = 3,   // face-centred cubic
	Diamond        = 4,   // diamond cubic
	// Hexagonal
	HCP            = 5,   // hexagonal close-packed
	Hexagonal      = 6,
	// Tetragonal
	Tetragonal     = 7,
	BCT            = 8,
	// Orthorhombic
	Orthorhombic   = 9,
	// Monoclinic
	Monoclinic     = 10,
	// Triclinic
	Triclinic      = 11,
	// Wurtzite / Zinc-blende (compound shorthand)
	ZincBlende     = 12,
	Wurtzite       = 13,
	// Custom (explicit lattice vectors supplied)
	Custom         = 99,
};

inline LatticeType lattice_type_from_string(const std::string& s) {
	if (s == "sc"           || s == "SC")           return LatticeType::SC;
	if (s == "bcc"          || s == "BCC")          return LatticeType::BCC;
	if (s == "fcc"          || s == "FCC")          return LatticeType::FCC;
	if (s == "diamond"      || s == "Diamond")      return LatticeType::Diamond;
	if (s == "hcp"          || s == "HCP")          return LatticeType::HCP;
	if (s == "hexagonal"    || s == "Hexagonal")    return LatticeType::Hexagonal;
	if (s == "tetragonal"   || s == "Tetragonal")   return LatticeType::Tetragonal;
	if (s == "bct"          || s == "BCT")          return LatticeType::BCT;
	if (s == "orthorhombic" || s == "Orthorhombic") return LatticeType::Orthorhombic;
	if (s == "monoclinic"   || s == "Monoclinic")   return LatticeType::Monoclinic;
	if (s == "triclinic"    || s == "Triclinic")    return LatticeType::Triclinic;
	if (s == "zincblende"   || s == "ZincBlende")   return LatticeType::ZincBlende;
	if (s == "wurtzite"     || s == "Wurtzite")     return LatticeType::Wurtzite;
	if (s == "custom"       || s == "Custom")       return LatticeType::Custom;
	return LatticeType::Unknown;
}

inline const char* lattice_type_name(LatticeType t) {
	switch (t) {
		case LatticeType::SC:           return "sc";
		case LatticeType::BCC:          return "bcc";
		case LatticeType::FCC:          return "fcc";
		case LatticeType::Diamond:      return "diamond";
		case LatticeType::HCP:          return "hcp";
		case LatticeType::Hexagonal:    return "hexagonal";
		case LatticeType::Tetragonal:   return "tetragonal";
		case LatticeType::BCT:          return "bct";
		case LatticeType::Orthorhombic: return "orthorhombic";
		case LatticeType::Monoclinic:   return "monoclinic";
		case LatticeType::Triclinic:    return "triclinic";
		case LatticeType::ZincBlende:   return "zincblende";
		case LatticeType::Wurtzite:     return "wurtzite";
		case LatticeType::Custom:       return "custom";
		default:                        return "unknown";
	}
}

// ============================================================================
// CrystalConstructorSection  —  unified crystal / PBC constructor state
//
// Populated by:
//   a) parsing a CrystalModule(...) constructor expression in [objects]
//   b) mapping legacy [pbc] / [cell] keys at parse time
// ============================================================================

struct CrystalConstructorSection {
	// --- Lattice type ---------------------------------------------------------
	LatticeType lattice_type        = LatticeType::Unknown;

	// --- Lattice parameters ---------------------------------------------------
	double a_A = 0.0;   // Å
	double b_A = 0.0;   // Å — if zero, assumed = a
	double c_A = 0.0;   // Å — if zero, assumed = a (cubic) or c (hcp)
	double alpha_deg = 90.0;
	double beta_deg  = 90.0;
	double gamma_deg = 90.0;

	// Custom lattice vectors (used when lattice_type == Custom)
	std::array<double,3> vec_a = {0,0,0};
	std::array<double,3> vec_b = {0,0,0};
	std::array<double,3> vec_c = {0,0,0};

	// --- Species --------------------------------------------------------------
	std::vector<std::string> species;  // e.g. ["Ni"] or ["Ga","N"]

	// For compound lattices: fractional basis positions per species
	// basis[i] = {x, y, z} fractional
	std::vector<std::array<double,3>> basis;

	// --- Supercell ------------------------------------------------------------
	int nx = 1;
	int ny = 1;
	int nz = 1;

	// --- Periodic boundary conditions -----------------------------------------
	bool pbc_x = true;
	bool pbc_y = true;
	bool pbc_z = true;

	// --- Relaxation -----------------------------------------------------------
	bool   relax              = false;
	int    relax_max_steps    = 500;
	double relax_fmax_eV_A    = 0.01;
	std::string relax_method  = "conjugate_gradient";  // "conjugate_gradient" | "bfgs" | "fire"

	// --- Defects (optional) ---------------------------------------------------
	// vacancy_fraction: fraction of lattice sites replaced with vacancies
	double vacancy_fraction   = 0.0;
	// substitution_species: replace a fraction of species[0] with another element
	std::string substitution_species;
	double substitution_frac  = 0.0;

	// --- Disorder / thermal displacement -----------------------------------
	bool   thermal_displacement = false;
	double temperature_K        = 0.0;  // 0 = disabled

	// --- Translation (WO-66P) -------------------------------------------------
	// Target formats for crystal state translation
	bool   translate_to_xyz          = true;
	bool   translate_to_cell_comment = true;  // CELL block in xyz header
	bool   translate_to_cif_summary  = false;
	bool   translate_to_json         = false;

	// --- Source ---------------------------------------------------------------
	// If non-empty, was populated from this constructor path (for diagnostics)
	std::string constructor_path;
	int source_line = 0;

	// --- Validation helpers ---------------------------------------------------
	bool has_lattice()   const noexcept { return lattice_type != LatticeType::Unknown; }
	bool has_species()   const noexcept { return !species.empty(); }
	bool is_cubic()      const noexcept {
		return lattice_type == LatticeType::SC  ||
			   lattice_type == LatticeType::BCC ||
			   lattice_type == LatticeType::FCC ||
			   lattice_type == LatticeType::Diamond;
	}
	bool has_custom_vectors() const noexcept {
		return lattice_type == LatticeType::Custom &&
			   (vec_a[0] != 0.0 || vec_a[1] != 0.0 || vec_a[2] != 0.0);
	}
};

} // namespace vsim
