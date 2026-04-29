// include/ufx_auto2/thermo_generator.hpp
// UFX_AUTO_2 Phase 7 -- Thermo + EOS Generator
// VSEPR-SIM v5 beta9
//
// ThermoGenerator populates rec.thermo and rec.eos using:
//   1. Element-family heuristic ranges for Hf, Cp, Tb, Tm
//   2. Shomate polynomial estimate (5-coefficient from group/period)
//   3. EOS parameter estimates from critical property correlations
//
// All outputs are labelled:
//   method_tag    = "shomate_heuristic_phase7"
//   confidence    = 0.35
//
// property_values rows written (block_tier=3):
//   block='thermo': delta_Hf_kJ_mol, delta_Gf_kJ_mol, S_298_J_mol_K,
//                   cp (one row per T point), melting_point_K,
//                   boiling_point_K, critical_T_K, critical_P_Pa,
//                   acentric_factor, density_kg_m3, viscosity_Pa_s,
//                   thermal_conductivity_W_mK
//   block='eos':    density_kg_m3 (one row per T/P point)

#pragma once

#include "v4/uff/ufx_material_record.hpp"
#include "ufx_auto2/axis_config.hpp"

#include <string>

struct sqlite3;

namespace vsepr::ufx {

// ============================================================================
// FillThermoOptions / Result
// ============================================================================

struct FillThermoOptions {
	std::string db_path;
	int         batch         = 500;
	bool        verbose       = false;
	bool        nist_validate = false;   // fire NIST WebBook lookups if true
};

struct FillThermoResult {
	std::string db_path;
	int         processed = 0;
	int         filled    = 0;
	int         skipped   = 0;
	int         failed    = 0;
	bool        success   = false;
	std::string error_message;
};

// ============================================================================
// ThermoGenerator
// ============================================================================

class ThermoGenerator {
public:
	ThermoGenerator();

	// Populate rec.thermo and rec.eos from AxisSample.
	void fill(UFXMaterialRecord& rec, const AxisSample& s) const;

	// Build Cp(T) curve from Shomate heuristic [J/(mol·K)].
	static PropertyCurve estimate_cp_shomate(int Z,
											 double T_min_K,
											 double T_max_K,
											 int n_points = 20);

	// Estimate critical properties via Lee-Kesler correlations.
	static void estimate_critical_properties(UFXMaterialRecord& rec) noexcept;

	// Select EOS model from phase + temperature conditions.
	static std::string select_eos_model(const std::string& phase,
										double T_K,
										double critical_T_K) noexcept;

private:
	static double cp_seed_(int period, const std::string& family) noexcept;
	static double Hf_seed_(int Z, int ox_state) noexcept;
	static double Tm_estimate_(int Z, const std::string& family) noexcept;
	static double Tb_estimate_(int Z, const std::string& family) noexcept;

	// Period from atomic number Z.
	static int period_from_Z_(int Z) noexcept;
};

// ============================================================================
// Run fill-thermo over the DB (used by CLI)
// ============================================================================

FillThermoResult ufx_auto2_fill_thermo(const FillThermoOptions& opts);
void print_fill_thermo_result(const FillThermoResult& r);

} // namespace vsepr::ufx
