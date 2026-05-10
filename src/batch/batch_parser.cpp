/**
 * src/batch/batch_parser.cpp
 * ============================
 * WO-VSIM-62C — Batch Layer Parser Implementation
 *
 * Parses study-file (.vsim with [study]) into BatchDocument.
 * Follows the same tokenisation conventions as vsim_parser.cpp.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_parser.hpp"
#include "include/vsim/vsim_parser.hpp"  // for inline_base parsing

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vsim {
namespace batch {

// ── public factory ────────────────────────────────────────────────────────────

BatchDocument BatchParser::parse_file(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open())
		throw std::runtime_error("BatchParser: cannot open file: " + path);
	std::string src((std::istreambuf_iterator<char>(f)),
					 std::istreambuf_iterator<char>());
	BatchDocument doc = parse_string(src, path);
	doc.source_path = path;
	return doc;
}

BatchDocument BatchParser::parse_string(const std::string& src,
										 const std::string& source_path) {
	BatchParser p;
	p.parse_content(src);
	p.flush_axis();
	p.flush_case();
	p.flush_lib_stage();

	// If base.mode = "inline", parse the same source as a VsimDocument
	// (the study-specific sections are ignored by VsimParser — it skips unknowns)
	if (p.doc_.base.populated && p.doc_.base.mode == "inline") {
		try {
			p.doc_.inline_base = VsimParser::parse_string(src, source_path);
		} catch (...) {
			// Inline base parse failure is surfaced by validate()
		}
	}
	return p.doc_;
}

// ── core parse loop ───────────────────────────────────────────────────────────

void BatchParser::parse_content(const std::string& src) {
	std::istringstream stream(src);
	std::string line;

	while (std::getline(stream, line)) {
		std::string clean = trim(strip_comment(line));
		if (clean.empty()) continue;

		if (clean.front() == '[') {
			bool dbl = (clean.size() >= 2 && clean[1] == '[');
			size_t start = dbl ? 2 : 1;
			size_t end   = clean.rfind(dbl ? "]]" : "]");
			if (end == std::string::npos) continue; // malformed — skip
			std::string sec = trim(clean.substr(start, end - start));
			in_double_bracket_ = dbl;
			handle_section(sec);
			continue;
		}

		size_t eq = clean.find('=');
		if (eq == std::string::npos) continue; // skip unparseable lines

		std::string key = trim(clean.substr(0, eq));
		std::string raw = trim(clean.substr(eq + 1));
		if (key.empty()) continue;
		handle_key_value(key, raw);
	}
}

// ── section handler ───────────────────────────────────────────────────────────

void BatchParser::handle_section(const std::string& sec) {
	// Flush open array entries before switching sections
	if (sec != current_section_) {
		flush_axis();
		flush_case();
		flush_lib_stage();
	}
	current_section_ = sec;

	if (sec == "study") return;
	if (sec == "batch.base")   return;
	if (sec == "batch.design") return;
	if (sec == "batch.expand") return;
	if (sec == "batch.verify_policy") return;
	if (sec == "batch.override.verify") return;
	if (sec == "batch.aggregate.verify") return;
	if (sec == "batch.aggregate.verify.gates") return;
	if (sec == "batch.score") return;
	if (sec == "batch.require") return;
	if (sec == "seed") return;

	// [[batch.axis]] — start a new axis entry
	if (sec == "batch.axis") {
		flush_axis();
		doc_.axes.emplace_back();
		current_axis_ = &doc_.axes.back();
		return;
	}

	// [[batch.case]] — start a new case entry
	if (sec == "batch.case") {
		flush_case();
		doc_.cases.emplace_back();
		current_case_ = &doc_.cases.back();
		return;
	}

	// [formation.library.<name>] and [[formation.library.<name>.stage]]
	if (sec.rfind("formation.library.", 0) == 0) {
		std::string rest = sec.substr(18); // after "formation.library."
		auto dot = rest.find('.');
		if (dot == std::string::npos) {
			// [formation.library.<name>]
			flush_lib_stage();
			std::string lib_name = rest;
			if (doc_.formation_library.find(lib_name) == doc_.formation_library.end()) {
				FormationLibraryEntry entry;
				entry.name = lib_name;
				doc_.formation_library[lib_name] = std::move(entry);
			}
			current_lib_ = &doc_.formation_library[lib_name];
			current_stage_ = nullptr;
		} else {
			// [[formation.library.<name>.stage]]
			std::string lib_name = rest.substr(0, dot);
			std::string tail     = rest.substr(dot + 1);
			if (tail == "stage") {
				flush_lib_stage();
				if (doc_.formation_library.find(lib_name) == doc_.formation_library.end()) {
					FormationLibraryEntry entry;
					entry.name = lib_name;
					doc_.formation_library[lib_name] = std::move(entry);
				}
				current_lib_ = &doc_.formation_library[lib_name];
				current_lib_->stages.emplace_back();
				current_stage_ = &current_lib_->stages.back();
			}
		}
		return;
	}

	// All other sections (material, run, environment, etc.) are passed through
	// so the inline_base VsimParser can read them.  We just track the section name.
}

// ── key-value handler ─────────────────────────────────────────────────────────

void BatchParser::handle_key_value(const std::string& key, const std::string& raw) {
	const std::string& s = current_section_;

	// [study]
	if (s == "study") {
		if (key == "name")    { doc_.study.name    = unquote(raw); doc_.study.populated = true; return; }
		if (key == "type")    { doc_.study.type    = unquote(raw); return; }
		if (key == "goal")    { doc_.study.goal    = unquote(raw); return; }
		if (key == "version") { doc_.study.version = unquote(raw); return; }
		return;
	}

	// [batch.base]
	if (s == "batch.base") {
		if (key == "mode")   { doc_.base.mode   = unquote(raw); doc_.base.populated = true; return; }
		if (key == "script") { doc_.base.script  = unquote(raw); return; }
		return;
	}

	// [batch.design]
	if (s == "batch.design") {
		doc_.design.populated = true;
		if (key == "type")                { doc_.design.type                = unquote(raw); return; }
		if (key == "replicates_per_case") { doc_.design.replicates_per_case = parse_int(raw); return; }
		if (key == "seed_policy")         { doc_.design.seed_policy         = unquote(raw); return; }
		if (key == "abort_on_fail")       { doc_.design.abort_on_fail       = parse_bool(raw); return; }
		if (key == "rank_by")             { doc_.design.rank_by             = unquote(raw); return; }
		if (key == "max_parallel")        { doc_.design.max_parallel        = parse_int(raw); return; }
		if (key == "checkpoint")          { doc_.design.checkpoint          = parse_bool(raw); return; }
		if (key == "n_samples")           { doc_.design.n_samples           = parse_int(raw); return; }
		return;
	}

	// [batch.expand]
	if (s == "batch.expand") {
		doc_.expand.populated = true;
		if (key == "cases") { doc_.expand.cases = parse_bool(raw); return; }
		if (key == "axes")  { doc_.expand.axes  = parse_array(raw); return; }
		return;
	}

	// [batch.verify_policy]
	if (s == "batch.verify_policy") {
		if (key == "on_run_fail")           { doc_.verify_policy.on_run_fail           = unquote(raw); return; }
		if (key == "on_check_fail")         { doc_.verify_policy.on_check_fail         = unquote(raw); return; }
		if (key == "save_resolved_scripts") { doc_.verify_policy.save_resolved_scripts = parse_bool(raw); return; }
		return;
	}

	// [batch.override.verify]
	if (s == "batch.override.verify") {
		if (key == "tolerance_A")           { doc_.override_verify.tolerance_A           = parse_double(raw); return; }
		if (key == "relative_tolerance")    { doc_.override_verify.relative_tolerance    = parse_double(raw); return; }
		if (key == "max_msd_A2")            { doc_.override_verify.max_msd_A2            = parse_double(raw); return; }
		if (key == "coordination_tolerance"){ doc_.override_verify.coordination_tolerance= parse_int(raw);    return; }
		return;
	}

	// [batch.aggregate.verify]
	if (s == "batch.aggregate.verify") {
		if (key == "enabled")         { doc_.aggregate_verify.enabled         = parse_bool(raw); return; }
		if (key == "group_by")        { doc_.aggregate_verify.group_by        = parse_array(raw); return; }
		if (key == "statistics")      { doc_.aggregate_verify.statistics      = parse_array(raw); return; }
		if (key == "emit_matrix")     { doc_.aggregate_verify.emit_matrix     = parse_bool(raw); return; }
		if (key == "emit_failure_modes"){ doc_.aggregate_verify.emit_failure_modes = parse_bool(raw); return; }
		return;
	}

	// [batch.aggregate.verify.gates]
	if (s == "batch.aggregate.verify.gates") {
		if (key == "min_overall_pass_rate")   { doc_.aggregate_verify.gates.min_overall_pass_rate   = parse_double(raw); return; }
		if (key == "min_mass_pass_rate")      { doc_.aggregate_verify.gates.min_mass_pass_rate      = parse_double(raw); return; }
		if (key == "min_structure_pass_rate") { doc_.aggregate_verify.gates.min_structure_pass_rate = parse_double(raw); return; }
		if (key == "min_rdf_pass_rate")       { doc_.aggregate_verify.gates.min_rdf_pass_rate       = parse_double(raw); return; }
		if (key == "min_msd_pass_rate")       { doc_.aggregate_verify.gates.min_msd_pass_rate       = parse_double(raw); return; }
		return;
	}

	// [batch.score]
	if (s == "batch.score") {
		if (key == "rank_by") { doc_.score.rank_by = unquote(raw); return; }
		return;
	}

	// [batch.require]
	if (s == "batch.require") {
		if (key == "fail_if_missing") { doc_.require.fail_if_missing = parse_bool(raw);    return; }
		if (key == "files")           { doc_.require.files           = parse_array(raw);   return; }
		if (key == "checks")          { doc_.require.checks          = parse_array(raw);   return; }
		return;
	}

	// [seed]
	if (s == "seed") {
		doc_.seed.populated = true;
		if (key == "foundation") { doc_.seed.foundation = parse_u64(raw); return; }
		if (key == "defect")     { doc_.seed.defect     = parse_u64(raw); return; }
		if (key == "formation")  { doc_.seed.formation  = parse_u64(raw); return; }
		if (key == "thermal")    { doc_.seed.thermal    = parse_u64(raw); return; }
		if (key == "placement")  { doc_.seed.placement  = parse_u64(raw); return; }
		return;
	}

	// [[batch.axis]]
	if (s == "batch.axis" && current_axis_) {
		if (key == "name")        { current_axis_->name        = unquote(raw); return; }
		if (key == "target")      { current_axis_->target      = unquote(raw); return; }
		if (key == "kind")        { current_axis_->kind        = unquote(raw); return; }
		if (key == "values")      { current_axis_->values      = parse_array(raw); return; }
		if (key == "units")       { current_axis_->units       = unquote(raw); return; }
		if (key == "seed_source") { current_axis_->seed_source = unquote(raw); return; }
		if (key == "distribution"){ current_axis_->distribution= unquote(raw); return; }
		if (key == "mean")        { current_axis_->mean        = parse_double(raw); return; }
		if (key == "std")         { current_axis_->std_dev     = parse_double(raw); return; }
		if (key == "min")         { current_axis_->dist_min    = parse_double(raw); return; }
		if (key == "max")         { current_axis_->dist_max    = parse_double(raw); return; }
		if (key == "n_samples")   { current_axis_->n_samples   = parse_int(raw);    return; }
		return;
	}

	// [[batch.case]]
	if (s == "batch.case" && current_case_) {
		if (key == "name") { current_case_->name = unquote(raw); return; }
		// Any key containing a '.' is treated as a dot-path override
		if (key.find('.') != std::string::npos) {
			current_case_->overrides[key] = unquote(raw);
			return;
		}
		return;
	}

	// [formation.library.<name>]
	if (s.rfind("formation.library.", 0) == 0 && current_lib_ && !current_stage_) {
		if (key == "type")         { current_lib_->type         = unquote(raw); return; }
		if (key == "time_units")   { current_lib_->time_units   = unquote(raw); return; }
		if (key == "pressure_GPa") { current_lib_->pressure_GPa = parse_double(raw); return; }
		return;
	}

	// [[formation.library.<name>.stage]]
	if (s.rfind("formation.library.", 0) == 0 && current_stage_) {
		if (key == "kind") {
			current_stage_->kind = formation_stage_kind_from_string(unquote(raw));
			return;
		}
		if (key == "temperature_K")      { current_stage_->temperature_K      = parse_double(raw); return; }
		if (key == "duration_ps")        { current_stage_->duration_ps        = parse_double(raw); return; }
		if (key == "pressure_GPa")       { current_stage_->pressure_GPa       = parse_double(raw); return; }
		if (key == "from_temperature_K") { current_stage_->from_temperature_K = parse_double(raw); return; }
		if (key == "to_temperature_K")   { current_stage_->to_temperature_K   = parse_double(raw); return; }
		if (key == "profile")            { current_stage_->profile            = unquote(raw);      return; }
		if (key == "from_pressure_GPa")  { current_stage_->from_pressure_GPa  = parse_double(raw); return; }
		if (key == "to_pressure_GPa")    { current_stage_->to_pressure_GPa    = parse_double(raw); return; }
		if (key == "max_steps")          { current_stage_->max_steps          = parse_int(raw);    return; }
		if (key == "converge")           { current_stage_->converge           = parse_bool(raw);   return; }
		if (key == "from_field_V_A")     { current_stage_->from_field_V_A     = parse_double(raw); return; }
		if (key == "to_field_V_A")       { current_stage_->to_field_V_A       = parse_double(raw); return; }
		if (key == "field_axis")         { current_stage_->field_axis         = unquote(raw);      return; }
		return;
	}

	// All other keys (material, run, etc.) are silently passed through for inline_base
}

// ── flush helpers ─────────────────────────────────────────────────────────────

void BatchParser::flush_axis() {
	current_axis_ = nullptr;
}

void BatchParser::flush_case() {
	current_case_ = nullptr;
}

void BatchParser::flush_lib_stage() {
	current_lib_   = nullptr;
	current_stage_ = nullptr;
}

// ── validate ──────────────────────────────────────────────────────────────────

std::vector<std::string> BatchParser::validate(const BatchDocument& doc) {
	std::vector<std::string> errors;

	auto err = [&](const std::string& msg){ errors.push_back(msg); };

	if (doc.study.name.empty())
		err("[study] name is required");

	if (!doc.base.populated || doc.base.mode.empty())
		err("[batch.base] mode is required");
	else if (doc.base.mode == "template" && doc.base.script.empty())
		err("[batch.base] script is required when mode = template");

	for (const auto& ax : doc.axes) {
		if (ax.name.empty())
			err("[[batch.axis]] name is required");
		if (ax.target.empty())
			err("[[batch.axis]] target is required");
		if (ax.kind == "static" && ax.values.empty())
			err("[[batch.axis]] values required when kind = static (axis: " + ax.name + ")");
		if (ax.kind == "stochastic" && ax.seed_source.empty())
			err("[[batch.axis]] seed_source required when kind = stochastic (axis: " + ax.name + ")");
		if (ax.kind == "formation" && ax.target != "formation.path")
			err("[[batch.axis]] formation axis target must be formation.path (axis: " + ax.name + ")");
		if (ax.kind == "formation") {
			for (const auto& v : ax.values) {
				if (doc.formation_library.find(v) == doc.formation_library.end())
					err("[[batch.axis]] formation path '" + v + "' not declared in [formation.library]");
			}
		}
	}

	for (const auto& c : doc.cases) {
		if (c.name.empty())
			err("[[batch.case]] name is required");
	}

	if (doc.aggregate_verify.enabled && doc.aggregate_verify.group_by.empty())
		err("[batch.aggregate.verify] group_by required when enabled");

	const std::string& dt = doc.design.type;
	if ((dt == "latin_hypercube" || dt == "random") && doc.design.n_samples == 0)
		err("[batch.design] n_samples required for design type '" + dt + "'");

	return errors;
}

// ── string helpers ────────────────────────────────────────────────────────────

std::string BatchParser::trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return {};
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

std::string BatchParser::strip_comment(const std::string& s) {
	bool in_str = false;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '"') in_str = !in_str;
		if (!in_str && s[i] == '#') return s.substr(0, i);
	}
	return s;
}

std::string BatchParser::unquote(const std::string& s) {
	if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
		return s.substr(1, s.size() - 2);
	return s;
}

bool BatchParser::parse_bool(const std::string& s) {
	return s == "true" || s == "1" || s == "yes";
}

double BatchParser::parse_double(const std::string& s) {
	try { return std::stod(s); } catch (...) { return 0.0; }
}

int BatchParser::parse_int(const std::string& s) {
	try { return std::stoi(s); } catch (...) { return 0; }
}

uint64_t BatchParser::parse_u64(const std::string& s) {
	try { return std::stoull(s); } catch (...) { return 0; }
}

std::vector<std::string> BatchParser::parse_array(const std::string& s) {
	// Accepts: [a, b, c]  or  [\"a\", \"b\"]  or  a, b, c
	std::string inner = trim(s);
	if (!inner.empty() && inner.front() == '[') {
		auto end = inner.rfind(']');
		inner = (end != std::string::npos) ? inner.substr(1, end - 1) : inner.substr(1);
	}

	std::vector<std::string> result;
	std::istringstream ss(inner);
	std::string token;
	while (std::getline(ss, token, ',')) {
		std::string t = trim(unquote(trim(token)));
		if (!t.empty()) result.push_back(t);
	}
	return result;
}

} // namespace batch
} // namespace vsim
