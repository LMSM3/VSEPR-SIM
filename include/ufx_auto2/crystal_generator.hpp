// include/ufx_auto2/crystal_generator.hpp
// UFX_AUTO_2 Phase 8 -- Crystal / Solid-State Generator
// VSEPR-SIM v5 beta9
//
// CrystalGenerator populates rec.crystal for solid-phase records.
// Gas and liquid records are a no-op.
//
// All outputs are labelled:
//   method_tag = "crystal_heuristic_phase8"
//   confidence  = 0.30
//
// property_values rows written (block_tier=4, block_name='crystal'):
//   space_group, space_group_number, lattice_a, lattice_b, lattice_c,
//   lattice_alpha_deg, unit_cell_volume, density_g_cm3, packing_fraction,
//   formation_energy_eV_atom, energy_above_hull_eV_atom,
//   band_gap_eV, debye_temperature_K

#pragma once

#include "v4/uff/ufx_material_record.hpp"
#include "ufx_auto2/axis_config.hpp"

#include <string>
#include <vector>
#include <utility>

struct sqlite3;

namespace vsepr::ufx {

// ============================================================================
// FillCrystalOptions / Result
// ============================================================================

struct FillCrystalOptions {
	std::string db_path;
	int         batch       = 500;
	bool        verbose     = false;
	bool        mp_validate = false;   // fire Materials Project lookups if true
};

struct FillCrystalResult {
	std::string db_path;
	int         processed = 0;
	int         filled    = 0;
	int         skipped   = 0;   // non-solid records
	int         failed    = 0;
	bool        success   = false;
	std::string error_message;
};

// ============================================================================
// CrystalGenerator
// ============================================================================

class CrystalGenerator {
public:
	CrystalGenerator();

	// Populate rec.crystal from axis sample.
	// Returns false (no-op) if rec.identity.phase != "solid".
	bool fill(UFXMaterialRecord& rec, const AxisSample& s) const;

	// Estimate lattice parameter a from element radius + coordination.
	static double estimate_a_angstrom(int Z,
									  int coordination,
									  double ionic_radius_est_angstrom) noexcept;

	// Map coordination number + element family to most probable space group.
	static std::string probable_space_group(const std::string& family,
											int coordination) noexcept;

	// Space group to ITA number (approximate for common groups).
	static int space_group_number(const std::string& sg) noexcept;

	// Compute unit cell volume from a, b, c (assuming cubic for now).
	static double unit_cell_volume_cubic(double a_angstrom) noexcept;

	// Estimate density from atomic mass, coordination, and lattice parameter.
	static double estimate_density_g_cm3(int Z,
										 double atomic_weight,
										 double a_angstrom,
										 double packing_fraction) noexcept;

private:
	// Ionic radius estimate in Angstrom from Z and oxidation state.
	static double ionic_radius_(int Z, int oxidation_state) noexcept;
};

// ============================================================================
// Run fill-crystal over the DB (used by CLI)
// ============================================================================

FillCrystalResult ufx_auto2_fill_crystal(const FillCrystalOptions& opts);
void print_fill_crystal_result(const FillCrystalResult& r);

} // namespace vsepr::ufx
