#pragma once
// =============================================================================
// xsuite.hpp  —  .X Saved Executable Suite Format (v5.1.3~2)
// =============================================================================
//
// A .X file is the lean top-level descriptor for a saved VSEPR-SIM run suite.
// It is not a coordinate file, trajectory, or checkpoint.
// It is the file that says:
//
//   Here is the saved system.
//   Here are the files.
//   Here is what to compile.
//   Here is what to run.
//   Here is what to validate.
//   Here is what to replay.
//
// Format: INI-style sections with key = value pairs.
//
// Required sections: [xsuite], [run], [files]
// Optional sections: [build], [hash], [actions], [outputs]
//
// CLI integration: `vsepr x <sub-command> <file.X>`
//   inspect   show suite contents and file status
//   validate  check hashes/contracts/file presence
//   run       compile (if enabled) then run entry script
//   replay    open trajectory/rich replay in desktop viewer
//   export    regenerate reports and artifacts
//   open      double-click launcher — 2 CMD + Qt desktop
//
// =============================================================================

#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <stdexcept>

namespace vsepr {
namespace xsuite {

// ---------------------------------------------------------------------------
// XSuiteFile — full parsed representation of a .X file
// ---------------------------------------------------------------------------

struct XSuiteFile {
	// [xsuite]
	std::string name;
	std::string version    = "5.2";
	std::string mode       = "saved_run_suite";
	std::string created_by = "vsepr";

	// [run]
	std::string entry_script;       // required — .vsim entry point
	std::string run_mode     = "molecule";
	int         num_steps    = 10000;
	double      dt           = 1.0e-15;

	// [build]
	bool        compile      = false;
	std::string compiler     = "auto";
	std::string target       = "vsepr";
	std::string build_config = "release";

	// [files]
	std::string xyz_path;
	std::string xyza_path;
	std::string xyzc_path;
	std::string xyzf_path;
	std::string xyzfull_path;

	// [hash]
	std::string script_hash;
	std::string snapshot_hash;
	std::string checkpoint_hash;
	std::string trajectory_hash;
	std::string suite_hash;

	// [actions]
	bool run            = true;
	bool validate       = true;
	bool replay         = false;
	bool export_outputs = true;

	// [outputs]
	std::string report_path;
	std::string json_path;
	std::string log_path;

	// -----------------------------------------------------------------------
	// WO-XSUITE-02B  —  Live Persistent Instance + Batch Presolve Layer
	// -----------------------------------------------------------------------

	// [live]
	bool        live_enabled             = false;
	bool        live_persistent_instance = false;
	int         live_state_flush_interval  = 25;
	int         live_health_flush_interval = 10;

	// [render]  (streaming, not graphics driver)
	bool        render_enabled       = false;
	std::string render_atomic_stream;
	std::string render_analysis_stream;
	int         render_fps_atomic    = 15;
	int         render_fps_analysis  = 2;

	// [checkpoint]
	bool        checkpoint_enabled   = false;
	std::string checkpoint_format    = "xyzc";
	std::string checkpoint_directory = "checkpoints/";
	int         checkpoint_interval  = 500;
	int         checkpoint_keep_last = 5;
	bool        checkpoint_write_hash = true;

	// [presolve]
	bool        presolve_enabled          = false;
	std::string presolve_mode             = "batch_eigen";
	int         presolve_batch_count      = 16;
	int         presolve_seed_start       = 1000;
	int         presolve_seed_stride      = 1;
	int         presolve_max_steps        = 2000;
	bool        presolve_extract_matrix   = true;
	bool        presolve_extract_eigen    = true;
	std::string presolve_write_basis;
	std::string presolve_write_summary;

	// [eigenmine]
	bool        eigenmine_enabled          = false;
	int64_t     eigenmine_target_solves    = 10000000;
	int         eigenmine_batch_count      = 1000;
	int         eigenmine_seeds_per_batch  = 100;
	int         eigenmine_systems_per_seed = 100;
	std::string eigenmine_matrix_source    = "state_force_event";
	int         eigenmine_mode_rank_limit  = 256;
	double      eigenmine_min_recurrence   = 0.70;
	double      eigenmine_max_recon_error  = 0.05;
	std::string eigenmine_write_modes;
	std::string eigenmine_write_basis;
	std::string eigenmine_write_summary;

	// [curvefit]
	bool        curvefit_enabled         = false;
	std::string curvefit_source;
	std::string curvefit_target;
	std::string curvefit_model           = "poly_log_eigen_hybrid";
	int         curvefit_max_order       = 3;
	double      curvefit_regularization  = 1.0e-4;
	double      curvefit_train_fraction  = 0.80;
	double      curvefit_val_fraction    = 0.20;
	std::string curvefit_write_model;
	std::string curvefit_write_coefficients;
	std::string curvefit_write_report;

	// [release_gate]
	bool        release_gate_enabled         = false;
	std::string release_gate_baseline;
	std::string release_gate_candidate;
	double      release_gate_require_hash    = 0.999;
	double      release_gate_require_curvefit = 0.85;
	double      release_gate_require_eigen   = 0.80;
	double      release_gate_max_failure     = 0.01;
	std::string release_gate_write_report;
	std::string release_gate_write_comparison;
};

// ---------------------------------------------------------------------------
// Parse result
// ---------------------------------------------------------------------------

struct XSuiteParseResult {
	bool        ok      = false;
	XSuiteFile  suite;
	std::string error;
	std::vector<std::string> warnings;
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace detail {

inline std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return {};
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

inline std::string lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}

inline bool parse_bool(const std::string& v) {
	std::string lv = lower(trim(v));
	return (lv == "true" || lv == "1" || lv == "yes" || lv == "on");
}

using SectionMap = std::unordered_map<std::string,
					   std::unordered_map<std::string, std::string>>;

inline SectionMap parse_ini(std::istream& in) {
	SectionMap sections;
	std::string current;
	std::string line;
	while (std::getline(in, line)) {
		std::string t = trim(line);
		if (t.empty() || t[0] == '#' || t[0] == ';') continue;
		if (t.front() == '[' && t.back() == ']') {
			current = lower(t.substr(1, t.size() - 2));
			continue;
		}
		auto eq = t.find('=');
		if (eq == std::string::npos) continue;
		std::string key = lower(trim(t.substr(0, eq)));
		std::string val = trim(t.substr(eq + 1));
		// Strip inline comments
		auto hash = val.find('#');
		if (hash != std::string::npos) val = trim(val.substr(0, hash));
		// Strip surrounding quotes
		if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
			val = val.substr(1, val.size() - 2);
		sections[current][key] = val;
	}
	return sections;
}

} // namespace detail

// ---------------------------------------------------------------------------
// xsuite_parse — main parser entry point
// ---------------------------------------------------------------------------

inline XSuiteParseResult xsuite_parse(const std::string& path) {
	XSuiteParseResult res;

	if (!std::filesystem::exists(path)) {
		res.error = "file not found: " + path;
		return res;
	}

	std::ifstream f(path);
	if (!f) {
		res.error = "cannot open: " + path;
		return res;
	}

	auto sections = detail::parse_ini(f);
	XSuiteFile& s = res.suite;

	// [xsuite] — required
	if (sections.find("xsuite") == sections.end()) {
		res.error = "missing required section [xsuite]";
		return res;
	}
	{
		auto& sec = sections["xsuite"];
		if (sec.count("name"))       s.name       = sec["name"];
		if (sec.count("version"))    s.version    = sec["version"];
		if (sec.count("mode"))       s.mode       = sec["mode"];
		if (sec.count("created_by")) s.created_by = sec["created_by"];
	}

	// [run] — required
	if (sections.find("run") == sections.end()) {
		res.error = "missing required section [run]";
		return res;
	}
	{
		auto& sec = sections["run"];
		if (sec.count("entry"))      s.entry_script = sec["entry"];
		if (sec.count("mode"))       s.run_mode     = sec["mode"];
		if (sec.count("num_steps"))  s.num_steps    = std::stoi(sec["num_steps"]);
		if (sec.count("dt"))         s.dt           = std::stod(sec["dt"]);
	}

	if (s.entry_script.empty()) {
		res.error = "run.entry is required but missing";
		return res;
	}

	// [files] — required
	if (sections.find("files") == sections.end()) {
		res.error = "missing required section [files]";
		return res;
	}
	{
		auto& sec = sections["files"];
		if (sec.count("script"))     s.entry_script = sec["script"]; // override if set
		if (sec.count("snapshot"))   s.xyz_path     = sec["snapshot"];
		if (sec.count("xyza"))       s.xyza_path    = sec["xyza"];
		if (sec.count("checkpoint")) s.xyzc_path    = sec["checkpoint"];
		if (sec.count("trajectory")) s.xyzf_path    = sec["trajectory"];
		if (sec.count("rich"))       s.xyzfull_path = sec["rich"];
	}

	// [build] — optional
	if (sections.count("build")) {
		auto& sec = sections["build"];
		if (sec.count("enabled"))  s.compile      = detail::parse_bool(sec["enabled"]);
		if (sec.count("compiler")) s.compiler     = sec["compiler"];
		if (sec.count("target"))   s.target       = sec["target"];
		if (sec.count("config"))   s.build_config = sec["config"];
	}

	// [hash] — optional; unknown fields silently ignored
	if (sections.count("hash")) {
		auto& sec = sections["hash"];
		if (sec.count("script_hash"))     s.script_hash     = sec["script_hash"];
		if (sec.count("snapshot_hash"))   s.snapshot_hash   = sec["snapshot_hash"];
		if (sec.count("checkpoint_hash")) s.checkpoint_hash = sec["checkpoint_hash"];
		if (sec.count("trajectory_hash")) s.trajectory_hash = sec["trajectory_hash"];
		if (sec.count("suite_hash"))      s.suite_hash      = sec["suite_hash"];
	}

	// [actions] — optional
	if (sections.count("actions")) {
		auto& sec = sections["actions"];
		if (sec.count("compile")) s.compile        = detail::parse_bool(sec["compile"]);
		if (sec.count("run"))     s.run            = detail::parse_bool(sec["run"]);
		if (sec.count("validate"))s.validate       = detail::parse_bool(sec["validate"]);
		if (sec.count("replay"))  s.replay         = detail::parse_bool(sec["replay"]);
		if (sec.count("export"))  s.export_outputs = detail::parse_bool(sec["export"]);
	}

	// [outputs] — optional
	if (sections.count("outputs")) {
		auto& sec = sections["outputs"];
		if (sec.count("report")) s.report_path = sec["report"];
		if (sec.count("json"))   s.json_path   = sec["json"];
		if (sec.count("log"))    s.log_path    = sec["log"];
	}

	// -----------------------------------------------------------------------
	// WO-XSUITE-02B  —  new optional sections
	// -----------------------------------------------------------------------

	// [live]
	if (sections.count("live")) {
		auto& sec = sections["live"];
		if (sec.count("enabled"))             s.live_enabled              = detail::parse_bool(sec["enabled"]);
		if (sec.count("persistent_instance")) s.live_persistent_instance  = detail::parse_bool(sec["persistent_instance"]);
		if (sec.count("state_flush_interval"))  s.live_state_flush_interval  = std::stoi(sec["state_flush_interval"]);
		if (sec.count("health_flush_interval")) s.live_health_flush_interval = std::stoi(sec["health_flush_interval"]);
	}

	// [render]
	if (sections.count("render")) {
		auto& sec = sections["render"];
		if (sec.count("enabled"))         s.render_enabled        = detail::parse_bool(sec["enabled"]);
		if (sec.count("atomic_stream"))   s.render_atomic_stream  = sec["atomic_stream"];
		if (sec.count("analysis_stream")) s.render_analysis_stream= sec["analysis_stream"];
		if (sec.count("fps_atomic"))      s.render_fps_atomic     = std::stoi(sec["fps_atomic"]);
		if (sec.count("fps_analysis"))    s.render_fps_analysis   = std::stoi(sec["fps_analysis"]);
	}

	// [checkpoint]
	if (sections.count("checkpoint")) {
		auto& sec = sections["checkpoint"];
		if (sec.count("enabled"))    s.checkpoint_enabled    = detail::parse_bool(sec["enabled"]);
		if (sec.count("format"))     s.checkpoint_format     = sec["format"];
		if (sec.count("directory"))  s.checkpoint_directory  = sec["directory"];
		if (sec.count("interval"))   s.checkpoint_interval   = std::stoi(sec["interval"]);
		if (sec.count("keep_last"))  s.checkpoint_keep_last  = std::stoi(sec["keep_last"]);
		if (sec.count("write_hash")) s.checkpoint_write_hash = detail::parse_bool(sec["write_hash"]);
	}

	// [presolve]
	if (sections.count("presolve")) {
		auto& sec = sections["presolve"];
		if (sec.count("enabled"))             s.presolve_enabled        = detail::parse_bool(sec["enabled"]);
		if (sec.count("mode"))                s.presolve_mode           = sec["mode"];
		if (sec.count("batch_count"))         s.presolve_batch_count    = std::stoi(sec["batch_count"]);
		if (sec.count("seed_start"))          s.presolve_seed_start     = std::stoi(sec["seed_start"]);
		if (sec.count("seed_stride"))         s.presolve_seed_stride    = std::stoi(sec["seed_stride"]);
		if (sec.count("max_presolve_steps"))  s.presolve_max_steps      = std::stoi(sec["max_presolve_steps"]);
		if (sec.count("extract_matrix"))      s.presolve_extract_matrix = detail::parse_bool(sec["extract_matrix"]);
		if (sec.count("extract_eigen_basis")) s.presolve_extract_eigen  = detail::parse_bool(sec["extract_eigen_basis"]);
		if (sec.count("write_basis"))         s.presolve_write_basis    = sec["write_basis"];
		if (sec.count("write_summary"))       s.presolve_write_summary  = sec["write_summary"];
	}

	// [eigenmine]
	if (sections.count("eigenmine")) {
		auto& sec = sections["eigenmine"];
		if (sec.count("enabled"))               s.eigenmine_enabled          = detail::parse_bool(sec["enabled"]);
		if (sec.count("target_solves"))         s.eigenmine_target_solves    = std::stoll(sec["target_solves"]);
		if (sec.count("batch_count"))           s.eigenmine_batch_count      = std::stoi(sec["batch_count"]);
		if (sec.count("seeds_per_batch"))       s.eigenmine_seeds_per_batch  = std::stoi(sec["seeds_per_batch"]);
		if (sec.count("systems_per_seed"))      s.eigenmine_systems_per_seed = std::stoi(sec["systems_per_seed"]);
		if (sec.count("matrix_source"))         s.eigenmine_matrix_source    = sec["matrix_source"];
		if (sec.count("mode_rank_limit"))       s.eigenmine_mode_rank_limit  = std::stoi(sec["mode_rank_limit"]);
		if (sec.count("min_recurrence"))        s.eigenmine_min_recurrence   = std::stod(sec["min_recurrence"]);
		if (sec.count("max_reconstruction_error")) s.eigenmine_max_recon_error = std::stod(sec["max_reconstruction_error"]);
		if (sec.count("write_modes"))           s.eigenmine_write_modes      = sec["write_modes"];
		if (sec.count("write_basis"))           s.eigenmine_write_basis      = sec["write_basis"];
		if (sec.count("write_summary"))         s.eigenmine_write_summary    = sec["write_summary"];
	}

	// [curvefit]
	if (sections.count("curvefit")) {
		auto& sec = sections["curvefit"];
		if (sec.count("enabled"))            s.curvefit_enabled         = detail::parse_bool(sec["enabled"]);
		if (sec.count("source"))             s.curvefit_source          = sec["source"];
		if (sec.count("target"))             s.curvefit_target          = sec["target"];
		if (sec.count("model"))              s.curvefit_model           = sec["model"];
		if (sec.count("max_order"))          s.curvefit_max_order       = std::stoi(sec["max_order"]);
		if (sec.count("regularization"))     s.curvefit_regularization  = std::stod(sec["regularization"]);
		if (sec.count("train_fraction"))     s.curvefit_train_fraction  = std::stod(sec["train_fraction"]);
		if (sec.count("validation_fraction"))s.curvefit_val_fraction    = std::stod(sec["validation_fraction"]);
		if (sec.count("write_model"))        s.curvefit_write_model     = sec["write_model"];
		if (sec.count("write_coefficients")) s.curvefit_write_coefficients = sec["write_coefficients"];
		if (sec.count("write_report"))       s.curvefit_write_report    = sec["write_report"];
	}

	// [release_gate]
	if (sections.count("release_gate")) {
		auto& sec = sections["release_gate"];
		if (sec.count("enabled"))               s.release_gate_enabled         = detail::parse_bool(sec["enabled"]);
		if (sec.count("baseline_version"))      s.release_gate_baseline        = sec["baseline_version"];
		if (sec.count("candidate_version"))     s.release_gate_candidate       = sec["candidate_version"];
		if (sec.count("require_hash_success"))  s.release_gate_require_hash    = std::stod(sec["require_hash_success"]);
		if (sec.count("require_curvefit_score"))s.release_gate_require_curvefit= std::stod(sec["require_curvefit_score"]);
		if (sec.count("require_eigen_quality")) s.release_gate_require_eigen   = std::stod(sec["require_eigen_quality"]);
		if (sec.count("max_failure_rate"))      s.release_gate_max_failure     = std::stod(sec["max_failure_rate"]);
		if (sec.count("write_gate_report"))     s.release_gate_write_report    = sec["write_gate_report"];
		if (sec.count("write_comparison"))      s.release_gate_write_comparison= sec["write_comparison"];
	}

	// Warnings for missing optional files
	auto warn_missing = [&](const std::string& label, const std::string& p) {
		if (p.empty()) {
			res.warnings.push_back(label + " not specified");
		} else if (!std::filesystem::exists(p)) {
			res.warnings.push_back(label + " not found: " + p);
		}
	};
	warn_missing("trajectory (.xyzf)", s.xyzf_path);
	warn_missing("checkpoint (.xyzc)", s.xyzc_path);
	warn_missing("rich replay (.xyzFull)", s.xyzfull_path);

	res.ok = true;
	return res;
}

// ---------------------------------------------------------------------------
// xsuite_inspect — print formatted suite summary to stdout
// ---------------------------------------------------------------------------

inline void xsuite_inspect(const XSuiteFile& s, std::ostream& out = std::cout) {
	auto field = [&](const char* lbl, const std::string& v) {
		if (!v.empty()) out << "  " << lbl << ": " << v << "\n";
	};
	auto flag = [&](const char* lbl, bool v) {
		out << "  " << lbl << ": " << (v ? "yes" : "no") << "\n";
	};

	out << "\n=== .X Suite: " << s.name << " ===\n";
	out << "[xsuite]\n";
	field("version",    s.version);
	field("mode",       s.mode);
	field("created_by", s.created_by);

	out << "[run]\n";
	field("entry",     s.entry_script);
	field("run_mode",  s.run_mode);
	out << "  num_steps: " << s.num_steps << "\n";
	out << "  dt: " << s.dt << "\n";

	out << "[files]\n";
	auto file_status = [&](const char* lbl, const std::string& p) {
		if (p.empty()) {
			out << "  " << lbl << ": (not set)\n";
		} else {
			bool exists = std::filesystem::exists(p);
			out << "  " << lbl << ": " << p << (exists ? " [OK]" : " [MISSING]") << "\n";
		}
	};
	file_status("script",      s.entry_script);
	file_status("snapshot",    s.xyz_path);
	file_status("xyza",        s.xyza_path);
	file_status("checkpoint",  s.xyzc_path);
	file_status("trajectory",  s.xyzf_path);
	file_status("rich",        s.xyzfull_path);

	out << "[build]\n";
	flag("compile", s.compile);
	field("compiler", s.compiler);
	field("target",   s.target);
	field("config",   s.build_config);

	out << "[actions]\n";
	flag("run",     s.run);
	flag("validate",s.validate);
	flag("replay",  s.replay);
	flag("export",  s.export_outputs);

	if (!s.report_path.empty() || !s.json_path.empty() || !s.log_path.empty()) {
		out << "[outputs]\n";
		field("report", s.report_path);
		field("json",   s.json_path);
		field("log",    s.log_path);
	}

	if (!s.suite_hash.empty()) {
		out << "[hash]\n";
		field("suite_hash", s.suite_hash);
	}
	out << "\n";
}

// ---------------------------------------------------------------------------
// xsuite_validate — verify file presence and optional hash contracts
// Returns number of validation failures (0 = pass)
// ---------------------------------------------------------------------------

inline int xsuite_validate(const XSuiteFile& s, std::ostream& out = std::cout) {
	int failures = 0;

	auto check = [&](const char* lbl, const std::string& p, bool required) {
		if (p.empty()) {
			if (required) {
				out << "  [FAIL] " << lbl << ": not specified\n";
				++failures;
			}
			return;
		}
		if (!std::filesystem::exists(p)) {
			out << "  [FAIL] " << lbl << ": not found — " << p << "\n";
			if (required) ++failures;
		} else {
			out << "  [OK]   " << lbl << ": " << p << "\n";
		}
	};

	out << "Validating suite: " << s.name << "\n";
	check("entry script",      s.entry_script, true);
	check("snapshot (.xyz)",   s.xyz_path,     false);
	check("checkpoint (.xyzc)",s.xyzc_path,    false);
	check("trajectory (.xyzf)",s.xyzf_path,    false);
	check("rich (.xyzFull)",   s.xyzfull_path, false);

	if (failures == 0) {
		out << "  Validation PASSED\n";
	} else {
		out << "  Validation FAILED (" << failures << " error(s))\n";
	}
	return failures;
}

} // namespace xsuite
} // namespace vsepr
