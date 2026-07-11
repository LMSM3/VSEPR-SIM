// src/ufx_auto2/macro_property_generator.cpp
// UFX_AUTO_2 Phase 9 -- Mechanics + Transport Macro Bridge Generator
// VSEPR-SIM v5 beta9

#include "ufx_auto2/macro_property_generator.hpp"
#include "v4/uff/ufx_schema.hpp"

// -- existing codebase --
#include "atomistic/core/thermodynamics.hpp"      // atomistic::thermo::kB, NA
#include "atomistic/core/empirical_reference.hpp" // empirical::LJ_REFS, CRYSTAL_REFS

#include <sqlite3.h>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <algorithm>

namespace vsepr::ufx {

// ============================================================================
// Tabulated intrinsic thermal conductivity k_0 (W/m/K) at 298 K
// Source: CRC Handbook of Chemistry & Physics, 103rd ed., Table "Thermal
//         Conductivity of Metallic Elements". Values are for the pure
//         polycrystalline element in the solid phase.
// ============================================================================

static double tabulated_k0_W_mK(int Z) noexcept {
	static const std::unordered_map<int,double> K0 = {
		{3,  84.7}, // Li
		{4, 200.0}, // Be
		{5,  27.4}, // B
		{6, 119.0}, // C (graphite along a)
		{11,142.0}, // Na
		{12,156.0}, // Mg
		{13,237.0}, // Al
		{14,149.0}, // Si
		{19, 102.0},// K
		{20, 201.0},// Ca
		{21,  15.8},// Sc
		{22,  21.9},// Ti
		{23,  30.7},// V
		{24,  93.9},// Cr
		{25,   7.8},// Mn
		{26,  80.4},// Fe
		{27,  100.0},// Co
		{28,  90.9},// Ni
		{29, 401.0},// Cu
		{30, 116.0},// Zn
		{31,  40.6},// Ga
		{32,  59.9},// Ge
		{37,  58.2},// Rb
		{38,  35.4},// Sr
		{39,  17.2},// Y
		{40,  22.7},// Zr
		{41,  53.7},// Nb
		{42, 138.0},// Mo
		{44, 117.0},// Ru
		{45, 150.0},// Rh
		{46,  71.8},// Pd
		{47, 429.0},// Ag
		{48,  96.8},// Cd
		{49,  81.8},// In
		{50,  66.8},// Sn
		{51,  24.3},// Sb
		{52,   2.35},// Te
		{55,  35.9},// Cs
		{56,  18.4},// Ba
		{57,  13.4},// La
		{58,  11.3},// Ce
		{59,  12.5},// Pr
		{60,  16.5},// Nd
		{62,  13.3},// Sm
		{63,  13.9},// Eu
		{64,  10.6},// Gd
		{65,  11.1},// Tb
		{66,  10.7},// Dy
		{67,  16.2},// Ho
		{68,  14.5},// Er
		{69,  16.9},// Tm
		{70,  34.9},// Yb
		{71,  16.4},// Lu
		{72,  23.0},// Hf
		{73,  57.5},// Ta
		{74, 173.0},// W
		{75,  48.0},// Re
		{76,  87.6},// Os
		{77, 147.0},// Ir
		{78,  71.6},// Pt
		{79, 318.0},// Au
		{80,   8.30},// Hg (liquid ~8.3)
		{81,  46.1},// Tl
		{82,  35.3},// Pb
		{83,   7.97},// Bi
		{90,  54.0},// Th
		{92,  27.6},// U
		{94,   6.74},// Pu
	};
	auto it = K0.find(Z);
	if (it != K0.end()) return it->second;
	// Fallback: rough period-based estimate
	return 50.0;
}

// ============================================================================
// Atomic number — routes through empirical::LJ_REFS first (UFF periodic table),
// then a small supplement for elements absent from that table.
// ============================================================================

static int macro_atomic_number(const std::string& sym) noexcept {
	for (int i = 0; i < empirical::N_LJ_REFS; ++i)
		if (std::string(empirical::LJ_REFS[i].symbol) == sym)
			return empirical::LJ_REFS[i].Z;
	static const std::unordered_map<std::string,int> extra = {
		{"Sc",21},{"Tc",43},{"Ru",44},{"Rh",45},{"In",49},{"Sb",51},
		{"Te",52},{"I",53},{"La",57},{"Ce",58},{"Re",75},{"Os",76},
		{"Tl",81},{"Bi",83},{"Th",90},{"U",92},{"Pu",94},
	};
	auto it = extra.find(sym);
	return (it != extra.end()) ? it->second : 0;
}

// ============================================================================
// MacroPropertyGenerator statics
// ============================================================================

int MacroPropertyGenerator::period_from_Z_(int Z) noexcept {
	if (Z <=  2) return 1;
	if (Z <= 10) return 2;
	if (Z <= 18) return 3;
	if (Z <= 36) return 4;
	if (Z <= 54) return 5;
	if (Z <= 86) return 6;
	return 7;
}

double MacroPropertyGenerator::k_phonon_estimate_(int Z, int period,
												   double debye_T_K) noexcept
{
	// Use tabulated k_0 at 298 K as the anchor value.
	// Temperature scaling: k ~ 1/T for umklapp-dominated metals (T > theta_D/2).
	// At 298 K we just return k_0 corrected by a Debye factor.
	double k0 = tabulated_k0_W_mK(Z);

	// If Debye temperature is known, apply a small correction for ionic character:
	// more ionic (low theta_D relative to Tm) -> reduce k by up to 40%.
	if (debye_T_K > 50.0) {
		// Normalise: high theta_D = good phonon transport = no penalty
		double factor = std::min(1.0, debye_T_K / 400.0);
		factor = 0.60 + 0.40 * factor;
		k0 *= factor;
	}

	(void)period;
	return std::max(0.1, std::min(450.0, k0));
}

double MacroPropertyGenerator::k_eff_maxwell_garnett(double k_solid,
													   double k_fluid,
													   double porosity) noexcept
{
	if (porosity <= 0.0) return k_solid;
	if (porosity >= 1.0) return k_fluid;
	// Maxwell-Garnett: k_eff = k_solid * (2*k_solid + k_fluid - 2*phi*(k_solid-k_fluid))
	//                                   / (2*k_solid + k_fluid + phi*(k_solid-k_fluid))
	double num = 2.0*k_solid + k_fluid - 2.0*porosity*(k_solid - k_fluid);
	double den = 2.0*k_solid + k_fluid +     porosity*(k_solid - k_fluid);
	return (den > 0.0) ? k_solid * num / den : k_solid;
}

double MacroPropertyGenerator::E_eff_voigt(double E_bulk,
											double porosity) noexcept
{
	// Voigt upper bound: E_eff = E_bulk * (1 - porosity)
	return E_bulk * std::max(0.0, 1.0 - porosity);
}

double MacroPropertyGenerator::permeability_kozeny_carman(double d_particle_m,
														   double porosity) noexcept
{
	if (d_particle_m <= 0.0 || porosity <= 0.0 || porosity >= 1.0) return 0.0;
	// k = d^2 * phi^3 / (180 * (1-phi)^2)
	double phi  = porosity;
	double denom = 180.0 * (1.0 - phi) * (1.0 - phi);
	if (denom <= 0.0) return 0.0;
	return (d_particle_m * d_particle_m * phi * phi * phi) / denom;
}

// ============================================================================
// fill_mechanical
// E_eff ~ (r1 / D1) * coordination * k_harmonic_factor * density_factor
// ============================================================================

MacroPropertyGenerator::MacroPropertyGenerator() = default;

void MacroPropertyGenerator::fill_mechanical(UFXMaterialRecord& rec,
											  const AxisSample& s) const
{
	auto& mech = rec.mechanical;

	// UFF LJ parameters: use force_field block if populated; otherwise pull
	// directly from empirical::LJ_REFS (Rappe 1992) for the element.
	double r1 = rec.force_field.r1 > 0.0 ? rec.force_field.r1 : 0.0;
	double D1 = rec.force_field.D1 > 0.0 ? rec.force_field.D1 : 0.0;
	if (r1 <= 0.0 || D1 <= 0.0) {
		int Z_elem = macro_atomic_number(s.element);
		auto lj = (Z_elem > 0) ? empirical::find_lj(Z_elem) : std::nullopt;
		if (lj) {
			if (r1 <= 0.0) r1 = lj->sigma;       // Å
			if (D1 <= 0.0) D1 = lj->epsilon;     // kcal/mol
		}
		if (r1 <= 0.0) r1 = 1.5;
		if (D1 <= 0.0) D1 = 0.1;
	}
	double coord = static_cast<double>(s.coordination);

	// Convert kcal/mol/A^3 -> GPa (1 kcal/(mol·A^3) ≈ 0.0695 GPa)
	double E_raw = D1 * coord / (r1 * r1 * r1);
	double E_GPa = E_raw * 0.0695 * 1000.0;  // heuristic scale factor

	// Clamp to physical range
	E_GPa = std::max(0.001, std::min(1200.0, E_GPa));

	double porosity = s.porosity.has_value() ? *s.porosity : 0.0;
	E_GPa = E_eff_voigt(E_GPa, porosity);

	// Isotropic relationships
	double nu = 0.30;  // typical Poisson ratio
	double G_GPa = E_GPa / (2.0 * (1.0 + nu));
	double K_GPa = E_GPa / (3.0 * (1.0 - 2.0 * nu));

	mech.E_eff_GPa    = E_GPa;
	mech.G_eff_GPa    = G_GPa;
	mech.K_eff_GPa    = K_GPa;
	mech.poisson_ratio = nu;

	// Yield strength ~ E/100 (Hall-Petch-like rough estimate)
	mech.yield_strength_MPa = E_GPa * 10.0;

	// Test environment / strain rate from axis sample
	mech.strain_rate   = s.strain_rate.has_value() ? *s.strain_rate : 1.0e-3;
	mech.temperature_K = s.temperature_K;

	// Hard rule: label the approximation
	mech.test_method = "phase9_bridge_heuristic";
}

// ============================================================================
// fill_transport
// k_eff from phonon model + Maxwell-Garnett porous correction
// D from Stokes-Einstein
// ============================================================================

void MacroPropertyGenerator::fill_transport(UFXMaterialRecord& rec,
											  const AxisSample& s) const
{
	auto& trans = rec.transport;

	int Z      = macro_atomic_number(s.element);
	int period = period_from_Z_(Z);

	double debye_T = rec.crystal.debye_temperature_K.has_value()
					 ? *rec.crystal.debye_temperature_K : 300.0;

	double k_solid = k_phonon_estimate_(Z, period, debye_T);

	double porosity    = s.porosity.has_value()   ? *s.porosity   : 0.0;
	double tortuosity  = s.tortuosity.has_value() ? *s.tortuosity : 1.0;
	double k_fluid     = 0.025;  // air at 298K

	trans.k_eff_W_mK  = k_eff_maxwell_garnett(k_solid, k_fluid, porosity);
	trans.porosity     = porosity;
	trans.tortuosity   = tortuosity;

	// Stokes-Einstein diffusivity: D = kT / (3*pi*eta*d)
	// Use viscosity = 1e-3 Pa·s (water-like), d = 3 Angstrom
	// kB in SI: convert from kcal/(mol·K) via NA
	constexpr double kB_SI = atomistic::thermo::kB * 4184.0 / atomistic::thermo::NA;
	double T    = s.temperature_K;
	double eta  = 1.0e-3;
	double d_m  = 3.0e-10;
	double D_m2s = (kB_SI * T) / (3.0 * M_PI * eta * d_m);
	D_m2s = std::min(D_m2s, 1.0e-4);  // clamp per sanity check

	// Apply tortuosity correction: D_eff = D * phi / tau^2
	if (tortuosity > 0.0 && porosity > 0.0)
		D_m2s = D_m2s * porosity / (tortuosity * tortuosity);
	D_m2s = std::max(1.0e-15, D_m2s);

	trans.diffusivity_m2_s = D_m2s;

	// Kozeny-Carman permeability (assume d_particle = 50 microns)
	double d_particle = 50.0e-6;
	trans.permeability_m2 = permeability_kozeny_carman(d_particle, porosity);

	// Darcy coefficient ~ k_permeability / viscosity
	if (trans.permeability_m2.has_value() && *trans.permeability_m2 > 0.0)
		trans.darcy_coefficient = *trans.permeability_m2 / eta;

	// Mass transfer coefficient (simplistic): h_m ~ D/d_particle
	if (d_particle > 0.0)
		trans.mass_transfer_coefficient = D_m2s / d_particle;
}

// ============================================================================
// DB-level fill loop
// ============================================================================

static int64_t insert_macro_pv(sqlite3* db,
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
		"VALUES (?,5,?,?,?,?,?,?,?,'generated',"
		"        'phase9_bridge_heuristic',0.30);";

	sqlite3_stmt* st = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return -1;

	sqlite3_bind_int64 (st, 1, material_id);
	sqlite3_bind_text  (st, 2, block_name.c_str(),    -1, SQLITE_STATIC);
	sqlite3_bind_text  (st, 3, property_name.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_double(st, 4, value_real);
	sqlite3_bind_text  (st, 5, units.c_str(),         -1, SQLITE_STATIC);
	sqlite3_bind_double(st, 6, temperature_K);
	sqlite3_bind_double(st, 7, pressure_Pa);
	sqlite3_bind_text  (st, 8, phase.c_str(),         -1, SQLITE_STATIC);

	int rc = sqlite3_step(st);
	sqlite3_finalize(st);
	if (rc != SQLITE_DONE) return -1;
	return sqlite3_last_insert_rowid(db);
}

FillMacroResult ufx_auto2_fill_macro(const FillMacroOptions& opts) {
	FillMacroResult result;
	result.db_path = opts.db_path;

	std::string err;
	sqlite3* db = ufx_open_db_rw(opts.db_path, err);
	if (!db) {
		result.error_message = "Cannot open DB: " + err;
		return result;
	}

	// Mechanical fill
	if (opts.fill_mechanical) {
		const char* mech_sql =
			"SELECT mr.id, mr.material_key, "
			"       id_pv.value_text AS element, "
			"       ox_pv.value_real AS ox_state, "
			"       co_pv.value_real AS coordination, "
			"       ph_pv.value_text AS phase, "
			"       ef_pv.value_text AS element_family, "
			"       r1_pv.value_real AS r1, "
			"       d1_pv.value_real AS D1 "
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
			"LEFT JOIN property_values r1_pv ON r1_pv.material_id = mr.id "
			"                               AND r1_pv.block_name  = 'force_field' "
			"                               AND r1_pv.property_name = 'r1' "
			"LEFT JOIN property_values d1_pv ON d1_pv.material_id = mr.id "
			"                               AND d1_pv.block_name  = 'force_field' "
			"                               AND d1_pv.property_name = 'D1' "
			"WHERE NOT EXISTS ( "
			"  SELECT 1 FROM property_values pv "
			"  WHERE pv.material_id = mr.id "
			"    AND pv.block_name  = 'mechanical' "
			") "
			"AND mr.source_class NOT IN ('rejected') "
			"LIMIT ?;";

		sqlite3_stmt* qstmt = nullptr;
		if (sqlite3_prepare_v2(db, mech_sql, -1, &qstmt, nullptr) == SQLITE_OK) {
			sqlite3_bind_int(qstmt, 1, opts.batch);

			MacroPropertyGenerator gen;

			while (sqlite3_step(qstmt) == SQLITE_ROW) {
				result.processed_mechanical++;

				int64_t     mid    = sqlite3_column_int64(qstmt, 0);
				const char* mkey_c = (const char*)sqlite3_column_text(qstmt, 1);
				const char* elem_c = (const char*)sqlite3_column_text(qstmt, 2);
				double      ox     = sqlite3_column_double(qstmt, 3);
				double      coord  = sqlite3_column_double(qstmt, 4);
				const char* ph_c   = (const char*)sqlite3_column_text(qstmt, 5);
				const char* ef_c   = (const char*)sqlite3_column_text(qstmt, 6);
				double      r1     = sqlite3_column_double(qstmt, 7);
				double      D1     = sqlite3_column_double(qstmt, 8);

				std::string mkey  = mkey_c ? mkey_c : "";
				std::string elem  = elem_c ? elem_c : "";
				std::string phase = ph_c   ? ph_c   : "solid";
				std::string efam  = ef_c   ? ef_c   : "transition_metal";

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

				UFXMaterialRecord rec;
				rec.force_field.r1 = r1 > 0.0 ? r1 : 1.5;
				rec.force_field.D1 = D1 > 0.0 ? D1 : 0.1;

				gen.fill_mechanical(rec, s);
				auto& mech = rec.mechanical;

				double T = 298.15, P = 101325.0;
				auto ins_m = [&](const std::string& prop, double val,
								 const std::string& units) -> bool {
					int64_t rid = insert_macro_pv(db, mid, "mechanical", prop, val, units, T, P, phase);
					if (rid > 0) ufx_insert_provenance(db, rid,
						"phase9_bridge_heuristic", "MacroGenerator_mechanical_phase9", 0.30);
					return rid > 0;
				};

				bool ok = true;
				ok &= ins_m("E_eff_GPa",    mech.E_eff_GPa.value_or(0.0),             "GPa");
				ok &= ins_m("G_eff_GPa",    mech.G_eff_GPa.value_or(0.0),             "GPa");
				ok &= ins_m("K_eff_GPa",    mech.K_eff_GPa.value_or(0.0),             "GPa");
				ok &= ins_m("poisson_ratio",mech.poisson_ratio.value_or(0.3),          "");
				ok &= ins_m("yield_MPa",    mech.yield_strength_MPa.value_or(0.0),    "MPa");

				if (opts.verbose) {
					std::cout << "  [fill-mechanical] " << mkey
							  << "  E=" << mech.E_eff_GPa.value_or(0.0)
							  << " GPa" << (ok ? " OK" : " ERR") << "\n";
				}

				if (ok) result.filled_mechanical++;
				else    result.failed++;
			}
			sqlite3_finalize(qstmt);
		}
	}

	// Transport fill
	if (opts.fill_transport) {
		const char* trans_sql =
			"SELECT mr.id, mr.material_key, "
			"       id_pv.value_text AS element, "
			"       ph_pv.value_text AS phase, "
			"       ef_pv.value_text AS element_family, "
			"       dbt_pv.value_real AS debye_T "
			"FROM material_records mr "
			"LEFT JOIN property_values id_pv ON id_pv.material_id = mr.id "
			"                               AND id_pv.block_name  = 'identity' "
			"                               AND id_pv.property_name = 'element' "
			"LEFT JOIN property_values ph_pv ON ph_pv.material_id = mr.id "
			"                               AND ph_pv.block_name  = 'identity' "
			"                               AND ph_pv.property_name = 'phase' "
			"LEFT JOIN property_values ef_pv ON ef_pv.material_id = mr.id "
			"                               AND ef_pv.block_name  = 'identity' "
			"                               AND ef_pv.property_name = 'element_family' "
			"LEFT JOIN property_values dbt_pv ON dbt_pv.material_id = mr.id "
			"                                AND dbt_pv.block_name  = 'crystal' "
			"                                AND dbt_pv.property_name = 'debye_temperature_K' "
			"WHERE NOT EXISTS ( "
			"  SELECT 1 FROM property_values pv "
			"  WHERE pv.material_id = mr.id "
			"    AND pv.block_name  = 'transport' "
			") "
			"AND mr.source_class NOT IN ('rejected') "
			"LIMIT ?;";

		sqlite3_stmt* qstmt = nullptr;
		if (sqlite3_prepare_v2(db, trans_sql, -1, &qstmt, nullptr) == SQLITE_OK) {
			sqlite3_bind_int(qstmt, 1, opts.batch);

			MacroPropertyGenerator gen;

			while (sqlite3_step(qstmt) == SQLITE_ROW) {
				result.processed_transport++;

				int64_t     mid    = sqlite3_column_int64(qstmt, 0);
				const char* mkey_c = (const char*)sqlite3_column_text(qstmt, 1);
				const char* elem_c = (const char*)sqlite3_column_text(qstmt, 2);
				const char* ph_c   = (const char*)sqlite3_column_text(qstmt, 3);
				const char* ef_c   = (const char*)sqlite3_column_text(qstmt, 4);
				double debye_T     = sqlite3_column_double(qstmt, 5);

				std::string mkey  = mkey_c ? mkey_c : "";
				std::string elem  = elem_c ? elem_c : "";
				std::string phase = ph_c   ? ph_c   : "solid";
				std::string efam  = ef_c   ? ef_c   : "transition_metal";

				if (elem.empty()) {
					auto pos = mkey.find('_');
					if (pos != std::string::npos) elem = mkey.substr(0, pos);
				}
				if (elem.empty()) { result.failed++; continue; }

				AxisSample s;
				s.element        = elem;
				s.element_family = efam;
				s.phase          = phase;
				s.temperature_K  = 298.15;

				UFXMaterialRecord rec;
				if (debye_T > 0.0)
					rec.crystal.debye_temperature_K = debye_T;

				gen.fill_transport(rec, s);
				auto& trans = rec.transport;

				double T = 298.15, P = 101325.0;
				auto ins_t = [&](const std::string& prop, double val,
								 const std::string& units) -> bool {
					int64_t rid = insert_macro_pv(db, mid, "transport", prop, val, units, T, P, phase);
					if (rid > 0) ufx_insert_provenance(db, rid,
						"phase9_bridge_heuristic", "MacroGenerator_transport_phase9", 0.30);
					return rid > 0;
				};

				bool ok = true;
				ok &= ins_t("k_eff_W_mK",          trans.k_eff_W_mK.value_or(0.0),              "W/(m K)");
				ok &= ins_t("diffusivity_m2_s",     trans.diffusivity_m2_s.value_or(0.0),        "m2/s");
				ok &= ins_t("permeability_m2",      trans.permeability_m2.value_or(0.0),         "m2");
				ok &= ins_t("porosity",             trans.porosity.value_or(0.0),                "");
				ok &= ins_t("tortuosity",           trans.tortuosity.value_or(1.0),              "");
				ok &= ins_t("darcy_coeff",          trans.darcy_coefficient.value_or(0.0),       "");
				ok &= ins_t("mass_transfer_coeff",  trans.mass_transfer_coefficient.value_or(0.0),"m/s");

				if (opts.verbose) {
					std::cout << "  [fill-transport] " << mkey
							  << "  k=" << trans.k_eff_W_mK.value_or(0.0)
							  << " W/mK" << (ok ? " OK" : " ERR") << "\n";
				}

				if (ok) result.filled_transport++;
				else    result.failed++;
			}
			sqlite3_finalize(qstmt);
		}
	}

	sqlite3_close(db);
	result.success = (result.failed == 0);
	return result;
}

void print_fill_macro_result(const FillMacroResult& r) {
	std::cout << "\n-- fill-macro summary --\n";
	std::cout << "  DB                   : " << r.db_path               << "\n";
	std::cout << "  Mechanical processed : " << r.processed_mechanical  << "\n";
	std::cout << "  Mechanical filled    : " << r.filled_mechanical     << "\n";
	std::cout << "  Transport processed  : " << r.processed_transport   << "\n";
	std::cout << "  Transport filled     : " << r.filled_transport      << "\n";
	std::cout << "  Failed               : " << r.failed                << "\n";
	std::cout << "  Status               : " << (r.success ? "OK" : "FAIL") << "\n";
	if (!r.error_message.empty())
		std::cout << "  Error                : " << r.error_message     << "\n";
	std::cout << "\n";
}

} // namespace vsepr::ufx
