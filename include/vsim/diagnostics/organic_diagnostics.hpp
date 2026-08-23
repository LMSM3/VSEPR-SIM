/**
 * organic_diagnostics.hpp  -  Organic / peptide scale diagnostics
 *
 * WO-66O  |  Organic/Peptide Scale Diagnostics
 * v5.1.4  |  v5.0.0-main
 *
 * Provides scale-aware diagnostics for organic molecules and peptide chains.
 * Extends the existing domain="peptide" / domain="small_molecule" parser
 * infrastructure with a dedicated [diagnostics.organic] section.
 *
 * Design contract:
 *   - Diagnostic results are purely observational — they do not modify the
 *     simulation trajectory.
 *   - All thresholds carry scientifically motivated defaults but are
 *     user-overridable from the .vsim script.
 *   - Output is appended to the [output] manifest and optionally written to a
 *     separate TSV file.
 *
 * Diagnostic codes assigned to this module: VSIM-D010 … VSIM-D029
 */

#pragma once

#include <string>
#include <vector>

namespace vsim {

// ============================================================================
// PeptideChainDiagnostics  —  per-chain diagnostics for domain="peptide"
// ============================================================================

struct PeptideChainDiagnostics {
	// Backbone geometry
	bool   check_phi_psi          = true;   // Ramachandran φ/ψ angle coverage
	bool   check_omega             = true;   // ω planarity (|ω − 180°| < omega_tol_deg)
	double omega_tol_deg           = 10.0;

	// Secondary structure
	bool   infer_helix             = true;   // α-helix: φ ≈ −57°, ψ ≈ −47°
	bool   infer_sheet             = true;   // β-sheet: φ ≈ −119°, ψ ≈ +113°
	bool   infer_turn              = true;   // β-turn (i → i+3 H-bond)
	double helix_phi_tol_deg       = 20.0;
	double helix_psi_tol_deg       = 20.0;
	double sheet_phi_tol_deg       = 25.0;
	double sheet_psi_tol_deg       = 25.0;

	// Hydrogen bond diagnostics
	bool   check_hbonds            = true;
	double hbond_donor_accept_A    = 3.5;   // D…A cutoff [Å]
	double hbond_angle_tol_deg     = 40.0;  // D-H…A angle tolerance

	// Chirality audit
	bool   check_chirality         = true;  // flag L→D epimerisation events
	bool   allow_d_amino_acids     = false; // D-AA are flagged as warnings if false

	// End-to-end distance and radius of gyration
	bool   compute_end_to_end      = true;
	bool   compute_rg              = true;

	// Output
	bool   write_ramachandran_tsv  = false;
	bool   write_ss_assignment_tsv = true;
	bool   write_hbond_tsv         = false;
};

// ============================================================================
// SmallMoleculeDiagnostics  —  diagnostics for domain="small_molecule"
// ============================================================================

struct SmallMoleculeDiagnostics {
	// Bond geometry
	bool   check_bond_lengths      = true;
	double bond_length_tol_frac    = 0.15;  // fractional deviation from reference

	// Angle geometry
	bool   check_bond_angles       = true;
	double bond_angle_tol_deg      = 15.0;

	// Planarity for aromatic rings
	bool   check_aromatic_planarity = true;
	double planarity_rmsd_tol_A    = 0.05;  // Å

	// Chirality / stereocentre audit
	bool   check_stereocentres     = true;

	// Charge consistency
	bool   check_formal_charge     = true;  // flag if formal charge != net charge from forcefield

	// Output
	bool   write_bond_audit_tsv    = false;
	bool   write_angle_audit_tsv   = false;
};

// ============================================================================
// OrganicScaleSection  —  [diagnostics.organic]
// ============================================================================

struct OrganicScaleSection {
	bool   enabled                 = false;

	// Which domain(s) to run diagnostics on
	bool   run_on_peptide          = true;
	bool   run_on_small_molecule   = true;

	// Sampling cadence
	int    sample_every_n_steps    = 100;   // 0 = only at end
	bool   sample_at_end           = true;

	// Sub-sections
	PeptideChainDiagnostics    peptide;
	SmallMoleculeDiagnostics   small_molecule;

	// Output control
	std::string output_prefix      = "organic_diag";
	bool        write_summary_json = true;
	bool        write_per_frame    = false;

	// Severity: "warn" = emit VSIM-D0xx warning; "error" = abort run
	std::string violation_severity = "warn";
};

// ============================================================================
// OrganicDiagnosticResult  —  one diagnostic finding (runtime product)
// ============================================================================

struct OrganicDiagnosticResult {
	std::string code;           // e.g. "VSIM-D012"
	std::string level;          // "info" | "warn" | "error"
	std::string message;
	int         step    = -1;
	int         atom_id = -1;   // -1 = molecule-level
	std::string chain;
	int         residue = -1;   // -1 = not applicable
};

} // namespace vsim
