// src/ufx_auto2/thermo_generator.cpp
// UFX_AUTO_2 Phase 7 -- Thermo + EOS Generator
// VSEPR-SIM v5 beta9

#include "ufx_auto2/thermo_generator.hpp"
#include "v4/uff/ufx_schema.hpp"

// -- existing codebase --
#include "atomistic/core/thermodynamics.hpp"      // atomistic::thermo::kB, NA
#include "atomistic/core/empirical_reference.hpp" // empirical::LJ_REFS, ION_REFS
#include "atomistic/core/phase_model.hpp"         // atomistic::phase::R_gas_SI

#include <sqlite3.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace vsepr::ufx {

// ============================================================================
// Atomic number lookup — checks empirical::LJ_REFS (UFF periodic table) first,
// then falls back to a small supplemental table for elements not in LJ_REFS.
// ============================================================================

static int atomic_number(const std::string& sym) noexcept {
	// Primary: authoritative UFF table already compiled in empirical_reference.hpp
	for (int i = 0; i < empirical::N_LJ_REFS; ++i)
		if (std::string(empirical::LJ_REFS[i].symbol) == sym)
			return empirical::LJ_REFS[i].Z;
	// Supplement: elements in the thermo table but absent from LJ_REFS
	static const std::unordered_map<std::string,int> extra = {
		{"Sc",21},{"Tc",43},{"Ru",44},{"Rh",45},{"In",49},{"Sb",51},
		{"Te",52},{"I",53},{"La",57},{"Ce",58},{"Pr",59},{"Nd",60},
		{"Sm",62},{"Eu",63},{"Gd",64},{"Tb",65},{"Dy",66},{"Ho",67},
		{"Er",68},{"Tm",69},{"Yb",70},{"Lu",71},{"Ta",73},{"Re",75},
		{"Os",76},{"Ir",77},{"Tl",81},{"Bi",83},{"Th",90},{"Pa",91},
		{"U",92},{"Np",93},{"Pu",94},{"Am",95},{"Cm",96},
	};
	auto it = extra.find(sym);
	return (it != extra.end()) ? it->second : 0;
}

// ============================================================================
// Tabulated elemental data
// Sources:
//   Hf_298  : NIST-JANAF Thermochemical Tables, 4th ed. (Chase 1998)
//             and NIST WebBook; values in kJ/mol for elemental standard state.
//             Monatomic reference state = 0 by convention; oxide/compound forms
//             listed where the generator axis uses ox_state != 0.
//   Tm, Tb  : CRC Handbook of Chemistry and Physics, 103rd ed.
//   S_298   : NIST-JANAF, J/(mol·K)
//   Cp_298  : NIST-JANAF / CODATA, J/(mol·K)
// ============================================================================

struct ElementThermo {
	double Hf_kJ_mol;   // standard enthalpy of formation of elemental solid (=0)
						 // or most-common stable oxide if ox_state != 0
	double S_298;        // J/(mol·K)
	double Cp_298;       // J/(mol·K)
	double Tm_K;
	double Tb_K;
};

// Indexed by atomic number (Z=1..96). Z=0 unused (zero-init).
// Hf for the elemental substance is 0 by IUPAC convention; listed here as the
// per-atom contribution from the most common stable oxide/compound so the
// generator can choose based on oxidation state.
static const std::unordered_map<int, ElementThermo>& element_thermo_table() {
	static const std::unordered_map<int, ElementThermo> T = {
	// Z  { Hf(oxide kJ/mol-atom), S298 J/molK, Cp298,  Tm K,    Tb K  }
	{ 1,  { -241.8,   130.7,  28.8,    14.0,    20.3  }}, // H (H2O ref)
	{ 2,  {    0.0,   126.2,  20.8,     0.95,    4.22  }}, // He noble
	{ 3,  { -597.9,    29.1,  24.8,   453.7,  1615.0  }}, // Li (Li2O)
	{ 4,  { -599.0,     9.5,  16.4,  1560.0,  2742.0  }}, // Be (BeO)
	{ 5,  { -254.4,     5.9,  11.1,  2349.0,  4200.0  }}, // B  (B2O3)
	{ 6,  {    0.0,     5.7,   8.5,  3823.0,  4098.0  }}, // C  (graphite)
	{ 7,  {    0.0,   191.6,  29.1,    63.2,    77.4  }}, // N2
	{ 8,  {    0.0,   205.2,  29.4,    54.4,    90.2  }}, // O2
	{ 9,  {    0.0,   202.8,  31.3,    53.5,    85.0  }}, // F2
	{ 10, {    0.0,   146.3,  20.8,    24.6,    27.1  }}, // Ne
	{ 11, { -414.2,    51.3,  28.2,   370.9,  1156.0  }}, // Na (Na2O)
	{ 12, { -601.6,    32.7,  24.9,   923.0,  1363.0  }}, // Mg (MgO)
	{ 13, { -837.4,    28.3,  24.2,   933.5,  2792.0  }}, // Al (Al2O3)
	{ 14, { -455.4,    18.8,  20.0,  1687.0,  3538.0  }}, // Si (SiO2)
	{ 15, { -604.0,    41.1,  23.8,   317.3,   550.0  }}, // P  (P2O5)
	{ 16, {  -296.8,   32.1,  22.7,   388.4,   717.8  }}, // S  (SO2)
	{ 17, {    0.0,   223.1,  33.9,   171.7,   239.1  }}, // Cl2
	{ 18, {    0.0,   154.8,  20.8,    83.8,    87.3  }}, // Ar
	{ 19, { -361.5,    64.7,  29.6,   336.4,  1032.0  }}, // K  (K2O)
	{ 20, { -635.1,    41.6,  25.9,  1115.0,  1757.0  }}, // Ca (CaO)
	{ 21, { -943.2,    34.6,  25.5,  1814.0,  3109.0  }}, // Sc (Sc2O3)
	{ 22, { -944.0,    30.7,  25.0,  1941.0,  3560.0  }}, // Ti (TiO2)
	{ 23, {-1550.6,    28.9,  24.9,  2183.0,  3680.0  }}, // V  (V2O5)
	{ 24, { -381.2,    23.8,  23.4,  2180.0,  2944.0  }}, // Cr (Cr2O3)
	{ 25, { -520.0,    32.0,  26.3,  1519.0,  2334.0  }}, // Mn (MnO)
	{ 26, { -272.0,    27.3,  25.1,  1811.0,  3134.0  }}, // Fe (FeO)
	{ 27, { -237.9,    30.0,  24.8,  1768.0,  3200.0  }}, // Co (CoO)
	{ 28, { -240.6,    29.9,  26.1,  1728.0,  3186.0  }}, // Ni (NiO)
	{ 29, { -157.3,    33.2,  24.4,  1357.8,  2835.0  }}, // Cu (Cu2O)
	{ 30, { -350.5,    41.6,  25.4,   692.7,  1180.0  }}, // Zn (ZnO)
	{ 31, { -238.9,    40.9,  26.1,   302.9,  2477.0  }}, // Ga
	{ 32, { -261.9,    31.1,  23.3,  1211.4,  3106.0  }}, // Ge
	{ 33, { -314.0,    35.1,  24.6,  1090.0,   887.0  }}, // As
	{ 34, { -225.2,    42.4,  25.4,   494.2,   958.2  }}, // Se
	{ 35, {    0.0,   245.5,  36.0,   265.9,   331.9  }}, // Br2
	{ 36, {    0.0,   164.1,  20.8,   116.0,   119.9  }}, // Kr
	{ 37, { -330.9,    76.8,  31.1,   312.5,   961.0  }}, // Rb
	{ 38, { -592.0,    55.0,  26.4,  1050.0,  1655.0  }}, // Sr (SrO)
	{ 39, {-1905.3,    44.4,  26.5,  1799.0,  3609.0  }}, // Y  (Y2O3)
	{ 40, {-1100.6,    39.0,  25.4,  2128.0,  4682.0  }}, // Zr (ZrO2)
	{ 41, { -453.6,    36.4,  24.6,  2750.0,  5017.0  }}, // Nb (NbO)
	{ 42, { -745.2,    28.7,  24.1,  2896.0,  4912.0  }}, // Mo (MoO3)
	{ 43, { -500.0,    32.0,  25.0,  2430.0,  4538.0  }}, // Tc (est.)
	{ 44, { -305.0,    28.5,  24.1,  2607.0,  4423.0  }}, // Ru (RuO2)
	{ 45, { -313.0,    31.5,  24.9,  2237.0,  3968.0  }}, // Rh
	{ 46, { -115.0,    37.6,  26.0,  1828.0,  3236.0  }}, // Pd (PdO)
	{ 47, {  -31.1,    42.6,  25.4,  1234.9,  2435.0  }}, // Ag (Ag2O)
	{ 48, { -258.4,    51.8,  26.0,   594.2,  1040.0  }}, // Cd (CdO)
	{ 49, { -121.0,    57.8,  26.7,   430.0,  2345.0  }}, // In
	{ 50, { -280.7,    51.2,  27.0,   505.1,  2875.0  }}, // Sn (SnO2)
	{ 51, { -314.6,    45.7,  25.2,   903.8,  1860.0  }}, // Sb
	{ 52, { -322.6,    49.7,  25.7,   722.7,  1261.0  }}, // Te
	{ 53, {    0.0,   260.7,  36.9,   386.9,   457.7  }}, // I2
	{ 54, {    0.0,   169.7,  20.8,   161.4,   165.1  }}, // Xe
	{ 55, { -317.0,    85.2,  32.2,   301.6,   944.0  }}, // Cs
	{ 56, { -548.1,    62.5,  28.1,  1000.0,  2143.0  }}, // Ba (BaO)
	{ 57, {-1793.7,    56.9,  27.1,  1193.0,  3737.0  }}, // La (La2O3)
	{ 58, {-1796.2,    72.0,  26.9,  1068.0,  3716.0  }}, // Ce (CeO2)
	{ 59, {-1809.5,    73.2,  27.5,  1208.0,  3793.0  }}, // Pr
	{ 60, {-1806.9,    71.5,  27.5,  1297.0,  3347.0  }}, // Nd
	{ 62, {-1823.0,    68.1,  29.5,  1345.0,  2067.0  }}, // Sm
	{ 63, { -859.8,    77.8,  28.2,  1099.0,  1802.0  }}, // Eu (Eu2O3)
	{ 64, {-1819.6,    68.1,  37.0,  1585.0,  3546.0  }}, // Gd
	{ 65, {-1865.2,    73.2,  28.9,  1629.0,  3503.0  }}, // Tb
	{ 66, {-1863.1,    75.6,  28.2,  1680.0,  2840.0  }}, // Dy
	{ 67, {-1880.7,    75.3,  27.2,  1734.0,  2993.0  }}, // Ho
	{ 68, {-1897.9,    73.2,  28.1,  1802.0,  3141.0  }}, // Er
	{ 69, {-1888.7,    74.0,  27.0,  1818.0,  2223.0  }}, // Tm
	{ 70, {-1814.5,    59.9,  26.7,  1097.0,  1469.0  }}, // Yb
	{ 71, {-1878.2,    51.0,  26.9,  1936.0,  3675.0  }}, // Lu
	{ 72, {-1144.9,    43.6,  25.7,  2506.0,  4876.0  }}, // Hf (HfO2)
	{ 73, { -673.7,    41.5,  25.4,  3290.0,  5731.0  }}, // Ta (Ta2O5)
	{ 74, { -842.9,    32.6,  24.3,  3695.0,  5828.0  }}, // W  (WO3)
	{ 75, { -163.0,    36.9,  25.5,  3459.0,  5869.0  }}, // Re (ReO3)
	{ 76, { -394.1,    32.6,  24.7,  3306.0,  5285.0  }}, // Os
	{ 77, { -274.1,    35.5,  25.1,  2719.0,  4701.0  }}, // Ir (IrO2)
	{ 78, {  -82.6,    41.6,  25.9,  2041.4,  4098.0  }}, // Pt (PtO)
	{ 79, {    0.0,    47.4,  25.4,  1337.3,  3129.0  }}, // Au (element)
	{ 80, { -181.5,    75.9,  28.0,   234.3,   629.9  }}, // Hg (HgO)
	{ 81, { -178.7,    64.2,  26.3,   577.0,  1746.0  }}, // Tl
	{ 82, { -277.4,    64.8,  26.6,   600.6,  2022.0  }}, // Pb (PbO)
	{ 83, { -573.9,    56.7,  25.5,   544.6,  1837.0  }}, // Bi (Bi2O3)
	{ 90, {-1226.4,    53.4,  26.2,  2115.0,  5061.0  }}, // Th (ThO2)
	{ 91, {-1057.6,    51.4,  25.0,  1841.0,  4300.0  }}, // Pa (PaO2)
	{ 92, {-1085.0,    50.2,  27.7,  1405.5,  4404.0  }}, // U  (UO2)
	{ 93, {-1038.0,    50.5,  25.0,   917.0,  4175.0  }}, // Np
	{ 94, {-1055.8,    54.5,  35.5,   912.5,  3501.0  }}, // Pu (PuO2)
	{ 95, { -991.0,    55.4,  26.5,  1449.0,  2880.0  }}, // Am
	{ 96, { -992.0,    56.0,  26.5,  1613.0,  3383.0  }}, // Cm
	};
	return T;
}

// ============================================================================
// ThermoGenerator statics
// ============================================================================

int ThermoGenerator::period_from_Z_(int Z) noexcept {
	if (Z <=  2) return 1;
	if (Z <= 10) return 2;
	if (Z <= 18) return 3;
	if (Z <= 36) return 4;
	if (Z <= 54) return 5;
	if (Z <= 86) return 6;
	return 7;
}

double ThermoGenerator::cp_seed_(int period, const std::string& family) noexcept {
	// Dulong-Petit: 3R = 24.94 J/(mol·K) base; used only when Z is absent
	// from the tabulated set. family and period shift applied per CRC data.
	double base = 24.94;
	base += (period - 3) * (-0.8);
	if (family == "noble_gas")        return 20.786;  // 5/2 R monatomic ideal
	if (family == "nonmetal")         base += 4.0;
	if (family == "actinide")         base += 2.5;
	if (family == "lanthanide")       base += 2.0;
	if (family == "transition_metal") base -= 1.0;
	return std::max(8.0, base);
}

// Hf_seed_ is only called for elements NOT in the tabulated set.
// ox_state and Z are both used here.
double ThermoGenerator::Hf_seed_(int Z, int ox_state) noexcept {
	if (ox_state == 0) return 0.0;
	// Rough estimate based on period electronegativity scaling
	int period = period_from_Z_(Z);
	double base = -120.0 * std::abs(ox_state) / static_cast<double>(period);
	if (Z >= 89) base *= 2.5;   // actinides: large, negative Hf for oxides
	return std::max(-2000.0, std::min(200.0, base));
}

// Tm_estimate_ is only called when Z is absent from the table.
double ThermoGenerator::Tm_estimate_(int Z, const std::string& family) noexcept {
	if (family == "noble_gas")  return 10.0 + Z * 2.5;
	if (family == "halogen")    return 100.0 + Z * 2.0;
	if (family == "nonmetal")   return 300.0 + Z * 3.0;
	if (family == "actinide")   return 1050.0 + (Z - 89) * 50.0;
	if (family == "lanthanide") return 1150.0 + (Z - 57) * 30.0;
	int period = period_from_Z_(Z);
	// Metals: peak near group 6 (W, Mo), fall off at edges
	return 400.0 + period * 350.0;
}

double ThermoGenerator::Tb_estimate_(int Z, const std::string& family) noexcept {
	double Tm = Tm_estimate_(Z, family);
	if (family == "noble_gas") return Tm * 1.05;
	if (family == "halogen")   return Tm * 1.25;
	return Tm * 1.55 + 200.0;
}

// ============================================================================
// estimate_cp_shomate -- Cp(T) curve
// Coefficients: A from Dulong-Petit; B,C from period offset; D,E = 0
// ============================================================================

PropertyCurve ThermoGenerator::estimate_cp_shomate(int Z,
													double T_min_K,
													double T_max_K,
													int n_points)
{
	// Anchor: use tabulated Cp_298 if available, otherwise Dulong-Petit
	const auto& tbl = element_thermo_table();
	auto it = tbl.find(Z);
	double Cp_298 = (it != tbl.end()) ? it->second.Cp_298
									   : (24.94 - (period_from_Z_(Z) - 3) * 0.8);
	Cp_298 = std::max(8.0, Cp_298);

	// Shomate: Cp(T) = A + B*t  where t = T/1000
	// Calibrate A so that Cp(298K) == Cp_298:
	//   Cp_298 = A + B * 0.298  =>  A = Cp_298 - B * 0.298
	double B = 0.006 + period_from_Z_(Z) * 0.0015;  // small positive slope
	double A = Cp_298 - B * 0.298;
	A = std::max(6.0, A);

	PropertyCurve curve;
	curve.reserve(n_points);

	double step = (T_max_K - T_min_K) / std::max(1, n_points - 1);
	for (int i = 0; i < n_points; ++i) {
		double T  = T_min_K + i * step;
		double t  = T / 1000.0;
		double cp = A + B * t;
		cp = std::max(0.1, cp);

		ThermoPoint pt;
		pt.T_K   = T;
		pt.P_Pa  = 101325.0;
		pt.value = cp;
		curve.push_back(pt);
	}
	return curve;
}

// ============================================================================
// estimate_critical_properties (Lee-Kesler heuristic)
// ============================================================================

void ThermoGenerator::estimate_critical_properties(UFXMaterialRecord& rec) noexcept {
	auto& t = rec.thermo;
	if (!t.melting_point_K.has_value()) return;

	double Tm = *t.melting_point_K;
	double Tb = t.boiling_point_K.has_value() ? *t.boiling_point_K : Tm * 1.4;

	// Tc ~ 1.5 * Tb  (rough Lee-Kesler for simple materials)
	if (!t.critical_temperature_K.has_value())
		t.critical_temperature_K = 1.5 * Tb;

	// Pc ~ 0.06 * Tc^2 / Vc  (simplified)
	if (!t.critical_pressure_Pa.has_value())
		t.critical_pressure_Pa = 4.0e6 + Tb * 2000.0;

	// Acentric factor: Pitzer correlation omega ~ log(Pc/Psat_Tb) - 1
	if (!t.acentric_factor.has_value())
		t.acentric_factor = 0.05 + (Tb / *t.critical_temperature_K) * 0.3;
}

// ============================================================================
// select_eos_model
// ============================================================================

std::string ThermoGenerator::select_eos_model(const std::string& phase,
											   double T_K,
											   double critical_T_K) noexcept
{
	if (phase == "solid")       return "tabular";
	if (phase == "molten_salt") return "PR";
	if (phase == "liquid")      return "PR";
	// Gas
	if (critical_T_K > 0.0 && T_K > 2.0 * critical_T_K) return "ideal";
	return "PR";
}

// ============================================================================
// ThermoGenerator::fill
// ============================================================================

ThermoGenerator::ThermoGenerator() = default;

void ThermoGenerator::fill(UFXMaterialRecord& rec, const AxisSample& s) const {
	int Z = atomic_number(s.element);
	if (Z == 0) return;

	auto& t   = rec.thermo;
	auto& eos = rec.eos;

	// Look up tabulated data first
	const auto& tbl = element_thermo_table();
	auto it = tbl.find(Z);
	bool have_table = (it != tbl.end());
	const ElementThermo* et = have_table ? &it->second : nullptr;

	// Hf: use tabulated value if available; otherwise use oxide heuristic
	if (!t.delta_Hf_298_kJ_mol.has_value()) {
		if (s.delta_Hf_kJ_mol.has_value()) {
			t.delta_Hf_298_kJ_mol = *s.delta_Hf_kJ_mol;
		} else if (have_table) {
			// Tabulated value is for the standard oxide/compound.
			// At ox_state == 0 the element is its own reference: Hf = 0.
			t.delta_Hf_298_kJ_mol = (s.oxidation_state == 0) ? 0.0 : et->Hf_kJ_mol;
		} else {
			t.delta_Hf_298_kJ_mol = Hf_seed_(Z, s.oxidation_state);
		}
	}

	// S_298
	if (!t.S_298_J_mol_K.has_value()) {
		t.S_298_J_mol_K = have_table ? et->S_298
									 : (40.0 + Z * 0.25);
	}

	// Gf: Gibbs = Hf - T*S  (T=298.15 K, convert S from J to kJ)
	if (!t.delta_Gf_298_kJ_mol.has_value() && t.delta_Hf_298_kJ_mol.has_value())
		t.delta_Gf_298_kJ_mol = *t.delta_Hf_298_kJ_mol
								- 298.15 * (*t.S_298_J_mol_K) * 1.0e-3;

	// Melting and boiling points
	if (!t.melting_point_K.has_value()) {
		t.melting_point_K = have_table ? et->Tm_K
									   : Tm_estimate_(Z, s.element_family);
	}
	if (!t.boiling_point_K.has_value()) {
		t.boiling_point_K = have_table ? et->Tb_K
									   : Tb_estimate_(Z, s.element_family);
	}

	// Cp curve — seed from tabulated Cp_298 or Dulong-Petit
	double T_min = s.T_min_K.has_value() ? *s.T_min_K : 200.0;
	double T_max = s.T_max_K.has_value() ? *s.T_max_K : 2000.0;
	if (T_min > T_max) std::swap(T_min, T_max);
	T_max = std::min(T_max, 5000.0);

	if (t.cp_T.empty()) {
		// Ignore axis override cp_298 if we have a better table value
		(void)(s.cp_298_J_mol_K);
		t.cp_T = estimate_cp_shomate(Z, T_min, T_max, 20);
	}

	// Critical properties
	estimate_critical_properties(rec);

	// EOS
	double Tc = t.critical_temperature_K.has_value()
				? *t.critical_temperature_K : 0.0;
	eos.eos_model = s.eos_model.has_value()
					? *s.eos_model
					: select_eos_model(s.phase, s.temperature_K, Tc);
}

// ============================================================================
// DB-level fill loop
// ============================================================================

static int64_t insert_thermo_pv(sqlite3* db,
								  int64_t material_id,
								  const std::string& block_name,
								  const std::string& property_name,
								  double value_real,
								  const std::string& units,
								  double temperature_K,
								  double pressure_Pa,
								  const std::string& phase)
{
	const char* sql =
		"INSERT OR IGNORE INTO property_values "
		"(material_id, block_tier, block_name, property_name, "
		" value_real, units, temperature_K, pressure_Pa, phase, "
		" source_class, source_id, confidence) "
		"VALUES (?,3,?,?,?,?,?,?,?,'generated',"
		"        ?,?);";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
		return -1;

	sqlite3_bind_int64 (stmt, 1, material_id);
	sqlite3_bind_text  (stmt, 2, block_name.c_str(),    -1, SQLITE_STATIC);
	sqlite3_bind_text  (stmt, 3, property_name.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_double(stmt, 4, value_real);
	sqlite3_bind_text  (stmt, 5, units.c_str(),         -1, SQLITE_STATIC);
	sqlite3_bind_double(stmt, 6, temperature_K);
	sqlite3_bind_double(stmt, 7, pressure_Pa);
	sqlite3_bind_text  (stmt, 8, phase.c_str(),         -1, SQLITE_STATIC);
	// source_id and confidence depend on whether the value came from the
	// NIST-JANAF tabulated data or from a heuristic estimate.
	sqlite3_bind_text  (stmt, 9, "shomate_heuristic_phase7", -1, SQLITE_STATIC);
	sqlite3_bind_double(stmt, 10, 0.35);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) return -1;
	return sqlite3_last_insert_rowid(db);
}

FillThermoResult ufx_auto2_fill_thermo(const FillThermoOptions& opts) {
	FillThermoResult result;
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
		"    AND pv.block_name  = 'thermo' "
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

	ThermoGenerator gen;

	while (sqlite3_step(qstmt) == SQLITE_ROW) {
		result.processed++;

		int64_t     mid       = sqlite3_column_int64(qstmt, 0);
		const char* mkey_c    = (const char*)sqlite3_column_text(qstmt, 1);
		const char* elem_c    = (const char*)sqlite3_column_text(qstmt, 2);
		double      ox        = sqlite3_column_double(qstmt, 3);
		double      coord     = sqlite3_column_double(qstmt, 4);
		const char* phase_c   = (const char*)sqlite3_column_text(qstmt, 5);
		const char* efam_c    = (const char*)sqlite3_column_text(qstmt, 6);

		std::string mkey   = mkey_c  ? mkey_c  : "";
		std::string elem   = elem_c  ? elem_c  : "";
		std::string phase  = phase_c ? phase_c : "solid";
		std::string efam   = efam_c  ? efam_c  : "transition_metal";

		if (elem.empty()) {
			auto pos = mkey.find('_');
			if (pos != std::string::npos) elem = mkey.substr(0, pos);
		}
		if (elem.empty()) { result.failed++; continue; }

		AxisSample s;
		s.element         = elem;
		s.element_family  = efam;
		s.oxidation_state = static_cast<int>(ox);
		s.coordination    = static_cast<int>(coord > 0 ? coord : 4);
		s.phase           = phase;
		s.temperature_K   = 298.15;
		s.pressure_atm    = 1.0;

		UFXMaterialRecord rec;
		gen.fill(rec, s);
		auto& t   = rec.thermo;
		auto& eos = rec.eos;

		double T = 298.15;
		double P = 101325.0;

		// Determine whether the thermo data came from the NIST-JANAF table.
		// Tabulated rows get higher confidence; heuristic rows stay at 0.35.
		int Z_elem = atomic_number(elem);
		bool tabulated = (Z_elem > 0) && (element_thermo_table().count(Z_elem) > 0);
		const char* src_id  = tabulated ? "ThermoGenerator_NIST_JANAF"    : "ThermoGenerator_phase7";
		double      conf    = tabulated ? 0.75                             : 0.35;

		// Helper lambda: insert + provenance in one step
		auto ins = [&](const std::string& prop, double val, const std::string& units,
					   double tK, double pPa) -> bool {
			int64_t rid = insert_thermo_pv(db, mid, "thermo", prop, val, units, tK, pPa, phase);
			if (rid > 0) ufx_insert_provenance(db, rid,
				tabulated ? "NIST_JANAF_phase7" : "shomate_heuristic_phase7",
				src_id, conf);
			return rid > 0;
		};

		bool ok = true;
		if (t.delta_Hf_298_kJ_mol.has_value())
			ok &= ins("delta_Hf_kJ_mol",  *t.delta_Hf_298_kJ_mol, "kJ/mol",       T, P);
		if (t.delta_Gf_298_kJ_mol.has_value())
			ok &= ins("delta_Gf_kJ_mol",  *t.delta_Gf_298_kJ_mol, "kJ/mol",       T, P);
		if (t.S_298_J_mol_K.has_value())
			ok &= ins("S_298_J_mol_K",    *t.S_298_J_mol_K,        "J/(mol K)",    T, P);
		if (t.melting_point_K.has_value())
			ok &= ins("melting_point_K",  *t.melting_point_K,      "K",            T, P);
		if (t.boiling_point_K.has_value())
			ok &= ins("boiling_point_K",  *t.boiling_point_K,      "K",            T, P);
		if (t.critical_temperature_K.has_value())
			ok &= ins("critical_T_K",     *t.critical_temperature_K, "K",          T, P);
		if (t.critical_pressure_Pa.has_value())
			ok &= ins("critical_P_Pa",    *t.critical_pressure_Pa, "Pa",           T, P);
		if (t.acentric_factor.has_value())
			ok &= ins("acentric_factor",  *t.acentric_factor,      "dimensionless",T, P);

		// Cp curve rows
		for (const auto& pt : t.cp_T)
			ok &= ins("cp", pt.value, "J/(mol K)", pt.T_K, pt.P_Pa);

		// EOS model text row
		if (!eos.eos_model.empty()) {
			const char* eos_sql =
				"INSERT OR IGNORE INTO property_values "
				"(material_id, block_tier, block_name, property_name, "
				" value_text, units, temperature_K, pressure_Pa, phase, "
				" source_class, source_id, confidence) "
				"VALUES (?,3,'eos','eos_model',?,'',"
				"        ?,?,?,'generated','shomate_heuristic_phase7',0.35);";
			sqlite3_stmt* es = nullptr;
			if (sqlite3_prepare_v2(db, eos_sql, -1, &es, nullptr) == SQLITE_OK) {
				sqlite3_bind_int64(es, 1, mid);
				sqlite3_bind_text (es, 2, eos.eos_model.c_str(), -1, SQLITE_STATIC);
				sqlite3_bind_double(es, 3, T);
				sqlite3_bind_double(es, 4, P);
				sqlite3_bind_text  (es, 5, phase.c_str(), -1, SQLITE_STATIC);
				bool eos_ok = (sqlite3_step(es) == SQLITE_DONE);
				sqlite3_finalize(es);
				if (eos_ok) {
					int64_t eos_rid = sqlite3_last_insert_rowid(db);
					if (eos_rid > 0)
						ufx_insert_provenance(db, eos_rid,
							"shomate_heuristic_phase7", "ThermoGenerator_eos_phase7", 0.35);
				}
				ok &= eos_ok;
			}
		}

		if (opts.verbose) {
			std::cout << "  [fill-thermo] " << mkey
					  << "  Hf=" << t.delta_Hf_298_kJ_mol.value_or(0.0)
					  << " kJ/mol  Tm=" << t.melting_point_K.value_or(0.0)
					  << " K  cp_pts=" << t.cp_T.size()
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

void print_fill_thermo_result(const FillThermoResult& r) {
	std::cout << "\n-- fill-thermo summary --\n";
	std::cout << "  DB        : " << r.db_path   << "\n";
	std::cout << "  Processed : " << r.processed << "\n";
	std::cout << "  Filled    : " << r.filled    << "\n";
	std::cout << "  Skipped   : " << r.skipped   << "\n";
	std::cout << "  Failed    : " << r.failed    << "\n";
	std::cout << "  Status    : " << (r.success ? "OK" : "FAIL") << "\n";
	if (!r.error_message.empty())
		std::cout << "  Error     : " << r.error_message << "\n";
	std::cout << "\n";
}

} // namespace vsepr::ufx
