// include/ufx_auto2/web_validator.hpp
// UFX_AUTO_2 Phase 5 -- Web Validator
// VSEPR-SIM v5 beta8 -> beta9 target
//
// WebValidator drives validate-web cycles:
//   load batch of generated/warn records
//   -> build identity queries (formula, MW, InChIKey, species existence)
//   -> fetch via IWebFetcher (cache-backed)
//   -> compare against record
//   -> write validation_records rows
//   -> apply confidence cap or score adjustment

#pragma once

#include "ufx_auto2/web_fetcher.hpp"
#include <string>

// Forward-declare sqlite3
struct sqlite3;

namespace vsepr::ufx {

// ============================================================================
// WebValidateOptions
// ============================================================================

struct WebValidateOptions {
	std::string db_path;
	std::string cache_dir  = "web_cache";
	int         batch      = 25;
	bool        verbose    = false;
	std::string run_dir;
};

struct WebValidateResult {
	std::string db_path;
	int         checked        = 0;
	int         found          = 0;
	int         missing        = 0;
	int         conflict       = 0;
	int         identity_fail  = 0;
	bool        success        = false;
	std::string error_message;
};

// ============================================================================
// WebValidator
// ============================================================================

class WebValidator {
public:
	WebValidator(IWebFetcher& fetcher, sqlite3* db)
		: fetcher_(fetcher), db_(db) {}

	WebValidateResult validate_batch(const WebValidateOptions& opts);

private:
	IWebFetcher& fetcher_;
	sqlite3*     db_;

	// Apply a confidence cap to all validation_records for this material.
	void apply_confidence_cap_(int64_t material_id, double cap);

	// Write one validation_records row.
	void insert_validation_(int64_t material_id,
							const std::string& validator_id,
							const std::string& type,
							bool passed, double score,
							const std::string& notes);
};

void print_web_validate_result(const WebValidateResult& r);

} // namespace vsepr::ufx
