// src/ufx_auto2/local_sanity.cpp
// UFX_AUTO_2 Phases 2-9 -- Local Sanity Checker Implementation
// VSEPR-SIM v5 beta9

#include "ufx_auto2/local_sanity.hpp"
#include <cmath>
#include <sstream>

namespace vsepr::ufx {

// ============================================================================
// LocalCheckResult helpers
// ============================================================================

std::string LocalCheckResult::reason_string() const {
	if (reasons.empty()) return "";
	std::ostringstream out;
	for (std::size_t i = 0; i < reasons.size(); ++i) {
		if (i > 0) out << "; ";
		out << reasons[i];
	}
	return out.str();
}

// ============================================================================
// Known geometries (Phase 2 set -- matches AxisConfig::default_phase2)
// ============================================================================

const std::vector<std::string>& LocalSanityChecker::known_geometries() noexcept {
	static const std::vector<std::string> k_geometries = {
		"linear",
		"trigonal_planar",
		"tetrahedral",
		"square_planar",
		"trigonal_bipyramidal",
		"octahedral",
		"pentagonal_bipyramidal",
		// Accept empty / unknown without rejecting — generator may produce these
		// for deferred axes.  The check below treats unknown as a warning, not
		// a hard rejection.
	};
	return k_geometries;
}

// ============================================================================
// check_identity_
// ============================================================================

LocalCheckResult LocalSanityChecker::check_identity_(const IdentityBlock& id) {
	if (id.material_key.empty())
		return LocalCheckResult::fail("missing material_key");

	if (id.elements.empty())
		return LocalCheckResult::fail("missing elements");

	if (id.formula.empty())
		return LocalCheckResult::fail("missing formula");

	if (id.phase.empty())
		return LocalCheckResult::fail("missing phase");

	if (id.coordination_number < 0 || id.coordination_number > 12)
		return LocalCheckResult::fail(
			"coordination_number out of range [0,12]: "
			+ std::to_string(id.coordination_number));

	// Geometry: warn (not reject) if not in known list — deferred axes allowed.
	// Hard reject only on empty geometry_tag.
	if (id.geometry_tag.empty())
		return LocalCheckResult::fail("missing geometry_tag");

	return LocalCheckResult::ok();
}

// ============================================================================
// check_provenance_
// Hard rule: no provenance = no promotion (and no GENERATED status).
// ============================================================================

LocalCheckResult LocalSanityChecker::check_provenance_(const ProvenanceBlock& prov) {
	if (!prov.has_provenance())
		return LocalCheckResult::fail("missing provenance (hard rule: no provenance = no promotion)");

	if (prov.source_class_tag.empty())
		return LocalCheckResult::fail("missing provenance.source_class_tag");

	return LocalCheckResult::ok();
}

// ============================================================================
// check_state_
// Hard rule: T, P, and phase must be present on every property.
// Phase 2: these come from the AxisSample and are always set by the generator,
// but we verify defensively.
// ============================================================================

LocalCheckResult LocalSanityChecker::check_state_(
	const IdentityBlock& id,
	double temperature_K,
	double pressure_atm)
{
	if (!std::isfinite(temperature_K) || temperature_K <= 0.0)
		return LocalCheckResult::fail(
			"temperature_K not finite or <= 0: " + std::to_string(temperature_K));

	if (!std::isfinite(pressure_atm) || pressure_atm < 0.0)
		return LocalCheckResult::fail(
			"pressure_atm not finite or < 0: " + std::to_string(pressure_atm));

	if (id.phase.empty())
		return LocalCheckResult::fail("phase missing (state requires T, P, phase)");

	return LocalCheckResult::ok();
}

// ============================================================================
// check_force_field_
// ============================================================================

LocalCheckResult LocalSanityChecker::check_force_field_(const ForceFieldBlock& ff) {
	// If the atom_type is empty something went badly wrong in the generator.
	if (ff.atom_type.empty())
		return LocalCheckResult::fail("force_field.atom_type is empty");

	// Check that numerical fields are finite when non-zero.
	// A zero value is allowed (stub / deferred), but NaN/inf is not.
	auto check_finite = [&](double v, const char* name) -> LocalCheckResult {
		if (!std::isfinite(v))
			return LocalCheckResult::fail(
				std::string("force_field.") + name + " is not finite");
		return LocalCheckResult::ok();
	};

	LocalCheckResult r = LocalCheckResult::ok();
	r.merge(check_finite(ff.r1,     "r1"));
	r.merge(check_finite(ff.theta0, "theta0"));
	r.merge(check_finite(ff.x1,     "x1"));
	r.merge(check_finite(ff.D1,     "D1"));
	r.merge(check_finite(ff.zeta,   "zeta"));
	r.merge(check_finite(ff.Z1,     "Z1"));

	if (ff.parameter_confidence < 0.0 || ff.parameter_confidence > 1.0)
		r.merge(LocalCheckResult::fail("force_field.parameter_confidence out of [0,1]"));

	return r;
}

// ============================================================================
// check  (primary Tier 0-1 entry point)
// ============================================================================

LocalCheckResult LocalSanityChecker::check(const UFXMaterialRecord& record) {
	LocalCheckResult result = LocalCheckResult::ok();

	result.merge(check_identity_(record.identity));
	result.merge(check_provenance_(record.provenance));

	if (record.provenance.unit_conversion_trace.empty())
		result.merge(LocalCheckResult::fail("provenance.unit_conversion_trace missing (T/P state required)"));

	result.merge(check_force_field_(record.force_field));

	return result;
}

// ============================================================================
// check_molecular  (Tier 2 -- Phase 6)
// ============================================================================

LocalCheckResult LocalSanityChecker::check_molecular(const UFXMaterialRecord& record) {
	LocalCheckResult result = LocalCheckResult::ok();
	const auto& m = record.molecular;

	if (m.molecular_weight <= 0.0)
		result.merge(LocalCheckResult::fail(
			"molecular_descriptor_invalid: molecular_weight <= 0"));

	if (m.heavy_atom_count < 1)
		result.merge(LocalCheckResult::fail(
			"molecular_descriptor_invalid: heavy_atom_count < 1"));

	if (record.identity.is_populated() && m.inchikey.empty())
		result.merge(LocalCheckResult::fail(
			"molecular_descriptor_invalid: inchikey empty on populated identity"));

	return result;
}

// ============================================================================
// check_thermo  (Tier 3 -- Phase 7)
// ============================================================================

LocalCheckResult LocalSanityChecker::check_thermo(const UFXMaterialRecord& record) {
	LocalCheckResult result = LocalCheckResult::ok();
	const auto& t  = record.thermo;
	const auto& eos = record.eos;
	const std::string& phase = record.identity.phase;

	// melting point must be positive for solids
	if (phase == "solid") {
		if (t.melting_point_K.has_value() && *t.melting_point_K <= 0.0)
			result.merge(LocalCheckResult::fail(
				"thermo_block_invalid: melting_point_K <= 0 for solid"));
	}

	// boiling_point > melting_point if both present
	if (t.boiling_point_K.has_value() && t.melting_point_K.has_value()) {
		if (*t.boiling_point_K <= *t.melting_point_K)
			result.merge(LocalCheckResult::fail(
				"thermo_block_invalid: boiling_point_K <= melting_point_K"));
	}

	// Cp(T) curve: if present, must have >= 2 points and no negative Cp
	if (!t.cp_T.empty()) {
		if (t.cp_T.size() < 2)
			result.merge(LocalCheckResult::fail(
				"thermo_block_invalid: cp_T curve has fewer than 2 points"));
		for (const auto& pt : t.cp_T) {
			if (pt.value < 0.0) {
				result.merge(LocalCheckResult::fail(
					"thermo_block_invalid: negative Cp in cp_T curve"));
				break;
			}
		}
	}

	// delta_Hf plausibility check by identity phase (broad bounds)
	if (t.delta_Hf_298_kJ_mol.has_value()) {
		double hf = *t.delta_Hf_298_kJ_mol;
		if (hf < -2000.0 || hf > 500.0)
			result.merge(LocalCheckResult::fail(
				"thermo_block_invalid: delta_Hf_298 outside plausible range [-2000, 500] kJ/mol"));
	}

	// EOS model must be non-empty if EOS block is used
	if (!eos.eos_model.empty()) {
		// eos_model present but value must be known
		static const std::vector<std::string> k_eos_models = {
			"ideal", "vdW", "RK", "PR", "tabular", "mixed_PvT"
		};
		bool found = false;
		for (const auto& m : k_eos_models)
			if (eos.eos_model == m) { found = true; break; }
		if (!found)
			result.merge(LocalCheckResult::fail(
				"thermo_block_invalid: unknown eos_model '" + eos.eos_model + "'"));
	}

	return result;
}

// ============================================================================
// check_crystal  (Tier 4 -- Phase 8, solid only)
// ============================================================================

LocalCheckResult LocalSanityChecker::check_crystal(const UFXMaterialRecord& record) {
	LocalCheckResult result = LocalCheckResult::ok();
	const auto& c = record.crystal;

	// Only applies to solid-phase records
	if (record.identity.phase != "solid")
		return result;

	// If crystal block appears populated, validate it
	if (c.space_group.empty() && c.lattice_abc_angstrom.x <= 0.0)
		return result;  // block not filled yet -- not an error

	if (c.space_group.empty())
		result.merge(LocalCheckResult::fail(
			"crystal_block_invalid: space_group is empty"));

	if (c.lattice_abc_angstrom.x <= 0.0)
		result.merge(LocalCheckResult::fail(
			"crystal_block_invalid: lattice_a <= 0"));

	if (c.density_g_cm3 <= 0.0)
		result.merge(LocalCheckResult::fail(
			"crystal_block_invalid: density_g_cm3 <= 0"));

	if (c.packing_fraction < 0.10 || c.packing_fraction > 0.82) {
		if (c.packing_fraction != 0.0)  // allow unset (0.0)
			result.merge(LocalCheckResult::fail(
				"crystal_block_invalid: packing_fraction outside [0.10, 0.82]"));
	}

	if (c.energy_above_hull_eV_atom.has_value()) {
		double hull = *c.energy_above_hull_eV_atom;
		if (hull < -0.1 || hull > 1.5)
			result.merge(LocalCheckResult::fail(
				"crystal_block_invalid: energy_above_hull_eV_atom outside [-0.1, 1.5]"));
	}

	return result;
}

// ============================================================================
// check_macro  (Tier 5 -- Phase 9)
// ============================================================================

LocalCheckResult LocalSanityChecker::check_macro(const UFXMaterialRecord& record) {
	LocalCheckResult result = LocalCheckResult::ok();
	const auto& mech = record.mechanical;
	const auto& trans = record.transport;

	// E_eff
	if (mech.E_eff_GPa.has_value()) {
		double E = *mech.E_eff_GPa;
		if (E < 0.001 || E > 1200.0)
			result.merge(LocalCheckResult::fail(
				"macro_block_invalid: E_eff_GPa outside [0.001, 1200]"));
	}

	// k_eff
	if (trans.k_eff_W_mK.has_value() && *trans.k_eff_W_mK <= 0.0)
		result.merge(LocalCheckResult::fail(
			"macro_block_invalid: k_eff_W_mK <= 0"));

	// diffusivity
	if (trans.diffusivity_m2_s.has_value()) {
		double D = *trans.diffusivity_m2_s;
		if (D <= 0.0 || D > 1.0e-4)
			result.merge(LocalCheckResult::fail(
				"macro_block_invalid: diffusivity_m2_s outside (0, 1e-4]"));
	}

	// porosity
	if (trans.porosity.has_value()) {
		double phi = *trans.porosity;
		if (phi < 0.0 || phi > 0.98)
			result.merge(LocalCheckResult::fail(
				"macro_block_invalid: porosity outside [0, 0.98]"));
	}

	// Hard rule: approximation_label must be set if mechanical block is populated
	if (mech.E_eff_GPa.has_value() && mech.test_method.empty())
		result.merge(LocalCheckResult::fail(
			"macro_block_invalid: mechanical.test_method (approximation_label) is empty (hard rule: label the approximation)"));

	return result;
}

// ============================================================================
// check_all  (all tiers)
// ============================================================================

LocalCheckResult LocalSanityChecker::check_all(const UFXMaterialRecord& record) {
	LocalCheckResult result = LocalCheckResult::ok();
	result.merge(check(record));
	result.merge(check_molecular(record));
	result.merge(check_thermo(record));
	result.merge(check_crystal(record));
	result.merge(check_macro(record));
	return result;
}

} // namespace vsepr::ufx
