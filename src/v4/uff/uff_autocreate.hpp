// src/v4/uff/uff_autocreate.hpp
// Formation Engine v4.1.0 -- UFF auto-creator / fallback parameter generator
//
// Handles the case where UFFReferenceProvider::lookup() returns nullopt.
// The reference provider does NOT create parameters; that is exclusively
// this module's responsibility.
//
// Rev 1 fallback strategy (in priority order):
//   1. Element-group interpolation using Rappé 1992 published neighbours
//   2. Period-scaled radius heuristic
//   3. Universal stub (confidence = Estimated, physically usable for topology
//      but not for quantitative energy evaluation)

#pragma once

#include "uff_table.hpp"
#include "uff_reference_provider.hpp"
#include <string>

namespace vsepr::uff {

// Chemical context provided to the auto-creator to help it make
// a reasonable fallback choice.
struct ChemicalContext {
	std::string element;            // e.g. "Os", "Hg"
	int         atomic_number = 0;  // Z number; used for period/group scaling
	int         coordination  = 0;  // anticipated coordination number
	std::string geometry_tag;       // anticipated geometry, or "" if unknown
	double      oxidation_state = 0.0;
};

class UFFAutoCreator {
public:
	// reference_provider is used to borrow published neighbours for
	// interpolation when the exact atom type is absent.
	explicit UFFAutoCreator(const UFFReferenceProvider& reference_provider);

	// Generate a best-effort UFF entry for the given atom type and context.
	// Always returns a populated entry (confidence = Estimated or Derived).
	// Never returns Missing unless atom_type is completely empty.
	UFFEntry create_missing_entry(const std::string& atom_type,
								  const ChemicalContext& ctx) const;

private:
	const UFFReferenceProvider& ref_;

	// Attempt period/group interpolation from two neighbouring published entries.
	std::optional<UFFEntry> interpolate_from_neighbours_(
		const std::string& atom_type,
		const ChemicalContext& ctx) const;

	// Universal stub: period-scaled radii and generic vdW parameters.
	UFFEntry universal_stub_(const std::string& atom_type,
							 const ChemicalContext& ctx) const;
};

} // namespace vsepr::uff
