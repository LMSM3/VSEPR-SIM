// include/ufx_auto2/molecular_descriptor_generator.hpp
// UFX_AUTO_2 Phase 6 -- Molecular Descriptor Generator
// VSEPR-SIM v5 beta9
//
// MolecularDescriptorGenerator fills rec.molecular from AxisSample and
// periodic table arithmetic. All values are deterministic -- no network calls.
// SMILES is drawn from the SmilesVocab table or left empty if no entry exists.
//
// property_values rows written (block_tier=2, block_name='molecular'):
//   molecular_weight, exact_mass, heavy_atom_count,
//   hbond_donor_count, hbond_acceptor_count, inchikey

#pragma once

#include "v4/uff/ufx_material_record.hpp"
#include "ufx_auto2/axis_config.hpp"

#include <string>
#include <cstdint>

struct sqlite3;

namespace vsepr::ufx {

// ============================================================================
// FillMolecularOptions
// ============================================================================

struct FillMolecularOptions {
	std::string db_path;
	int         batch    = 500;
	bool        verbose  = false;
};

struct FillMolecularResult {
	std::string db_path;
	int         processed = 0;
	int         filled    = 0;
	int         skipped   = 0;   // already had molecular block
	int         failed    = 0;
	bool        success   = false;
	std::string error_message;
};

// ============================================================================
// MolecularDescriptorGenerator
// ============================================================================

class MolecularDescriptorGenerator {
public:
	MolecularDescriptorGenerator();

	// Fill rec.molecular from axis sample + periodic table data.
	// Does NOT call the network. All values are deterministic.
	void fill(UFXMaterialRecord& rec, const AxisSample& s) const;

	// Compute molecular weight from element symbol (atomic weight lookup).
	// Accepts simple element symbol or minimal formula like "Fe2O3".
	static double molecular_weight_from_formula(const std::string& formula) noexcept;

	// Monoisotopic exact mass from element symbol.
	static double exact_mass_from_element(const std::string& symbol) noexcept;

	// Build a stub InChIKey from element + oxidation + coordination.
	// Format: "UFXSTUB-<ELEMENT><OX><COORD>-A" (not a real InChIKey).
	static std::string inchikey_stub(const std::string& element,
									 int oxidation_state,
									 int coordination);

	// Derive molecular formula string for a single-element coordination complex.
	static std::string formula_from_identity(const std::string& element,
											 int oxidation_state,
											 int coordination);

private:
	// Atomic weight (g/mol) by element symbol. Returns 0.0 if unknown.
	static double atomic_weight_(const std::string& sym) noexcept;

	// Monoisotopic mass by element symbol. Returns 0.0 if unknown.
	static double monoisotopic_mass_(const std::string& sym) noexcept;
};

// ============================================================================
// Run fill-molecular over the DB (used by CLI)
// ============================================================================

FillMolecularResult ufx_auto2_fill_molecular(const FillMolecularOptions& opts);
void print_fill_molecular_result(const FillMolecularResult& r);

} // namespace vsepr::ufx
