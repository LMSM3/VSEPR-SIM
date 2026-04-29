/**
 * vsim_parser.cpp — .vsim file parser implementation
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include "vsim/vsim_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vsim {

// ============================================================================
// Public factory methods
// ============================================================================

VsimDocument VsimParser::parse_file(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open())
		throw ParseError(0, "cannot open file: " + path);

	std::string content((std::istreambuf_iterator<char>(f)),
						 std::istreambuf_iterator<char>());
	return parse_string(content, path);
}

VsimDocument VsimParser::parse_string(const std::string& content,
									  const std::string& source_path) {
	VsimParser p;
	p.doc_.source_path = source_path;
	p.doc_.exports.write_xyz = true;  // default on
	p.parse_content(content);
	return p.doc_;
}

// ============================================================================
// Core parse loop
// ============================================================================

void VsimParser::parse_content(const std::string& content) {
	std::istringstream stream(content);
	std::string line;
	int line_no = 0;

	while (std::getline(stream, line)) {
		++line_no;
		std::string clean = strip_comment(line);
		clean = trim(clean);
		if (clean.empty()) continue;

		// Section header: [name] or [[name]]
		if (clean.front() == '[') {
			// Strip outer brackets (handle [[molecule]] as [simulation.molecule])
			bool double_bracket = (clean.size() >= 2 && clean[1] == '[');
			size_t start = double_bracket ? 2 : 1;
			size_t end   = clean.rfind(double_bracket ? "]]" : "]");
			if (end == std::string::npos)
				throw ParseError(line_no, "unclosed section bracket");
			std::string sec = trim(clean.substr(start, end - start));
			handle_section(sec, line_no);
			continue;
		}

		// Key = value
		size_t eq = clean.find('=');
		if (eq == std::string::npos)
			throw ParseError(line_no, "expected '=' in: " + clean);

		std::string key = trim(clean.substr(0, eq));
		std::string raw = trim(clean.substr(eq + 1));

		if (key.empty())
			throw ParseError(line_no, "empty key before '='");

		handle_key_value(key, raw, line_no);
	}
}

// ============================================================================
// Section dispatch
// ============================================================================

void VsimParser::handle_section(const std::string& sec, int /*line_no*/) {
	current_section_ = sec;

	// Reset golden sub-block flags
	in_test_run_block_      = false;
	in_test_analysis_block_ = false;
	in_suite_limits_        = false;
	in_suite_smoke_         = false;

	in_molecule_block_ = (sec == "simulation.molecule" || sec == "molecule");
	if (in_molecule_block_) {
		doc_.simulation.molecules.emplace_back();
		return;
	}

	// [test.<name>] — bare test block; start a new entry
	if (sec.rfind("test.", 0) == 0) {
		std::string rest = sec.substr(5); // everything after "test."
		// [test.<name>.run] or [test.<name>.analysis]
		auto dot = rest.rfind('.');
		if (dot != std::string::npos) {
			std::string name = rest.substr(0, dot);
			std::string sub  = rest.substr(dot + 1);
			current_test_name_ = name;
			// Make sure the entry exists
			bool found = false;
			for (auto& e : doc_.golden_tests) if (e.name == name) { found = true; break; }
			if (!found) { GoldenTestEntry e; e.name = name; doc_.golden_tests.push_back(e); }
			in_test_run_block_      = (sub == "run");
			in_test_analysis_block_ = (sub == "analysis");
		} else {
			current_test_name_ = rest;
			bool found = false;
			for (auto& e : doc_.golden_tests) if (e.name == rest) { found = true; break; }
			if (!found) { GoldenTestEntry e; e.name = rest; doc_.golden_tests.push_back(e); }
		}
		return;
	}

	if (sec == "suite.limits") { in_suite_limits_ = true; return; }
	if (sec == "suite.smoke")  { in_suite_smoke_  = true; return; }

	// Normalise dotted sub-sections to their root for dispatch:
	// [simulation.molecule] -> in_molecule_block_ already set above
	// [kernel], [kernel.trace], [sweep], [runner], etc. -> raw_sections
}

// ============================================================================
// Key-value dispatch
// ============================================================================

void VsimParser::handle_key_value(const std::string& key,
								   const std::string& raw,
								   int line_no) {
	Value val = parse_value(raw, line_no);

	if (current_section_ == "project") {
		apply_project_key(key, val, line_no);
	} else if (current_section_ == "simulation") {
		apply_simulation_key(key, val, line_no);
	} else if (in_molecule_block_) {
		apply_molecule_key(key, val, line_no);
	} else if (current_section_ == "export") {
		apply_export_key(key, val, line_no);
	} else if (current_section_ == "export.visual") {
		apply_export_visual_key(key, val, line_no);
	} else if (current_section_ == "visual" || current_section_ == "visualization") {
		apply_visual_key(key, val, line_no);
	} else if (current_section_ == "visual.external" || current_section_ == "visual external") {
		apply_visual_external_key(key, val, line_no);
	} else if (current_section_ == "variance") {
		apply_variance_key(key, val, line_no);
	} else if (current_section_ == "N_evolution" || current_section_ == "n_evolution") {
		apply_n_evolution_key(key, val, line_no);
	} else if (current_section_ == "while") {
		apply_while_key(key, val, line_no);
	} else if (current_section_ == "batch") {
		apply_batch_key(key, val, line_no);
	} else if (current_section_ == "cell") {
		apply_cell_key(key, val, line_no);
	} else if (current_section_ == "boundary") {
		apply_boundary_key(key, val, line_no);
	} else if (current_section_ == "pbc") {
		apply_pbc_key(key, val, line_no);
	} else if (current_section_ == "defaults.run") {
		apply_defaults_run_key(key, val);
	} else if (current_section_ == "defaults.analysis") {
		apply_defaults_analysis_key(key, val);
	} else if (!current_test_name_.empty() && in_test_run_block_) {
		apply_test_run_key(key, val);
	} else if (!current_test_name_.empty() && in_test_analysis_block_) {
		apply_test_analysis_key(key, val);
	} else if (!current_test_name_.empty()) {
		apply_test_key(key, val);
	} else if (current_section_ == "suite" && !in_suite_limits_ && !in_suite_smoke_) {
		apply_suite_key(key, val);
	} else if (in_suite_limits_) {
		apply_suite_limits_key(key, val);
	} else if (in_suite_smoke_) {
		apply_suite_smoke_key(key, val);
	} else if (current_section_ == "report") {
		apply_report_key(key, val);
	} else {
		// Unknown section — store in raw_sections
		doc_.raw_sections[current_section_][key] = val;
	}
}

// ============================================================================
// Section-specific appliers
// ============================================================================

void VsimParser::apply_project_key(const std::string& key, const Value& val, int line_no) {
	if (key == "name") {
		doc_.project.name = value_is_string(val) ? as_string(val) : to_string(val);
	} else if (key == "version") {
		doc_.project.version = value_is_string(val) ? as_string(val) : to_string(val);
	} else if (key == "seed_base") {
		doc_.project.seed_base = value_is_int(val) ? static_cast<uint64_t>(as_int(val))
													: static_cast<uint64_t>(numeric(val));
	} else if (key == "determinism") {
		doc_.project.determinism = value_is_bool(val) ? as_bool(val) : true;
	} else if (key == "description") {
		doc_.project.description = value_is_string(val) ? as_string(val) : to_string(val);
	} else {
		doc_.raw_sections["project"][key] = val;
	}
}

void VsimParser::apply_simulation_key(const std::string& key, const Value& val, int line_no) {
	if (key == "fire_max_steps") {
		doc_.simulation.fire_max_steps = static_cast<int>(numeric(val));
	} else if (key == "fire_dt_fs") {
		doc_.simulation.fire_dt_fs = numeric(val);
	} else if (key == "box_size_ang") {
		doc_.simulation.box_size_ang = numeric(val);
	} else if (key == "periodic") {
		doc_.simulation.periodic = value_is_bool(val) ? as_bool(val) : false;
	} else if (key == "use_ewald") {
		doc_.simulation.use_ewald = value_is_bool(val) ? as_bool(val) : false;
	} else if (key == "ewald_alpha") {
		doc_.simulation.ewald_alpha = numeric(val);
	} else if (key == "ewald_rcut") {
		doc_.simulation.ewald_rcut = numeric(val);
	} else if (key == "ewald_kmax") {
		doc_.simulation.ewald_kmax = static_cast<int>(numeric(val));
	} else if (key == "formation_preset") {
		doc_.simulation.formation_preset = value_is_string(val) ? as_string(val) : to_string(val);
	} else if (key == "step_delay_ms") {
		doc_.simulation.step_delay_ms   = static_cast<int>(numeric(val));
	} else if (key == "resim_delay_ms") {
		doc_.simulation.resim_delay_ms  = static_cast<int>(numeric(val));
	} else if (key == "smooth_resim") {
		doc_.simulation.smooth_resim    = value_is_bool(val) ? as_bool(val) : true;
	} else {
		doc_.raw_sections["simulation"][key] = val;
	}
}

void VsimParser::apply_molecule_key(const std::string& key, const Value& val, int line_no) {
	if (doc_.simulation.molecules.empty())
		doc_.simulation.molecules.emplace_back();

	auto& m = doc_.simulation.molecules.back();

	if (key == "formula") {
		m.formula = value_is_string(val) ? as_string(val) : to_string(val);
	} else if (key == "count") {
		m.count = static_cast<int>(numeric(val));
	} else if (key == "temperature_K" || key == "temperature") {
		m.temperature_K = numeric(val);
	} else if (key == "lattice") {
		m.lattice = value_is_string(val) ? as_string(val) : to_string(val);
	} else if (key == "layer_mode") {
		m.layer_mode = value_is_string(val) ? as_string(val) : to_string(val);
	} else if (key == "n_layers") {
		m.n_layers = static_cast<int>(numeric(val));
	}
	// Unknown molecule keys silently ignored (forward-compatible)
}

void VsimParser::apply_export_key(const std::string& key, const Value& val, int line_no) {
	auto as_flag = [&]() -> bool {
		return value_is_bool(val) ? as_bool(val) : (to_string(val) == "true");
	};

	if      (key == "write_xyz")                  doc_.exports.write_xyz                 = as_flag();
	else if (key == "write_xyzf")                 doc_.exports.write_xyzf                = as_flag();
	else if (key == "write_xyzfull")              doc_.exports.write_xyzfull             = as_flag();
	else if (key == "write_pdb")                  doc_.exports.write_pdb                 = as_flag();
	else if (key == "write_analysis_json")        doc_.exports.write_analysis_json       = as_flag();
	else if (key == "write_metrics_tsv")          doc_.exports.write_metrics_tsv         = as_flag();
	else if (key == "write_cluster_json")         doc_.exports.write_cluster_json        = as_flag();
	else if (key == "write_fingerprint_json")     doc_.exports.write_fingerprint_json    = as_flag();
	else if (key == "write_events_json")          doc_.exports.write_events_json         = as_flag();
	else if (key == "write_symbolic_trace_json")  doc_.exports.write_symbolic_trace_json = as_flag();
	else if (key == "write_report_md")            doc_.exports.write_report_md           = as_flag();
	else if (key == "write_summary_csv")          doc_.exports.write_summary_csv         = as_flag();
	else if (key == "write_dashboard_json")       doc_.exports.write_dashboard_json      = as_flag();
	else if (key == "write_manifest_json")        doc_.exports.write_manifest_json       = as_flag();
	else if (key == "write_step_file")            doc_.exports.write_step_file           = as_flag();
	else if (key == "write_vtp_mesh")             doc_.exports.write_vtp_mesh            = as_flag();
	else if (key == "write_actual_hashes_tsv")    doc_.exports.write_actual_hashes_tsv   = as_flag();
	else if (key == "output_dir")                 doc_.exports.output_dir = value_is_string(val) ? as_string(val) : to_string(val);
	else doc_.raw_sections["export"][key] = val;
}

void VsimParser::apply_export_visual_key(const std::string& key, const Value& val, int /*line_no*/) {
	auto as_flag = [&]() -> bool {
		return value_is_bool(val) ? as_bool(val) : (to_string(val) == "true");
	};
	auto as_num = [&]() -> double { return numeric(val); };
	auto as_str = [&]() -> std::string {
		return value_is_string(val) ? as_string(val) : to_string(val);
	};

	if      (key == "write_svg_figures")          doc_.export_visual.write_svg_figures         = as_flag();
	else if (key == "write_png_snapshots")        doc_.export_visual.write_png_snapshots       = as_flag();
	else if (key == "write_rdf_svg")              doc_.export_visual.write_rdf_svg             = as_flag();
	else if (key == "write_energy_trace_svg")     doc_.export_visual.write_energy_trace_svg    = as_flag();
	else if (key == "write_packing_heatmap_svg")  doc_.export_visual.write_packing_heatmap_svg = as_flag();
	else if (key == "write_defect_map_svg")       doc_.export_visual.write_defect_map_svg      = as_flag();
	else if (key == "write_cluster_map_svg")      doc_.export_visual.write_cluster_map_svg     = as_flag();
	else if (key == "write_trajectory_gif")       doc_.export_visual.write_trajectory_gif      = as_flag();
	else if (key == "write_overlay_cycle_gif")    doc_.export_visual.write_overlay_cycle_gif   = as_flag();
	else if (key == "gif_frame_skip")             doc_.export_visual.gif_frame_skip            = static_cast<int>(as_num());
	else if (key == "gif_delay_cs")               doc_.export_visual.gif_delay_cs              = static_cast<int>(as_num());
	else if (key == "write_html_dashboard")       doc_.export_visual.write_html_dashboard      = as_flag();
	else if (key == "write_webgl_bundle")         doc_.export_visual.write_webgl_bundle        = as_flag();
	else if (key == "write_sse_descriptor")       doc_.export_visual.write_sse_descriptor      = as_flag();
	else if (key == "sse_port")                   doc_.export_visual.sse_port                  = static_cast<int>(as_num());
	else if (key == "write_report_pdf")           doc_.export_visual.write_report_pdf          = as_flag();
	else if (key == "write_report_html")          doc_.export_visual.write_report_html         = as_flag();
	else if (key == "visual_output_dir")          doc_.export_visual.visual_output_dir         = as_str();
	else doc_.raw_sections["export.visual"][key] = val;
}

void VsimParser::apply_visual_key(const std::string& key, const Value& val, int /*line_no*/) {
	auto as_flag = [&]() -> bool {
		return value_is_bool(val) ? as_bool(val) : (to_string(val) == "true");
	};
	auto as_str = [&]() -> std::string {
		return value_is_string(val) ? as_string(val) : to_string(val);
	};
	auto as_num = [&]() -> double { return numeric(val); };

	if      (key == "output_type")              doc_.visual.output_type              = as_str();
	else if (key == "animation_mode")           doc_.visual.animation_mode           = as_str();
	else if (key == "show_proxy_table")         doc_.visual.show_proxy_table         = as_flag();
	else if (key == "show_convergence_trace")   doc_.visual.show_convergence_trace   = as_flag();
	else if (key == "show_event_timeline")      doc_.visual.show_event_timeline      = as_flag();
	else if (key == "show_bar_chart")           doc_.visual.show_bar_chart           = as_flag();
	else if (key == "show_symbolic_trace")      doc_.visual.show_symbolic_trace      = as_flag();
	else if (key == "show_animation_cues")      doc_.visual.show_animation_cues      = as_flag();
	else if (key == "show_audit_table")         doc_.visual.show_audit_table         = as_flag();
	else if (key == "show_steady_state_marker") doc_.visual.show_steady_state_marker = as_flag();
	else if (key == "show_snapshot_chart")      doc_.visual.show_snapshot_chart      = as_flag();
	else if (key == "show_rdf_plot")            doc_.visual.show_rdf_plot            = as_flag();
	else if (key == "show_energy_heatmap")      doc_.visual.show_energy_heatmap      = as_flag();
	else if (key == "show_defect_map")          doc_.visual.show_defect_map          = as_flag();
	else if (key == "show_phase_field")         doc_.visual.show_phase_field         = as_flag();
	else if (key == "gl_show_axes")             doc_.visual.gl_show_axes             = as_flag();
	else if (key == "gl_show_neighbours")       doc_.visual.gl_show_neighbours       = as_flag();
	else if (key == "gl_overlay_hold_s")        doc_.visual.gl_overlay_hold_s        = static_cast<float>(as_num());
	else if (key == "gl_auto_orbit")            doc_.visual.gl_auto_orbit            = as_flag();
	else if (key == "gl_window_width")          doc_.visual.gl_window_width          = static_cast<int>(as_num());
	else if (key == "gl_window_height")         doc_.visual.gl_window_height         = static_cast<int>(as_num());
	else if (key == "web_port")                 doc_.visual.web_port                 = static_cast<int>(as_num());
	else if (key == "web_auto_open")            doc_.visual.web_auto_open            = as_flag();
	else if (key == "render_interval")          doc_.visual.render_interval          = static_cast<int>(as_num());
	else if (key == "overlay_sequence") {
		if (value_is_list(val)) {
			doc_.visual.overlay_sequence.clear();
			for (const auto& s : as_list(val))
				doc_.visual.overlay_sequence.push_back(s);
		} else {
			std::string raw = as_str();
			doc_.visual.overlay_sequence.clear();
			std::istringstream ss(raw);
			std::string tok;
			while (std::getline(ss, tok, ',')) {
				std::string t = trim(tok);
				if (!t.empty()) doc_.visual.overlay_sequence.push_back(t);
			}
		}
	}
	else doc_.raw_sections["visual"][key] = val;
}

// ============================================================================
// [visual.external] applier
// ============================================================================

void VsimParser::apply_visual_external_key(const std::string& key, const Value& val, int /*line_no*/) {
	auto as_flag = [&]() -> bool {
		return value_is_bool(val) ? as_bool(val) : (to_string(val) == "true");
	};
	auto as_str = [&]() -> std::string {
		return value_is_string(val) ? as_string(val) : to_string(val);
	};

	if      (key == "enabled")          doc_.visual_external.enabled          = as_flag();
	else if (key == "export_format")    doc_.visual_external.export_format    = as_str();
	else if (key == "export_frame_png") doc_.visual_external.export_frame_png = as_flag();
	else if (key == "export_trajectory")doc_.visual_external.export_trajectory= as_flag();
	else if (key == "show_progress")    doc_.visual_external.show_progress    = as_flag();
	else if (key == "render_interval")  doc_.visual_external.render_interval  = static_cast<int>(as_num());
	else if (key == "render") {
		// "render = state_current" or list form
		if (value_is_list(val)) {
			for (const auto& s : as_list(val))
				doc_.visual_external.render_targets.push_back(s);
		} else {
			doc_.visual_external.render_targets.push_back(as_str());
		}
	}
	else doc_.raw_sections["visual.external"][key] = val;
}

// ============================================================================
// [variance] applier
// Syntax: name = field window [threshold]
// e.g.:   energy_var = "energy.total" "last 50" 0.01
// Or flat keys: field, window, threshold, name
// ============================================================================

void VsimParser::apply_variance_key(const std::string& key, const Value& val, int /*line_no*/) {
	auto as_str = [&]() -> std::string {
		return value_is_string(val) ? as_string(val) : to_string(val);
	};

	// Each key is either a probe declaration (key=probe_name, val=field)
	// or a control flag.
	if (key == "print_results") {
		doc_.variance_cfg.print_results = value_is_bool(val) ? as_bool(val) : true;
		return;
	}

	// key is probe name; val is "field window [threshold]" or just field
	VarianceProbe p;
	p.name = key;

	std::string raw = as_str();
	// Try to split: first token = field, second = window words (may be multi-token)
	// Format: "energy.total last 50 0.01"
	std::istringstream ss(raw);
	std::string tok;
	std::vector<std::string> tokens;
	while (ss >> tok) tokens.push_back(tok);

	if (!tokens.empty()) p.field = tokens[0];

	// Reassemble window: tokens[1..] until a numeric token that looks like a threshold
	std::string window_acc;
	size_t i = 1;
	for (; i < tokens.size(); ++i) {
		bool is_num = false;
		try { std::stod(tokens[i]); is_num = true; } catch (...) {}
		if (is_num) { p.threshold = std::stod(tokens[i]); break; }
		if (!window_acc.empty()) window_acc += " ";
		window_acc += tokens[i];
	}
	if (window_acc.empty()) window_acc = "all";
	p.window = window_acc;

	doc_.variance_cfg.probes.push_back(std::move(p));
}

// ============================================================================
// [N_evolution] applier
// Same flat pattern as variance.
// ============================================================================

void VsimParser::apply_n_evolution_key(const std::string& key, const Value& val, int /*line_no*/) {
	auto as_str = [&]() -> std::string {
		return value_is_string(val) ? as_string(val) : to_string(val);
	};

	if (key == "print_results") {
		doc_.n_evolution_cfg.print_results = value_is_bool(val) ? as_bool(val) : true;
		return;
	}

	NEvolutionProbe p;
	p.name = key;

	std::string raw = as_str();
	std::istringstream ss(raw);
	std::string tok;
	std::vector<std::string> tokens;
	while (ss >> tok) tokens.push_back(tok);

	if (!tokens.empty()) p.target = tokens[0];
	std::string window_acc;
	for (size_t i = 1; i < tokens.size(); ++i) {
		bool is_num = false;
		try { std::stod(tokens[i]); is_num = true; } catch (...) {}
		if (is_num) { p.threshold = std::stod(tokens[i]); break; }
		if (!window_acc.empty()) window_acc += " ";
		window_acc += tokens[i];
	}
	if (window_acc.empty()) window_acc = "all";
	p.window = window_acc;

	doc_.n_evolution_cfg.probes.push_back(std::move(p));
}

// ============================================================================
// [while] applier
// Each key = guard name; val = "condition | body_steps | max_iters"
// Or flat: name, condition, body_steps, max_iters, iter_delay_ms
// ============================================================================

void VsimParser::apply_while_key(const std::string& key, const Value& val, int /*line_no*/) {
	auto as_str = [&]() -> std::string {
		return value_is_string(val) ? as_string(val) : to_string(val);
	};
	auto as_num = [&]() -> double { return numeric(val); };

	// Flat control keys
	if (key == "body_steps" && !doc_.while_cfg.guards.empty()) {
		doc_.while_cfg.guards.back().body_steps = static_cast<int>(as_num());
		return;
	}
	if (key == "max_iters" && !doc_.while_cfg.guards.empty()) {
		doc_.while_cfg.guards.back().max_iters = static_cast<int>(as_num());
		return;
	}
	if (key == "iter_delay_ms" && !doc_.while_cfg.guards.empty()) {
		doc_.while_cfg.guards.back().iter_delay_ms = static_cast<int>(as_num());
		return;
	}
	if (key == "measure" && !doc_.while_cfg.guards.empty()) {
		if (value_is_list(val)) {
			for (const auto& s : as_list(val))
				doc_.while_cfg.guards.back().measure.push_back(s);
		} else {
			doc_.while_cfg.guards.back().measure.push_back(as_str());
		}
		return;
	}

	// New guard: key = guard name, val = condition string
	WhileGuard g;
	g.name = key;
	g.condition = as_str();
	doc_.while_cfg.guards.push_back(std::move(g));
}

// ============================================================================
// [batch] applier
// ============================================================================

void VsimParser::apply_batch_key(const std::string& key, const Value& val, int /*line_no*/) {
	auto as_str = [&]() -> std::string {
		return value_is_string(val) ? as_string(val) : to_string(val);
	};
	auto as_num = [&]() -> double { return numeric(val); };
	auto as_flag = [&]() -> bool {
		return value_is_bool(val) ? as_bool(val) : (to_string(val) == "true");
	};

	// Top-level batch flags
	if      (key == "print_plan")    { doc_.batch_cfg.print_plan    = as_flag(); return; }
	else if (key == "abort_on_fail") { doc_.batch_cfg.abort_on_fail = as_flag(); return; }

	// Per-job keys (after a job is declared with: job = "name")
	if (key == "job") {
		BatchJob j;
		j.name = as_str();
		doc_.batch_cfg.jobs.push_back(std::move(j));
		return;
	}
	if (doc_.batch_cfg.jobs.empty()) {
		doc_.raw_sections["batch"][key] = val;
		return;
	}
	auto& cur = doc_.batch_cfg.jobs.back();

	if      (key == "seed_count")    { cur.seed_count  = static_cast<int>(as_num());  return; }
	else if (key == "export_each")   { cur.export_each = as_flag();                   return; }
	else if (key == "aggregate")     { cur.aggregate   = as_flag();                   return; }
	else if (key == "run") {
		// shorthand: run = <scenario_name>  →  per_run_actions entry
		cur.per_run_actions.push_back("run " + as_str());
	}
	else if (key == "analyze") {
		cur.per_run_actions.push_back("analyze " + as_str());
	}
	else if (key == "export") {
		cur.per_run_actions.push_back("export " + as_str());
	}
	else if (key == "measure") {
		cur.per_run_actions.push_back("measure " + as_str());
	}
	else {
		// Sweep param: key = param name, val = list or space-separated values
		std::vector<std::string> vals;
		if (value_is_list(val)) {
			vals = as_list(val);
		} else {
			std::istringstream ss(as_str());
			std::string tok;
			while (ss >> tok) vals.push_back(tok);
		}
		cur.sweep_params[key] = vals;
	}
}

// ============================================================================
// Golden suite section appliers
// ============================================================================

// Helper: apply a key/value to a GoldenTestRunConfig
void VsimParser::apply_golden_run_config(GoldenTestRunConfig& cfg,
										 const std::string& key,
										 const Value& val) {
	auto as_str  = [&]() -> std::string { return value_is_string(val) ? as_string(val) : to_string(val); };

	if      (key == "mode")                    cfg.mode                    = as_str();
	else if (key == "temperature")             cfg.temperature             = static_cast<int>(numeric(val));
	else if (key == "relax_steps")             cfg.relax_steps             = static_cast<int>(numeric(val));
	else if (key == "canonical_target")        cfg.canonical_target        = as_str();
	else if (key == "spin_initialisation")     cfg.spin_initialisation     = as_str();
	else if (key == "cluster_cutoff_angstrom") cfg.cluster_cutoff_angstrom = numeric(val);
	else if (key == "seed")                    cfg.seed                    = static_cast<uint64_t>(numeric(val));
}

// Helper: apply a key/value to a GoldenTestAnalysis
void VsimParser::apply_golden_analysis(GoldenTestAnalysis& ana,
									   const std::string& key,
									   const Value& val) {
	bool flag = value_is_bool(val) ? as_bool(val) : false;

	if      (key == "coordination_number")   ana.coordination_number   = flag;
	else if (key == "packing_fraction")      ana.packing_fraction      = flag;
	else if (key == "lattice_rmsd")          ana.lattice_rmsd          = flag;
	else if (key == "canonical_hash")        ana.canonical_hash        = flag;
	else if (key == "bond_angle")            ana.bond_angle            = flag;
	else if (key == "dipole_moment")         ana.dipole_moment         = flag;
	else if (key == "strain_tensor_proxy")   ana.strain_tensor_proxy   = flag;
	else if (key == "pore_volume")           ana.pore_volume           = flag;
	else if (key == "linker_angle_dist")     ana.linker_angle_dist     = flag;
	else if (key == "linking_number")        ana.linking_number        = flag;
	else if (key == "writhe")                ana.writhe                = flag;
	else if (key == "pair_distribution")     ana.pair_distribution     = flag;
	else if (key == "fivefold_symmetry")     ana.fivefold_symmetry     = flag;
	else if (key == "phason_strain")         ana.phason_strain         = flag;
	else if (key == "tile_frequency_ratio")  ana.tile_frequency_ratio  = flag;
	else if (key == "local_isomorphism")     ana.local_isomorphism     = flag;
	else if (key == "frustration_index")     ana.frustration_index     = flag;
	else if (key == "spin_structure_factor") ana.spin_structure_factor = flag;
	else if (key == "static_susceptibility") ana.static_susceptibility = flag;
}

void VsimParser::apply_defaults_run_key(const std::string& key, const Value& val) {
	apply_golden_run_config(doc_.golden_defaults.run, key, val);
}

void VsimParser::apply_defaults_analysis_key(const std::string& key, const Value& val) {
	apply_golden_analysis(doc_.golden_defaults.analysis, key, val);
}

void VsimParser::apply_test_key(const std::string& key, const Value& val) {
	auto& e = doc_.golden_tests.back(); // guaranteed to exist by handle_section
	auto as_str  = [&]() -> std::string { return value_is_string(val) ? as_string(val) : to_string(val); };

	if      (key == "group")            e.group            = as_str();
	else if (key == "type")             e.type             = as_str();
	else if (key == "formula")          e.formula          = as_str();
	else if (key == "geometry")         e.geometry         = as_str();
	else if (key == "structure")        e.structure        = as_str();
	else if (key == "central_atom")     e.central_atom     = as_str();
	else if (key == "ligands")          e.ligands          = as_str();
	else if (key == "lone_pairs")       e.lone_pairs       = static_cast<int>(numeric(val));
	else if (key == "basis")            e.basis            = as_str();
	else if (key == "supercell")        e.supercell        = as_str();
	else if (key == "topology")         e.topology         = as_str();
	else if (key == "symmetry")         e.symmetry         = as_str();
	else if (key == "periodicity")      e.periodicity      = as_str();
	else if (key == "projection_from")  e.projection_from  = as_str();
	else if (key == "tiling_type")      e.tiling_type      = as_str();
	else if (key == "patch_size")       e.patch_size       = as_str();
	else if (key == "inflation_ratio")  e.inflation_ratio  = as_str();
	else if (key == "long_range_order") e.long_range_order = value_is_bool(val) ? as_bool(val) : false;
	else if (key == "bulk")             e.bulk             = value_is_bool(val) ? as_bool(val) : true;
	else if (key == "node_species")     e.node_species     = as_str();
	else if (key == "linker")           e.linker           = as_str();
	else if (key == "linker_role")      e.linker_role      = as_str();
	else if (key == "coordination")     e.coordination_hint = as_str();
	else if (key == "vacancy_model")    e.vacancy_model    = as_str();
	else if (key == "magnetic_sites")   e.magnetic_sites   = as_str();
	else if (key == "spin")             e.spin             = as_str();
	else if (key == "exchange_model")   e.exchange_model   = as_str();
	else if (key == "frustration")      e.frustration      = as_str();
	else if (key == "frustration_origin") e.frustration_origin = as_str();
	else if (key == "ground_state")     e.ground_state     = as_str();
	else if (key == "strain_magnitude") e.strain_magnitude = numeric(val);
	else if (key == "strain_mode")      e.strain_mode      = as_str();
	else if (key == "strain_axis")      e.strain_axis      = as_str();
	else if (key == "component_count")  e.component_count  = static_cast<int>(numeric(val));
	else if (key == "component_type")   e.component_type   = as_str();
	else if (key == "link_invariant")   e.link_invariant   = as_str();
	else if (key == "pairwise_linked")  e.pairwise_linked  = value_is_bool(val) ? as_bool(val) : false;
	else if (key == "globally_linked")  e.globally_linked  = value_is_bool(val) ? as_bool(val) : false;
	else if (key == "expected_hash")    e.expected_hash    = as_str();
	else if (key == "skip_reason")      e.skip_reason      = as_str();
	else if (key == "skip_target")      e.skip_target      = as_str();
	else if (key == "skip_blocks_release") e.skip_blocks_release = value_is_bool(val) ? as_bool(val) : false;
}

void VsimParser::apply_test_run_key(const std::string& key, const Value& val) {
	apply_golden_run_config(doc_.golden_tests.back().run, key, val);
}

void VsimParser::apply_test_analysis_key(const std::string& key, const Value& val) {
	apply_golden_analysis(doc_.golden_tests.back().analysis, key, val);
}

void VsimParser::apply_suite_key(const std::string& key, const Value& val) {
	auto as_str  = [&]() -> std::string { return value_is_string(val) ? as_string(val) : to_string(val); };

	if (key == "enabled") {
		doc_.suite.enabled = value_is_bool(val) ? as_bool(val) : false;
	} else if (key == "tests") {
		// Comma-separated group selectors: "group:molecule, group:ionic_crystal, ..."
		std::string raw = as_str();
		std::istringstream ss(raw);
		std::string tok;
		doc_.suite.groups.clear();
		while (std::getline(ss, tok, ',')) {
			std::string t = trim(tok);
			// strip "group:" prefix to keep just the group name
			const std::string prefix = "group:";
			if (t.rfind(prefix, 0) == 0) t = t.substr(prefix.size());
			if (!t.empty()) doc_.suite.groups.push_back(t);
		}
	} else if (key == "run_modes") {
		std::string raw = as_str();
		std::istringstream ss(raw);
		std::string tok;
		doc_.suite.run_modes.clear();
		while (std::getline(ss, tok, ',')) {
			std::string t = trim(tok);
			if (!t.empty()) doc_.suite.run_modes.push_back(t);
		}
	} else if (key == "strict_order") {
		doc_.suite.strict_order = value_is_bool(val) ? as_bool(val) : true;
	}
}

void VsimParser::apply_suite_limits_key(const std::string& key, const Value& val) {
	if      (key == "max_runtime_per_test_seconds") doc_.suite.max_runtime_per_test_seconds = static_cast<int>(numeric(val));
	else if (key == "fail_fast")                    doc_.suite.fail_fast                    = value_is_bool(val) ? as_bool(val) : true;
	else if (key == "continue_on_warning")          doc_.suite.continue_on_warning          = value_is_bool(val) ? as_bool(val) : false;
}

void VsimParser::apply_suite_smoke_key(const std::string& key, const Value& val) {
	auto as_str = [&]() -> std::string { return value_is_string(val) ? as_string(val) : to_string(val); };

	if (key == "groups") {
		std::string raw = as_str();
		std::istringstream ss(raw);
		std::string tok;
		doc_.suite.smoke_groups.clear();
		while (std::getline(ss, tok, ',')) {
			std::string t = trim(tok);
			const std::string prefix = "group:";
			if (t.rfind(prefix, 0) == 0) t = t.substr(prefix.size());
			if (!t.empty()) doc_.suite.smoke_groups.push_back(t);
		}
	} else if (key == "purpose") {
		doc_.suite.smoke_purpose = as_str();
	}
}

void VsimParser::apply_report_key(const std::string& key, const Value& val) {
	auto as_str  = [&]() -> std::string { return value_is_string(val) ? as_string(val) : to_string(val); };

	if      (key == "title")                        doc_.golden_report.title                        = as_str();
	else if (key == "include_test_manifest")        doc_.golden_report.include_test_manifest        = value_is_bool(val) ? as_bool(val) : true;
	else if (key == "include_actual_hashes")        doc_.golden_report.include_actual_hashes        = value_is_bool(val) ? as_bool(val) : true;
	else if (key == "include_hash_mismatches")      doc_.golden_report.include_hash_mismatches      = value_is_bool(val) ? as_bool(val) : true;
	else if (key == "include_placeholder_failures") doc_.golden_report.include_placeholder_failures = value_is_bool(val) ? as_bool(val) : false;
	else if (key == "include_smoke_tests")          doc_.golden_report.include_smoke_tests          = value_is_bool(val) ? as_bool(val) : true;
}

// ============================================================================
// Value parser
// ============================================================================

Value VsimParser::parse_value(const std::string& raw, int line_no) {
	if (raw.empty())
		throw ParseError(line_no, "empty value");

	// Quoted string
	if (raw.front() == '"' && raw.back() == '"' && raw.size() >= 2) {
		return raw.substr(1, raw.size() - 2);
	}

	// Bool
	if (raw == "true")  return true;
	if (raw == "false") return false;

	// Bracketed list: [ a, b, c ]
	if (raw.front() == '[' && raw.back() == ']') {
		std::string inner = raw.substr(1, raw.size() - 2);
		std::vector<std::string> items;
		std::istringstream ss(inner);
		std::string tok;
		while (std::getline(ss, tok, ',')) {
			std::string t = trim(tok);
			if (t.size() >= 2 && t.front() == '"' && t.back() == '"')
				t = t.substr(1, t.size() - 2);
			if (!t.empty()) items.push_back(t);
		}
		return items;
	}

	// Unbracketed comma-separated tuple: 10.0, 12.0, 8.0
	// Detect before trying stod so the comma is not silently consumed.
	if (raw.find(',') != std::string::npos) {
		std::vector<std::string> items;
		std::istringstream ss(raw);
		std::string tok;
		while (std::getline(ss, tok, ',')) {
			std::string t = trim(tok);
			if (!t.empty()) items.push_back(t);
		}
		if (items.size() > 1)   // real tuple; single-element falls through to numeric
			return items;
	}

	// Integer vs float
	bool has_dot = raw.find('.') != std::string::npos;
	bool has_e   = raw.find('e') != std::string::npos || raw.find('E') != std::string::npos;
	try {
		if (has_dot || has_e) {
			return std::stod(raw);
		} else {
			return static_cast<int64_t>(std::stoll(raw));
		}
	} catch (...) {
		// Fallback: unquoted string
		return raw;
	}
}

// ============================================================================
// Helpers
// ============================================================================

std::string VsimParser::strip_comment(const std::string& line) {
	bool in_str = false;
	for (size_t i = 0; i < line.size(); ++i) {
		if (line[i] == '"') in_str = !in_str;
		if (!in_str && line[i] == '#') return line.substr(0, i);
	}
	return line;
}

std::string VsimParser::trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return {};
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

// ============================================================================
// WO-57B: [cell] applier
// ============================================================================

void VsimParser::apply_cell_key(const std::string& key, const Value& val, int line_no) {
	if (key == "type") {
		std::string t = value_is_string(val) ? as_string(val) : to_string(val);
		if (t == "triclinic") {
			throw ParseError(line_no,
				"[cell] type = triclinic is not supported in beta-8.\n"
				"              Only orthorhombic cells are implemented.\n"
				"              Triclinic support is reserved for a future release.");
		}
		if (t != "orthorhombic") {
			throw ParseError(line_no, "[cell] unknown type '" + t + "'; expected orthorhombic");
		}
		doc_.cell.type = t;
	} else if (key == "lengths") {
		// Accept "[Lx, Ly, Lz]" (list), "Lx, Ly, Lz" (unbracketed tuple), or scalar (cubic)
		if (value_is_list(val)) {
			// Parser already split it: bracketed form [a, b, c]
			const auto& lst = as_list(val);
			if (lst.size() != 3)
				throw ParseError(line_no, "[cell] lengths must have exactly 3 values");
			try {
				doc_.cell.lx = std::stod(lst[0]);
				doc_.cell.ly = std::stod(lst[1]);
				doc_.cell.lz = std::stod(lst[2]);
			} catch (...) {
				throw ParseError(line_no, "[cell] lengths: non-numeric value");
			}
		} else if (value_is_string(val)) {
			// Unquoted string that contains commas: "10.0, 12.0, 8.0"
			// parse_value returns a string when it can't parse as numeric due to commas
			const std::string& s = as_string(val);
			std::vector<double> parts;
			std::istringstream ss(s);
			std::string tok;
			while (std::getline(ss, tok, ',')) {
				std::string t2 = trim(tok);
				if (t2.empty()) continue;
				try { parts.push_back(std::stod(t2)); }
				catch (...) { throw ParseError(line_no, "[cell] lengths: non-numeric value '" + t2 + "'"); }
			}
			if (parts.size() == 1) {
				doc_.cell.lx = doc_.cell.ly = doc_.cell.lz = parts[0];
			} else if (parts.size() == 3) {
				doc_.cell.lx = parts[0]; doc_.cell.ly = parts[1]; doc_.cell.lz = parts[2];
			} else {
				throw ParseError(line_no, "[cell] lengths must be 1 or 3 values, got " + std::to_string(parts.size()));
			}
		} else {
			// Single numeric: square box shorthand
			double v = numeric(val);
			doc_.cell.lx = doc_.cell.ly = doc_.cell.lz = v;
		}
		// Validate
		if (doc_.cell.lx <= 0.0 || doc_.cell.ly <= 0.0 || doc_.cell.lz <= 0.0)
			throw ParseError(line_no, "[cell] all lengths must be > 0");
	} else if (key == "units") {
		std::string u = value_is_string(val) ? as_string(val) : to_string(val);
		if (u == "nm") {
			doc_.cell.units = u;
		} else if (u == "angstrom" || u == "angstroms" || u == "Angstrom" || u == "A") {
			doc_.cell.units = "angstrom";
		} else {
			throw ParseError(line_no, "[cell] unknown units '" + u + "'; expected angstrom");
		}
	} else {
		(void)line_no;
	}
}

// ============================================================================
// WO-57B: [boundary] applier
// ============================================================================

namespace {
	// Validate a boundary mode string and return canonical lowercase form.
	// Throws ParseError for reserved or unknown modes.
	std::string canonical_boundary_mode(const std::string& s, int line_no) {
		if (s == "periodic" || s == "Periodic") return "periodic";
		if (s == "open"     || s == "Open")     return "open";
		if (s == "reflective" || s == "Reflective")
			throw vsim::ParseError(line_no,
				"[boundary] mode = reflective is reserved in beta-8 and not yet implemented");
		if (s == "absorbing" || s == "Absorbing")
			throw vsim::ParseError(line_no,
				"[boundary] mode = absorbing is reserved in beta-8 and not yet implemented");
		throw vsim::ParseError(line_no,
			"[boundary] unknown mode '" + s + "'; expected: periodic | open");
	}
} // anon namespace

void VsimParser::apply_boundary_key(const std::string& key, const Value& val, int line_no) {
	std::string s = value_is_string(val) ? as_string(val) : to_string(val);

	if (key == "x") {
		doc_.boundary.x = canonical_boundary_mode(s, line_no);
	} else if (key == "y") {
		doc_.boundary.y = canonical_boundary_mode(s, line_no);
	} else if (key == "z") {
		doc_.boundary.z = canonical_boundary_mode(s, line_no);
	} else if (key == "mode") {
		// Compact form: mode = periodic  (sets all axes implied by "axes" key or all three)
		std::string mode = canonical_boundary_mode(s, line_no);
		// Store for later; if an "axes" key follows we apply selectively, else apply to all
		doc_.boundary.x = mode;
		doc_.boundary.y = mode;
		doc_.boundary.z = mode;
	} else if (key == "axes") {
		// Compact form: axes = x,y,z  or  axes = x,y  etc.
		// The previously-parsed "mode" is already in boundary.x/y/z.
		// This key optionally re-restricts which axes carry the compact mode.
		// For simplicity we treat it as documentation: if present, check it lists valid axes.
		std::string axes_str = s;
		// Minimal validation: only x, y, z characters allowed
		for (char c : axes_str) {
			if (c != 'x' && c != 'y' && c != 'z' && c != ',' && c != ' ')
				throw ParseError(line_no, "[boundary] axes contains invalid character '" + std::string(1,c) + "'");
		}
		// No structural change needed: "mode = periodic" + "axes = x,y,z" is identical to x=y=z=periodic
	} else {
		(void)line_no;
	}
}

// ============================================================================
// WO-57B: [pbc] applier
// ============================================================================

void VsimParser::apply_pbc_key(const std::string& key, const Value& val, int line_no) {
	if (key == "minimum_image") {
		if (!value_is_bool(val))
			throw ParseError(line_no, "[pbc] minimum_image must be true or false");
		doc_.pbc.minimum_image = as_bool(val);
	} else if (key == "wrap_positions") {
		std::string s = value_is_string(val) ? as_string(val) : to_string(val);
		doc_.pbc.wrap_positions_str = s;
		if      (s == "never")       doc_.pbc.wrap_positions = WrapMode::Never;
		else if (s == "after_step")  doc_.pbc.wrap_positions = WrapMode::AfterStep;
		else if (s == "after_force") doc_.pbc.wrap_positions = WrapMode::AfterForce;
		else if (s == "on_export")   doc_.pbc.wrap_positions = WrapMode::OnExport;
		else throw ParseError(line_no,
			"[pbc] wrap_positions: unknown value '" + s + "'; "
			"expected: never | after_step | after_force | on_export");
	} else if (key == "track_images") {
		if (!value_is_bool(val))
			throw ParseError(line_no, "[pbc] track_images must be true or false");
		doc_.pbc.track_images = as_bool(val);
	} else if (key == "unwrap_for_diffusion") {
		if (!value_is_bool(val))
			throw ParseError(line_no, "[pbc] unwrap_for_diffusion must be true or false");
		doc_.pbc.unwrap_for_diffusion = as_bool(val);
	} else {
		(void)line_no;
	}
}

} // namespace vsim
