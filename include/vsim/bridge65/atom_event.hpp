#pragma once
/**
 * atom_event.hpp
 * ==============
 * Per-atom event record for the WO-BRIDGE-65 classical MD / empirical bridge.
 *
 * Records energy changes, collision events, bond create/break/stretch,
 * force spikes, thermal spikes, and energy anomalies on a per-atom basis.
 *
 * Design rules:
 *   - Plain aggregate — no virtual dispatch.
 *   - All quantities in eV and Angstrom (eV/Å for forces).
 *   - state_hash must NOT include log counters (replay integrity).
 *   - event_hash includes the event stream (change detection).
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include <cstdint>
#include <string>
#include <vector>

namespace vsim::bridge65 {

// ============================================================================
// Event classification
// ============================================================================

enum class AtomEventType : uint8_t {
	EnergyUpdate   = 0,
	Collision      = 1,
	BondCreated    = 2,
	BondBroken     = 3,
	BondStretched  = 4,
	ForceSpike     = 5,
	ThermalSpike   = 6,
	EnergyAnomaly  = 7,
};

inline const char* atom_event_type_name(AtomEventType t) noexcept {
	switch (t) {
		case AtomEventType::EnergyUpdate:  return "energy_update";
		case AtomEventType::Collision:     return "collision";
		case AtomEventType::BondCreated:   return "bond_created";
		case AtomEventType::BondBroken:    return "bond_broken";
		case AtomEventType::BondStretched: return "bond_stretched";
		case AtomEventType::ForceSpike:    return "force_spike";
		case AtomEventType::ThermalSpike:  return "thermal_spike";
		case AtomEventType::EnergyAnomaly: return "energy_anomaly";
	}
	return "unknown";
}

// ============================================================================
// Energy snapshot for a single atom at one event point
// ============================================================================

struct AtomEventEnergy {
	double kinetic_eV        = 0.0;  // K_i — kinetic energy
	double potential_local_eV = 0.0; // U_i — local potential estimate
	double total_local_eV    = 0.0;  // E_i_total = K_i + U_i
	double delta_eV          = 0.0;  // dE_i since last sample
};

// ============================================================================
// Force snapshot for a single atom at one event point
// ============================================================================

struct AtomEventForce {
	double fx_eV_A        = 0.0;
	double fy_eV_A        = 0.0;
	double fz_eV_A        = 0.0;
	double magnitude_eV_A = 0.0; // |F_i|
};

// ============================================================================
// Full per-atom event record
// E_i(t) = [ id_i, t, K_i, U_i, E_total, dE, |F|, C, B, R ]
// ============================================================================

struct AtomEvent {
	// Simulation context
	std::uint64_t step   = 0;
	double        time_s = 0.0;

	// Atom identity
	int         atom_id = -1;
	std::string symbol;              // e.g. "Fe", "O", "H"

	// Event classification
	AtomEventType      type        = AtomEventType::EnergyUpdate;
	std::vector<int>   partner_ids;  // Collision/bond partners

	// Physics quantities
	AtomEventEnergy energy;
	AtomEventForce  force;

	// Geometry
	double partner_distance_A = 0.0; // Distance to nearest relevant partner (Å)
	double cutoff_A           = 0.0; // Applicable cutoff radius (Å)

	// Metadata
	double      confidence  = 0.0;  // Classifier confidence [0, 1]
	std::string reason;             // Human-readable event reason
	std::string sym_partner;        // Symbol of first partner (display)

	// Hashing (replay integrity)
	std::string state_hash; // Simulation state hash — must not include log counters
	std::string event_hash; // Event stream hash — changes when events change
};

} // namespace vsim::bridge65
