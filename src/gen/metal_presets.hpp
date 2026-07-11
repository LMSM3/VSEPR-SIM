#pragma once
/**
 * metal_presets.hpp -- Static material database for metal/alloy generators
 * =========================================================================
 * VSEPR-SIM  |  branch: v5.0.0-beta.7-step-attempt
 *
 * Provides compile-time descriptors for four metals / alloys chosen to
 * exercise every lattice type the builder supports:
 *
 *   Nitinol      (NiTi)          B2   shape-memory alloy
 *   Tungsten     (W)             BCC  refractory pure metal
 *   Inconel 625  (Ni-Cr-Mo-Nb)  FCC  high-temperature superalloy
 *   Ti-6Al-4V    (Ti-Al-V)      HCP  aerospace structural alloy
 *
 * Unit system follows xyz_unified.hpp §1.1:
 *   coords   Å     energy  kcal/mol     charge  e
 */

#include <string>
#include <vector>
#include <array>

namespace vsepr {
namespace gen {

// ---------------------------------------------------------------------------
// Site descriptor — one Wyckoff / basis site in the primitive cell
// ---------------------------------------------------------------------------
struct SiteDesc {
	std::string symbol;   // element symbol
	int         Z;        // atomic number
	double      frac[3];  // fractional coordinates in the primitive cell
	double      q_default; // representative partial charge (e) for .xyza
};

// ---------------------------------------------------------------------------
// Lattice type tag
// ---------------------------------------------------------------------------
enum class LatticeType { BCC, FCC, HCP, B2 };

// ---------------------------------------------------------------------------
// MaterialPreset — everything needed to build a supercell
// ---------------------------------------------------------------------------
struct MaterialPreset {
	std::string  name;          // human-readable label
	std::string  tag;           // short filesystem-safe tag (no spaces)
	LatticeType  lattice;
	double       a;             // lattice parameter a  (Å)
	double       c;             // lattice parameter c  (Å; =a for cubic)
	double       density_gcc;   // reference density (g/cm³)

	// Basis sites (fractional coords).  For pure metals, exactly one site.
	// For alloys, sites are filled in order; the builder cycles through them
	// deterministically when tiling the supercell.
	std::vector<SiteDesc> basis;

	// Alloy composition label (e.g. "Ni-50 Ti-50 at%")
	std::string composition;

	// Per-atom reference energy for the .xyza energy column (kcal/mol)
	double ref_energy_per_atom;
};

// ---------------------------------------------------------------------------
// Database
// ---------------------------------------------------------------------------
inline std::vector<MaterialPreset> default_presets() {
	std::vector<MaterialPreset> db;

	// ------------------------------------------------------------------
	// 1. Nitinol — NiTi, B2 (CsCl-type ordered BCC)
	//    a = 3.015 Å  (experimental 300 K)
	//    B2: Ni at (0,0,0), Ti at (0.5,0.5,0.5)
	// ------------------------------------------------------------------
	{
		MaterialPreset m;
		m.name               = "Nitinol";
		m.tag                = "NiTi_B2";
		m.lattice            = LatticeType::B2;
		m.a                  = 3.015;
		m.c                  = 3.015;
		m.density_gcc        = 6.45;
		m.composition        = "Ni-50 Ti-50 at%";
		m.ref_energy_per_atom = -87.3;   // DFT-PBE cohesive approx.

		m.basis = {
			{ "Ni", 28, {0.0, 0.0, 0.0},         -0.12 },
			{ "Ti", 22, {0.5, 0.5, 0.5},          +0.12 },
		};
		db.push_back(std::move(m));
	}

	// ------------------------------------------------------------------
	// 2. Tungsten — W, BCC pure metal
	//    a = 3.165 Å  (experimental)
	// ------------------------------------------------------------------
	{
		MaterialPreset m;
		m.name               = "Tungsten";
		m.tag                = "W_BCC";
		m.lattice            = LatticeType::BCC;
		m.a                  = 3.165;
		m.c                  = 3.165;
		m.density_gcc        = 19.25;
		m.composition        = "W-100 at%";
		m.ref_energy_per_atom = -203.6;  // cohesive energy kcal/mol

		m.basis = {
			{ "W", 74, {0.0, 0.0, 0.0},  0.0 },
		};
		db.push_back(std::move(m));
	}

	// ------------------------------------------------------------------
	// 3. Inconel 625 — Ni-Cr-Mo-Nb FCC superalloy
	//    Nominal: Ni~61%, Cr~22%, Mo~9%, Nb~4%, Fe~4% (simplified to 4-site)
	//    a = 3.575 Å  (Ni FCC baseline; Cr/Mo/Nb substitute on same FCC sites)
	//    Site substitution cycles: Ni Ni Ni Cr Mo Nb (per 6 FCC atoms tiled)
	// ------------------------------------------------------------------
	{
		MaterialPreset m;
		m.name               = "Inconel625";
		m.tag                = "IN625_FCC";
		m.lattice            = LatticeType::FCC;
		m.a                  = 3.575;
		m.c                  = 3.575;
		m.density_gcc        = 8.44;
		m.composition        = "Ni-61 Cr-22 Mo-9 Nb-4 at% (nom.)";
		m.ref_energy_per_atom = -102.8;

		// FCC has one site; alloying handled by the builder's occupancy cycle
		// We encode the alloy cycle as multiple pseudo-sites, all at (0,0,0);
		// the builder reads them in round-robin fashion.
		m.basis = {
			{ "Ni", 28, {0.0, 0.0, 0.0},  -0.05 },
			{ "Ni", 28, {0.0, 0.0, 0.0},  -0.05 },
			{ "Ni", 28, {0.0, 0.0, 0.0},  -0.05 },
			{ "Cr", 24, {0.0, 0.0, 0.0},  +0.08 },
			{ "Mo", 42, {0.0, 0.0, 0.0},  +0.04 },
			{ "Nb", 41, {0.0, 0.0, 0.0},  +0.06 },
		};
		db.push_back(std::move(m));
	}

	// ------------------------------------------------------------------
	// 4. Ti-6Al-4V — HCP aerospace alloy
	//    a = 2.921 Å, c = 4.620 Å  (α-phase, 300 K)
	//    HCP basis: (0,0,0) and (1/3,2/3,1/2)
	//    Alloy cycle: Ti Ti Ti Al V  (per 5 HCP atoms)
	// ------------------------------------------------------------------
	{
		MaterialPreset m;
		m.name               = "Ti-6Al-4V";
		m.tag                = "Ti64_HCP";
		m.lattice            = LatticeType::HCP;
		m.a                  = 2.921;
		m.c                  = 4.620;
		m.density_gcc        = 4.43;
		m.composition        = "Ti-90 Al-6 V-4 wt% (nom.)";
		m.ref_energy_per_atom = -113.0;

		// Two-atom HCP primitive cell; substitution cycle applied across tiled atoms
		m.basis = {
			{ "Ti", 22, {0.000, 0.000, 0.000},  +0.07 },
			{ "Ti", 22, {0.333, 0.667, 0.500},  +0.07 },
		};
		// Alloy occupancy cycle (applied after supercell tiling, before output)
		// Encoded separately so the builder can substitute on demand.
		// Format: { symbol, Z, charge }
		// "Ti Ti Ti Ti Ti Ti Al Al V V" → 60% Ti, 20% Al, 20% V (per 10)
		db.back().composition += " | cycle: Ti*6 Al*2 V*2 per 10 atoms";
		db.push_back(std::move(m));
	}

	return db;
}

// ---------------------------------------------------------------------------
// Alloy occupancy cycle for substitutional disorder
// ---------------------------------------------------------------------------
struct OccupancyCycle {
	std::string  symbol;
	int          Z;
	double       q;
};

// Returns the substitution cycle for a given material tag.
// FCC random-alloy and HCP alloys use these; B2/pure BCC ignores them.
inline std::vector<OccupancyCycle> occupancy_cycle(const std::string& tag) {
	if (tag == "IN625_FCC") {
		return {
			{"Ni",28,-0.05},{"Ni",28,-0.05},{"Ni",28,-0.05},
			{"Cr",24,+0.08},{"Mo",42,+0.04},{"Nb",41,+0.06}
		};
	}
	if (tag == "Ti64_HCP") {
		return {
			{"Ti",22,+0.07},{"Ti",22,+0.07},{"Ti",22,+0.07},
			{"Ti",22,+0.07},{"Ti",22,+0.07},{"Ti",22,+0.07},
			{"Al",13,-0.04},{"Al",13,-0.04},
			{"V", 23,+0.02},{"V", 23,+0.02}
		};
	}
	return {}; // pure / ordered: no substitution
}

} // namespace gen
} // namespace vsepr
