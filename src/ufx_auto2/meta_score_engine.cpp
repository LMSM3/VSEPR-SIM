// src/ufx_auto2/meta_score_engine.cpp
// UFX_AUTO_2 Phase 10 -- Meta-Score Engine + Coverage Scheduler
// VSEPR-SIM v5 beta9

#include "ufx_auto2/meta_score_engine.hpp"
#include "v4/uff/ufx_schema.hpp"

#include <sqlite3.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>

namespace vsepr::ufx {

// ============================================================================
// MetaScoreEngine constructor
// ============================================================================

MetaScoreEngine::MetaScoreEngine() = default;

// ============================================================================
// Component scoring functions
// ============================================================================

double MetaScoreEngine::weirdness_(const UFXMaterialRecord& rec,
								   const CoverageMap& coverage) const noexcept
{
	// Weirdness: how unusual is this record vs. periodic neighbours.
	// Proxy: low coverage in the same region -> more weird
	RegionKey key = CoverageMap::make_key_from_identity(rec.identity);
	int64_t cnt   = coverage.count_for(key);
	int64_t total = static_cast<int64_t>(coverage.region_count());
	if (total == 0) return 0.5;
	// Regions with count < 3 are considered weird
	double w = (cnt < 3) ? (1.0 - cnt / 3.0) : 0.05;

	// Boost for actinides and exotic geometries
	if (rec.identity.geometry_tag == "pentagonal_bipyramidal") w += 0.15;
	if (!rec.identity.elements.empty()) {
		int Z = rec.identity.atomic_numbers.empty() ? 0
				: rec.identity.atomic_numbers[0];
		if (Z >= 89) w += 0.2;  // actinides
	}
	return std::min(1.0, w);
}

double MetaScoreEngine::novelty_(const UFXMaterialRecord& rec,
								  const CoverageMap& coverage) const noexcept
{
	// Novelty: distance from existing validated entries.
	// Proxy: low count in region -> novel
	RegionKey key = CoverageMap::make_key_from_identity(rec.identity);
	int64_t cnt   = coverage.count_for(key);
	if (cnt == 0) return 1.0;
	return 1.0 / (1.0 + std::log(1.0 + cnt));
}

double MetaScoreEngine::macro_usefulness_(const UFXMaterialRecord& rec) const noexcept
{
	// Macro usefulness: how well does this entry help predict
	// E_eff, k_eff, dP, damage, phase stability.
	// Proxy: records with mechanical and transport blocks + solid phase score higher.
	double u = 0.0;
	if (rec.mechanical.E_eff_GPa.has_value())  u += 0.25;
	if (rec.transport.k_eff_W_mK.has_value())  u += 0.25;
	if (rec.crystal.density_g_cm3 > 0.0)       u += 0.20;
	if (rec.thermo.melting_point_K.has_value()) u += 0.15;
	if (rec.identity.phase == "solid")          u += 0.15;
	return std::min(1.0, u);
}

double MetaScoreEngine::extrapolation_risk_(const UFXMaterialRecord& rec) const noexcept
{
	// Risk: how far outside validated space is this record.
	// Proxy: low provenance confidence -> high risk.
	double conf = rec.provenance.confidence;
	if (conf <= 0.0) return 1.0;
	if (conf >= 1.0) return 0.0;
	return 1.0 - conf;
}

double MetaScoreEngine::local_consistency_(const UFXMaterialRecord& rec,
										   const CoverageMap& coverage) const noexcept
{
	// Local consistency: agreement with immediate periodic neighbours.
	// Proxy: if coordination is in normal range and force-field is valid -> 0.8
	int coord = rec.identity.coordination_number;
	bool normal_coord = (coord >= 2 && coord <= 8);
	bool ff_valid     = (rec.force_field.r1 > 0.0 && rec.force_field.D1 > 0.0);
	double score = 0.5;
	if (normal_coord) score += 0.2;
	if (ff_valid)     score += 0.3;
	(void)coverage;
	return std::min(1.0, score);
}

// ============================================================================
// compute
// ============================================================================

void MetaScoreEngine::compute(UFXMaterialRecord& rec,
							   const CoverageMap& coverage) const
{
	auto& m = rec.meta;

	m.weirdness         = weirdness_        (rec, coverage);
	m.novelty           = novelty_          (rec, coverage);
	m.macro_usefulness  = macro_usefulness_ (rec);
	m.extrapolation_risk = extrapolation_risk_(rec);
	m.local_consistency = local_consistency_(rec, coverage);

	m.confidence = rec.provenance.confidence > 0.0
				   ? rec.provenance.confidence : 0.35;
	m.coverage   = 0.0;  // will be set after DB query in score loop
	m.source_conflict    = 0.0;
	m.simulation_stability = 0.8;  // default optimistic; no simulation yet

	// Composite score model:
	// S = 0.25*identity + 0.20*local + 0.20*reference + 0.15*web + 0.10*repeat + 0.10*macro
	//
	// S_web: reflects how well the external identity has been confirmed.
	// - 'validated' source_class: web check passed -> 0.90
	// - 'generated' with high confidence (>=0.80): assumed plausible -> 0.65
	// - 'generated' with lower confidence: conservative -> 0.40
	// - otherwise: no web information -> 0.30
	double S_identity  = rec.identity.is_populated() ? 1.0 : 0.0;
	double S_local     = m.local_consistency;
	double S_reference = m.confidence;
	double S_web;
	{
		const std::string& sc = rec.provenance.source_class_tag;
		double conf = rec.provenance.confidence;
		if (sc == "validated" || sc == "experimental" || sc == "reference")
			S_web = 0.90;
		else if (sc == "generated" && conf >= 0.80)
			S_web = 0.65;
		else if (sc == "generated")
			S_web = 0.40;
		else
			S_web = 0.30;
	}
	double S_repeat    = 1.0 - m.weirdness * 0.5;
	double S_macro     = m.macro_usefulness;

	m.composite_score = 0.25*S_identity + 0.20*S_local + 0.20*S_reference +
						0.15*S_web      + 0.10*S_repeat + 0.10*S_macro;

	m.composite_score = std::max(0.0, std::min(1.0, m.composite_score));

	// Promotion class
	if (m.composite_score >= 0.95)
		m.promotion_class = "validated_high";
	else if (m.composite_score >= 0.85)
		m.promotion_class = "validated_warn";
	else if (m.composite_score >= 0.70)
		m.promotion_class = "generated_low";
	else
		m.promotion_class = "rejected";
}

// ============================================================================
// region_priority
// p(region) = (1-coverage)^2 * (1+macro_usefulness_avg) * (1-extrap_risk) * (1+weird*0.2)
// ============================================================================

double MetaScoreEngine::region_priority(const RegionKey& region,
										 const CoverageMap& coverage,
										 const std::map<RegionKey, RegionMeta>& meta_map) const noexcept
{
	double cov = 0.0;
	double wu  = 0.0;
	double mu  = 0.5;
	double er  = 0.5;

	int64_t cnt   = coverage.count_for(region);
	int64_t total = static_cast<int64_t>(coverage.region_count());
	if (total > 0) {
		// Simple proxy: coverage = cnt / (cnt + 10)
		cov = static_cast<double>(cnt) / (cnt + 10.0);
	}

	auto it = meta_map.find(region);
	if (it != meta_map.end()) {
		wu = it->second.weirdness_avg;
		mu = it->second.macro_usefulness_avg;
		er = it->second.extrapolation_risk;
	}

	double priority = (1.0 - cov) * (1.0 - cov)
					* (1.0 + mu)
					* (1.0 - er)
					* (1.0 + wu * 0.2);

	return std::max(1.0e-6, priority);
}

// ============================================================================
// build_priority_map
// ============================================================================

std::map<RegionKey, double> MetaScoreEngine::build_priority_map(
	const CoverageMap& coverage,
	const std::map<RegionKey, RegionMeta>& meta_map) const
{
	std::map<RegionKey, double> weights;

	// Collect all known regions
	std::vector<RegionKey> regions = coverage.all_regions();

	double total_w = 0.0;
	for (const auto& r : regions) {
		double w = region_priority(r, coverage, meta_map);
		weights[r] = w;
		total_w += w;
	}

	// Normalise
	if (total_w > 0.0) {
		for (auto& kv : weights)
			kv.second /= total_w;
	}

	return weights;
}

// ============================================================================
// update_coverage
// ============================================================================

void MetaScoreEngine::update_coverage(CoverageMap& coverage,
									   const UFXMaterialRecord& rec) const
{
	RegionKey key = CoverageMap::make_key_from_identity(rec.identity);
	coverage.record_hit(key);
}

// ============================================================================
// DB-level score loop
// ============================================================================

static int64_t insert_meta_pv(sqlite3* db,
							int64_t material_id,
							const std::string& property_name,
							double value_real,
							double temperature_K,
							double pressure_Pa,
							const std::string& phase)
{
	const char* sql =
		"INSERT OR REPLACE INTO property_values "
		"(material_id, block_tier, block_name, property_name, "
		" value_real, units, temperature_K, pressure_Pa, phase, "
		" source_class, source_id, confidence) "
		"VALUES (?,9,'meta',?,?,'',"
		"        ?,?,?,'generated','meta_score_engine_phase10',1.0);";

	sqlite3_stmt* st = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return -1;

	sqlite3_bind_int64 (st, 1, material_id);
	sqlite3_bind_text  (st, 2, property_name.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_double(st, 3, value_real);
	sqlite3_bind_double(st, 4, temperature_K);
	sqlite3_bind_double(st, 5, pressure_Pa);
	sqlite3_bind_text  (st, 6, phase.c_str(), -1, SQLITE_STATIC);

	int rc = sqlite3_step(st);
	sqlite3_finalize(st);
	if (rc != SQLITE_DONE) return -1;
	return sqlite3_last_insert_rowid(db);
}

ScoreResult ufx_auto2_score(const ScoreOptions& opts) {
	ScoreResult result;
	result.db_path = opts.db_path;

	std::string err;
	sqlite3* db = ufx_open_db_rw(opts.db_path, err);
	if (!db) {
		result.error_message = "Cannot open DB: " + err;
		return result;
	}

	// Load coverage map
	CoverageMap coverage;
	coverage.load_from_db(opts.db_path);

	// Query records to score
	const char* query_sql = opts.recompute
		? "SELECT mr.id, mr.material_key, "
		  "       ph_pv.value_text AS phase, "
		  "       co_pv.value_real AS confidence, "
		  "       r1_pv.value_real AS r1, "
		  "       d1_pv.value_real AS D1, "
		  "       co_n_pv.value_real AS coordination, "
		  "       em_pv.value_real AS E_eff, "
		  "       km_pv.value_real AS k_eff "
		  "FROM material_records mr "
		  "LEFT JOIN property_values ph_pv ON ph_pv.material_id = mr.id AND ph_pv.block_name='identity' AND ph_pv.property_name='phase' "
		  "LEFT JOIN property_values co_pv ON co_pv.material_id = mr.id AND co_pv.block_name='identity' AND co_pv.property_name='oxidation_state' "
		  "LEFT JOIN property_values r1_pv ON r1_pv.material_id = mr.id AND r1_pv.block_name='force_field' AND r1_pv.property_name='r1' "
		  "LEFT JOIN property_values d1_pv ON d1_pv.material_id = mr.id AND d1_pv.block_name='force_field' AND d1_pv.property_name='D1' "
		  "LEFT JOIN property_values co_n_pv ON co_n_pv.material_id = mr.id AND co_n_pv.block_name='identity' AND co_n_pv.property_name='coordination_number' "
		  "LEFT JOIN property_values em_pv ON em_pv.material_id = mr.id AND em_pv.block_name='mechanical' AND em_pv.property_name='E_eff_GPa' "
		  "LEFT JOIN property_values km_pv ON km_pv.material_id = mr.id AND km_pv.block_name='transport' AND km_pv.property_name='k_eff_W_mK' "
		  "WHERE mr.source_class NOT IN ('rejected') "
		  "LIMIT ?;"

		: "SELECT mr.id, mr.material_key, "
		  "       ph_pv.value_text AS phase, "
		  "       co_pv.value_real AS confidence, "
		  "       r1_pv.value_real AS r1, "
		  "       d1_pv.value_real AS D1, "
		  "       co_n_pv.value_real AS coordination, "
		  "       em_pv.value_real AS E_eff, "
		  "       km_pv.value_real AS k_eff "
		  "FROM material_records mr "
		  "LEFT JOIN property_values ph_pv ON ph_pv.material_id = mr.id AND ph_pv.block_name='identity' AND ph_pv.property_name='phase' "
		  "LEFT JOIN property_values co_pv ON co_pv.material_id = mr.id AND co_pv.block_name='identity' AND co_pv.property_name='oxidation_state' "
		  "LEFT JOIN property_values r1_pv ON r1_pv.material_id = mr.id AND r1_pv.block_name='force_field' AND r1_pv.property_name='r1' "
		  "LEFT JOIN property_values d1_pv ON d1_pv.material_id = mr.id AND d1_pv.block_name='force_field' AND d1_pv.property_name='D1' "
		  "LEFT JOIN property_values co_n_pv ON co_n_pv.material_id = mr.id AND co_n_pv.block_name='identity' AND co_n_pv.property_name='coordination_number' "
		  "LEFT JOIN property_values em_pv ON em_pv.material_id = mr.id AND em_pv.block_name='mechanical' AND em_pv.property_name='E_eff_GPa' "
		  "LEFT JOIN property_values km_pv ON km_pv.material_id = mr.id AND km_pv.block_name='transport' AND km_pv.property_name='k_eff_W_mK' "
		  "WHERE NOT EXISTS ( "
		  "  SELECT 1 FROM property_values pv "
		  "  WHERE pv.material_id = mr.id AND pv.block_name = 'meta' "
		  ") "
		  "AND mr.source_class NOT IN ('rejected') "
		  "LIMIT ?;";

	sqlite3_stmt* qstmt = nullptr;
	if (sqlite3_prepare_v2(db, query_sql, -1, &qstmt, nullptr) != SQLITE_OK) {
		result.error_message = std::string("Query prepare: ") + sqlite3_errmsg(db);
		sqlite3_close(db);
		return result;
	}
	sqlite3_bind_int(qstmt, 1, opts.batch);

	MetaScoreEngine engine;
	double sum_composite = 0.0;

	while (sqlite3_step(qstmt) == SQLITE_ROW) {
		result.processed++;

		int64_t     mid    = sqlite3_column_int64(qstmt, 0);
		const char* mkey_c = (const char*)sqlite3_column_text(qstmt, 1);
		const char* ph_c   = (const char*)sqlite3_column_text(qstmt, 2);
		double conf        = sqlite3_column_double(qstmt, 3);
		double r1          = sqlite3_column_double(qstmt, 4);
		double D1          = sqlite3_column_double(qstmt, 5);
		double coord_n     = sqlite3_column_double(qstmt, 6);
		double E_eff       = sqlite3_column_double(qstmt, 7);
		double k_eff       = sqlite3_column_double(qstmt, 8);

		std::string mkey  = mkey_c ? mkey_c : "";
		std::string phase = ph_c   ? ph_c   : "solid";

		UFXMaterialRecord rec;
		rec.identity.material_key    = mkey;
		rec.identity.phase           = phase;
		rec.identity.coordination_number = static_cast<int>(coord_n);
		rec.provenance.confidence    = conf > 0.0 ? conf : 0.35;
		rec.force_field.r1           = r1  > 0.0 ? r1   : 1.5;
		rec.force_field.D1           = D1  > 0.0 ? D1   : 0.1;
		if (E_eff > 0.0) rec.mechanical.E_eff_GPa = E_eff;
		if (k_eff > 0.0) rec.transport.k_eff_W_mK = k_eff;
		if (rec.identity.phase == "solid") rec.crystal.density_g_cm3 = 5.0;

		engine.compute(rec, coverage);
		auto& m = rec.meta;

		m.coverage = coverage.count_for(CoverageMap::make_key_from_identity(rec.identity))
					 / std::max(1.0, (double)coverage.region_count());

		double T = 298.15, P = 101325.0;
		auto ins = [&](const std::string& prop, double val) -> bool {
			int64_t rid = insert_meta_pv(db, mid, prop, val, T, P, phase);
			if (rid > 0) ufx_insert_provenance(db, rid,
				"meta_score_engine_phase10", "MetaScoreEngine_phase10", 1.0);
			return rid > 0;
		};

		bool ok = true;
		ok &= ins("weirdness",            m.weirdness);
		ok &= ins("coverage",             m.coverage);
		ok &= ins("confidence",           m.confidence);
		ok &= ins("novelty",              m.novelty);
		ok &= ins("local_consistency",    m.local_consistency);
		ok &= ins("source_conflict",      m.source_conflict);
		ok &= ins("macro_usefulness",     m.macro_usefulness);
		ok &= ins("simulation_stability", m.simulation_stability);
		ok &= ins("extrapolation_risk",   m.extrapolation_risk);
		ok &= ins("composite_score",      m.composite_score);

		if (opts.verbose) {
			std::cout << "  [score] " << mkey
					  << "  S=" << m.composite_score
					  << "  weird=" << m.weirdness
					  << (ok ? " OK" : " ERR") << "\n";
		}

		if (ok) {
			result.scored++;
			sum_composite += m.composite_score;
			if (m.weirdness > result.max_weird) {
				result.max_weird     = m.weirdness;
				result.max_weird_key = mkey;
			}
		} else {
			result.failed++;
		}

		engine.update_coverage(coverage, rec);
	}
	sqlite3_finalize(qstmt);

	if (result.scored > 0)
		result.avg_composite = sum_composite / result.scored;

	result.weakest_region = coverage.weakest_region();

	sqlite3_close(db);
	result.success = (result.failed == 0);
	return result;
}

void print_score_result(const ScoreResult& r) {
	std::cout << "\n-- Meta-Score Summary --\n";
	std::cout << "  DB                 : " << r.db_path       << "\n";
	std::cout << "  Processed          : " << r.processed     << "\n";
	std::cout << "  Scored             : " << r.scored        << "\n";
	std::cout << "  Failed             : " << r.failed        << "\n";
	std::cout << "  Avg composite score: " << r.avg_composite << "\n";
	if (!r.max_weird_key.empty())
		std::cout << "  Highest weirdness  : " << r.max_weird_key
				  << " (" << r.max_weird << ")\n";
	if (!r.weakest_region.empty())
		std::cout << "  Weakest region     : " << r.weakest_region << "\n";
	std::cout << "  Priority bias      : yes (Phase 10 scheduler)\n";
	std::cout << "  Status             : " << (r.success ? "OK" : "FAIL") << "\n";
	if (!r.error_message.empty())
		std::cout << "  Error              : " << r.error_message << "\n";
	std::cout << "\n";
}

} // namespace vsepr::ufx
