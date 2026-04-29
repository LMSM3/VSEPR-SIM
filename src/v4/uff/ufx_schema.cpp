// src/v4/uff/ufx_schema.cpp
// UFX_AUTO_2 -- SQLite Schema Implementation
// Formation Engine v4.1.0 / VSEPR-SIM v5 beta8 -> beta9 target

#include "ufx_schema.hpp"

#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <string>

// Local sanity checker was previously used by the validate pass.
// Validation rows are now written by auto2_randomfill at generation time.
// This include is kept for any future direct use; it causes no harm.
#include "ufx_auto2/local_sanity.hpp"

namespace vsepr::ufx {

// ============================================================================
// DDL strings
// ============================================================================

const char* k_ddl_material_records = R"SQL(
CREATE TABLE IF NOT EXISTS material_records (
	id              INTEGER PRIMARY KEY AUTOINCREMENT,
	material_key    TEXT    NOT NULL UNIQUE,
	formula         TEXT    NOT NULL,
	phase           TEXT    NOT NULL DEFAULT '',
	elements        TEXT    NOT NULL DEFAULT '',   -- comma-separated
	atomic_numbers  TEXT    NOT NULL DEFAULT '',   -- comma-separated
	source_class    TEXT    NOT NULL DEFAULT 'generated',
	created_at      TEXT    NOT NULL DEFAULT (datetime('now','utc')),
	updated_at      TEXT    NOT NULL DEFAULT (datetime('now','utc'))
);
CREATE INDEX IF NOT EXISTS idx_mr_source_class ON material_records(source_class);
CREATE INDEX IF NOT EXISTS idx_mr_formula       ON material_records(formula);
)SQL";

// Every property value, regardless of tier, lands here first.
// State columns temperature_K, pressure_Pa, and phase are REQUIRED
// per hard rule: "A property value without attached T, P, and phase
// is a decorative lie."
const char* k_ddl_property_values = R"SQL(
CREATE TABLE IF NOT EXISTS property_values (
	id              INTEGER PRIMARY KEY AUTOINCREMENT,
	material_id     INTEGER NOT NULL REFERENCES material_records(id),
	block_tier      INTEGER NOT NULL DEFAULT 0,   -- 0-9
	block_name      TEXT    NOT NULL,             -- e.g. 'force_field', 'thermo', 'crystal'
	property_name   TEXT    NOT NULL,             -- e.g. 'r1', 'melting_point_K', 'band_gap_eV'
	value_real      REAL,
	value_text      TEXT,
	units           TEXT    NOT NULL DEFAULT '',
	temperature_K   REAL    NOT NULL DEFAULT 298.15,
	pressure_Pa     REAL    NOT NULL DEFAULT 101325.0,
	phase           TEXT    NOT NULL DEFAULT '',
	source_class    TEXT    NOT NULL DEFAULT 'generated',
	confidence      REAL    NOT NULL DEFAULT 0.0,
	uncertainty     REAL    NOT NULL DEFAULT 0.0,
	source_id       TEXT    NOT NULL DEFAULT '',
	created_at      TEXT    NOT NULL DEFAULT (datetime('now','utc'))
);
CREATE INDEX IF NOT EXISTS idx_pv_material_id    ON property_values(material_id);
CREATE INDEX IF NOT EXISTS idx_pv_block_name     ON property_values(block_name);
CREATE INDEX IF NOT EXISTS idx_pv_property_name  ON property_values(property_name);
CREATE INDEX IF NOT EXISTS idx_pv_source_class   ON property_values(source_class);
)SQL";

const char* k_ddl_validation_records = R"SQL(
CREATE TABLE IF NOT EXISTS validation_records (
	id              INTEGER PRIMARY KEY AUTOINCREMENT,
	material_id     INTEGER NOT NULL REFERENCES material_records(id),
	validator_id    TEXT    NOT NULL,   -- 'local_identity', 'local_ff', 'nist_webbook', 'pubchem', etc.
	validation_type TEXT    NOT NULL,   -- 'identity', 'molecular', 'thermo', 'spectral', 'simulation'
	passed          INTEGER NOT NULL DEFAULT 0,  -- 0 = fail, 1 = pass
	score           REAL    NOT NULL DEFAULT 0.0,
	method_tag      TEXT    NOT NULL DEFAULT '',
	notes           TEXT    NOT NULL DEFAULT '',
	validated_at    TEXT    NOT NULL DEFAULT (datetime('now','utc'))
);
CREATE INDEX IF NOT EXISTS idx_vr_material_id  ON validation_records(material_id);
CREATE INDEX IF NOT EXISTS idx_vr_validator_id ON validation_records(validator_id);
CREATE INDEX IF NOT EXISTS idx_vr_passed       ON validation_records(passed);
)SQL";

const char* k_ddl_generation_axes = R"SQL(
CREATE TABLE IF NOT EXISTS generation_axes (
	id              INTEGER PRIMARY KEY AUTOINCREMENT,
	material_id     INTEGER NOT NULL REFERENCES material_records(id),
	run_id          TEXT    NOT NULL DEFAULT '',
	axis_config     TEXT    NOT NULL DEFAULT '{}',  -- JSON blob of axis parameters
	seed            INTEGER NOT NULL DEFAULT 0,
	generated_at    TEXT    NOT NULL DEFAULT (datetime('now','utc'))
);
CREATE INDEX IF NOT EXISTS idx_ga_material_id ON generation_axes(material_id);
CREATE INDEX IF NOT EXISTS idx_ga_run_id      ON generation_axes(run_id);
)SQL";

const char* k_ddl_promotion_history = R"SQL(
CREATE TABLE IF NOT EXISTS promotion_history (
	id              INTEGER PRIMARY KEY AUTOINCREMENT,
	material_id     INTEGER NOT NULL REFERENCES material_records(id),
	from_class      TEXT    NOT NULL,
	to_class        TEXT    NOT NULL,
	composite_score REAL    NOT NULL DEFAULT 0.0,
	reason          TEXT    NOT NULL DEFAULT '',
	promoted_at     TEXT    NOT NULL DEFAULT (datetime('now','utc'))
);
CREATE INDEX IF NOT EXISTS idx_ph_material_id ON promotion_history(material_id);
CREATE INDEX IF NOT EXISTS idx_ph_to_class    ON promotion_history(to_class);
)SQL";

const char* k_ddl_property_provenance = R"SQL(
CREATE TABLE IF NOT EXISTS property_provenance (
	id              INTEGER PRIMARY KEY AUTOINCREMENT,
	property_id     INTEGER NOT NULL REFERENCES property_values(id),
	source_id       TEXT    NOT NULL,
	source_class    TEXT    NOT NULL,
	method_tag      TEXT    NOT NULL DEFAULT '',
	raw_hash        TEXT    NOT NULL DEFAULT '',   -- SHA-256 of raw API/file response
	confidence      REAL    NOT NULL DEFAULT 0.0,
	uncertainty     REAL    NOT NULL DEFAULT 0.0,
	unit_trace      TEXT    NOT NULL DEFAULT '',   -- unit conversion trace
	retrieved_at    TEXT    NOT NULL DEFAULT (datetime('now','utc')),
	notes           TEXT    NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_pp_property_id ON property_provenance(property_id);
CREATE INDEX IF NOT EXISTS idx_pp_source_id   ON property_provenance(source_id);
)SQL";

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// Execute a DDL string that may contain multiple statements separated by
// semicolons (sqlite3_exec handles this natively).
// Returns the number of statements executed (approximate: counts semicolons).
static bool exec_ddl(sqlite3* db, const char* ddl, std::string& err_out) {
	char* errmsg = nullptr;
	int rc = sqlite3_exec(db, ddl, nullptr, nullptr, &errmsg);
	if (rc != SQLITE_OK) {
		if (errmsg) {
			err_out = errmsg;
			sqlite3_free(errmsg);
		} else {
			err_out = "sqlite3_exec returned " + std::to_string(rc);
		}
		return false;
	}
	return true;
}

static int64_t query_int64(sqlite3* db, const char* sql) {
	sqlite3_stmt* stmt = nullptr;
	int64_t result = 0;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			result = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	return result;
}

static std::string query_text(sqlite3* db, const char* sql) {
	sqlite3_stmt* stmt = nullptr;
	std::string result;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			const unsigned char* txt = sqlite3_column_text(stmt, 0);
			if (txt) result = reinterpret_cast<const char*>(txt);
		}
		sqlite3_finalize(stmt);
	}
	return result;
}

} // anonymous namespace

// ============================================================================
// ufx_auto2_init_db
// ============================================================================

InitResult ufx_auto2_init_db(const std::string& db_path) {
	InitResult result;
	result.db_path = db_path;

	sqlite3* db = nullptr;
	int rc = sqlite3_open(db_path.c_str(), &db);
	if (rc != SQLITE_OK) {
		result.success       = false;
		result.error_message = sqlite3_errmsg(db);
		sqlite3_close(db);
		return result;
	}

	// Enable WAL mode for better concurrent write performance.
	sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
	sqlite3_exec(db, "PRAGMA foreign_keys=ON;",  nullptr, nullptr, nullptr);

	const char* ddls[] = {
		k_ddl_material_records,
		k_ddl_property_values,
		k_ddl_validation_records,
		k_ddl_generation_axes,
		k_ddl_promotion_history,
		k_ddl_property_provenance,
	};
	constexpr int k_table_count = 6;

	for (int i = 0; i < k_table_count; ++i) {
		std::string err;
		if (!exec_ddl(db, ddls[i], err)) {
			result.success       = false;
			result.error_message = "Table " + std::to_string(i) + ": " + err;
			sqlite3_close(db);
			return result;
		}
		result.tables_created++;
	}

	// Phase 10: v_meta_scores view (CREATE IF NOT EXISTS -- safe to re-run)
	const char* k_view_meta_scores = R"SQL(
CREATE VIEW IF NOT EXISTS v_meta_scores AS
SELECT
	mr.id,
	mr.material_key,
	mr.source_class,
	MAX(CASE WHEN pv.property_name = 'composite_score'
			 THEN pv.value_real END)  AS composite_score,
	MAX(CASE WHEN pv.property_name = 'weirdness'
			 THEN pv.value_real END)  AS weirdness,
	MAX(CASE WHEN pv.property_name = 'macro_usefulness'
			 THEN pv.value_real END)  AS macro_usefulness,
	MAX(CASE WHEN pv.property_name = 'extrapolation_risk'
			 THEN pv.value_real END)  AS extrapolation_risk
FROM material_records mr
JOIN property_values pv ON pv.material_id = mr.id
WHERE pv.block_name = 'meta'
GROUP BY mr.id;
)SQL";
	{
		std::string err;
		exec_ddl(db, k_view_meta_scores, err);  // non-fatal if view already exists
	}

	sqlite3_close(db);
	result.success = true;
	return result;
}

// ============================================================================
// ufx_auto2_audit_db
// ============================================================================

AuditReport ufx_auto2_audit_db(const std::string& db_path) {
	AuditReport report;
	report.db_path = db_path;

	sqlite3* db = nullptr;
	int rc = sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
	if (rc != SQLITE_OK) {
		report.success       = false;
		report.error_message = sqlite3_errmsg(db);
		sqlite3_close(db);
		return report;
	}

	// Record counts by source_class
	report.total_records      = query_int64(db, "SELECT COUNT(*) FROM material_records;");
	report.reference_count    = query_int64(db,
		"SELECT COUNT(*) FROM material_records WHERE source_class='reference';");
	report.generated_count    = query_int64(db,
		"SELECT COUNT(*) FROM material_records WHERE source_class='generated';");
	report.validated_count    = query_int64(db,
		"SELECT COUNT(*) FROM material_records WHERE source_class='validated';");
	report.rejected_count     = query_int64(db,
		"SELECT COUNT(*) FROM material_records WHERE source_class='rejected';");
	report.derived_count      = query_int64(db,
		"SELECT COUNT(*) FROM material_records WHERE source_class='derived';");
	report.imported_count     = query_int64(db,
		"SELECT COUNT(*) FROM material_records WHERE source_class='imported';");
	report.experimental_count = query_int64(db,
		"SELECT COUNT(*) FROM material_records WHERE source_class='experimental';");
	report.simulated_count    = query_int64(db,
		"SELECT COUNT(*) FROM material_records WHERE source_class='simulated';");

	// Property counts
	report.total_properties = query_int64(db, "SELECT COUNT(*) FROM property_values;");

	// Missing provenance: property_values rows with no matching property_provenance
	report.missing_provenance = query_int64(db,
		"SELECT COUNT(*) FROM property_values pv "
		"WHERE NOT EXISTS (SELECT 1 FROM property_provenance pp WHERE pp.property_id = pv.id);");

	// Missing validation: generated records with no validation_records entry
	report.missing_validation = query_int64(db,
		"SELECT COUNT(*) FROM material_records mr "
		"WHERE mr.source_class = 'generated' "
		"AND NOT EXISTS (SELECT 1 FROM validation_records vr WHERE vr.material_id = mr.id);");

	// Conflict count: validation_records where passed=0
	report.conflict_count = query_int64(db,
		"SELECT COUNT(*) FROM validation_records WHERE passed=0;");

	// Promotion history counts
	report.total_promotions = query_int64(db,
		"SELECT COUNT(*) FROM promotion_history WHERE to_class NOT IN ('rejected');");
	report.total_rejections = query_int64(db,
		"SELECT COUNT(*) FROM promotion_history WHERE to_class='rejected';");

	// Phase 3: validation_status counts
	report.validation_pass_count = query_int64(db,
		"SELECT COUNT(*) FROM validation_records WHERE passed=1;");
	report.validation_fail_count = query_int64(db,
		"SELECT COUNT(*) FROM validation_records WHERE passed=0;");

	// Phase 3: score statistics over all validation_records
	{
		const char* sql_stats =
			"SELECT AVG(score), MIN(score), MAX(score) FROM validation_records;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql_stats, -1, &stmt, nullptr) == SQLITE_OK) {
			if (sqlite3_step(stmt) == SQLITE_ROW) {
				if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
					report.score_avg = sqlite3_column_double(stmt, 0);
				if (sqlite3_column_type(stmt, 1) != SQLITE_NULL)
					report.score_min = sqlite3_column_double(stmt, 1);
				if (sqlite3_column_type(stmt, 2) != SQLITE_NULL)
					report.score_max = sqlite3_column_double(stmt, 2);
			}
			sqlite3_finalize(stmt);
		}
	}

	// Phase 3: top 5 rejected reasons (most frequent notes on failed validation_records)
	{
		const char* sql_reasons =
			"SELECT notes, COUNT(*) AS cnt FROM validation_records "
			"WHERE passed=0 AND notes != '' "
			"GROUP BY notes ORDER BY cnt DESC LIMIT 5;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql_reasons, -1, &stmt, nullptr) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				const unsigned char* note = sqlite3_column_text(stmt, 0);
				int64_t cnt               = sqlite3_column_int64(stmt, 1);
				if (note) {
					report.top_rejected_reasons.push_back(
						std::string(reinterpret_cast<const char*>(note))
						+ " (" + std::to_string(cnt) + "x)");
				}
			}
			sqlite3_finalize(stmt);
		}
	}

	// Weakest axis region: generation_axes axis_config value that appears fewest times
	// (simplified: just show the run_id with fewest records)
	report.weakest_axis_region = query_text(db,
		"SELECT run_id FROM generation_axes "
		"GROUP BY run_id ORDER BY COUNT(*) ASC LIMIT 1;");
	if (report.weakest_axis_region.empty()) report.weakest_axis_region = "(no generation runs)";

	// Highest conflict source: validator_id with most failures
	report.highest_conflict_source = query_text(db,
		"SELECT validator_id FROM validation_records "
		"WHERE passed=0 GROUP BY validator_id ORDER BY COUNT(*) DESC LIMIT 1;");
	if (report.highest_conflict_source.empty()) report.highest_conflict_source = "(none)";

	sqlite3_close(db);
	report.success = true;
	return report;
}

// ============================================================================
// ufx_auto2_print_audit
// ============================================================================

void ufx_auto2_print_audit(const AuditReport& report) {
	std::cout << "\n";
	std::cout << "╔══════════════════════════════════════════════════════════╗\n";
	std::cout << "║            UFX AUTO2 — DATABASE AUDIT REPORT             ║\n";
	std::cout << "╚══════════════════════════════════════════════════════════╝\n";
	std::cout << "  DB path : " << report.db_path << "\n";

	if (!report.success) {
		std::cout << "  ERROR   : " << report.error_message << "\n\n";
		return;
	}

	std::cout << "\n  ── Record Counts ──────────────────────────────────────\n";
	std::cout << "  Total records         : " << report.total_records << "\n";
	std::cout << "    reference           : " << report.reference_count    << "\n";
	std::cout << "    generated           : " << report.generated_count    << "\n";
	std::cout << "    validated           : " << report.validated_count    << "\n";
	std::cout << "    rejected            : " << report.rejected_count     << "\n";
	std::cout << "    derived             : " << report.derived_count      << "\n";
	std::cout << "    imported            : " << report.imported_count     << "\n";
	std::cout << "    experimental        : " << report.experimental_count << "\n";
	std::cout << "    simulated           : " << report.simulated_count    << "\n";

	std::cout << "\n  ── Property Counts ────────────────────────────────────\n";
	std::cout << "  Total property values : " << report.total_properties    << "\n";
	std::cout << "  Missing provenance    : " << report.missing_provenance  << "\n";
	std::cout << "  Missing validation    : " << report.missing_validation  << "\n";
	std::cout << "  Validation conflicts  : " << report.conflict_count      << "\n";

	std::cout << "\n  ── Validation Status ──────────────────────────────────\n";
	std::cout << "  Validation pass       : " << report.validation_pass_count << "\n";
	std::cout << "  Validation fail       : " << report.validation_fail_count << "\n";

	std::cout << "\n  ── Score Statistics ───────────────────────────────────\n";
	std::cout << std::fixed << std::setprecision(4);
	std::cout << "  Score avg             : " << report.score_avg << "\n";
	std::cout << "  Score min             : " << report.score_min << "\n";
	std::cout << "  Score max             : " << report.score_max << "\n";

	std::cout << "\n  ── Top Rejected Reasons ───────────────────────────────\n";
	if (report.top_rejected_reasons.empty()) {
		std::cout << "  (none)\n";
	} else {
		for (const auto& r : report.top_rejected_reasons) {
			std::cout << "  " << r << "\n";
		}
	}

	std::cout << "\n  ── Promotion History ──────────────────────────────────\n";
	std::cout << "  Total promotions      : " << report.total_promotions << "\n";
	std::cout << "  Total rejections      : " << report.total_rejections << "\n";

	std::cout << "\n  ── Coverage Intelligence ──────────────────────────────\n";
	std::cout << "  Weakest axis region   : " << report.weakest_axis_region      << "\n";
	std::cout << "  Highest conflict src  : " << report.highest_conflict_source  << "\n";

	// Missing provenance warning (hard rule enforcement)
	if (report.missing_provenance > 0) {
		std::cout << "\n  [WARN] " << report.missing_provenance
				  << " property value(s) have no provenance record.\n"
				  << "         These cannot be promoted. (Hard rule: no provenance = no promotion.)\n";
	}

	std::cout << "\n";
}

// ============================================================================
// ufx_open_db_rw
// ============================================================================

sqlite3* ufx_open_db_rw(const std::string& db_path, std::string& err_out) {
	sqlite3* db = nullptr;
	int rc = sqlite3_open(db_path.c_str(), &db);
	if (rc != SQLITE_OK) {
		err_out = sqlite3_errmsg(db);
		sqlite3_close(db);
		return nullptr;
	}
	sqlite3_exec(db, "PRAGMA journal_mode=WAL;",  nullptr, nullptr, nullptr);
	sqlite3_exec(db, "PRAGMA foreign_keys=ON;",   nullptr, nullptr, nullptr);
	sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
	return db;
}

// ============================================================================
// ufx_insert_provenance
// ============================================================================

bool ufx_insert_provenance(sqlite3*           db,
						   int64_t            property_values_id,
						   const std::string& source_id,
						   const std::string& method_tag,
						   double             confidence)
{
	if (!db || property_values_id <= 0) return false;

	const char* sql =
		"INSERT OR IGNORE INTO property_provenance "
		"(property_id, source_id, source_class, method_tag, confidence) "
		"VALUES (?, ?, 'generated', ?, ?);";

	sqlite3_stmt* st = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK)
		return false;

	sqlite3_bind_int64 (st, 1, property_values_id);
	sqlite3_bind_text  (st, 2, source_id.c_str(),  -1, SQLITE_STATIC);
	sqlite3_bind_text  (st, 3, method_tag.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_double(st, 4, confidence);

	int rc = sqlite3_step(st);
	sqlite3_finalize(st);
	return (rc == SQLITE_DONE || rc == SQLITE_ROW);
}

// ============================================================================
// ufx_insert_material_record
// ============================================================================

int64_t ufx_insert_material_record(sqlite3* db, const UFXMaterialRecord& rec) {
	if (!db) return -1;

	// Build comma-separated element and atomic-number strings
	std::string elements_str;
	std::string atomic_numbers_str;
	for (std::size_t i = 0; i < rec.identity.elements.size(); ++i) {
		if (i > 0) { elements_str += ','; atomic_numbers_str += ','; }
		elements_str += rec.identity.elements[i];
		if (i < rec.identity.atomic_numbers.size())
			atomic_numbers_str += std::to_string(rec.identity.atomic_numbers[i]);
	}

	const char* sql =
		"INSERT OR IGNORE INTO material_records "
		"(material_key, formula, phase, elements, atomic_numbers, source_class, "
		" created_at, updated_at) "
		"VALUES (?,?,?,?,?,?,?,?);";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;

	const std::string sc = to_string(rec.source_class);

	sqlite3_bind_text(stmt, 1, rec.identity.material_key.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, rec.identity.formula.c_str(),       -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, rec.identity.phase.c_str(),         -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, elements_str.c_str(),               -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, atomic_numbers_str.c_str(),         -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, sc.c_str(),                         -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 7, rec.created_at.c_str(),             -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 8, rec.updated_at.c_str(),             -1, SQLITE_TRANSIENT);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_DONE) {
		int64_t rowid = sqlite3_last_insert_rowid(db);
		// If INSERT OR IGNORE did nothing (duplicate), rowid == 0
		return (rowid > 0) ? rowid : -2;
	}
	return -1;
}

// ============================================================================
// ufx_insert_generation_axes
// ============================================================================

bool ufx_insert_generation_axes(sqlite3* db,
								int64_t material_id,
								const std::string& run_id,
								const std::string& axis_config_json,
								uint64_t seed)
{
	if (!db || material_id <= 0) return false;

	const char* sql =
		"INSERT INTO generation_axes "
		"(material_id, run_id, axis_config, seed) "
		"VALUES (?,?,?,?);";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

	sqlite3_bind_int64(stmt, 1, material_id);
	sqlite3_bind_text (stmt, 2, run_id.c_str(),           -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 3, axis_config_json.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(seed));

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_DONE);
}

// ============================================================================
// ufx_auto2_validate
// Counts existing local_sanity rows for generated/rejected records, and for
// any records still lacking a local_sanity row (legacy / pre-fix DBs), infers
// pass/fail from source_class and back-fills a validation_records row.
// No in-memory LocalSanityChecker reconstruction — the check runs at
// generation time (auto2_randomfill) where the full record is available.
// ============================================================================

ValidateResult ufx_auto2_validate(const ValidateOptions& opts) {
	ValidateResult result;
	result.db_path = opts.db_path;

	std::string err;
	sqlite3* db = ufx_open_db_rw(opts.db_path, err);
	if (!db) {
		result.success       = false;
		result.error_message = "Cannot open DB: " + err;
		return result;
	}

	// ------------------------------------------------------------------
	// 1. Count already-written local_sanity rows (written by randomfill).
	// ------------------------------------------------------------------
	{
		const char* sql_count =
			"SELECT passed, COUNT(*) "
			"FROM validation_records "
			"WHERE validator_id = 'local_sanity' "
			"GROUP BY passed;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql_count, -1, &stmt, nullptr) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				int    p   = sqlite3_column_int(stmt, 0);
				int    cnt = sqlite3_column_int(stmt, 1);
				if (p) result.passed  += cnt;
				else   result.failed  += cnt;
				result.checked += cnt;
			}
			sqlite3_finalize(stmt);
		}
	}

	// ------------------------------------------------------------------
	// 2. Back-fill: find records that still have no local_sanity row.
	//    For these (legacy / pre-fix DBs) infer from source_class.
	//    'rejected'  -> failed (score 0.0)
	//    everything else -> passed (score 1.0)
	// ------------------------------------------------------------------
	const char* sql_legacy =
		"SELECT id, source_class FROM material_records "
		"WHERE NOT EXISTS ("
		"  SELECT 1 FROM validation_records vr "
		"  WHERE vr.material_id = material_records.id "
		"  AND   vr.validator_id = 'local_sanity'"
		") LIMIT ?;";

	const char* sql_insert_vr =
		"INSERT INTO validation_records "
		"(material_id, validator_id, validation_type, passed, score, method_tag, notes) "
		"VALUES (?,?,?,?,?,?,?);";

	const char* sql_reject =
		"UPDATE material_records SET source_class='rejected', "
		"updated_at=datetime('now','utc') WHERE id=?;";

	struct LegacyRow { int64_t id; std::string source_class; };
	std::vector<LegacyRow> legacy;
	{
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql_legacy, -1, &stmt, nullptr) == SQLITE_OK) {
			sqlite3_bind_int(stmt, 1, opts.batch);
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				LegacyRow r;
				r.id = sqlite3_column_int64(stmt, 0);
				auto c = sqlite3_column_text(stmt, 1);
				if (c) r.source_class = reinterpret_cast<const char*>(c);
				legacy.push_back(std::move(r));
			}
			sqlite3_finalize(stmt);
		}
	}

	if (!legacy.empty()) {
		sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

		for (const auto& row : legacy) {
			bool is_rejected = (row.source_class == "rejected");
			int    passed    = is_rejected ? 0 : 1;
			double score     = is_rejected ? 0.0 : 1.0;
			const char* notes = is_rejected
				? "inferred from source_class=rejected (legacy record)"
				: "inferred from source_class (legacy record)";

			{
				sqlite3_stmt* stmt = nullptr;
				if (sqlite3_prepare_v2(db, sql_insert_vr, -1, &stmt, nullptr) == SQLITE_OK) {
					sqlite3_bind_int64 (stmt, 1, row.id);
					sqlite3_bind_text  (stmt, 2, "local_sanity",       -1, SQLITE_STATIC);
					sqlite3_bind_text  (stmt, 3, "identity",           -1, SQLITE_STATIC);
					sqlite3_bind_int   (stmt, 4, passed);
					sqlite3_bind_double(stmt, 5, score);
					sqlite3_bind_text  (stmt, 6, "source_class_infer", -1, SQLITE_STATIC);
					sqlite3_bind_text  (stmt, 7, notes,                -1, SQLITE_STATIC);
					sqlite3_step(stmt);
					sqlite3_finalize(stmt);
				}
			}

			if (is_rejected) {
				// Ensure source_class stays 'rejected'
				sqlite3_stmt* stmt = nullptr;
				if (sqlite3_prepare_v2(db, sql_reject, -1, &stmt, nullptr) == SQLITE_OK) {
					sqlite3_bind_int64(stmt, 1, row.id);
					sqlite3_step(stmt);
					sqlite3_finalize(stmt);
				}
				++result.failed;
			} else {
				++result.passed;
			}
			++result.checked;
		}

		sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
	}

	sqlite3_close(db);

	result.success = true;

	if (opts.verbose) {
		std::cout << "[UFX_AUTO_2] validate: checked=" << result.checked
				  << " passed=" << result.passed
				  << " failed=" << result.failed << "\n";
	}

	return result;
}

void print_validate_result(const ValidateResult& r) {
	std::cout << "\n[UFX_AUTO_2] validate\n"
			  << "  db        : " << r.db_path  << "\n"
			  << "  checked   : " << r.checked  << "\n"
			  << "  passed    : " << r.passed   << "\n"
			  << "  failed    : " << r.failed   << "\n";
	if (!r.success)
		std::cout << "  ERROR     : " << r.error_message << "\n";
	std::cout << "\n";
}

// ============================================================================
// ufx_auto2_promote
// Evaluates composite scores from validation_records, transitions source_class,
// and writes promotion_history rows.
//
// Score model (UFX_continual_2.tex §17):
//   S = average of all validation_records.score for this material_id
//   >= min_score  -> validated
//   >= warn_score -> generated (kept, low confidence)
//   < warn_score  -> rejected
// ============================================================================

PromoteResult ufx_auto2_promote(const PromoteOptions& opts) {
	PromoteResult result;
	result.db_path = opts.db_path;

	std::string err;
	sqlite3* db = ufx_open_db_rw(opts.db_path, err);
	if (!db) {
		result.success       = false;
		result.error_message = "Cannot open DB: " + err;
		return result;
	}

	// Load generated records that have at least one validation row.
	const char* sql_candidates =
		"SELECT mr.id, mr.source_class, AVG(vr.score) AS avg_score "
		"FROM material_records mr "
		"JOIN validation_records vr ON vr.material_id = mr.id "
		"WHERE mr.source_class = 'generated' "
		"GROUP BY mr.id;";

	struct Candidate { int64_t id; double score; };
	std::vector<Candidate> candidates;

	{
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql_candidates, -1, &stmt, nullptr) != SQLITE_OK) {
			result.success       = false;
			result.error_message = "Failed to prepare candidate query";
			sqlite3_close(db);
			return result;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Candidate c;
			c.id    = sqlite3_column_int64 (stmt, 0);
			c.score = sqlite3_column_double(stmt, 2);
			candidates.push_back(c);
		}
		sqlite3_finalize(stmt);
	}

	const char* sql_update_class =
		"UPDATE material_records SET source_class=?, updated_at=datetime('now','utc') "
		"WHERE id=?;";
	const char* sql_history =
		"INSERT INTO promotion_history "
		"(material_id, from_class, to_class, composite_score, reason) "
		"VALUES (?,?,?,?,?);";

	sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

	for (const auto& c : candidates) {
		std::string to_class;
		std::string reason;

		if (c.score >= opts.min_score) {
			to_class = "validated";
			reason   = "score>=" + std::to_string(opts.min_score);
			++result.promoted;
		} else if (c.score >= opts.warn_score) {
			// Keep as generated but log the warn
			to_class = "generated";
			reason   = "score_warn";
			++result.warned;
		} else {
			to_class = "rejected";
			reason   = "score<" + std::to_string(opts.warn_score);
			++result.rejected;
		}

		// Update source_class
		if (to_class != "generated") {
			sqlite3_stmt* stmt = nullptr;
			if (sqlite3_prepare_v2(db, sql_update_class, -1, &stmt, nullptr) == SQLITE_OK) {
				sqlite3_bind_text  (stmt, 1, to_class.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_int64 (stmt, 2, c.id);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
		}

		// Always write promotion_history
		{
			sqlite3_stmt* stmt = nullptr;
			if (sqlite3_prepare_v2(db, sql_history, -1, &stmt, nullptr) == SQLITE_OK) {
				sqlite3_bind_int64 (stmt, 1, c.id);
				sqlite3_bind_text  (stmt, 2, "generated",      -1, SQLITE_STATIC);
				sqlite3_bind_text  (stmt, 3, to_class.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_double(stmt, 4, c.score);
				sqlite3_bind_text  (stmt, 5, reason.c_str(),   -1, SQLITE_TRANSIENT);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
		}
	}

	sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
	sqlite3_close(db);

	result.success = true;

	if (opts.verbose) {
		std::cout << "[UFX_AUTO_2] promote: promoted=" << result.promoted
				  << " warned=" << result.warned
				  << " rejected=" << result.rejected << "\n";
	}

	return result;
}

void print_promote_result(const PromoteResult& r) {
	std::cout << "\n[UFX_AUTO_2] promote\n"
			  << "  db        : " << r.db_path  << "\n"
			  << "  promoted  : " << r.promoted  << "\n"
			  << "  warned    : " << r.warned    << "\n"
			  << "  rejected  : " << r.rejected  << "\n";
	if (!r.success)
		std::cout << "  ERROR     : " << r.error_message << "\n";
	std::cout << "\n";
}

} // namespace vsepr::ufx
