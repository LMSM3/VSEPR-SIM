// include/ufx_auto2/material_generator.hpp
// UFX_AUTO_2 Phase 2 -- Material Record Generator
// VSEPR-SIM v5 beta8 -> beta9
//
// MaterialRecordGenerator converts an AxisSample into a UFXMaterialRecord
// with populated IdentityBlock, ForceFieldBlock, and ProvenanceBlock.
//
// Force-field generation bridge:
//   1. Attempt LocalUFFReferenceProvider::lookup(element) -> Published entry
//   2. On nullopt: UFFAutoCreator::create_missing_entry() -> Estimated entry
//   Either way the result is mapped into a ForceFieldBlock.
//
// Hard rules enforced:
//   - ProvenanceBlock is always populated (no provenance = no promotion)
//   - material_key is deterministic from the sample fields + short hash
//   - state (T, P, phase) is recorded on identity

#pragma once

#include "axis_config.hpp"
#include "ufx_auto2/coverage_map.hpp"
#include "v4/uff/ufx_material_record.hpp"
#include <string>

namespace vsepr::ufx {

struct GeneratorOptions {
	bool   use_uff_reference  = true;   // bridge LocalUFFReferenceProvider
	bool   use_uff_autocreate = true;   // fallback to UFFAutoCreator
	double generated_confidence = 0.40; // base confidence for GENERATED records
};

class MaterialRecordGenerator {
public:
	explicit MaterialRecordGenerator(const GeneratorOptions& opts = {});

	// Primary entry point. Converts a sample into a record.
	// Always returns a fully populated record (may be REJECTED class
	// if identity cannot be formed, but that is caught by sanity check).
	UFXMaterialRecord from_axis_sample(const AxisSample& sample) const;

private:
	GeneratorOptions opts_;

	// Build the IdentityBlock from a sample.
	IdentityBlock build_identity_(const AxisSample& sample) const;

	// Build the ForceFieldBlock using UFF reference + autocreate bridge.
	ForceFieldBlock build_force_field_(const AxisSample& sample) const;

	// Build the ProvenanceBlock. Always populated.
	ProvenanceBlock build_provenance_(const AxisSample& sample,
									  const ForceFieldBlock& ff) const;

	// Derive a collision-resistant material_key from sample fields.
	// Format: {element}_ox{ox}_coord{c}_{geom}_{phase}_{hash4}
	static std::string make_material_key_(const AxisSample& sample);

	// Derive atomic number from element symbol (Z=1..118, 0 if unknown).
	static int atomic_number_for_(const std::string& element) noexcept;

	// Derive period (1..7) from Z.
	static int period_for_Z_(int Z) noexcept;
};

} // namespace vsepr::ufx
