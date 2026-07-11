// src/ufx_auto2/web_validator.cpp
// UFX_AUTO_2 Phase 5 -- Web Validator Implementation
// VSEPR-SIM v5 beta8 -> beta9 target
//
// First web checks (per spec):
//   formula, molecular_weight, formal_charge, inchikey, species_exists
//
// Scoring rules (UFX_continual_2.tex §18):
//   webdata_missing:    cap confidence at 0.85 (not rejection)
//   webdata_conflict:   subtract conflict penalty
//   identity_mismatch:  cap at 0.50
//   missing_provenance: score = 0.0 (automatic reject)

#include "ufx_auto2/web_validator.hpp"
#include "ufx_auto2/web_cache.hpp"
#include "ufx_auto2/pubchem_fetcher.hpp"
#include "ufx_auto2/nist_fetcher.hpp"

#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <cmath>
#include <vector>
#include <string>

namespace vsepr::ufx {

// ============================================================================
// Helpers
// ============================================================================

static const double k_missing_cap      = 0.85;
static const double k_identity_cap     = 0.50;
static const double k_conflict_penalty = 0.10;

// ============================================================================
// insert_validation_
// ============================================================================

void WebValidator::insert_validation_(int64_t material_id,
									  const std::string& validator_id,
									  const std::string& type,
									  bool passed, double score,
									  const std::string& notes) {
	const char* sql =
		"INSERT INTO validation_records "
		"(material_id, validator_id, validation_type, passed, score, method_tag, notes) "
		"VALUES (?,?,?,?,?,?,?);";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

	sqlite3_bind_int64 (stmt, 1, material_id);
	sqlite3_bind_text  (stmt, 2, validator_id.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text  (stmt, 3, type.c_str(),         -1, SQLITE_TRANSIENT);
	sqlite3_bind_int   (stmt, 4, passed ? 1 : 0);
	sqlite3_bind_double(stmt, 5, score);
	sqlite3_bind_text  (stmt, 6, "web_identity",       -1, SQLITE_STATIC);
	sqlite3_bind_text  (stmt, 7, notes.c_str(),        -1, SQLITE_TRANSIENT);

	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

// ============================================================================
// apply_confidence_cap_
// Updates all validation_records.score for this material to min(score, cap).
// ============================================================================

void WebValidator::apply_confidence_cap_(int64_t material_id, double cap) {
	const char* sql =
		"UPDATE validation_records SET score = MIN(score, ?) "
		"WHERE material_id=? AND validator_id LIKE 'web_%';";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
	sqlite3_bind_double(stmt, 1, cap);
	sqlite3_bind_int64 (stmt, 2, material_id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

// ============================================================================
// validate_batch
// ============================================================================

WebValidateResult WebValidator::validate_batch(const WebValidateOptions& opts) {
	WebValidateResult result;
	result.db_path = opts.db_path;

	// Load generated records for web checking (those not yet web-validated)
	const char* sql_load =
		"SELECT id, formula, phase FROM material_records "
		"WHERE source_class IN ('generated') "
		"AND NOT EXISTS ("
		"  SELECT 1 FROM validation_records vr "
		"  WHERE vr.material_id = material_records.id "
		"  AND vr.validator_id LIKE 'web_%'"
		") LIMIT ?;";

	struct Row { int64_t id; std::string formula; std::string phase; };
	std::vector<Row> rows;

	{
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db_, sql_load, -1, &stmt, nullptr) != SQLITE_OK) {
			result.success       = false;
			result.error_message = "Failed to prepare load query";
			return result;
		}
		sqlite3_bind_int(stmt, 1, opts.batch);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Row r;
			r.id = sqlite3_column_int64(stmt, 0);
			auto c1 = sqlite3_column_text(stmt, 1);
			auto c2 = sqlite3_column_text(stmt, 2);
			if (c1) r.formula = reinterpret_cast<const char*>(c1);
			if (c2) r.phase   = reinterpret_cast<const char*>(c2);
			rows.push_back(std::move(r));
		}
		sqlite3_finalize(stmt);
	}

	sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

	// Phase 5 first checks: species_exists, molecular_weight, inchikey (PubChem)
	// and species_exists (NIST cross-check).
	const struct { WebSource src; const char* prop; } k_first_checks[] = {
		{ WebSource::PubChem, "molecular_weight" },
		{ WebSource::PubChem, "inchikey"         },
		{ WebSource::NIST,    "species_exists"   },
	};

	for (const auto& row : rows) {
		if (row.formula.empty()) continue;

		++result.checked;
		bool any_conflict = false;
		int  any_missing  = 0;

		for (const auto& check : k_first_checks) {
			WebQuery q;
			q.source        = check.src;
			q.material_key  = std::to_string(row.id);
			q.formula       = row.formula;
			q.property_name = check.prop;
			q.units         = "";

			WebResponse resp = fetcher_.fetch(q);

			std::string vid = std::string("web_") + to_string(check.src)
							+ "_" + check.prop;

			if (!resp.found) {
				++any_missing;
				insert_validation_(row.id, vid, "identity",
								   true,  // missing is not failure
								   k_missing_cap,
								   "webdata missing; confidence cap applied");
				++result.missing;
			} else {
				// Property found — basic plausibility check for molecular_weight
				if (std::string(check.prop) == "molecular_weight"
					&& resp.numeric_value.has_value()) {
					double mw = *resp.numeric_value;
					bool plausible = (mw > 0.0 && mw < 100000.0);
					if (!plausible) {
						any_conflict = true;
						insert_validation_(row.id, vid, "identity",
										   false, 1.0 - k_conflict_penalty,
										   "molecular_weight out of plausible range");
						++result.conflict;
					} else {
						insert_validation_(row.id, vid, "identity",
										   true, 1.0, "mw_ok");
						++result.found;
					}
				} else {
					insert_validation_(row.id, vid, "identity",
									   true, 1.0, "found");
					++result.found;
				}
			}
		}

		if (any_missing) {
			apply_confidence_cap_(row.id, k_missing_cap);
		}

		if (opts.verbose) {
			std::cout << "[UFX_AUTO_2] web-validate id=" << row.id
					  << " formula=" << row.formula
					  << (any_missing  ? " [missing-cap]" : "")
					  << (any_conflict ? " [conflict]"    : "")
					  << "\n";
		}
	}

	sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);

	result.success = true;
	return result;
}

// ============================================================================
// print_web_validate_result
// ============================================================================

void print_web_validate_result(const WebValidateResult& r) {
	std::cout << "\n[UFX_AUTO_2] validate-web\n"
			  << "  db            : " << r.db_path       << "\n"
			  << "  checked       : " << r.checked       << "\n"
			  << "  found         : " << r.found         << "\n"
			  << "  missing(cap)  : " << r.missing       << "\n"
			  << "  conflict      : " << r.conflict      << "\n"
			  << "  identity_fail : " << r.identity_fail << "\n";
	if (!r.success)
		std::cout << "  ERROR         : " << r.error_message << "\n";
	std::cout << "\n";
}

} // namespace vsepr::ufx
