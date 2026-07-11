// src/cli/cmd_ufx.cpp
// UFX AUTO2 CLI Command Implementation -- Phases 2-10
// Formation Engine v4.1.0 / VSEPR-SIM v5 beta9

#include "cmd_ufx.hpp"
#include "display.hpp"
#include "v4/uff/ufx_schema.hpp"
#include "ufx_auto2/auto2_randomfill.hpp"
#include "ufx_auto2/web_validator.hpp"
#include "ufx_auto2/web_cache.hpp"
#include "ufx_auto2/pubchem_fetcher.hpp"
#include "ufx_auto2/nist_fetcher.hpp"
#include "ufx_auto2/molecular_descriptor_generator.hpp"
#include "ufx_auto2/thermo_generator.hpp"
#include "ufx_auto2/crystal_generator.hpp"
#include "ufx_auto2/macro_property_generator.hpp"
#include "ufx_auto2/meta_score_engine.hpp"

#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace vsepr::cli {

// ============================================================================
// Help text
// ============================================================================

std::string UfxCommand::Help() const {
	return R"(
UFX AUTO2 — Materials State Database Generator
===============================================

USAGE:
	vsepr ufx auto2 <subcommand> [OPTIONS]

SUBCOMMANDS:
	init              Create (or verify) the UFX_AUTO_2 SQLite database.
	audit             Run audit queries and print a coverage/health report.
	randomfill        Generate candidate material records and store them.
	validate          Re-run local sanity checks over unvalidated records.
	promote           Evaluate composite scores and transition source_class.
	validate-web      Fetch external identity checks from PubChem / NIST.

	fill-molecular    Phase 6: fill molecular descriptor block (Tier 2).
	fill-thermo       Phase 7: fill thermo + EOS blocks (Tier 3).
	fill-crystal      Phase 8: fill crystal block for solid records (Tier 4).
	fill-transport    Phase 9: fill transport block (Tier 5).
	fill-mechanical   Phase 9: fill mechanical block (Tier 5).
	fill-macro        Phase 9: fill both transport + mechanical (alias).
	score             Phase 10: compute meta-score block (Tier 9).

Common options: --db <path>  --batch <n>  --verbose

EXAMPLES:
	vsepr ufx auto2 init
	vsepr ufx auto2 randomfill    --count 500  --db output/ufx_auto2.sqlite
	vsepr ufx auto2 fill-molecular --batch 500 --db output/ufx_auto2.sqlite
	vsepr ufx auto2 fill-thermo   --batch 500 --db output/ufx_auto2.sqlite
	vsepr ufx auto2 fill-crystal  --batch 500 --db output/ufx_auto2.sqlite
	vsepr ufx auto2 fill-macro    --batch 500 --db output/ufx_auto2.sqlite
	vsepr ufx auto2 score         --batch 500 --db output/ufx_auto2.sqlite
	vsepr ufx auto2 validate      --batch 200 --db output/ufx_auto2.sqlite
	vsepr ufx auto2 promote       --min-score 0.90
	vsepr ufx auto2 audit         --db output/ufx_auto2.sqlite

CYCLE ORDER (Phase 10 full pipeline):
	randomfill -> fill-molecular -> fill-thermo -> fill-crystal
	-> fill-transport -> fill-mechanical -> score
	-> validate -> validate-web -> promote -> audit

HARD RULES:
	- No provenance = no promotion.
	- State (T, P, phase) required on every property_values row.
	- Fill steps process only records missing the target block (idempotent).
	- Phase 10 steers sampling; it does not drop records.
)";
}

// ============================================================================
// Execute dispatch: ufx auto2 <subcommand>
// ============================================================================

int UfxCommand::Execute(const std::vector<std::string>& args) {
	if (args.size() < 2) {
		Display::Error("Usage: vsepr ufx auto2 <init|audit|randomfill> [--db <path>]");
		std::cout << Help();
		return 1;
	}

	const std::string& qualifier = args[0];  // "auto2"
	const std::string& subcmd    = args[1];

	if (qualifier != "auto2") {
		Display::Error("Unknown UFX qualifier '" + qualifier + "'. Expected 'auto2'.");
		return 1;
	}

	std::vector<std::string> sub_args(args.begin() + 2, args.end());

	if (subcmd == "init")                  return run_init_                (sub_args);
	if (subcmd == "audit")                 return run_audit_               (sub_args);
	if (subcmd == "randomfill")            return run_randomfill_          (sub_args);
	if (subcmd == "validate")              return run_validate_            (sub_args);
	if (subcmd == "promote")               return run_promote_             (sub_args);
	if (subcmd == "validate-web")          return run_validate_web_        (sub_args);
	if (subcmd == "backfill-provenance")   return run_backfill_provenance_ (sub_args);
	if (subcmd == "clear-block")           return run_clear_block_         (sub_args);

	// ── Phase 6-10 — parse options once, dispatch via registry ──────────────
	const Auto2Options opt = parse_auto2_options_(sub_args);

	if (opt.db_path.empty()) {
		Display::Error("Missing --db <path>. Provide a database path.");
		return 2;
	}

	struct CommandSpec {
		std::string_view        name;
		int (UfxCommand::*handler)(const Auto2Options&) const;
	};

	static constexpr CommandSpec kRegistry[] = {
		{ "fill-molecular",  &UfxCommand::run_fill_molecular_  },
		{ "fill-thermo",     &UfxCommand::run_fill_thermo_     },
		{ "fill-crystal",    &UfxCommand::run_fill_crystal_    },
		{ "fill-transport",  &UfxCommand::run_fill_transport_  },
		{ "fill-mechanical", &UfxCommand::run_fill_mechanical_ },
		{ "fill-macro",      &UfxCommand::run_fill_macro_      },
		{ "score",           &UfxCommand::run_score_           },
	};

	for (const auto& spec : kRegistry) {
		if (subcmd == spec.name)
			return (this->*spec.handler)(opt);
	}

	Display::Error("Unknown ufx auto2 subcommand '" + subcmd + "'.");
	Display::Info("Run 'vsepr ufx auto2 --help' for available subcommands.");
	return 1;
}

// ============================================================================
// ufx auto2 init
// ============================================================================

int UfxCommand::run_init_(const std::vector<std::string>& args) const {
	const std::string db_path = resolve_db_path_(args, "ufx_auto2.sqlite");

	std::cout << "[UFX AUTO2] Initialising database: " << db_path << "\n";

	vsepr::ufx::InitResult result = vsepr::ufx::ufx_auto2_init_db(db_path);

	if (!result.success) {
		Display::Error("Database initialisation failed: " + result.error_message);
		return 1;
	}

	std::cout << "[UFX AUTO2] Database ready.\n";
	std::cout << "            Path   : " << result.db_path     << "\n";
	std::cout << "            Tables : " << result.tables_created << " schema group(s) applied\n";
	std::cout << "\n";
	std::cout << "  Tables created (IF NOT EXISTS):\n";
	std::cout << "    material_records\n";
	std::cout << "    property_values\n";
	std::cout << "    validation_records\n";
	std::cout << "    generation_axes\n";
	std::cout << "    promotion_history\n";
	std::cout << "    property_provenance\n";
	std::cout << "\n";
	std::cout << "  Next steps:\n";
	std::cout << "    vsepr ufx auto2 audit  --db " << db_path << "\n";
	std::cout << "\n";

	return 0;
}

// ============================================================================
// ufx auto2 audit
// ============================================================================

int UfxCommand::run_audit_(const std::vector<std::string>& args) const {
	const std::string db_path = resolve_db_path_(args, "ufx_auto2.sqlite");

	vsepr::ufx::AuditReport report = vsepr::ufx::ufx_auto2_audit_db(db_path);

	vsepr::ufx::ufx_auto2_print_audit(report);

	return report.success ? 0 : 1;
}

// ============================================================================
// resolve_db_path_
// ============================================================================

int UfxCommand::run_randomfill_(const std::vector<std::string>& args) const {
	vsepr::ufx::Auto2RandomFillOptions opt;
	opt.db_path = resolve_db_path_(args, "ufx_auto2.sqlite");
	opt.verbose = true;

	for (std::size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--count" && i + 1 < args.size()) {
			try { opt.count = std::stoi(args[i + 1]); }
			catch (const std::exception&) {
				Display::Error("--count value is not a valid integer: " + args[i + 1]);
				return 1;
			}
		}
		if (args[i] == "--seed" && i + 1 < args.size()) {
			try { opt.seed = static_cast<uint64_t>(std::stoull(args[i + 1])); }
			catch (const std::exception&) {
				Display::Error("--seed value is not a valid integer: " + args[i + 1]);
				return 1;
			}
		}
	}

	if (opt.count <= 0) {
		Display::Error("--count must be a positive integer.");
		return 1;
	}

	vsepr::ufx::RandomFillResult result = vsepr::ufx::run_auto2_randomfill(opt);
	vsepr::ufx::print_randomfill_result(result);

	if (result.success) {
		// Immediate audit so the user sees counts right after fill.
		vsepr::ufx::AuditReport report = vsepr::ufx::ufx_auto2_audit_db(opt.db_path);
		vsepr::ufx::ufx_auto2_print_audit(report);
	}

	return result.success ? 0 : 1;
}

// ============================================================================
// resolve_db_path_
// ============================================================================

std::string UfxCommand::resolve_db_path_(const std::vector<std::string>& args,
										 const std::string& default_path) {
	for (std::size_t i = 0; i + 1 < args.size(); ++i) {
		if (args[i] == "--db") return args[i + 1];
	}
	return default_path;
}

// ============================================================================
// ufx auto2 validate
// ============================================================================

int UfxCommand::run_validate_(const std::vector<std::string>& args) const {
	vsepr::ufx::ValidateOptions opt;
	opt.db_path = resolve_db_path_(args, "ufx_auto2.sqlite");
	opt.verbose = true;

	for (std::size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--batch" && i + 1 < args.size()) {
			try { opt.batch = std::stoi(args[i + 1]); }
			catch (const std::exception&) {
				Display::Error("--batch value is not a valid integer: " + args[i + 1]);
				return 1;
			}
		}
		if (args[i] == "--run-dir" && i + 1 < args.size()) {
			opt.run_dir = args[i + 1];
		}
	}

	vsepr::ufx::ValidateResult result = vsepr::ufx::ufx_auto2_validate(opt);
	vsepr::ufx::print_validate_result(result);

	return result.success ? 0 : 1;
}

// ============================================================================
// ufx auto2 promote
// ============================================================================

int UfxCommand::run_promote_(const std::vector<std::string>& args) const {
	vsepr::ufx::PromoteOptions opt;
	opt.db_path = resolve_db_path_(args, "ufx_auto2.sqlite");
	opt.verbose = true;

	for (std::size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--min-score" && i + 1 < args.size()) {
			try { opt.min_score = std::stod(args[i + 1]); }
			catch (const std::exception&) {
				Display::Error("--min-score value is not a valid number: " + args[i + 1]);
				return 1;
			}
		}
		if (args[i] == "--warn-score" && i + 1 < args.size()) {
			try { opt.warn_score = std::stod(args[i + 1]); }
			catch (const std::exception&) {
				Display::Error("--warn-score value is not a valid number: " + args[i + 1]);
				return 1;
			}
		}
		if (args[i] == "--run-dir" && i + 1 < args.size()) {
			opt.run_dir = args[i + 1];
		}
	}

	vsepr::ufx::PromoteResult result = vsepr::ufx::ufx_auto2_promote(opt);
	vsepr::ufx::print_promote_result(result);

	if (result.success) {
		vsepr::ufx::AuditReport report = vsepr::ufx::ufx_auto2_audit_db(opt.db_path);
		vsepr::ufx::ufx_auto2_print_audit(report);
	}

	return result.success ? 0 : 1;
}

// ============================================================================
// ============================================================================
// ufx auto2 backfill-provenance  (one-shot legacy repair)
// ============================================================================

int UfxCommand::run_backfill_provenance_(const std::vector<std::string>& args) const {
	const std::string db_path = resolve_db_path_(args, "ufx_auto2.sqlite");

	std::string err;
	sqlite3* db = vsepr::ufx::ufx_open_db_rw(db_path, err);
	if (!db) {
		Display::Error("Cannot open DB: " + err);
		return 1;
	}

	// Insert a provenance row for every property_values row that has none.
	// The source_id and confidence are copied directly from property_values.
	const char* sql =
		"INSERT OR IGNORE INTO property_provenance "
		"(property_id, source_id, source_class, method_tag, confidence) "
		"SELECT pv.id, pv.source_id, 'generated', "
		"       'backfill_legacy_' || pv.block_name, "
		"       pv.confidence "
		"FROM property_values pv "
		"WHERE NOT EXISTS ( "
		"  SELECT 1 FROM property_provenance pp WHERE pp.property_id = pv.id "
		");";

	char* sqlerr = nullptr;
	int rc = sqlite3_exec(db, sql, nullptr, nullptr, &sqlerr);

	int64_t inserted = 0;
	if (rc == SQLITE_OK) {
		inserted = sqlite3_changes(db);
	} else {
		std::string msg = sqlerr ? sqlerr : "unknown error";
		sqlite3_free(sqlerr);
		sqlite3_close(db);
		Display::Error("Backfill failed: " + msg);
		return 1;
	}

	sqlite3_close(db);

	std::cout << "\n-- backfill-provenance summary --\n";
	std::cout << "  DB        : " << db_path << "\n";
	std::cout << "  Inserted  : " << inserted << " provenance rows\n";
	std::cout << "  Status    : OK\n\n";
	return 0;
}

// ============================================================================
// ufx auto2 clear-block  — purge property_values rows for a named block
//   so the fill step will re-compute them with updated physics.
//   Usage: vsepr ufx auto2 clear-block --block <name> --db <path>
//   Allowed block names: thermo eos crystal mechanical transport meta
// ============================================================================

int UfxCommand::run_clear_block_(const std::vector<std::string>& args) const {
	const std::string db_path = resolve_db_path_(args, "ufx_auto2.sqlite");

	std::string block_name;
	for (std::size_t i = 0; i < args.size(); ++i) {
		if ((args[i] == "--block" || args[i] == "-b") && i + 1 < args.size())
			block_name = args[++i];
	}

	static const std::vector<std::string> allowed = {
		"thermo","eos","crystal","mechanical","transport","meta","molecular"
	};
	if (block_name.empty() ||
		std::find(allowed.begin(), allowed.end(), block_name) == allowed.end()) {
		Display::Error("--block must be one of: thermo eos crystal mechanical transport meta molecular");
		return 1;
	}

	std::string err;
	sqlite3* db = vsepr::ufx::ufx_open_db_rw(db_path, err);
	if (!db) {
		Display::Error("Cannot open DB: " + err);
		return 1;
	}

	// Delete provenance rows first (FK constraint), then property_values
	const char* del_prov =
		"DELETE FROM property_provenance "
		"WHERE property_id IN ("
		"  SELECT id FROM property_values WHERE block_name = ?"
		");";
	const char* del_pv =
		"DELETE FROM property_values WHERE block_name = ?;";

	int64_t n_prov = 0, n_pv = 0;
	for (const char* sql : {del_prov, del_pv}) {
		sqlite3_stmt* st = nullptr;
		if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, block_name.c_str(), -1, SQLITE_STATIC);
			sqlite3_step(st);
			sqlite3_finalize(st);
			if (sql == del_prov) n_prov = sqlite3_changes(db);
			else                 n_pv   = sqlite3_changes(db);
		}
	}
	sqlite3_close(db);

	std::cout << "\n-- clear-block summary --\n";
	std::cout << "  DB            : " << db_path    << "\n";
	std::cout << "  Block cleared : " << block_name << "\n";
	std::cout << "  Rows deleted  : " << n_pv       << " property_values\n";
	std::cout << "  Prov deleted  : " << n_prov     << " property_provenance\n";
	std::cout << "  Status        : OK\n\n";
	return 0;
}

// ============================================================================
// ufx auto2 validate-web  (Phase 5 stub — full implementation in step-8)
// ============================================================================

int UfxCommand::run_validate_web_(const std::vector<std::string>& args) const {
	const std::string db_path  = resolve_db_path_(args, "ufx_auto2.sqlite");

	int batch = 25;
	std::string cache_dir = "web_cache";
	std::string run_dir;
	bool verbose = true;

	for (std::size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--batch" && i + 1 < args.size()) {
			try { batch = std::stoi(args[i + 1]); }
			catch (...) {
				Display::Error("--batch value is not a valid integer: " + args[i + 1]);
				return 1;
			}
		}
		if (args[i] == "--cache"   && i + 1 < args.size()) cache_dir = args[i + 1];
		if (args[i] == "--run-dir" && i + 1 < args.size()) run_dir   = args[i + 1];
	}

	// Open DB
	std::string db_err;
	sqlite3* db = vsepr::ufx::ufx_open_db_rw(db_path, db_err);
	if (!db) {
		Display::Error("Cannot open database: " + db_err);
		return 1;
	}

	// Build caching fetcher chain: PubChem + NIST behind a shared cache
	vsepr::ufx::WebCache      cache(cache_dir);
	vsepr::ufx::PubChemFetcher pubchem;
	vsepr::ufx::NISTFetcher    nist;

	// Use PubChem as primary (CachingFetcher wraps it)
	vsepr::ufx::CachingFetcher cached_pubchem(pubchem, cache);

	vsepr::ufx::WebValidator validator(cached_pubchem, db);

	vsepr::ufx::WebValidateOptions opts;
	opts.db_path   = db_path;
	opts.cache_dir = cache_dir;
	opts.batch     = batch;
	opts.verbose   = verbose;
	opts.run_dir   = run_dir;

	vsepr::ufx::WebValidateResult result = validator.validate_batch(opts);
	vsepr::ufx::print_web_validate_result(result);

	sqlite3_close(db);

	return result.success ? 0 : 1;
}

// ============================================================================
// parse_auto2_options_
// ============================================================================

Auto2Options UfxCommand::parse_auto2_options_(const std::vector<std::string>& args) {
	Auto2Options opt;
	for (std::size_t i = 0; i < args.size(); ++i) {
		if      (args[i] == "--db"            && i + 1 < args.size()) opt.db_path       = args[++i];
		else if (args[i] == "--batch"         && i + 1 < args.size()) {
			try { opt.batch = std::stoi(args[++i]); } catch (...) {}
		}
		else if (args[i] == "--validate-batch"&& i + 1 < args.size()) {
			try { opt.validate_batch = std::stoi(args[++i]); } catch (...) {}
		}
		else if (args[i] == "--web-batch"     && i + 1 < args.size()) {
			try { opt.web_batch = std::stoi(args[++i]); } catch (...) {}
		}
		else if (args[i] == "--min-score"     && i + 1 < args.size()) {
			try { opt.min_score = std::stod(args[++i]); } catch (...) {}
		}
		else if (args[i] == "--cache"         && i + 1 < args.size()) opt.cache_dir     = args[++i];
		else if (args[i] == "--run-dir"       && i + 1 < args.size()) opt.run_dir       = args[++i];
		else if (args[i] == "--recompute")    opt.recompute    = true;
		else if (args[i] == "--quiet")        opt.verbose      = false;
		else if (args[i] == "--verbose")      opt.verbose      = true;
		else if (args[i] == "--web-validate") opt.web_validate = true;
	}
	return opt;
}

// ============================================================================
// print_phase_banner_
// ============================================================================

void UfxCommand::print_phase_banner_(std::string_view phase_label,
									 std::string_view block_name,
									 std::string_view db_path,
									 int batch) {
	static constexpr std::string_view kSpinner = "|/-\\";
	static int s_spin_idx = 0;
	const char spin = kSpinner[s_spin_idx++ % 4];

	std::cout << "\n";
	std::cout << "  [" << spin << "] UFX AUTO2 -- " << phase_label << "\n";
	std::cout << "      block : " << block_name << "\n";
	std::cout << "      db    : " << db_path    << "\n";
	std::cout << "      batch : " << batch      << "\n";
	std::cout << "\n";
}

// ============================================================================
// Phase 6 -- fill-molecular
// ============================================================================

int UfxCommand::run_fill_molecular_(const Auto2Options& opt) const {
	print_phase_banner_("Phase 6  fill-molecular",
						ufx_blocks::molecular,
						opt.db_path.string(),
						opt.batch);

	vsepr::ufx::FillMolecularOptions fopts;
	fopts.db_path = opt.db_path.string();
	fopts.batch   = opt.batch;
	fopts.verbose = opt.verbose;

	try {
		const vsepr::ufx::FillMolecularResult r = vsepr::ufx::ufx_auto2_fill_molecular(fopts);
		vsepr::ufx::print_fill_molecular_result(r);
		return r.success ? 0 : 1;
	} catch (const std::exception& e) {
		Display::Error(std::string("[fill-molecular] ") + e.what());
		return 1;
	}
}

// ============================================================================
// Phase 7 -- fill-thermo
// ============================================================================

int UfxCommand::run_fill_thermo_(const Auto2Options& opt) const {
	print_phase_banner_("Phase 7  fill-thermo",
						ufx_blocks::thermo,
						opt.db_path.string(),
						opt.batch);

	vsepr::ufx::FillThermoOptions fopts;
	fopts.db_path = opt.db_path.string();
	fopts.batch   = opt.batch;
	fopts.verbose = opt.verbose;

	try {
		const vsepr::ufx::FillThermoResult r = vsepr::ufx::ufx_auto2_fill_thermo(fopts);
		vsepr::ufx::print_fill_thermo_result(r);
		return r.success ? 0 : 1;
	} catch (const std::exception& e) {
		Display::Error(std::string("[fill-thermo] ") + e.what());
		return 1;
	}
}

// ============================================================================
// Phase 8 -- fill-crystal
// ============================================================================

int UfxCommand::run_fill_crystal_(const Auto2Options& opt) const {
	print_phase_banner_("Phase 8  fill-crystal",
						ufx_blocks::crystal,
						opt.db_path.string(),
						opt.batch);

	vsepr::ufx::FillCrystalOptions fopts;
	fopts.db_path = opt.db_path.string();
	fopts.batch   = opt.batch;
	fopts.verbose = opt.verbose;

	try {
		const vsepr::ufx::FillCrystalResult r = vsepr::ufx::ufx_auto2_fill_crystal(fopts);
		vsepr::ufx::print_fill_crystal_result(r);
		return r.success ? 0 : 1;
	} catch (const std::exception& e) {
		Display::Error(std::string("[fill-crystal] ") + e.what());
		return 1;
	}
}

// ============================================================================
// Phase 9 -- fill-transport (single block via fill-macro with flag)
// ============================================================================

int UfxCommand::run_fill_transport_(const Auto2Options& opt) const {
	print_phase_banner_("Phase 9  fill-transport",
						ufx_blocks::transport,
						opt.db_path.string(),
						opt.batch);

	vsepr::ufx::FillMacroOptions fopts;
	fopts.db_path          = opt.db_path.string();
	fopts.batch            = opt.batch;
	fopts.verbose          = opt.verbose;
	fopts.fill_transport   = true;
	fopts.fill_mechanical  = false;

	try {
		const vsepr::ufx::FillMacroResult r = vsepr::ufx::ufx_auto2_fill_macro(fopts);
		vsepr::ufx::print_fill_macro_result(r);
		return r.success ? 0 : 1;
	} catch (const std::exception& e) {
		Display::Error(std::string("[fill-transport] ") + e.what());
		return 1;
	}
}

// ============================================================================
// Phase 9 -- fill-mechanical (single block via fill-macro with flag)
// ============================================================================

int UfxCommand::run_fill_mechanical_(const Auto2Options& opt) const {
	print_phase_banner_("Phase 9  fill-mechanical",
						ufx_blocks::mechanical,
						opt.db_path.string(),
						opt.batch);

	vsepr::ufx::FillMacroOptions fopts;
	fopts.db_path          = opt.db_path.string();
	fopts.batch            = opt.batch;
	fopts.verbose          = opt.verbose;
	fopts.fill_transport   = false;
	fopts.fill_mechanical  = true;

	try {
		const vsepr::ufx::FillMacroResult r = vsepr::ufx::ufx_auto2_fill_macro(fopts);
		vsepr::ufx::print_fill_macro_result(r);
		return r.success ? 0 : 1;
	} catch (const std::exception& e) {
		Display::Error(std::string("[fill-mechanical] ") + e.what());
		return 1;
	}
}

// ============================================================================
// Phase 9 -- fill-macro (transport THEN mechanical, in order)
// ============================================================================

int UfxCommand::run_fill_macro_(const Auto2Options& opt) const {
	// Sequence matters: transport data feeds mechanical estimates.
	static constexpr std::pair<std::string_view, int (UfxCommand::*)(const Auto2Options&) const>
		kSequence[] = {
			{ "fill-transport",  &UfxCommand::run_fill_transport_  },
			{ "fill-mechanical", &UfxCommand::run_fill_mechanical_ },
		};

	for (const auto& [label, fn] : kSequence) {
		const int rc = (this->*fn)(opt);
		if (rc != 0) {
			Display::Error(std::string("[fill-macro] stopped after failed phase: ") + std::string(label));
			return rc;
		}
	}
	return 0;
}

// ============================================================================
// Phase 10 -- score
// ============================================================================

int UfxCommand::run_score_(const Auto2Options& opt) const {
	print_phase_banner_("Phase 10  score",
						ufx_blocks::meta,
						opt.db_path.string(),
						opt.batch);

	vsepr::ufx::ScoreOptions sopts;
	sopts.db_path   = opt.db_path.string();
	sopts.batch     = opt.batch;
	sopts.verbose   = opt.verbose;
	sopts.recompute = opt.recompute;

	try {
		const vsepr::ufx::ScoreResult r = vsepr::ufx::ufx_auto2_score(sopts);
		vsepr::ufx::print_score_result(r);
		return r.success ? 0 : 1;
	} catch (const std::exception& e) {
		Display::Error(std::string("[score] ") + e.what());
		return 1;
	}
}

} // namespace vsepr::cli
