// src/ufx_auto2/molecular_descriptor_generator.cpp
// UFX_AUTO_2 Phase 6 -- Molecular Descriptor Generator
// VSEPR-SIM v5 beta9

#include "ufx_auto2/molecular_descriptor_generator.hpp"
#include "ufx_auto2/smiles_vocab.hpp"
#include "v4/uff/ufx_schema.hpp"

// -- existing codebase --
#include "atomistic/core/empirical_reference.hpp" // empirical::LJ_REFS, ION_REFS, BOND_REFS
#include "pot/atomic_masses.hpp"                  // vsepr::get_atomic_mass(Z)

#include <sqlite3.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <unordered_map>

namespace vsepr::ufx {

// ============================================================================
// Atomic data tables
// ============================================================================

// monoisotopic_mass_table: unique data not available in other headers — kept.
static const std::unordered_map<std::string, double>& monoisotopic_mass_table() {
	static const std::unordered_map<std::string, double> tbl = {
		{"H",  1.00783}, {"C",  12.0000}, {"N",  14.0031}, {"O",  15.9949},
		{"F",  18.9984}, {"Na", 22.9898}, {"Mg", 23.9850}, {"Al", 26.9815},
		{"Si", 27.9769}, {"P",  30.9738}, {"S",  31.9721}, {"Cl", 34.9689},
		{"K",  38.9637}, {"Ca", 39.9626}, {"Ti", 47.9479}, {"V",  50.9440},
		{"Cr", 51.9405}, {"Mn", 54.9380}, {"Fe", 55.9349}, {"Co", 58.9332},
		{"Ni", 57.9353}, {"Cu", 62.9296}, {"Zn", 63.9291}, {"Br", 78.9183},
		{"Zr", 89.9047}, {"Mo", 97.9054}, {"Pd", 105.903}, {"Ag", 106.905},
		{"I",  126.905}, {"Cs", 132.905}, {"Ba", 137.905}, {"La", 138.906},
		{"W",  183.951}, {"Re", 186.956}, {"Ir", 190.961}, {"Pt", 194.965},
		{"Au", 196.967}, {"Pb", 207.977}, {"Th", 232.038}, {"U",  238.051},
		{"Pu", 244.064},
	};
	return tbl;
}

// ============================================================================
// MolecularDescriptorGenerator -- static helpers
// ============================================================================

// Atomic weight: route through empirical::LJ_REFS (UFF periodic table) to
// obtain Z, then use vsepr::get_atomic_mass(Z) (IUPAC 2021).
// Eliminates the former 60-line shadow weight table.
double MolecularDescriptorGenerator::atomic_weight_(const std::string& sym) noexcept {
	for (int i = 0; i < empirical::N_LJ_REFS; ++i) {
		if (std::string(empirical::LJ_REFS[i].symbol) == sym) {
			double m = vsepr::get_atomic_mass(
				static_cast<uint8_t>(empirical::LJ_REFS[i].Z));
			if (m > 0.0) return m;
		}
	}
	// Elements present in ION_REFS but not LJ_REFS (anions / light metals)
	for (int i = 0; i < empirical::N_ION_REFS; ++i) {
		const auto& ir = empirical::ION_REFS[i];
		// strip charge suffix: "Na+" -> "Na"
		std::string bare(ir.symbol);
		auto p = bare.find_first_of("+-");
		if (p != std::string::npos) bare.resize(p);
		if (bare == sym) {
			double m = vsepr::get_atomic_mass(static_cast<uint8_t>(ir.Z));
			if (m > 0.0) return m;
		}
	}
	return 0.0;
}

double MolecularDescriptorGenerator::monoisotopic_mass_(const std::string& sym) noexcept {
	auto& t = monoisotopic_mass_table();
	auto it = t.find(sym);
	if (it != t.end()) return it->second;
	return atomic_weight_(sym);   // fallback to avg if mono not tabulated
}


double MolecularDescriptorGenerator::molecular_weight_from_formula(
	const std::string& formula) noexcept
{
	// Simple single-element case: formula == element symbol
	double aw = atomic_weight_(formula);
	if (aw > 0.0) return aw;
	// Unrecognised formula -- return 0.0 (caller logs as missing)
	return 0.0;
}

double MolecularDescriptorGenerator::exact_mass_from_element(
	const std::string& symbol) noexcept
{
	return monoisotopic_mass_(symbol);
}

std::string MolecularDescriptorGenerator::inchikey_stub(const std::string& element,
														  int oxidation_state,
														  int coordination) {
	// Produce a deterministic stub that is clearly NOT a real InChIKey
	// but is unique for the (element, ox, coord) triple.
	std::ostringstream oss;
	oss << "UFXSTUB-"
		<< element
		<< (oxidation_state >= 0 ? "P" : "N")
		<< std::abs(oxidation_state)
		<< "C" << coordination
		<< "-A";
	return oss.str();
}

std::string MolecularDescriptorGenerator::formula_from_identity(
	const std::string& element,
	int oxidation_state,
	int coordination)
{
	(void)oxidation_state;
	if (coordination <= 1) return element;
	std::ostringstream oss;
	oss << element;
	if (coordination > 1) oss << coordination;
	return oss.str();
}

// ============================================================================
// MolecularDescriptorGenerator constructor
// ============================================================================

MolecularDescriptorGenerator::MolecularDescriptorGenerator() = default;

// ============================================================================
// fill -- populate rec.molecular deterministically
// ============================================================================

void MolecularDescriptorGenerator::fill(UFXMaterialRecord& rec,
										const AxisSample& s) const {
	auto& m = rec.molecular;

	// Formula
	m.molecular_formula = formula_from_identity(s.element,
												 s.oxidation_state,
												 s.coordination);

	// Weights
	m.molecular_weight = atomic_weight_(s.element);
	m.exact_mass       = monoisotopic_mass_(s.element);

	// Heavy atom count: for a simple coordination complex, each ligand is a
	// separate atom + the central atom itself. We use coordination as a proxy.
	m.heavy_atom_count = std::max(1, s.coordination);

	// SMILES from vocabulary (empty if not found)
	auto entry = SmilesVocab::lookup_entry(s.element,
										   s.oxidation_state,
										   s.coordination);
	m.canonical_smiles    = entry.smiles;
	m.hbond_donor_count   = entry.hbond_donor;
	m.hbond_acceptor_count = entry.hbond_acceptor;

	// Stub InChIKey
	m.inchikey = inchikey_stub(s.element, s.oxidation_state, s.coordination);

	// Propagate from AxisSample optional overrides
	if (s.canonical_smiles.has_value() && !s.canonical_smiles->empty())
		m.canonical_smiles = *s.canonical_smiles;
	if (s.heavy_atom_count.has_value())
		m.heavy_atom_count = *s.heavy_atom_count;
	if (s.hbond_donor_count.has_value())
		m.hbond_donor_count = *s.hbond_donor_count;
	if (s.hbond_acceptor_count.has_value())
		m.hbond_acceptor_count = *s.hbond_acceptor_count;
}

// ============================================================================
// ufx_auto2_fill_molecular -- DB-level fill loop
// ============================================================================

// Insert one property_values row for the molecular block.
// Returns the new rowid on success, -1 on failure / already exists.
static int64_t insert_molecular_pv(sqlite3* db,
								int64_t material_id,
								const std::string& property_name,
								double value_real,
								const std::string& value_text,
								const std::string& units,
								double temperature_K,
								double pressure_Pa,
								const std::string& phase)
{
	const char* sql =
		"INSERT OR IGNORE INTO property_values "
		"(material_id, block_tier, block_name, property_name, "
		" value_real, value_text, units, "
		" temperature_K, pressure_Pa, phase, "
		" source_class, source_id, confidence) "
		"VALUES (?,2,'molecular',?,?,?,?,?,?,?,'generated',"
		"        'periodic_table_arithmetic',0.80);";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
		return -1;

	sqlite3_bind_int64(stmt, 1, material_id);
	sqlite3_bind_text (stmt, 2, property_name.c_str(), -1, SQLITE_STATIC);

	if (!value_text.empty()) {
		sqlite3_bind_null  (stmt, 3);
		sqlite3_bind_text  (stmt, 4, value_text.c_str(), -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_double(stmt, 3, value_real);
		sqlite3_bind_null  (stmt, 4);
	}

	sqlite3_bind_text  (stmt, 5, units.c_str(),       -1, SQLITE_STATIC);
	sqlite3_bind_double(stmt, 6, temperature_K);
	sqlite3_bind_double(stmt, 7, pressure_Pa);
	sqlite3_bind_text  (stmt, 8, phase.c_str(),       -1, SQLITE_STATIC);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) return -1;
	return sqlite3_last_insert_rowid(db);
}

FillMolecularResult ufx_auto2_fill_molecular(const FillMolecularOptions& opts) {
	FillMolecularResult result;
	result.db_path = opts.db_path;

	std::string err;
	sqlite3* db = ufx_open_db_rw(opts.db_path, err);
	if (!db) {
		result.error_message = "Cannot open DB: " + err;
		return result;
	}

	// Query records that are missing molecular block rows
	const char* query_sql =
		"SELECT mr.id, mr.material_key, "
		"       id_pv.value_text AS element, "
		"       ox_pv.value_real AS ox_state, "
		"       co_pv.value_real AS coordination, "
		"       ph_pv.value_text AS phase "
		"FROM material_records mr "
		"LEFT JOIN property_values id_pv ON id_pv.material_id = mr.id "
		"                               AND id_pv.block_name  = 'identity' "
		"                               AND id_pv.property_name = 'element' "
		"LEFT JOIN property_values ox_pv ON ox_pv.material_id = mr.id "
		"                               AND ox_pv.block_name  = 'identity' "
		"                               AND ox_pv.property_name = 'oxidation_state' "
		"LEFT JOIN property_values co_pv ON co_pv.material_id = mr.id "
		"                               AND co_pv.block_name  = 'identity' "
		"                               AND co_pv.property_name = 'coordination_number' "
		"LEFT JOIN property_values ph_pv ON ph_pv.material_id = mr.id "
		"                               AND ph_pv.block_name  = 'identity' "
		"                               AND ph_pv.property_name = 'phase' "
		"WHERE NOT EXISTS ( "
		"  SELECT 1 FROM property_values pv "
		"  WHERE pv.material_id = mr.id "
		"    AND pv.block_name  = 'molecular' "
		") "
		"AND mr.source_class NOT IN ('rejected') "
		"LIMIT ?;";

	sqlite3_stmt* qstmt = nullptr;
	if (sqlite3_prepare_v2(db, query_sql, -1, &qstmt, nullptr) != SQLITE_OK) {
		result.error_message = std::string("Query prepare failed: ") +
								sqlite3_errmsg(db);
		sqlite3_close(db);
		return result;
	}
	sqlite3_bind_int(qstmt, 1, opts.batch);

	MolecularDescriptorGenerator gen;

	while (sqlite3_step(qstmt) == SQLITE_ROW) {
		result.processed++;

		int64_t     material_id = sqlite3_column_int64(qstmt, 0);
		const char* mkey_c      = (const char*)sqlite3_column_text(qstmt, 1);
		const char* elem_c      = (const char*)sqlite3_column_text(qstmt, 2);
		double      ox_state    = sqlite3_column_double(qstmt, 3);
		double      coord       = sqlite3_column_double(qstmt, 4);
		const char* phase_c     = (const char*)sqlite3_column_text(qstmt, 5);

		std::string material_key = mkey_c  ? mkey_c  : "";
		std::string element      = elem_c  ? elem_c  : "";
		std::string phase        = phase_c ? phase_c : "solid";

		if (element.empty()) {
			// Try to extract element from material_key prefix
			auto pos = material_key.find('_');
			if (pos != std::string::npos)
				element = material_key.substr(0, pos);
		}

		if (element.empty()) {
			result.failed++;
			continue;
		}

		// Build a minimal AxisSample for the generator
		AxisSample s;
		s.element        = element;
		s.oxidation_state = static_cast<int>(ox_state);
		s.coordination   = static_cast<int>(coord > 0 ? coord : 4);
		s.phase          = phase;
		s.temperature_K  = 298.15;
		s.pressure_atm   = 1.0;

		UFXMaterialRecord rec;
		gen.fill(rec, s);
		auto& m = rec.molecular;

		double T   = s.temperature_K;
		double P   = 101325.0;

		// Insert property rows; collect rowids for provenance
		struct PvRow { std::string name; int64_t rowid; };
		PvRow pv_rows[] = {
			{ "molecular_weight",      insert_molecular_pv(db, material_id, "molecular_weight",      m.molecular_weight,           "", "g/mol",   T, P, phase) },
			{ "exact_mass",            insert_molecular_pv(db, material_id, "exact_mass",            m.exact_mass,                 "", "g/mol",   T, P, phase) },
			{ "heavy_atom_count",      insert_molecular_pv(db, material_id, "heavy_atom_count",      (double)m.heavy_atom_count,   "", "count",   T, P, phase) },
			{ "hbond_donor_count",     insert_molecular_pv(db, material_id, "hbond_donor_count",     (double)m.hbond_donor_count,  "", "count",   T, P, phase) },
			{ "hbond_acceptor_count",  insert_molecular_pv(db, material_id, "hbond_acceptor_count",  (double)m.hbond_acceptor_count,"","count",  T, P, phase) },
			{ "inchikey",              insert_molecular_pv(db, material_id, "inchikey",              0.0, m.inchikey,               "string",   T, P, phase) },
		};

		bool ok = true;
		for (auto& pv : pv_rows) {
			if (pv.rowid > 0) {
				ufx_insert_provenance(db, pv.rowid,
					"periodic_table_arithmetic",
					"MolecularDescriptorGenerator_phase6",
					0.80);
			} else {
				ok = false;
			}
		}

		if (opts.verbose) {
			std::cout << "  [fill-molecular] " << material_key
					  << "  MW=" << m.molecular_weight
					  << " g/mol  heavy_atoms=" << m.heavy_atom_count
					  << (ok ? " OK" : " ERR") << "\n";
		}

		if (ok) result.filled++;
		else    result.failed++;
	}
	sqlite3_finalize(qstmt);
	sqlite3_close(db);

	result.success = (result.failed == 0);
	return result;
}

void print_fill_molecular_result(const FillMolecularResult& r) {
	std::cout << "\n-- fill-molecular summary --\n";
	std::cout << "  DB           : " << r.db_path    << "\n";
	std::cout << "  Processed    : " << r.processed  << "\n";
	std::cout << "  Filled       : " << r.filled     << "\n";
	std::cout << "  Skipped      : " << r.skipped    << "\n";
	std::cout << "  Failed       : " << r.failed     << "\n";
	std::cout << "  Status       : " << (r.success ? "OK" : "FAIL") << "\n";
	if (!r.error_message.empty())
		std::cout << "  Error        : " << r.error_message << "\n";
	std::cout << "\n";
}

} // namespace vsepr::ufx
