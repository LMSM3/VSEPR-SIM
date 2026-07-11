// include/ufx_auto2/macro_property_generator.hpp
// UFX_AUTO_2 Phase 9 -- Mechanics + Transport Macro Bridge Generator
// VSEPR-SIM v5 beta9
//
// MacroPropertyGenerator fills rec.mechanical and rec.transport using
// atomistic-to-engineering bridge correlations:
//
//   UFF curvature -> E_eff (Young's modulus)
//   Thermo        -> k_eff (thermal conductivity via Bridgman)
//   Crystal density -> bulk modulus estimate
//   EOS viscosity   -> Darcy coefficient
//
// All outputs are labelled:
//   method_tag   = "phase9_bridge_heuristic"
//   confidence   = 0.30
//   test_method  = "phase9_bridge_heuristic"  (approximation label -- hard rule)
//
// property_values rows written:
//   block='mechanical': E_eff_GPa, G_eff_GPa, K_eff_GPa, poisson_ratio, yield_MPa
//   block='transport':  k_eff_W_mK, diffusivity_m2_s, permeability_m2,
//                       porosity, tortuosity, darcy_coeff, mass_transfer_coeff

#pragma once

#include "v4/uff/ufx_material_record.hpp"
#include "ufx_auto2/axis_config.hpp"

#include <string>

struct sqlite3;

namespace vsepr::ufx {

// ============================================================================
// FillMacroOptions / Result
// ============================================================================

struct FillMacroOptions {
	std::string db_path;
	int         batch    = 500;
	bool        verbose  = false;
	bool        fill_mechanical = true;
	bool        fill_transport  = true;
};

struct FillMacroResult {
	std::string db_path;
	int         processed_mechanical = 0;
	int         processed_transport  = 0;
	int         filled_mechanical    = 0;
	int         filled_transport     = 0;
	int         failed               = 0;
	bool        success              = false;
	std::string error_message;
};

// ============================================================================
// MacroPropertyGenerator
// ============================================================================

class MacroPropertyGenerator {
public:
	MacroPropertyGenerator();

	// Fill rec.mechanical using UFF stiffness-to-modulus bridge.
	void fill_mechanical(UFXMaterialRecord& rec, const AxisSample& s) const;

	// Fill rec.transport using Bridgman correlations from thermo data.
	void fill_transport(UFXMaterialRecord& rec, const AxisSample& s) const;

	// Maxwell-Garnett effective thermal conductivity (porous medium).
	// k_fluid = 0.025 W/(m·K) for air at 298K if not known.
	static double k_eff_maxwell_garnett(double k_solid,
										double k_fluid,
										double porosity) noexcept;

	// Voigt bound effective Young's modulus.
	static double E_eff_voigt(double E_bulk, double porosity) noexcept;

	// Kozeny-Carman permeability.
	static double permeability_kozeny_carman(double d_particle_m,
											 double porosity) noexcept;

private:
	// Phonon thermal conductivity estimate.
	static double k_phonon_estimate_(int Z, int period,
									 double debye_T_K) noexcept;

	static int period_from_Z_(int Z) noexcept;
};

// ============================================================================
// Run fill-mechanical and/or fill-transport over the DB (used by CLI)
// ============================================================================

FillMacroResult ufx_auto2_fill_macro(const FillMacroOptions& opts);
void print_fill_macro_result(const FillMacroResult& r);

} // namespace vsepr::ufx
