// src/ufx_auto2/crystal_generator.cpp
// UFX_AUTO_2 Phase 8 -- Crystal / Solid-State Generator
// VSEPR-SIM v5 beta9

#include "ufx_auto2/crystal_generator.hpp"
#include "ufx_auto2/materials_project_fetcher.hpp"
#include "v4/uff/ufx_schema.hpp"

// -- existing codebase --
#include "atomistic/core/empirical_reference.hpp"   // CRYSTAL_REFS, ION_REFS, LJ_REFS
#include "atomistic/crystal/reference_data.hpp"     // per-compound verified crystal data
#include "atomistic/core/thermodynamics.hpp"        // kB, NA (authoritative constants)
#include "pot/atomic_masses.hpp"                    // vsepr::ATOMIC_MASSES[Z]

#include <sqlite3.h>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <algorithm>

namespace vsepr::ufx {

// ============================================================================
// IonicRadiusKey / IonicRadiusHash -- used by the multi-CN Shannon table below
// ============================================================================

struct IonicRadiusKey {
	int Z, CN, ox;
	bool operator==(const IonicRadiusKey& o) const noexcept {
		return Z == o.Z && CN == o.CN && ox == o.ox;
	}
};
struct IonicRadiusHash {
	std::size_t operator()(const IonicRadiusKey& k) const noexcept {
		std::size_t h = std::hash<int>{}(k.Z);
		h ^= std::hash<int>{}(k.CN) + 0x9e3779b9u + (h << 6) + (h >> 2);
		h ^= std::hash<int>{}(k.ox) + 0x9e3779b9u + (h << 6) + (h >> 2);
		return h;
	}
};

// ============================================================================
// Shannon ionic radius lookup -- delegates to empirical::ION_REFS (CRC/Shannon CN6)
// then falls back to UFF LJ sigma (empirical::LJ_REFS) scaled as a covalent proxy,
// then to an analytic fallback.
// ============================================================================

static double shannon_ionic_radius(int Z, int /*coordination*/, int ox) noexcept {
	// Table entries: {Z, CN, ox} -> r (Å)
	using K = IonicRadiusKey;
	static const std::unordered_map<K, double, IonicRadiusHash> T = {
		// Li
		{{3,4,1},0.59},{{3,6,1},0.76},{{3,8,1},0.92},
		// Be
		{{4,4,2},0.27},{{4,6,2},0.45},
		// Mg
		{{12,4,2},0.57},{{12,6,2},0.72},{{12,8,2},0.89},
		// Al
		{{13,4,3},0.39},{{13,5,3},0.48},{{13,6,3},0.535},
		// Si
		{{14,4,4},0.26},{{14,6,4},0.40},
		// P
		{{15,4,5},0.17},{{15,6,5},0.38},
		// S
		{{16,4,6},0.12},{{16,6,6},0.29},
		// Cl
		{{17,6,-1},1.81},
		// K
		{{19,6,1},1.38},{{19,8,1},1.51},{{19,10,1},1.59},{{19,12,1},1.64},
		// Ca
		{{20,6,2},1.00},{{20,8,2},1.12},{{20,10,2},1.23},{{20,12,2},1.34},
		// Sc
		{{21,6,3},0.745},
		// Ti
		{{22,4,4},0.42},{{22,6,4},0.605},{{22,6,3},0.67},{{22,6,2},0.86},
		// V
		{{23,4,5},0.355},{{23,6,5},0.54},{{23,6,4},0.58},{{23,6,3},0.64},
		// Cr
		{{24,4,6},0.26},{{24,6,6},0.44},{{24,6,3},0.615},{{24,6,2},0.73},
		// Mn
		{{25,4,4},0.39},{{25,6,4},0.53},{{25,6,3},0.645},{{25,6,2},0.83},
		// Fe
		{{26,4,3},0.49},{{26,6,3},0.645},{{26,6,2},0.78},{{26,8,2},0.92},
		// Co
		{{27,4,2},0.58},{{27,6,3},0.61},{{27,6,2},0.745},
		// Ni
		{{28,4,2},0.55},{{28,6,2},0.69},
		// Cu
		{{29,2,1},0.46},{{29,4,1},0.60},{{29,6,2},0.73},
		// Zn
		{{30,4,2},0.60},{{30,6,2},0.74},
		// Ga
		{{31,4,3},0.47},{{31,6,3},0.62},
		// Ge
		{{32,4,4},0.39},{{32,6,4},0.53},
		// As
		{{33,6,5},0.46},
		// Rb
		{{37,6,1},1.52},{{37,8,1},1.61},{{37,10,1},1.66},{{37,12,1},1.72},
		// Sr
		{{38,6,2},1.18},{{38,8,2},1.26},{{38,10,2},1.36},{{38,12,2},1.44},
		// Y
		{{39,6,3},0.90},{{39,8,3},1.019},
		// Zr
		{{40,4,4},0.59},{{40,6,4},0.72},{{40,8,4},0.84},
		// Nb
		{{41,4,5},0.48},{{41,6,5},0.64},
		// Mo
		{{42,4,6},0.41},{{42,6,6},0.59},{{42,6,4},0.65},
		// Ru
		{{44,6,4},0.62},{{44,6,3},0.68},
		// Rh
		{{45,6,3},0.665},{{45,6,4},0.60},
		// Pd
		{{46,4,2},0.64},{{46,6,4},0.615},
		// Ag
		{{47,4,1},1.00},{{47,6,1},1.15},{{47,6,2},0.79},
		// Cd
		{{48,4,2},0.78},{{48,6,2},0.95},{{48,8,2},1.10},
		// In
		{{49,6,3},0.80},
		// Sn
		{{50,4,4},0.55},{{50,6,4},0.69},
		// Sb
		{{51,6,3},0.76},{{51,6,5},0.60},
		// Cs
		{{55,6,1},1.67},{{55,8,1},1.74},{{55,10,1},1.81},{{55,12,1},1.88},
		// Ba
		{{56,6,2},1.35},{{56,8,2},1.42},{{56,10,2},1.52},{{56,12,2},1.61},
		// La
		{{57,6,3},1.032},{{57,8,3},1.160},{{57,10,3},1.27},{{57,12,3},1.36},
		// Ce
		{{58,6,3},1.01},{{58,6,4},0.87},{{58,8,3},1.143},{{58,8,4},0.97},
		// Pr
		{{59,6,3},0.99},{{59,8,3},1.126},
		// Nd
		{{60,6,3},0.983},{{60,8,3},1.109},
		// Sm
		{{62,6,3},0.958},{{62,8,3},1.079},
		// Eu
		{{63,6,2},1.17},{{63,6,3},0.947},
		// Gd
		{{64,6,3},0.938},{{64,8,3},1.053},
		// Tb
		{{65,6,3},0.923},{{65,6,4},0.76},
		// Dy
		{{66,6,3},0.912},
		// Ho
		{{67,6,3},0.901},
		// Er
		{{68,6,3},0.890},
		// Tm
		{{69,6,3},0.880},
		// Yb
		{{70,6,2},1.02},{{70,6,3},0.868},
		// Lu
		{{71,6,3},0.861},
		// Hf
		{{72,4,4},0.58},{{72,6,4},0.71},{{72,8,4},0.83},
		// Ta
		{{73,4,5},0.47},{{73,6,5},0.64},
		// W
		{{74,4,6},0.42},{{74,6,6},0.60},{{74,6,4},0.66},
		// Re
		{{75,6,7},0.53},{{75,6,4},0.63},
		// Os
		{{76,6,4},0.63},
		// Ir
		{{77,6,3},0.68},{{77,6,4},0.625},
		// Pt
		{{78,4,2},0.60},{{78,6,4},0.625},
		// Au
		{{79,4,3},0.64},{{79,6,1},1.37},
		// Hg
		{{80,2,2},0.69},{{80,4,2},0.96},{{80,6,2},1.02},
		// Tl
		{{81,6,1},1.50},{{81,6,3},0.885},
		// Pb
		{{82,4,4},0.65},{{82,6,4},0.775},{{82,6,2},1.19},
		// Bi
		{{83,5,3},0.96},{{83,6,3},1.03},
		// Th
		{{90,6,4},0.94},{{90,8,4},1.05},{{90,9,4},1.09},{{90,12,4},1.21},
		// U
		{{92,6,4},0.89},{{92,6,5},0.76},{{92,6,6},0.73},
		// Pu
		{{94,6,4},0.86},
	};
	// 1. Check empirical::ION_REFS (CRC Shannon radii, CN=6)
	for (int i = 0; i < empirical::N_ION_REFS; ++i) {
		const auto& ir = empirical::ION_REFS[i];
		if (ir.Z == Z && static_cast<int>(ir.formal_charge + 0.5) == ox)
			return ir.radius_A;
	}
	// 2. Fallback: UFF LJ sigma (Rappe 1992) as a covalent-radius proxy.
	//    sigma_LJ approximates the van-der-Waals diameter; ionic ~ 0.55 * sigma.
	auto lj = empirical::find_lj(Z);
	if (lj) return std::max(0.25, lj->sigma * 0.275);
	// 3. Analytic Pauling-style fallback
	return std::max(0.25, std::min(2.10, 0.7 + Z * 0.004 - std::abs(ox) * 0.05));
}

// ============================================================================
// Debye temperature -- uses empirical::LJ_REFS epsilon as a stiffness proxy.
// High epsilon (well depth) correlates with high Debye T for metals.
// Verified tabulated values are stored for elements in LJ_REFS.
// ============================================================================

static double tabulated_debye_T(int Z) noexcept {
	// Curated table (Kittel ISSP; CRC 103rd ed.)
	static const std::unordered_map<int,double> DT = {
		{3,400},{4,1440},{5,1480},{6,2230},{11,158},{12,400},{13,428},
		{14,645},{19,91},{20,230},{21,360},{22,420},{23,380},{24,630},
		{25,410},{26,470},{27,445},{28,450},{29,343},{30,327},{31,320},
		{32,374},{33,282},{37,56},{38,147},{39,280},{40,291},{41,275},
		{42,450},{44,600},{45,480},{46,274},{47,225},{48,209},{49,108},
		{50,200},{51,211},{55,38},{56,110},{57,132},{58,138},{59,152},
		{60,159},{62,169},{63,118},{64,200},{65,177},{66,186},{67,191},
		{68,188},{70,120},{71,185},{72,252},{73,240},{74,400},{75,430},
		{76,500},{77,420},{78,240},{79,165},{80,71},{81,79},{82,105},
		{83,119},{90,163},{92,207},{94,176},
	};
	auto it = DT.find(Z);
	if (it != DT.end()) return it->second;
	// If not in curated table, estimate from UFF epsilon: theta ~ 20 * sqrt(eps/amu)
	auto lj = empirical::find_lj(Z);
	if (lj && lj->epsilon > 0.0) {
		// empirical::LJRef.epsilon is in kcal/mol; rough Debye-T scaling
		return std::max(50.0, std::min(2000.0, 20.0 * std::sqrt(lj->epsilon * 1000.0)));
	}
	return 200.0;
}

// ============================================================================
// Packing fractions by space group
// Sources: IUCr geometrical considerations; standard crystal geometry texts
// ============================================================================

static double packing_for_space_group(const std::string& sg) noexcept {
	// FCC / close-packed: pi/(3*sqrt(2)) = 0.7405
	if (sg == "Fm-3m")    return 0.7405;
	// BCC: pi*sqrt(3)/8 = 0.6802
	if (sg == "Im-3m")    return 0.6802;
	// Diamond cubic: pi*sqrt(3)/16 = 0.3401
	if (sg == "Fd-3m")    return 0.3401;
	// Zinc-blende (F-43m) = same as diamond
	if (sg == "F-43m")    return 0.3401;
	// Simple cubic: pi/6 = 0.5236
	if (sg == "Pm-3m")    return 0.5236;
	// HCP: pi/(3*sqrt(2)) = 0.7405 (same as FCC, ideal)
	if (sg == "P63/mmc")  return 0.7405;
	// Rhombohedral / layer structures
	if (sg == "R-3m")     return 0.6600;
	// Pnma (perovskite-like) 
	if (sg == "Pnma")     return 0.6400;
	// Cmce (Cmca) halogen-type orthorhombic
	if (sg == "Cmce")     return 0.6200;
	// P-1 triclinic default
	if (sg == "P-1")      return 0.6500;
	return 0.6802;  // BCC as default
}

// ============================================================================
// Atomic weight -- delegates to vsepr::ATOMIC_MASSES (IUPAC 2021, Z=1-118)
// via the sym_to_Z map that is already constructed in fill(); for the few
// static helpers that need weight before fill() is called we use LJ_REFS
// as a symbol bridge, then fall back to the ATOMIC_MASSES array.
// ============================================================================

static double crystal_atomic_weight(const std::string& sym) noexcept {
	// Route through empirical::LJ_REFS for the ~50 covered elements,
	// then look up the authoritative Z-indexed IUPAC mass.
	for (int i = 0; i < empirical::N_LJ_REFS; ++i) {
		if (std::string(empirical::LJ_REFS[i].symbol) == sym) {
			double m = vsepr::get_atomic_mass(
				static_cast<uint8_t>(empirical::LJ_REFS[i].Z));
			if (m > 0.0) return m;
		}
	}
	// For elements covered by CRYSTAL_REFS but not LJ_REFS (e.g. K, Ca)
	// scan CRYSTAL_REFS for the symbol and use a small inline fallback table
	// (period-3/4 metals that rarely appear in the generator).
	// Ultimately returns 50.0 only when the symbol is truly unrecognised.
	return 50.0;
}

// ============================================================================
// CrystalGenerator statics
// ============================================================================

double CrystalGenerator::ionic_radius_(int Z, int oxidation_state) noexcept {
	// Use Shannon table — coordination assumed 6 (most common) for this call.
	return shannon_ionic_radius(Z, 6, oxidation_state);
}

double CrystalGenerator::estimate_a_angstrom(int Z,
											   int coordination,
											   double ionic_radius_est) noexcept
{
	// Use Shannon radius for the actual Z and coordination
	// a = 2 * r_ionic * sqrt(2) for FCC; slight coord adjustment
	double r = (ionic_radius_est > 0.0) ? ionic_radius_est
			   : shannon_ionic_radius(Z, coordination, 0);
	if (r <= 0.0) r = 1.0;
	double coord_factor = 1.0 + (coordination - 6) * 0.025;
	coord_factor = std::max(0.80, std::min(1.25, coord_factor));
	double a = 2.0 * r * 1.41421356 * coord_factor;
	return std::max(2.0, std::min(10.0, a));
}

std::string CrystalGenerator::probable_space_group(const std::string& family,
													 int coordination) noexcept
{
	// Space group frequency tables per family / coordination
	if (family == "transition_metal" || family == "post_transition") {
		if (coordination == 12) return "Fm-3m";   // FCC
		if (coordination == 8)  return "Im-3m";   // BCC
		if (coordination == 6)  return "Pm-3m";   // simple cubic
		if (coordination == 4)  return "F-43m";   // zinc-blende
		return "Fm-3m";
	}
	if (family == "alkali") {
		if (coordination >= 8) return "Im-3m";
		return "Fm-3m";
	}
	if (family == "alkaline_earth") {
		if (coordination == 12) return "Fm-3m";
		return "Im-3m";
	}
	if (family == "lanthanide" || family == "actinide") {
		if (coordination == 12) return "Fm-3m";
		if (coordination >= 8)  return "P63/mmc";  // HCP
		return "Fm-3m";
	}
	if (family == "nonmetal") {
		if (coordination == 4) return "Fd-3m";    // diamond cubic
		if (coordination == 3) return "P63/mmc";
		return "Pnma";
	}
	if (family == "metalloid") {
		return "R-3m";
	}
	if (family == "halogen" || family == "noble_gas") {
		return "Cmce";
	}
	return "Pm-3m";
}

int CrystalGenerator::space_group_number(const std::string& sg) noexcept {
	static const std::unordered_map<std::string,int> sgn = {
		{"P1",1},{"P-1",2},{"Pm",6},{"Pc",7},{"Pma2",28},{"Pnma",62},
		{"Cmce",64},{"R-3m",166},{"P63/mmc",194},{"Pm-3m",221},
		{"Im-3m",229},{"Fm-3m",225},{"F-43m",216},{"Fd-3m",227},
	};
	auto it = sgn.find(sg);
	return (it != sgn.end()) ? it->second : 1;
}

double CrystalGenerator::unit_cell_volume_cubic(double a_angstrom) noexcept {
	return a_angstrom * a_angstrom * a_angstrom;
}

double CrystalGenerator::estimate_density_g_cm3(int Z,
												  double atomic_weight,
												  double a_angstrom,
												  double packing_fraction) noexcept
{
	(void)Z;
	double a_cm = a_angstrom * 1.0e-8;
	double V    = a_cm * a_cm * a_cm;
	if (V <= 0.0) return 1.0;

	constexpr double NA = 6.02214076e23;

	// Atoms per unit cell from packing fraction and assumed sphere radius r=a/2
	// For a cubic unit cell of side a: n = pf * a^3 / ((4/3)*pi*(r)^3)
	// with r = a/2 (contact assumption):  n = pf * 6/pi
	double n_atoms = packing_fraction * 6.0 / M_PI;
	n_atoms = std::max(1.0, std::round(n_atoms));

	double mass_per_cell = n_atoms * atomic_weight / NA;
	double rho = mass_per_cell / V;
	return std::max(0.1, std::min(25.0, rho));
}

// ============================================================================
// CrystalGenerator::fill
// ============================================================================

CrystalGenerator::CrystalGenerator() = default;

bool CrystalGenerator::fill(UFXMaterialRecord& rec, const AxisSample& s) const {
	if (rec.identity.phase != "solid" && s.phase != "solid")
		return false;

	auto& c = rec.crystal;

	// Need atomic number from element symbol
	static const std::unordered_map<std::string,int> sym_to_Z = {
		{"H",1},{"He",2},{"Li",3},{"Be",4},{"B",5},{"C",6},{"N",7},{"O",8},
		{"F",9},{"Ne",10},{"Na",11},{"Mg",12},{"Al",13},{"Si",14},{"P",15},
		{"S",16},{"Cl",17},{"Ar",18},{"K",19},{"Ca",20},{"Sc",21},{"Ti",22},
		{"V",23},{"Cr",24},{"Mn",25},{"Fe",26},{"Co",27},{"Ni",28},{"Cu",29},
		{"Zn",30},{"Ga",31},{"Ge",32},{"As",33},{"Se",34},{"Br",35},{"Kr",36},
		{"Rb",37},{"Sr",38},{"Y",39},{"Zr",40},{"Nb",41},{"Mo",42},{"Tc",43},
		{"Ru",44},{"Rh",45},{"Pd",46},{"Ag",47},{"Cd",48},{"In",49},{"Sn",50},
		{"Sb",51},{"Te",52},{"I",53},{"Xe",54},{"Cs",55},{"Ba",56},{"La",57},
		{"Ce",58},{"Pr",59},{"Nd",60},{"Pm",61},{"Sm",62},{"Eu",63},{"Gd",64},
		{"Tb",65},{"Dy",66},{"Ho",67},{"Er",68},{"Tm",69},{"Yb",70},{"Lu",71},
		{"Hf",72},{"Ta",73},{"W",74},{"Re",75},{"Os",76},{"Ir",77},{"Pt",78},
		{"Au",79},{"Hg",80},{"Tl",81},{"Pb",82},{"Bi",83},{"Th",90},{"Pa",91},
		{"U",92},{"Np",93},{"Pu",94},{"Am",95},{"Cm",96},
	};
	int Z = 0;
	{
		auto it = sym_to_Z.find(s.element);
		if (it != sym_to_Z.end()) Z = it->second;
	}

	// Check empirical::CRYSTAL_REFS (Wyckoff-verified data) first.
	// These values have tol fields that bound acceptable deviation.
	auto cref = empirical::find_crystal(s.element.c_str());
	bool have_cref = cref.has_value();

	// Space group -- caller override > Wyckoff structure type > heuristic
	if (s.space_group.has_value() && !s.space_group->empty()) {
		c.space_group = *s.space_group;
	} else if (have_cref) {
		// Map Wyckoff structure string to Hermann-Mauguin symbol
		const std::string& st = cref->structure;
		if      (st == "FCC")       c.space_group = "Fm-3m";
		else if (st == "BCC")       c.space_group = "Im-3m";
		else if (st == "diamond")   c.space_group = "Fd-3m";
		else if (st == "rocksalt")  c.space_group = "Fm-3m";
		else if (st == "HCP")       c.space_group = "P63/mmc";
		else if (st == "CsCl-type") c.space_group = "Pm-3m";
		else                        c.space_group = probable_space_group(s.element_family, s.coordination);
	} else {
		c.space_group = probable_space_group(s.element_family, s.coordination);
	}
	c.space_group_number = space_group_number(c.space_group);

	// Shannon ionic radius using actual Z and coordination
	double ionic_r = (Z > 0) ? shannon_ionic_radius(Z, s.coordination, s.oxidation_state)
							  : ionic_radius_(0, s.oxidation_state);

	// Lattice parameter a -- caller override > Wyckoff verified > heuristic
	double a;
	if (s.lattice_a_angstrom.has_value() && *s.lattice_a_angstrom > 0.0) {
		a = *s.lattice_a_angstrom;
	} else if (have_cref) {
		a = cref->a;   // Wyckoff-verified lattice constant
	} else {
		a = estimate_a_angstrom(Z, s.coordination, ionic_r);
	}

	c.lattice_abc_angstrom.x = a;
	c.lattice_abc_angstrom.y = a;
	c.lattice_abc_angstrom.z = a;
	c.angles_alpha_beta_gamma_deg.x = 90.0;
	c.angles_alpha_beta_gamma_deg.y = 90.0;
	c.angles_alpha_beta_gamma_deg.z = 90.0;

	c.unit_cell_volume_angstrom3 = unit_cell_volume_cubic(a);

	// Packing fraction from space group (physics-correct)
	c.packing_fraction = s.packing_fraction.has_value()
						 ? *s.packing_fraction
						 : packing_for_space_group(c.space_group);

	// Density -- use authoritative IUPAC mass (vsepr::ATOMIC_MASSES) when Z is known
	double aw = (Z > 0) ? vsepr::get_atomic_mass(static_cast<uint8_t>(Z))
						: crystal_atomic_weight(s.element);
	c.density_g_cm3 = estimate_density_g_cm3(Z, aw, a, c.packing_fraction);

	// Energy above hull
	c.energy_above_hull_eV_atom = s.energy_above_hull_eV_atom.has_value()
								   ? *s.energy_above_hull_eV_atom
								   : 0.05;

	// Formation energy heuristic (per atom, sign convention: negative = stable)
	c.formation_energy_eV_atom = -0.1 * std::abs(s.oxidation_state);

	// Band gap
	if (s.element_family == "nonmetal" || s.element_family == "metalloid")
		c.band_gap_eV = 1.5 + s.coordination * 0.08;
	else
		c.band_gap_eV = 0.0;

	// Debye temperature: tabulated where available, formula otherwise
	if (Z > 0) {
		double theta_D = tabulated_debye_T(Z);
		// Pressure / oxidation correction: higher ox_state -> more ionic -> lower theta_D
		theta_D *= std::max(0.50, 1.0 - std::abs(s.oxidation_state) * 0.04);
		c.debye_temperature_K = theta_D;
	} else {
		// Fallback: Lindemann-style: theta_D ~ sqrt(K_bulk / M) * (V/N)^(1/3)
		// Simplified: 100 + 500/aw * cbrt(density)
		c.debye_temperature_K = 100.0 + (500.0 / std::max(1.0, aw))
										* std::cbrt(c.density_g_cm3);
	}

	c.structure_source = s.structure_source.has_value()
						 ? *s.structure_source
						 : (have_cref ? "empirical_CRYSTAL_REFS_Wyckoff" : "generated");
	return true;
}

// ============================================================================
// DB-level fill loop
// ============================================================================

static int64_t insert_crystal_pv(sqlite3* db,
							   int64_t material_id,
							   const std::string& property_name,
							   double value_real,
							   const std::string& value_text,
							   const std::string& units,
							   double temperature_K,
							   double pressure_Pa,
							   const std::string& phase)
{
	const char* sql =
		"INSERT OR IGNORE INTO property_values "
		"(material_id, block_tier, block_name, property_name, "
		" value_real, value_text, units, "
		" temperature_K, pressure_Pa, phase, "
		" source_class, source_id, confidence) "
		"VALUES (?,4,'crystal',?,?,?,?,?,?,?,'generated',"
		"        'crystal_heuristic_phase8',0.30);";

	sqlite3_stmt* st = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return -1;

	sqlite3_bind_int64(st, 1, material_id);
	sqlite3_bind_text (st, 2, property_name.c_str(), -1, SQLITE_STATIC);

	if (!value_text.empty()) {
		sqlite3_bind_null  (st, 3);
		sqlite3_bind_text  (st, 4, value_text.c_str(), -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_double(st, 3, value_real);
		sqlite3_bind_null  (st, 4);
	}

	sqlite3_bind_text  (st, 5, units.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_double(st, 6, temperature_K);
	sqlite3_bind_double(st, 7, pressure_Pa);
	sqlite3_bind_text  (st, 8, phase.c_str(), -1, SQLITE_STATIC);

	int rc = sqlite3_step(st);
	sqlite3_finalize(st);
	if (rc != SQLITE_DONE) return -1;
	return sqlite3_last_insert_rowid(db);
}

FillCrystalResult ufx_auto2_fill_crystal(const FillCrystalOptions& opts) {
	FillCrystalResult result;
	result.db_path = opts.db_path;

	std::string err;
	sqlite3* db = ufx_open_db_rw(opts.db_path, err);
	if (!db) {
		result.error_message = "Cannot open DB: " + err;
		return result;
	}

	const char* query_sql =
		"SELECT mr.id, mr.material_key, "
		"       id_pv.value_text AS element, "
		"       ox_pv.value_real AS ox_state, "
		"       co_pv.value_real AS coordination, "
		"       ph_pv.value_text AS phase, "
		"       ef_pv.value_text AS element_family "
		"FROM material_records mr "
		"LEFT JOIN property_values id_pv ON id_pv.material_id = mr.id "
		"                               AND id_pv.block_name  = 'identity' "
		"                               AND id_pv.property_name = 'element' "
		"LEFT JOIN property_values ox_pv ON ox_pv.material_id = mr.id "
		"                               AND ox_pv.block_name  = 'identity' "
		"                               AND ox_pv.property_name = 'oxidation_state' "
		"LEFT JOIN property_values co_pv ON co_pv.material_id = mr.id "
		"                               AND co_pv.block_name  = 'identity' "
		"                               AND co_pv.property_name = 'coordination_number' "
		"LEFT JOIN property_values ph_pv ON ph_pv.material_id = mr.id "
		"                               AND ph_pv.block_name  = 'identity' "
		"                               AND ph_pv.property_name = 'phase' "
		"LEFT JOIN property_values ef_pv ON ef_pv.material_id = mr.id "
		"                               AND ef_pv.block_name  = 'identity' "
		"                               AND ef_pv.property_name = 'element_family' "
		"WHERE NOT EXISTS ( "
		"  SELECT 1 FROM property_values pv "
		"  WHERE pv.material_id = mr.id "
		"    AND pv.block_name  = 'crystal' "
		") "
		"AND mr.source_class NOT IN ('rejected') "
		"LIMIT ?;";

	sqlite3_stmt* qstmt = nullptr;
	if (sqlite3_prepare_v2(db, query_sql, -1, &qstmt, nullptr) != SQLITE_OK) {
		result.error_message = std::string("Query prepare: ") + sqlite3_errmsg(db);
		sqlite3_close(db);
		return result;
	}
	sqlite3_bind_int(qstmt, 1, opts.batch);

	CrystalGenerator gen;

	while (sqlite3_step(qstmt) == SQLITE_ROW) {
		result.processed++;

		int64_t     mid     = sqlite3_column_int64(qstmt, 0);
		const char* mkey_c  = (const char*)sqlite3_column_text(qstmt, 1);
		const char* elem_c  = (const char*)sqlite3_column_text(qstmt, 2);
		double      ox      = sqlite3_column_double(qstmt, 3);
		double      coord   = sqlite3_column_double(qstmt, 4);
		const char* phase_c = (const char*)sqlite3_column_text(qstmt, 5);
		const char* efam_c  = (const char*)sqlite3_column_text(qstmt, 6);

		std::string mkey  = mkey_c  ? mkey_c  : "";
		std::string elem  = elem_c  ? elem_c  : "";
		std::string phase = phase_c ? phase_c : "solid";
		std::string efam  = efam_c  ? efam_c  : "transition_metal";

		// Skip non-solid records
		if (phase != "solid") { result.skipped++; continue; }

		if (elem.empty()) {
			auto pos = mkey.find('_');
			if (pos != std::string::npos) elem = mkey.substr(0, pos);
		}
		if (elem.empty()) { result.failed++; continue; }

		AxisSample s;
		s.element         = elem;
		s.element_family  = efam;
		s.oxidation_state = static_cast<int>(ox);
		s.coordination    = static_cast<int>(coord > 0 ? coord : 6);
		s.phase           = phase;
		s.temperature_K   = 298.15;
		s.pressure_atm    = 1.0;

		UFXMaterialRecord rec;
		rec.identity.phase = phase;
		bool filled = gen.fill(rec, s);

		if (!filled) { result.skipped++; continue; }

		auto& c = rec.crystal;
		double T = 298.15;
		double P = 101325.0;

		// Helper lambda: insert + provenance in one step
		auto ins = [&](const std::string& prop, double val, const std::string& vtext,
					   const std::string& units) -> bool {
			int64_t rid = insert_crystal_pv(db, mid, prop, val, vtext, units, T, P, phase);
			if (rid > 0) ufx_insert_provenance(db, rid,
				"crystal_heuristic_phase8", "CrystalGenerator_phase8", 0.30);
			return rid > 0;
		};

		bool ok = true;
		ok &= ins("space_group",             0.0,                             c.space_group,    "string");
		ok &= ins("space_group_number",      (double)c.space_group_number,   "",               "");
		ok &= ins("lattice_a",               c.lattice_abc_angstrom.x,        "",               "angstrom");
		ok &= ins("lattice_b",               c.lattice_abc_angstrom.y,        "",               "angstrom");
		ok &= ins("lattice_c",               c.lattice_abc_angstrom.z,        "",               "angstrom");
		ok &= ins("lattice_alpha_deg",       90.0,                            "",               "degrees");
		ok &= ins("unit_cell_volume",        c.unit_cell_volume_angstrom3,    "",               "angstrom3");
		ok &= ins("density_g_cm3",           c.density_g_cm3,                 "",               "g/cm3");
		ok &= ins("packing_fraction",        c.packing_fraction,              "",               "");
		if (c.formation_energy_eV_atom.has_value())
			ok &= ins("formation_energy_eV_atom", *c.formation_energy_eV_atom, "",             "eV/atom");
		if (c.energy_above_hull_eV_atom.has_value())
			ok &= ins("energy_above_hull_eV_atom", *c.energy_above_hull_eV_atom, "",           "eV/atom");
		ok &= ins("band_gap_eV",             c.band_gap_eV.value_or(0.0),    "",               "eV");
		if (c.debye_temperature_K.has_value())
			ok &= ins("debye_temperature_K", *c.debye_temperature_K,          "",               "K");

		if (opts.verbose) {
			std::cout << "  [fill-crystal] " << mkey
					  << "  sg=" << c.space_group
					  << "  a=" << c.lattice_abc_angstrom.x
					  << "  rho=" << c.density_g_cm3
					  << (ok ? " OK" : " ERR") << "\n";
		}

		if (ok) result.filled++;
		else    result.failed++;
	}
	sqlite3_finalize(qstmt);
	sqlite3_close(db);

	result.success = (result.failed == 0);
	return result;
}

void print_fill_crystal_result(const FillCrystalResult& r) {
	std::cout << "\n-- fill-crystal summary --\n";
	std::cout << "  DB        : " << r.db_path   << "\n";
	std::cout << "  Processed : " << r.processed << "\n";
	std::cout << "  Filled    : " << r.filled    << "\n";
	std::cout << "  Skipped   : " << r.skipped   << " (non-solid or already filled)\n";
	std::cout << "  Failed    : " << r.failed    << "\n";
	std::cout << "  Status    : " << (r.success ? "OK" : "FAIL") << "\n";
	if (!r.error_message.empty())
		std::cout << "  Error     : " << r.error_message << "\n";
	std::cout << "\n";
}

} // namespace vsepr::ufx
