#pragma once
/**
 * kernel_event.hpp — Central Kernel Event Hierarchy
 * ==================================================
 *
 * WO-56C: Every major computed event in VSEPR-SIM is a KernelEvent.
 *
 * The kernel is the spine. Everything else is an organ.
 *
 * Doctrine:
 *   xyz / xyzf / xyzFull
 *           ↓
 *   Central Kernel (routes calculation)
 *           ↓
 *   KernelEvent (symbolic + numeric trace)
 *           ↓
 *   KernelEventLog (append-only registry)
 *           ↓
 *   analysis layer → report → dashboard
 *
 * Each KernelEvent stores:
 *   - what was calculated   (event_type, equation_symbolic)
 *   - where it came from    (source_formula, frame_id, source_id)
 *   - which equation was used (equation_symbolic)
 *   - which values were substituted (equation_numeric)
 *   - what numeric answer resulted (result_value, result_unit)
 *   - a stable event ID     (event_id — monotonic uint64)
 *
 * Specializations:
 *   ReactionEvent         — A + B → C, ΔE
 *   ChemicalStateEvent    — bond / coordination / local energy change
 *   FormationEvent        — bead/lattice formation outcome
 *   DefectEvent           — vacancy, interstitial, substitution
 *   TransportEvent        — diffusion, ionic drift, permeation
 *   ContinualReportEvent  — script-declared metric snapshot (see .vsim [export])
 *
 * Design rules:
 *   - All fields are public aggregates. No hidden state.
 *   - Base KernelEvent carries enough for a full audit trail.
 *   - Derived types extend without overriding base fields.
 *   - No virtual dispatch (use KernelEventKind tag for type identification).
 *   - result_value is double. Unit is a plain string.
 *   - Timestamps are uint64_t simulation step counts (not wall clock).
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include <cstdint>
#include <string>
#include <vector>

namespace vsepr::kernel {

// ============================================================================
// Event type classification
// ============================================================================

enum class KernelEventKind : uint8_t {
	Unknown          = 0,
	Reaction         = 1,   // Chemical reaction: A + B → C
	ChemicalState    = 2,   // Local chemistry change (bond, coordination)
	Formation        = 3,   // Bead/lattice formation outcome
	Defect           = 4,   // Structural defect (vacancy, interstitial, substitution)
	Transport        = 5,   // Diffusion, drift, permeation
	ContinualReport  = 6,   // Rolling metric snapshot
};

inline const char* kind_name(KernelEventKind k) {
	switch (k) {
		case KernelEventKind::Reaction:        return "Reaction";
		case KernelEventKind::ChemicalState:   return "ChemicalState";
		case KernelEventKind::Formation:       return "Formation";
		case KernelEventKind::Defect:          return "Defect";
		case KernelEventKind::Transport:       return "Transport";
		case KernelEventKind::ContinualReport: return "ContinualReport";
		default:                               return "Unknown";
	}
}

// ============================================================================
// KernelEvent — base record
// ============================================================================

struct KernelEvent {
	uint64_t        event_id        = 0;    // Monotonic ID assigned by KernelEventLog
	KernelEventKind kind            = KernelEventKind::Unknown;

	// Provenance
	uint64_t        frame_id        = 0;    // Trajectory frame / simulation step
	std::string     source_formula;          // e.g. "C6H12", "C" (graphite), "Fe"
	int             source_particle_id = -1; // Persistent particle ID (-1 = system-level)

	// Equation trace (anti-black-box core)
	std::string     equation_symbolic;   // e.g. "A + B -> C"
	std::string     equation_numeric;    // e.g. "(-124.2) - (-80.1 + -39.7)"
	double          result_value    = 0.0;
	std::string     result_unit;         // e.g. "kcal/mol", "Å", "fraction"

	// Validity
	bool            is_valid        = true;
	std::string     warning;             // Non-empty if result is flagged

	// Constructors
	KernelEvent() = default;
	KernelEvent(KernelEventKind k, const std::string& formula, uint64_t frame)
		: kind(k), frame_id(frame), source_formula(formula) {}
};

// ============================================================================
// ReactionEvent — A + B → C, ΔE
// ============================================================================

/**
 * A chemical reaction event.
 *
 * Symbolic display example:
 *
 *   A + B -> C
 *   Delta_E = E_products - E_reactants
 *   Delta_E = (E_C) - (E_A + E_B)
 *   Delta_E = (-124.2) - (-80.1 + -39.7)
 *   Delta_E = -4.4 kcal/mol
 */
struct ReactionEvent : KernelEvent {
	std::vector<std::string> reactants;       // e.g. {"A", "B"}
	std::vector<std::string> products;        // e.g. {"C"}
	std::vector<double>      reactant_energies; // kcal/mol per reactant
	std::vector<double>      product_energies;  // kcal/mol per product
	double                   delta_E      = 0.0; // kcal/mol
	std::string              reaction_rule;      // e.g. "electrophilic_addition"
	bool                     exothermic   = false;

	ReactionEvent() { kind = KernelEventKind::Reaction; result_unit = "kcal/mol"; }

	void compute_delta_E() {
		double E_react = 0.0, E_prod = 0.0;
		for (double e : reactant_energies) E_react += e;
		for (double e : product_energies)  E_prod  += e;
		delta_E    = E_prod - E_react;
		result_value = delta_E;
		exothermic = (delta_E < 0.0);

		// Build numeric trace
		std::string rs, ps;
		for (size_t i = 0; i < reactants.size(); ++i) {
			if (i) rs += " + ";
			rs += reactants[i];
		}
		for (size_t i = 0; i < products.size(); ++i) {
			if (i) ps += " + ";
			ps += products[i];
		}
		equation_symbolic = rs + " -> " + ps;
		equation_numeric  = "Delta_E = (" + std::to_string(E_prod) + ") - ("
						  + std::to_string(E_react) + ") = "
						  + std::to_string(delta_E) + " kcal/mol";
	}
};

// ============================================================================
// ChemicalStateEvent — local chemistry change
// ============================================================================

/**
 * Records a change in local chemical state without a full reaction.
 *
 * Useful for:
 *   - Bond formation / breakage
 *   - Coordination number change
 *   - π-electron ring formation (graphene, PAH)
 *   - Ionic clustering (FLiBe)
 *   - Defect neighborhood reassignment
 */
struct ChemicalStateEvent : KernelEvent {
	int     particle_i          = -1;
	int     particle_j          = -1;   // -1 if single-particle event
	double  coordination_before = 0.0;
	double  coordination_after  = 0.0;
	double  local_energy_before = 0.0;  // kcal/mol
	double  local_energy_after  = 0.0;  // kcal/mol
	double  bond_length_ang     = 0.0;  // Å (0 = not applicable)
	std::string state_tag_before;       // e.g. "sp3", "sp2", "ionic"
	std::string state_tag_after;

	ChemicalStateEvent() { kind = KernelEventKind::ChemicalState; result_unit = "kcal/mol"; }

	void compute() {
		result_value      = local_energy_after - local_energy_before;
		equation_symbolic = "delta_E_local = E_after - E_before";
		equation_numeric  = "delta_E_local = " + std::to_string(local_energy_after)
						  + " - " + std::to_string(local_energy_before)
						  + " = " + std::to_string(result_value) + " kcal/mol";
	}
};

// ============================================================================
// FormationEvent — bead/lattice formation outcome
// ============================================================================

/**
 * Records the outcome of a formation run (beta-7 FormationOutput layer).
 *
 * Links to pipeline: FormationEvent → FingerprintRecord via event_id.
 */
struct FormationEvent : KernelEvent {
	int         n_beads         = 0;
	int         fire_steps      = 0;
	bool        converged       = false;
	double      final_energy    = 0.0;   // kcal/mol
	double      packing_fraction = 0.0;  // η
	std::string lattice_class;           // "FCC", "BCC", "HCP", "amorphous", etc.
	std::string formation_preset;

	FormationEvent() { kind = KernelEventKind::Formation; result_unit = "kcal/mol"; }

	void compute() {
		result_value      = final_energy;
		equation_symbolic = "U_total = sum_ij(U_vdW + U_Coul + U_steric + U_orient)";
		equation_numeric  = "U_total = " + std::to_string(final_energy) + " kcal/mol"
						  + "  [n=" + std::to_string(n_beads)
						  + ", steps=" + std::to_string(fire_steps)
						  + ", eta=" + std::to_string(packing_fraction) + "]";
		is_valid = converged;
		if (!converged) warning = "FIRE did not converge in " + std::to_string(fire_steps) + " steps";
	}
};

// ============================================================================
// DefectEvent — structural defect
// ============================================================================

enum class DefectType : uint8_t {
	Unknown       = 0,
	Vacancy       = 1,
	Interstitial  = 2,
	Substitution  = 3,
	FrenkelPair   = 4,
	Antisite      = 5,
	StackingFault = 6,
	GrainBoundary = 7,
};

inline const char* defect_name(DefectType d) {
	switch (d) {
		case DefectType::Vacancy:       return "Vacancy";
		case DefectType::Interstitial:  return "Interstitial";
		case DefectType::Substitution:  return "Substitution";
		case DefectType::FrenkelPair:   return "FrenkelPair";
		case DefectType::Antisite:      return "Antisite";
		case DefectType::StackingFault: return "StackingFault";
		case DefectType::GrainBoundary: return "GrainBoundary";
		default:                        return "Unknown";
	}
}

struct DefectEvent : KernelEvent {
	DefectType  defect_type     = DefectType::Unknown;
	int         site_id         = -1;         // Lattice site index
	double      formation_energy = 0.0;        // kcal/mol
	double      migration_energy = 0.0;        // kcal/mol (0 = not computed)
	std::string host_element;                  // e.g. "Fe"
	std::string defect_element;                // e.g. "Cr" (for substitution)

	DefectEvent() { kind = KernelEventKind::Defect; result_unit = "kcal/mol"; }

	void compute() {
		result_value      = formation_energy;
		equation_symbolic = "E_f = E_defect - E_perfect + sum(mu_i)";
		equation_numeric  = "E_f = " + std::to_string(formation_energy) + " kcal/mol"
						  + "  [" + defect_name(defect_type) + " at site "
						  + std::to_string(site_id) + "]";
	}
};

// ============================================================================
// TransportEvent — diffusion / drift / permeation
// ============================================================================

struct TransportEvent : KernelEvent {
	int         particle_id     = -1;
	double      displacement_ang = 0.0;  // Å — total displacement
	double      msd             = 0.0;   // Å² — mean squared displacement
	double      diffusivity     = 0.0;   // Å²/step (proxy)
	std::string transport_mode;          // "diffusion", "drift", "permeation"

	TransportEvent() { kind = KernelEventKind::Transport; result_unit = "Å²/step"; }

	void compute() {
		result_value      = diffusivity;
		equation_symbolic = "D_proxy = MSD / (6 * dt * N_steps)";
		equation_numeric  = "D_proxy = " + std::to_string(msd)
						  + " / (6 * steps) = " + std::to_string(diffusivity) + " Å²/step";
	}
};

// ============================================================================
// ContinualReportEvent — rolling metric snapshot
// ============================================================================

/**
 * Periodic snapshot of system metrics for dashboard / report output.
 *
 * Emission doctrine (beta-7+):
 *   ContinualReportEvent is emitted from SCRIPT-DECLARED paths only.
 *   Encode the desired snapshot cadence in a .vsim script using a [while]
 *   loop with an [export] section.  VsimRuntime will call compute() and
 *   record the event into KernelEventLog at each declared interval.
 *
 *   There is no autonomous background engine that emits this event.
 *   Autonomous reporting was deprecated in the beta-7 philosophical overhaul.
 */
struct ContinualReportEvent : KernelEvent {
	double total_energy     = 0.0;   // kcal/mol
	double temperature_K    = 0.0;
	double packing_fraction = 0.0;   // η̄
	double mean_coord_num   = 0.0;   // average coordination number
	double rmsd_ang         = 0.0;   // Å — RMSD from reference
	int    n_active_beads   = 0;
	int    report_interval  = 1;     // steps between reports

	ContinualReportEvent() { kind = KernelEventKind::ContinualReport; result_unit = "kcal/mol"; }

	void compute() {
		result_value      = total_energy;
		equation_symbolic = "snapshot at frame_id";
		equation_numeric  = "U=" + std::to_string(total_energy)
						  + " T=" + std::to_string(temperature_K)
						  + " eta=" + std::to_string(packing_fraction)
						  + " coord=" + std::to_string(mean_coord_num)
						  + " RMSD=" + std::to_string(rmsd_ang);
	}
};

} // namespace vsepr::kernel
