#pragma once
/**
 * include/vsim/analysis/mcf_cai.hpp
 * ====================================
 * WO-76 Step 3  |  MCF-CAI State Vector Model — C++ Schema
 *
 * Implements the 3×3 MCF-CAI object state grid defined in:
 *   docs/Theoretical/MCF_CAI_State_Vector_Model.tex  v1.0
 *
 * ── The Grid ────────────────────────────────────────────────────────────────
 *
 *           C (Carrier)      A (Action)      I (Information)
 *  Macro    M_C              M_A             M_I
 *  Chemical C_C              C_A             C_I
 *  Fundmntl F_C              F_A             F_I
 *
 *   V_obj = Σ_{s ∈ {M,C,F}} Σ_{b ∈ {C,A,I}}  V_{s,b} e_s ⊗ e_b
 *
 * ── Layer semantics (MCF axis) ──────────────────────────────────────────────
 *   Macro       S_mat — geometry, phase, grain structure, bulk object state
 *   Chemical    S3    — atoms, bonds, coordination, molecular geometry
 *   Fundamental S0–S2 — charge, spin proxy, nuclear state, colour bookkeeping
 *
 * ── Basis semantics (CAI axis) ──────────────────────────────────────────────
 *   Carrier   (C)  — "what is here"    — configuration, structure, geometry
 *   Action    (A)  — "what is happening" — forces, reactions, coupling
 *   Information(I) — "what has been lost" — sidecar audit, NEVER a force input
 *
 * ── Doctrine for the Information column ────────────────────────────────────
 *   ALL Information-column (I) fields are SIDECAR-ONLY.
 *   They must NEVER be used as force inputs.
 *   They must NEVER be written back into truth-state (.xyz / .xyzFull).
 *   They are derived audit quantities; they live only in the analysis layer.
 *
 *   M_I  ↔  IKK IV  S_mat: formation memory, coarse-graining loss
 *   C_I  ↔  IKK II  chemical distinguishability D_chem, projection loss
 *   F_I  ↔  IKK III |Ψ^hid⟩, projection error ε_{I,ab}, entropy trace
 *
 * ── Namespace note ──────────────────────────────────────────────────────────
 *   "CAI" in this file = Carrier/Action/Information (identity basis).
 *   "caf_channel" in vsim_document.hpp = Colour-Averaged Force (force channel,
 *   formerly "cai_channel", renamed in WO-76 Step 1 to avoid collision).
 *   These are distinct; do NOT conflate.
 *
 * ── VSIM parser section ─────────────────────────────────────────────────────
 *   [object.macro.carrier]      → McfCaiSection.grid[MACRO][CARRIER]
 *   [object.macro.action]       → McfCaiSection.grid[MACRO][ACTION]
 *   [object.macro.information]  → McfCaiSection.grid[MACRO][INFO]   (sidecar)
 *   [object.chemical.carrier]   → McfCaiSection.grid[CHEM][CARRIER]
 *   [object.chemical.action]    → McfCaiSection.grid[CHEM][ACTION]
 *   [object.chemical.information] → McfCaiSection.grid[CHEM][INFO]
 *   [object.fundamental.carrier]   → McfCaiSection.grid[FUND][CARRIER]
 *   [object.fundamental.action]    → McfCaiSection.grid[FUND][ACTION]
 *   [object.fundamental.information] → McfCaiSection.grid[FUND][INFO]
 *
 * Added: WO-76 Step 3 (Day 76)
 */

#include <array>
#include <string>

namespace vsim {
namespace analysis {

// ============================================================================
// Enum tags — axis identifiers for the 3×3 grid
// ============================================================================

/** MCF layer index (row of the grid). */
enum class McfLayer : int {
	MACRO    = 0,   // Material / macro scale
	CHEMICAL = 1,   // Atomistic / molecular scale
	FUND     = 2,   // Fundamental / subatomic proxy
};

/** CAI basis index (column of the grid). */
enum class CaiBasis : int {
	CARRIER     = 0,  // Configuration state ("what is here")
	ACTION      = 1,  // Coupling / dynamics ("what is happening")
	INFORMATION = 2,  // Identity audit ("what has been lost") — sidecar only
};

static constexpr int MCF_LAYERS  = 3;
static constexpr int CAI_BASES   = 3;

// ============================================================================
// McfCaiCell  —  one cell of the 3×3 grid
// ============================================================================
/**
 * A flat struct representing a single cell V_{s,b} of the MCF-CAI grid.
 * Carry layer and basis tags so a cell can be identified from a flat list
 * or grid array without external context.
 *
 * Field content per cell (from Section 4 of MCF_CAI_State_Vector_Model.tex):
 *
 *  M_C  Macro Carrier:
 *         position/orientation, phase, grain structure, bounding volume
 *       → centre_x/y/z, orientation_proxy, phase_label, grain_count
 *
 *  M_A  Macro Action:
 *         stress tensor proxy, heat flux, fracture probability, deformation
 *       → stress_proxy, heat_flux_proxy, fracture_prob, deformation_proxy
 *
 *  M_I  Macro Information (sidecar only):
 *         formation age, defect memory, coarse-graining residual
 *       → formation_age_fs, defect_memory, cg_residual
 *
 *  C_C  Chemical Carrier:
 *         atomic number Z, mass, bond topology proxy, coordination number
 *       → atomic_number, mass, coordination_number, bond_order_proxy
 *
 *  C_A  Chemical Action:
 *         reaction events flag, oxidation state, ionisation flag
 *       → reaction_active, oxidation_state, ionisation_flag
 *
 *  C_I  Chemical Information (sidecar only):
 *         bond entropy loss, chemical distinguishability D_chem, formation route
 *       → bond_entropy_loss, d_chem, formation_route
 *
 *  F_C  Fundamental Carrier:
 *         net charge, spin proxy, nuclear state, isotope label
 *       → net_charge, spin_proxy, nuclear_state, isotope_label
 *
 *  F_A  Fundamental Action:
 *         field coupling (EM, strong proxy, weak proxy), decay flag
 *       → em_coupling, strong_proxy, weak_proxy, decay_flag
 *
 *  F_I  Fundamental Information (sidecar only):
 *         hidden residual |Ψ^hid|, projection loss ΔD, entropy trace
 *       → psi_hid, projection_loss, entropy_trace
 *
 * NOTE: Information column fields (is_information() == true) are SIDECAR ONLY.
 *       They must not enter the force kernel.
 */
struct McfCaiCell {
	McfLayer  layer { McfLayer::MACRO };
	CaiBasis  basis { CaiBasis::CARRIER };

	// ---- Macro Carrier fields (layer=MACRO, basis=CARRIER) -----------------
	double  centre_x          { 0.0 };  // object centroid X (Å)
	double  centre_y          { 0.0 };  // object centroid Y (Å)
	double  centre_z          { 0.0 };  // object centroid Z (Å)
	double  orientation_proxy { 0.0 };  // [0,1] orientation order parameter
	std::string phase_label;            // e.g. "FCC", "liquid", "amorphous"
	int     grain_count       { 0 };    // number of grains (0 = not computed)

	// ---- Macro Action fields (layer=MACRO, basis=ACTION) -------------------
	double  stress_proxy      { 0.0 };  // scalar stress proxy (GPa)
	double  heat_flux_proxy   { 0.0 };  // heat flux proxy (W/m²)
	double  fracture_prob     { 0.0 };  // [0,1] fracture probability
	double  deformation_proxy { 0.0 };  // [0,1] deformation gradient proxy

	// ---- Macro Information fields (SIDECAR ONLY) ---------------------------
	double  formation_age_fs  { 0.0 };  // age of this formation record (fs)
	double  defect_memory     { 0.0 };  // [0,1] residual defect information
	double  cg_residual       { 0.0 };  // coarse-graining identity residual

	// ---- Chemical Carrier fields (layer=CHEMICAL, basis=CARRIER) -----------
	int     atomic_number     { 0 };    // Z
	double  mass              { 0.0 };  // atomic mass (amu)
	int     coordination_number{ 0 };   // CN
	double  bond_order_proxy  { 0.0 };  // effective bond order proxy

	// ---- Chemical Action fields (layer=CHEMICAL, basis=ACTION) -------------
	bool    reaction_active   { false };// true when reaction event in frame
	int     oxidation_state   { 0 };    // formal oxidation state
	bool    ionisation_flag   { false };// true when ionised

	// ---- Chemical Information fields (SIDECAR ONLY) ------------------------
	double  bond_entropy_loss { 0.0 };  // nats; bond topology entropy loss
	double  d_chem            { 0.0 };  // [0,1] chemical distinguishability
	std::string formation_route;        // e.g. "precipitation", "CVD"

	// ---- Fundamental Carrier fields (layer=FUND, basis=CARRIER) -----------
	double  net_charge        { 0.0 };  // elementary charge units
	double  spin_proxy        { 0.0 };  // [0,1] spin coherence proxy
	int     nuclear_state     { 0 };    // 0=ground, 1=excited, -1=unknown
	std::string isotope_label;          // e.g. "12C", "1H"

	// ---- Fundamental Action fields (layer=FUND, basis=ACTION) -------------
	double  em_coupling       { 0.0 };  // EM field coupling strength proxy
	double  strong_proxy      { 0.0 };  // strong-force proxy (binding energy)
	double  weak_proxy        { 0.0 };  // weak-force proxy (decay rate proxy)
	bool    decay_flag        { false };// true when decay event modelled

	// ---- Fundamental Information fields (SIDECAR ONLY) --------------------
	double  psi_hid           { 0.0 };  // |Ψ^hid| hidden residual norm
	double  projection_loss   { 0.0 };  // ΔD: distinguishability loss on projection
	double  entropy_trace     { 0.0 };  // entropy-loss trace for this cell

	// ---- Helpers -----------------------------------------------------------

	/** True when this cell belongs to the Information column (sidecar only). */
	[[nodiscard]] bool is_information() const noexcept {
		return basis == CaiBasis::INFORMATION;
	}

	/** True when this cell is safe to use as a force/dynamics input. */
	[[nodiscard]] bool is_dynamics_eligible() const noexcept {
		return basis != CaiBasis::INFORMATION;
	}

	/** Human-readable cell name e.g. "macro.carrier". */
	[[nodiscard]] std::string cell_name() const;
};

// ============================================================================
// McfCaiSection  —  the full 3×3 grid for one object
// ============================================================================
/**
 * The complete MCF-CAI state vector for one simulated object.
 * Stores all 9 cells as grid[MCF_LAYER][CAI_BASIS].
 *
 * Access pattern:
 *   sec.grid[static_cast<int>(McfLayer::MACRO)][static_cast<int>(CaiBasis::INFO)]
 *   sec.cell(McfLayer::MACRO, CaiBasis::INFORMATION)  // convenience accessor
 *
 * Populated by the parser when [object.<layer>.<basis>] sections are present.
 * present[l][b] is true when the corresponding section appeared in the script.
 */
struct McfCaiSection {
	std::array<std::array<McfCaiCell, CAI_BASES>, MCF_LAYERS> grid;
	std::array<std::array<bool,      CAI_BASES>, MCF_LAYERS> present {};

	// Initialise grid with correct layer/basis tags.
	McfCaiSection() noexcept {
		for (int l = 0; l < MCF_LAYERS; ++l) {
			for (int b = 0; b < CAI_BASES; ++b) {
				grid[l][b].layer = static_cast<McfLayer>(l);
				grid[l][b].basis = static_cast<CaiBasis>(b);
				present[l][b]    = false;
			}
		}
	}

	// Convenience mutable accessor.
	McfCaiCell& cell(McfLayer l, CaiBasis b) noexcept {
		return grid[static_cast<int>(l)][static_cast<int>(b)];
	}

	// Convenience const accessor.
	const McfCaiCell& cell(McfLayer l, CaiBasis b) const noexcept {
		return grid[static_cast<int>(l)][static_cast<int>(b)];
	}

	// Mark a cell present (called by parser when section encountered).
	void mark_present(McfLayer l, CaiBasis b) noexcept {
		present[static_cast<int>(l)][static_cast<int>(b)] = true;
	}

	// True if the given cell was explicitly set by a script section.
	bool is_present(McfLayer l, CaiBasis b) const noexcept {
		return present[static_cast<int>(l)][static_cast<int>(b)];
	}

	// True if ANY Information-column cell is present (sidecar audit active).
	bool has_information_column() const noexcept {
		for (int l = 0; l < MCF_LAYERS; ++l)
			if (present[l][static_cast<int>(CaiBasis::INFORMATION)]) return true;
		return false;
	}
};

// ============================================================================
// Free functions
// ============================================================================

/** Return the layer enum for a string ("macro", "chemical", "fundamental"). */
bool parse_mcf_layer(const std::string& s, McfLayer& out) noexcept;

/** Return the basis enum for a string ("carrier", "action", "information"). */
bool parse_cai_basis(const std::string& s, CaiBasis& out) noexcept;

/**
 * Parse a section name like "object.macro.carrier" into layer + basis.
 * Returns false if the section name does not match the pattern.
 * Input must start with "object." prefix (already stripped by caller if needed).
 */
bool parse_mcf_cai_section(const std::string& section,
							McfLayer& layer_out,
							CaiBasis& basis_out) noexcept;

} // namespace analysis
} // namespace vsim
