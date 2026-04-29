// src/v4/uff/uff_spotcheck.cpp
// Formation Engine v4.1.0 -- UFX spot-check harness implementation

#include "uff_spotcheck.hpp"
#include <sstream>

namespace vsepr::uff {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SpotCheckHarness::SpotCheckHarness(UFFTable&            table,
								   UFFAutoCreator&      auto_creator,
								   UFFProvenanceWriter& prov_writer)
	: table_(table)
	, auto_creator_(auto_creator)
	, prov_writer_(prov_writer)
{}

// ---------------------------------------------------------------------------
// run() -- single molecule
// ---------------------------------------------------------------------------

SpotCheckResult SpotCheckHarness::run(const SpotMolecule& mol) {
	SpotCheckResult result;
	result.test_id   = mol.test_id;
	result.molecule  = mol.name;
	result.passed    = true;

	int missing_count = 0;
	std::ostringstream notes;

	for (const auto& atom : mol.atoms) {
		auto entry = table_.lookup(atom.atom_type);

		if (!entry.has_value()) {
			// Auto-create fallback.
			ChemicalContext ctx;
			ctx.element       = atom.element;
			ctx.atomic_number = atom.atomic_number;
			ctx.coordination  = atom.coordination;
			ctx.geometry_tag  = atom.geometry_tag;

			UFFEntry generated =
				auto_creator_.create_missing_entry(atom.atom_type, ctx);

			table_.insert(generated);
			prov_writer_.write_generated_record(generated);
			prov_writer_.write_provenance_record(generated);

			if (generated.confidence == ParamConfidence::Estimated) {
				++missing_count;
				notes << atom.atom_type << " estimated by fallback; ";
			}
		} else {
			// Already in table -- record provenance if it was freshly looked up.
			// (Skip re-logging entries that were loaded from the baseline.)
		}
	}

	result.missing_count = missing_count;
	result.passed        = (missing_count == 0);

	if (notes.str().empty()) {
		result.notes = "all atom types resolved";
	} else {
		std::string n = notes.str();
		if (!n.empty() && n.back() == ' ') n.pop_back();  // trim trailing space
		if (!n.empty() && n.back() == ';') n.pop_back();  // trim trailing semicolon
		result.notes = n;
	}

	prov_writer_.write_validation_record(result);
	return result;
}

// ---------------------------------------------------------------------------
// run_all()
// ---------------------------------------------------------------------------

std::vector<SpotCheckResult> SpotCheckHarness::run_all(
	const std::vector<SpotMolecule>& molecules)
{
	std::vector<SpotCheckResult> results;
	results.reserve(molecules.size());
	for (const auto& mol : molecules) {
		results.push_back(run(mol));
	}
	return results;
}

// ---------------------------------------------------------------------------
// Smoke-test molecule definitions
// ---------------------------------------------------------------------------

// OsO4 -- osmium tetroxide, Td symmetry, Os oxidation state +8
// Os uses Os6+6 as the closest published UFF type for Os in high-oxidation
// tetrahedral environment; O uses O_2 (double-bond terminal oxygen).
SpotMolecule SpotCheckHarness::make_OsO4() {
	SpotMolecule m;
	m.test_id = "spot_001";
	m.name    = "OsO4";
	m.atoms   = {
		{ "Os", "Os6+6", 76, 4, "tetrahedral" },
		{ "O",  "O_2",    8, 1, "linear"      },
		{ "O",  "O_2",    8, 1, "linear"      },
		{ "O",  "O_2",    8, 1, "linear"      },
		{ "O",  "O_2",    8, 1, "linear"      },
	};
	return m;
}

// CH3HgI -- methylmercury iodide
// C uses C_3 (sp3 methyl carbon), H uses H_, Hg uses Hg1+2 (linear), I uses I_.
SpotMolecule SpotCheckHarness::make_CH3HgI() {
	SpotMolecule m;
	m.test_id = "spot_002";
	m.name    = "CH3HgI";
	m.atoms   = {
		{ "C",  "C_3",   6, 4, "tetrahedral" },
		{ "H",  "H_",    1, 1, "linear"      },
		{ "H",  "H_",    1, 1, "linear"      },
		{ "H",  "H_",    1, 1, "linear"      },
		{ "Hg", "Hg1+2",80, 2, "linear"      },
		{ "I",  "I_",   53, 1, "linear"      },
	};
	return m;
}

} // namespace vsepr::uff
