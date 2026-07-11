#pragma once
/**
 * bridge65_config.hpp
 * ===================
 * Configuration structs for the WO-BRIDGE-65 bridge layer.
 *
 * Mirrors the [bridge65] section of a VSIM script block.
 *
 * Example VSIM block:
 *
 *   [bridge65]
 *   enabled = true
 *   version = "5.0.14"
 *   mode = "classical_md_empirical_bridge"
 *   subatomic_enabled = false
 *
 *   [bridge65.empirical]
 *   source = "data/elements.empirical.json"
 *   python_refresh_allowed = true
 *   freeze_after_load = true
 *
 *   [bridge65.matrix]
 *   enabled = true
 *   policy = "data/bridge65.force_matrix.json"
 *   compare_against_existing = true
 *
 *   [bridge65.atom_events.thresholds]
 *   collision_distance_scale  = 1.15
 *   bond_create_confidence_min = 0.990
 *   bond_break_distance_scale = 1.65
 *   force_spike_sigma         = 5.0
 *   energy_anomaly_eV         = 1.0
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include "atom_event_logger.hpp"
#include <string>

namespace vsim::bridge65 {

// ============================================================================
// Empirical data layer
// ============================================================================

struct Bridge65EmpiricalConfig {
	bool        enabled                = true;
	std::string source                 = "data/elements.empirical.json";
	bool        python_refresh_allowed = true;
	bool        freeze_after_load      = true;
};

// ============================================================================
// Matrix-force policy layer
// ============================================================================

struct Bridge65MatrixConfig {
	bool        enabled                 = true;
	std::string policy                  = "data/bridge65.force_matrix.json";
	bool        compare_against_existing = true;
	double      force_agreement_min     = 0.999;
	double      energy_agreement_min    = 0.999;
};

// ============================================================================
// Event detection thresholds
// ============================================================================

struct Bridge65EventThresholds {
	double collision_distance_scale   = 1.15;  // × sum of covalent radii
	double bond_create_confidence_min = 0.990;
	double bond_break_distance_scale  = 1.65;  // × equilibrium bond length
	double force_spike_sigma          = 5.0;   // Σ above rolling mean
	double energy_anomaly_eV          = 1.0;   // |dE| threshold in eV
};

// ============================================================================
// Output paths
// ============================================================================

struct Bridge65OutputPaths {
	std::string trajectory    = "state/run.xyzf";
	std::string checkpoint    = "state/run.xyzc";
	std::string atom_events   = "events/atom_events.jsonl";
	std::string event_summary = "events/event_summary.tsv";
	std::string bridge_report = "reports/bridge65_report.md";
};

// ============================================================================
// Top-level bridge configuration
// ============================================================================

struct Bridge65Config {
	bool        enabled           = true;
	std::string version           = "5.0.14";
	std::string mode              = "classical_md_empirical_bridge";

	// Subatomic sampling — explicitly DISABLED for this work order.
	bool subatomic_enabled = false;

	Bridge65EmpiricalConfig empirical;
	Bridge65MatrixConfig     matrix;
	Bridge65EventThresholds  thresholds;
	AtomEventLoggerConfig    logger;
	Bridge65OutputPaths      outputs;
};

} // namespace vsim::bridge65
