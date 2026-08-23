#pragma once
/**
 * vsim_output_filter.hpp  —  WO-73E
 * ====================================
 * Converts raw VSIM simulation output records into EmpiricalDB-compatible
 * normalised rows for comparison against the external reference track.
 *
 *   raw VSIM output
 *     -> normalised interaction rows
 *       -> EmpiricalDB-compatible records
 *
 * Output row example (canonical field names per WO-73A):
 *   source_type:    simulation
 *   object_id:      pi_7
 *   family:         pi_N
 *   state:          gas
 *   casting:        field_field
 *   Lambda_nm:      1.18
 *   E_value:        -12.4
 *   UFF:            -10.1
 *   CFF:            0.71
 *   primary_scale:  S3
 *   gap_label:      pending
 *   run_id:         SIM_RUN_0073
 */

#include <string>
#include <vector>
#include <unordered_map>

namespace vsepr {
namespace multiscale {

// ============================================================================
// Raw VSIM output record (as produced by the simulation pipeline)
// ============================================================================

struct VsimRawRecord {
	std::string run_id;
	std::string object_id;

	// Raw field names may use non-canonical aliases — filter normalises them
	std::unordered_map<std::string, double> fields;  // key=field_name, value
	std::unordered_map<std::string, std::string> tags; // key=tag_name, value
};

// ============================================================================
// Normalised EmpiricalDB-compatible record (canonical names per WO-73A)
// ============================================================================

struct NormalisedRecord {
	std::string source_type    = "simulation";
	std::string object_id;
	std::string family;
	std::string state;
	std::string casting;
	double      Lambda_nm      = 0.0;
	double      E_value        = 0.0;
	double      UFF            = 0.0;
	double      CFF            = 0.0;
	std::string primary_scale;
	std::string gap_label      = "pending";
	std::string run_id;

	bool valid = false;
};

// ============================================================================
// VsimOutputFilter
// ============================================================================

class VsimOutputFilter {
public:
	VsimOutputFilter() = default;

	/**
	 * Normalise a single raw VSIM record into an EmpiricalDB-compatible row.
	 *
	 * Resolves banned aliases to canonical names (WO-73A naming_convention_map).
	 * Sets source_type = "simulation" and gap_label = "pending".
	 */
	NormalisedRecord normalise(const VsimRawRecord& raw) const;

	/**
	 * Batch normalise a set of raw records.
	 * Skips records missing required fields (object_id, run_id, Lambda).
	 */
	std::vector<NormalisedRecord> normalise_batch(
		const std::vector<VsimRawRecord>& records) const;

	/**
	 * Export normalised records as CSV string.
	 * Column order matches WO-73E example output.
	 */
	static std::string to_csv(const std::vector<NormalisedRecord>& records);

private:
	// Resolve a field value from raw record, trying canonical name then aliases
	static double resolve_double(const VsimRawRecord& raw,
								 const std::string& canonical,
								 const std::vector<std::string>& aliases,
								 double default_val = 0.0);

	static std::string resolve_string(const VsimRawRecord& raw,
									  const std::string& canonical,
									  const std::vector<std::string>& aliases,
									  const std::string& default_val = "");
};

} // namespace multiscale
} // namespace vsepr
