#pragma once
// WO-72W — material candidate record
// A single candidate material entry for the pretraining / property-discovery layer.
#include <string>
#include <vector>
#include <cstdint>

namespace vsim::ml {

struct MaterialCandidate {
	// Identity
	std::string formula;           // e.g. "Li2FeSiO4"
	std::string name;              // common name if known
	std::string crystal_system;    // cubic / hexagonal / monoclinic / ...

	// Target property being sought
	std::string target_property;   // e.g. "bandgap" | "conductivity" | "Tm"
	double      target_value = 0.0;
	std::string target_unit;       // eV / S/m / K / ...

	// Literature / source mapping
	std::string doi;               // DOI or URL
	std::string source_db;         // "ICSD" | "Materials Project" | "NIST" | "manual"

	// Formation / process route
	std::string formation_route;   // brief description
	std::string process_method;    // "solid-state" | "hydrothermal" | "CVD" | ...
	double      synthesis_temp_K = 0.0;

	// Recommendation metadata
	double confidence      = 0.0;  // 0–1: model confidence in this candidate
	std::string val_status;        // "validated" | "predicted" | "unverified"
	std::string notes;

	// Computed features (filled by PropertyTrendFinder)
	double electronegativity_mean = 0.0;
	double atomic_mass_mean       = 0.0;
	double valence_mean           = 0.0;
	uint32_t n_elements           = 0;
};

} // namespace vsim::ml
