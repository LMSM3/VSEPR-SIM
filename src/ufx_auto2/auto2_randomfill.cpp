// src/ufx_auto2/auto2_randomfill.cpp
// UFX_AUTO_2 Phase 2 -- Random Fill Main Loop
// VSEPR-SIM v5 beta8 -> beta9
//
// Implements run_auto2_randomfill() and Auto2RandomFillOptions.
// This is the generation spine:
//
//   load coverage map
//   -> for i in [0, count):
//      draw AxisSample
//      generate UFXMaterialRecord
//      run LocalSanityChecker
//      insert as GENERATED or REJECTED
//      write generation_axes row
//      update coverage map
//   -> print randomfill summary

#include "ufx_auto2/auto2_randomfill.hpp"

#include "ufx_auto2/axis_config.hpp"
#include "ufx_auto2/random_axis_sampler.hpp"
#include "ufx_auto2/coverage_map.hpp"
#include "ufx_auto2/material_generator.hpp"
#include "ufx_auto2/local_sanity.hpp"
#include "v4/uff/ufx_schema.hpp"
#include "v4/uff/ufx_material_record.hpp"

#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace vsepr::ufx {

// ============================================================================
// axis_sample_to_json
// Compact JSON representation of an AxisSample for generation_axes.axis_config
// ============================================================================

static std::string axis_sample_to_json(const AxisSample& s) {
	std::ostringstream j;
	j << "{"
	  << "\"element_family\":\"" << s.element_family << "\","
	  << "\"element\":\""        << s.element        << "\","
	  << "\"oxidation_state\":"  << s.oxidation_state << ","
	  << "\"coordination\":"     << s.coordination    << ","
	  << "\"geometry\":\""       << s.geometry        << "\","
	  << "\"phase\":\""          << s.phase           << "\","
	  << "\"temperature_K\":"    << s.temperature_K   << ","
	  << "\"pressure_atm\":"     << s.pressure_atm    << ","
	  << "\"target_property\":\"" << s.target_property << "\","
	  << "\"sample_index\":"     << s.sample_index    << ","
	  << "\"seed\":"             << s.seed_used
	  << "}";
	return j.str();
}

// ============================================================================
// run_id generation: run_YYYYMMDD_HHMMSS_{N}
// ============================================================================

static std::string make_run_id(int count) {
	auto now = std::chrono::system_clock::now();
	auto t   = std::chrono::system_clock::to_time_t(now);
	std::tm tm_utc{};
#if defined(_WIN32)
	gmtime_s(&tm_utc, &t);
#else
	gmtime_r(&t, &tm_utc);
#endif
	std::ostringstream out;
	out << "run_"
		<< std::put_time(&tm_utc, "%Y%m%d_%H%M%S")
		<< "_n" << count;
	return out.str();
}

// ============================================================================
// run_auto2_randomfill
// ============================================================================

RandomFillResult run_auto2_randomfill(const Auto2RandomFillOptions& opt) {
	RandomFillResult result;
	result.db_path = opt.db_path;

	// --- Open / init database ---
	std::string db_err;
	sqlite3* db = ufx_open_db_rw(opt.db_path, db_err);
	if (!db) {
		result.success = false;
		result.error_message = "Cannot open DB: " + db_err;
		return result;
	}

	// Ensure schema exists (idempotent)
	{
		InitResult ir = ufx_auto2_init_db(opt.db_path);
		if (!ir.success) {
			sqlite3_close(db);
			result.success = false;
			result.error_message = "Schema init failed: " + ir.error_message;
			return result;
		}
	}

	// --- Setup ---
	AxisConfig axes = AxisConfig::default_phase2();
	RandomAxisSampler sampler(axes, opt.seed);

	CoverageMap coverage;
	coverage.load_from_db(opt.db_path);

	MaterialRecordGenerator generator;
	const std::string run_id = make_run_id(opt.count);

	int generated = 0;
	int rejected  = 0;
	int skipped   = 0;   // duplicate material_key

	if (opt.verbose) {
		std::cout << "[UFX_AUTO_2] randomfill starting\n"
				  << "  run_id  : " << run_id      << "\n"
				  << "  count   : " << opt.count   << "\n"
				  << "  seed    : " << opt.seed    << "\n"
				  << "  db      : " << opt.db_path << "\n\n";
	}

	// Prepared SQL for inserting local_sanity validation rows.
	const char* sql_vr =
		"INSERT INTO validation_records "
		"(material_id, validator_id, validation_type, passed, score, method_tag, notes) "
		"VALUES (?,?,?,?,?,?,?);";

	// --- Begin transaction for batch performance ---
	sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

	for (int i = 0; i < opt.count; ++i) {
		AxisSample sample = sampler.next();

		// Generate candidate record
		UFXMaterialRecord record = generator.from_axis_sample(sample);

		// Run local sanity check
		LocalCheckResult check = LocalSanityChecker::check(record);

		if (!check.pass) {
			record.source_class              = SourceClass::Rejected;
			record.provenance.promotion_status = "local_reject";
			record.provenance.unit_conversion_trace = "reject: " + check.reason_string();
			++rejected;
		} else {
			record.source_class              = SourceClass::Generated;
			record.provenance.promotion_status = "unchecked";
		}

		// Insert material record
		int64_t rowid = ufx_insert_material_record(db, record);

		if (rowid == -2) {
			// Duplicate key — not an error, just a collision
			++skipped;
		} else if (rowid > 0) {
			// Write generation_axes row
			std::string axis_json = axis_sample_to_json(sample);
			RegionKey region = CoverageMap::make_key(sample);
			ufx_insert_generation_axes(db, rowid, region, axis_json, opt.seed);
			coverage.record_hit(region);

			// Write local_sanity validation row while the full record is in memory.
			{
				std::string notes = check.reason_string();
				if (notes.size() > 255) notes.resize(255);
				int    pass_int  = check.pass ? 1 : 0;
				double score_val = check.pass ? 1.0 : 0.0;

				sqlite3_stmt* vstmt = nullptr;
				if (sqlite3_prepare_v2(db, sql_vr, -1, &vstmt, nullptr) == SQLITE_OK) {
					sqlite3_bind_int64 (vstmt, 1, rowid);
					sqlite3_bind_text  (vstmt, 2, "local_sanity",       -1, SQLITE_STATIC);
					sqlite3_bind_text  (vstmt, 3, "identity",           -1, SQLITE_STATIC);
					sqlite3_bind_int   (vstmt, 4, pass_int);
					sqlite3_bind_double(vstmt, 5, score_val);
					sqlite3_bind_text  (vstmt, 6, "LocalSanityChecker", -1, SQLITE_STATIC);
					sqlite3_bind_text  (vstmt, 7, notes.c_str(),        -1, SQLITE_TRANSIENT);
					sqlite3_step(vstmt);
					sqlite3_finalize(vstmt);
				}
			}

			if (check.pass) ++generated;
		}

		// Commit in batches of 500 to avoid a single enormous transaction
		if ((i + 1) % 500 == 0) {
			sqlite3_exec(db, "COMMIT; BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
			if (opt.verbose) {
				std::cout << "  [" << (i + 1) << "/" << opt.count << "] "
						  << "gen=" << generated << " rej=" << rejected << "\n";
			}
		}
	}

	sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
	sqlite3_close(db);

	// --- Summary ---
	result.requested  = opt.count;
	result.generated  = generated;
	result.rejected   = rejected;
	result.skipped    = skipped;
	result.run_id     = run_id;
	result.success    = true;

	return result;
}

// ============================================================================
// print_randomfill_result
// ============================================================================

void print_randomfill_result(const RandomFillResult& r) {
	std::cout << "\n[UFX_AUTO_2] randomfill\n"
			  << "  run_id    : " << r.run_id    << "\n"
			  << "  requested : " << r.requested << "\n"
			  << "  generated : " << r.generated << "\n"
			  << "  rejected  : " << r.rejected  << "\n"
			  << "  skipped   : " << r.skipped   << " (duplicate key)\n"
			  << "  db        : " << r.db_path   << "\n";

	if (!r.success)
		std::cout << "  ERROR     : " << r.error_message << "\n";

	std::cout << "\n";
}

} // namespace vsepr::ufx
