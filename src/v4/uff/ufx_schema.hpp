// src/v4/uff/ufx_schema.hpp
// UFX_AUTO_2 -- SQLite Schema Definitions and Initialisation
// Formation Engine v4.1.0 / VSEPR-SIM v5 beta8 -> beta9 target
//
// Tables (from UFX_continual_2.tex §16):
//   material_records      -- identity, class, timestamps
//   property_values       -- all property data (generic; dedicated tables earn their place later)
//   validation_records    -- comparisons and scores
//   generation_axes       -- randomised creation parameters per run
//   promotion_history     -- source class transitions
//   property_provenance   -- source trail per property
//
// Hard rule: generic property_values table first.
// Dedicated tables (thermo, crystal, etc.) added only after usage stabilises.
//
// Usage:
//   #include "ufx_schema.hpp"
//   vsepr::ufx::ufx_auto2_init_db("path/to/ufx_auto2.sqlite");

#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward-declare sqlite3 to avoid pulling in the amalgamation header
// in every translation unit that includes ufx_schema.hpp.
struct sqlite3;

#include "v4/uff/ufx_material_record.hpp"

namespace vsepr::ufx {

// ---------------------------------------------------------------------------
// DDL strings — one per table
// ---------------------------------------------------------------------------

// Core identity + classification table.
// One row per unique material record.
extern const char* k_ddl_material_records;

// Generic property storage.
// Every property value, regardless of block tier, lands here first.
// State columns (temperature_K, pressure_Pa, phase) are REQUIRED per hard rule.
extern const char* k_ddl_property_values;

// Validation results — one row per (material_id, validator_id) pair.
extern const char* k_ddl_validation_records;

// Generation axes — records the axis config used to create each record.
extern const char* k_ddl_generation_axes;

// Promotion history — audit trail of SourceClass transitions.
extern const char* k_ddl_promotion_history;

// Per-property provenance — source trail attached to each property_values row.
extern const char* k_ddl_property_provenance;

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

struct InitResult {
	bool        success   = false;
	std::string db_path;
	std::string error_message;
	int         tables_created = 0;
};

// Create (or verify) the UFX_AUTO_2 SQLite database at db_path.
// Creates all 6 tables if they do not already exist (IF NOT EXISTS).
// Returns an InitResult indicating success and the number of tables created.
InitResult ufx_auto2_init_db(const std::string& db_path);

// ---------------------------------------------------------------------------
// Audit query helpers (used by ufx auto2 audit)
// ---------------------------------------------------------------------------

struct AuditReport {
	std::string db_path;
	int64_t total_records       = 0;

	// Record counts by source_class
	int64_t reference_count     = 0;
	int64_t generated_count     = 0;
	int64_t validated_count     = 0;
	int64_t rejected_count      = 0;
	int64_t derived_count       = 0;
	int64_t imported_count      = 0;
	int64_t experimental_count  = 0;
	int64_t simulated_count     = 0;

	// Property and provenance counts
	int64_t total_properties        = 0;
	int64_t missing_provenance      = 0;
	int64_t missing_validation      = 0;
	int64_t conflict_count          = 0;

	// Promotion history counts
	int64_t total_promotions        = 0;
	int64_t total_rejections        = 0;

	// Phase 3: validation_status counts (from validation_records)
	int64_t validation_pass_count   = 0;
	int64_t validation_fail_count   = 0;

	// Phase 3: score statistics (from validation_records.score)
	double  score_avg               = 0.0;
	double  score_min               = 0.0;
	double  score_max               = 0.0;

	// Phase 3: top rejected reasons (most frequent notes in failed validation_records)
	std::vector<std::string> top_rejected_reasons;

	// Coverage intelligence
	std::string weakest_axis_region;
	std::string highest_conflict_source;

	bool success = false;
	std::string error_message;
};

// Run audit queries against the database and return an AuditReport.
AuditReport ufx_auto2_audit_db(const std::string& db_path);

// Print the AuditReport to stdout in the UFX house style.
void ufx_auto2_print_audit(const AuditReport& report);

// ---------------------------------------------------------------------------
// Insert helpers (used by auto2_randomfill)
// ---------------------------------------------------------------------------

// Insert one material record. Returns the new rowid, or -1 on error.
// On UNIQUE conflict (duplicate material_key) returns -2 without inserting.
int64_t ufx_insert_material_record(sqlite3* db,
                                   const UFXMaterialRecord& record);

// Insert one generation_axes row tied to a material record.
// region_key is the CoverageMap::make_key() string for this sample.
bool ufx_insert_generation_axes(sqlite3* db,
                                int64_t   material_id,
                                const std::string& run_id,
                                const std::string& axis_config_json,
                                uint64_t  seed);

// Open a sqlite3* handle for read-write use (WAL mode, FK on).
// Caller must sqlite3_close() when done.
sqlite3* ufx_open_db_rw(const std::string& db_path, std::string& err_out);

// ---------------------------------------------------------------------------
// Provenance helper — shared by all Phase 6-10 fill generators
// ---------------------------------------------------------------------------

// Insert a property_provenance row for the given property_values rowid.
// source_id   — e.g. "periodic_table_arithmetic", "shomate_heuristic_phase7"
// method_tag  — short human label for the algorithm
// confidence  — same confidence value written to the property_values row
// Returns true if the row was inserted (or already existed via INSERT OR IGNORE).
bool ufx_insert_provenance(sqlite3*           db,
                           int64_t            property_values_id,
                           const std::string& source_id,
                           const std::string& method_tag,
                           double             confidence);

// ---------------------------------------------------------------------------
// Validate helpers (Phase 3 / 4)
// ---------------------------------------------------------------------------

struct ValidateOptions {
	std::string db_path;
	int         batch     = 100;  // number of generated records to validate per call
	bool        verbose   = false;
	std::string run_dir;          // optional: path for per-cycle log output
};

struct ValidateResult {
	std::string db_path;
	int         checked   = 0;
	int         passed    = 0;
	int         failed    = 0;
	bool        success   = false;
	std::string error_message;
};

// Re-run LocalSanityChecker over unvalidated generated records.
// Writes rows to validation_records and updates source_class on hard failures.
ValidateResult ufx_auto2_validate(const ValidateOptions& opts);

void print_validate_result(const ValidateResult& r);

// ---------------------------------------------------------------------------
// Promote helpers (Phase 3 / 4)
// ---------------------------------------------------------------------------

struct PromoteOptions {
	std::string db_path;
	double      min_score = 0.92;  // composite_score threshold for VALIDATED_HIGH
	double      warn_score = 0.85; // threshold for VALIDATED_WARN (kept as generated_low)
	bool        verbose   = false;
	std::string run_dir;
};

struct PromoteResult {
	std::string db_path;
	int         promoted  = 0;
	int         warned    = 0;
	int         rejected  = 0;
	bool        success   = false;
	std::string error_message;
};

// Evaluate composite scores for validated records and transition source_class.
// Writes rows to promotion_history.
PromoteResult ufx_auto2_promote(const PromoteOptions& opts);

void print_promote_result(const PromoteResult& r);


} // namespace vsepr::ufx
