// src/v4/uff/uff_spotcheck.hpp
// Formation Engine v4.1.0 -- UFX spot-check harness stub
//
// Purpose: confirm that known molecules can be loaded, atom-typed,
//          assigned UFF parameters, and logged consistently.
//
// This harness validates table BEHAVIOUR, not physics accuracy.
// Full energy/gradient validation is a v4.1.0 Tier 1 concern, deferred.
//
// Initial molecules: OsO4, CH3HgI

#pragma once

#include "uff_table.hpp"
#include "uff_autocreate.hpp"
#include "uff_provenance_writer.hpp"
#include <string>
#include <vector>

namespace vsepr::uff {

// A single atom specification used as input to the harness.
struct SpotAtom {
	std::string element;
	std::string atom_type;     // UFF label to resolve
	int         atomic_number = 0;
	int         coordination  = 0;
	std::string geometry_tag;
};

// A molecule specification for one spot-check run.
struct SpotMolecule {
	std::string             test_id;
	std::string             name;
	std::vector<SpotAtom>   atoms;
};

// ---------------------------------------------------------------------------
// SpotCheckHarness
// ---------------------------------------------------------------------------

class SpotCheckHarness {
public:
	// table        -- the live UFF runtime table (pre-loaded with baseline)
	// auto_creator -- fallback generator for missing entries
	// prov_writer  -- provenance + validation log target
	SpotCheckHarness(UFFTable&             table,
					 UFFAutoCreator&       auto_creator,
					 UFFProvenanceWriter&  prov_writer);

	// Run a single molecule check.
	// Resolves each atom's UFF entry (from table or auto-creator),
	// writes provenance for any auto-created entries,
	// and writes a validation row to the log.
	// Returns the SpotCheckResult.
	SpotCheckResult run(const SpotMolecule& molecule);

	// Run a list of molecules and return all results.
	std::vector<SpotCheckResult> run_all(
		const std::vector<SpotMolecule>& molecules);

	// Built-in molecule definitions for the two smoke-test targets.
	static SpotMolecule make_OsO4();
	static SpotMolecule make_CH3HgI();

private:
	UFFTable&            table_;
	UFFAutoCreator&      auto_creator_;
	UFFProvenanceWriter& prov_writer_;
};

} // namespace vsepr::uff
