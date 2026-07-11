// include/ufx_auto2/local_sanity.hpp
// UFX_AUTO_2 Phases 2-9 -- Local Sanity Checker
// VSEPR-SIM v5 beta9
//
// LocalSanityChecker validates a UFXMaterialRecord before it is stored.
// Phase 2 checks (Tier 0–1):
//   - identity fields non-empty
//   - provenance populated
//   - T/P/phase state present
//   - coordination in valid range [0, 12]
//   - geometry tag recognized
//   - force-field values finite
//
// Phase 6 checks (Tier 2 -- molecular_descriptor_invalid):
//   - molecular_weight > 0
//   - heavy_atom_count >= 1
//   - inchikey non-empty if identity fully populated
//
// Phase 7 checks (Tier 3 -- thermo_block_invalid):
//   - melting_point_K > 0 if phase == solid
//   - boiling_point_K > melting_point_K if both present
//   - cp_T curve has >= 2 points and no negative Cp
//   - delta_Hf_298 in plausible range for element family
//   - eos.eos_model non-empty
//
// Phase 8 checks (Tier 4 -- crystal_block_invalid, solid-phase only):
//   - space_group non-empty
//   - lattice_abc.x > 0
//   - density_g_cm3 > 0
//   - packing_fraction in [0.10, 0.82]
//   - energy_above_hull in [-0.1, 1.5]
//
// Phase 9 checks (Tier 5 -- macro_block_invalid):
//   - E_eff_GPa in [0.001, 1200]
//   - k_eff_W_mK > 0
//   - diffusivity_m2_s > 0 and <= 1e-4
//   - porosity in [0, 0.98]
//   - mechanical.approximation_label non-empty (hard rule: label the approximation)
//
// A record that fails any check is stored as SourceClass::Rejected with
// the failure reason recorded in provenance.source_note.

#pragma once

#include "v4/uff/ufx_material_record.hpp"
#include <string>
#include <vector>

namespace vsepr::ufx {

// ============================================================================
// LocalCheckResult
// ============================================================================

struct LocalCheckResult {
	bool                     pass    = false;
	std::vector<std::string> reasons;   // populated on failure

	// Convenience constructors
	static LocalCheckResult ok() {
		LocalCheckResult r;
		r.pass = true;
		return r;
	}

	static LocalCheckResult fail(const std::string& reason) {
		LocalCheckResult r;
		r.pass = false;
		r.reasons.push_back(reason);
		return r;
	}

	// Combine: accumulates failures; result passes only if all pass.
	LocalCheckResult& merge(const LocalCheckResult& other) {
		if (!other.pass) {
			pass = false;
			reasons.insert(reasons.end(), other.reasons.begin(), other.reasons.end());
		}
		return *this;
	}

	std::string reason_string() const;   // join reasons with "; "
};

// ============================================================================
// LocalSanityChecker
// ============================================================================

class LocalSanityChecker {
public:
	// Recognized geometry tags (Phase 2 set).
	static const std::vector<std::string>& known_geometries() noexcept;

	// Run all Phase 2 checks (Tier 0–1). Returns combined result.
	static LocalCheckResult check(const UFXMaterialRecord& record);

	// Tier 2 -- molecular descriptor checks (Phase 6).
	// Failure tag: molecular_descriptor_invalid
	static LocalCheckResult check_molecular(const UFXMaterialRecord& record);

	// Tier 3 -- thermo + EOS checks (Phase 7).
	// Failure tag: thermo_block_invalid
	static LocalCheckResult check_thermo(const UFXMaterialRecord& record);

	// Tier 4 -- crystal block checks (Phase 8, solid-phase only).
	// Failure tag: crystal_block_invalid
	static LocalCheckResult check_crystal(const UFXMaterialRecord& record);

	// Tier 5 -- mechanical + transport checks (Phase 9).
	// Failure tag: macro_block_invalid
	static LocalCheckResult check_macro(const UFXMaterialRecord& record);

	// Run all checks from all populated tiers (Phase 2–9).
	static LocalCheckResult check_all(const UFXMaterialRecord& record);

private:
	static LocalCheckResult check_identity_  (const IdentityBlock&    id);
	static LocalCheckResult check_provenance_(const ProvenanceBlock&   prov);
	static LocalCheckResult check_state_     (const IdentityBlock&    id,
											  double temperature_K,
											  double pressure_atm);
	static LocalCheckResult check_force_field_(const ForceFieldBlock& ff);
};

} // namespace vsepr::ufx
