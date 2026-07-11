/**
 * src/batch/batch_merger.cpp
 * ============================
 * WO-VSIM-62C — Axis Application & Template Loading Implementation
 *
 * Dot-path table (spec §2.4):
 *   environment.*          → EnvironmentSection
 *   run.*                  → RunSection
 *   material.*             → MaterialSection
 *   simulation.*           → SimulationSection
 *   pbc.*                  → PBCSection
 *   cell.*                 → CellSection
 *   observe.*              → ObserveSection
 *   analysis.structure.*   → VsimStructureAnalysisSection
 *   analysis.sampling.*    → VsimSamplingSection
 *   analysis.scale_sampling.* → VsimScaleSamplingSection
 *   analysis.inference.*   → VsimAnalysisInferenceSection
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_merger.hpp"
#include "include/vsim/vsim_parser.hpp"

#include <algorithm>
#include <stdexcept>

namespace vsim {
namespace batch {

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string to_lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}

static bool parse_bool_val(const std::string& v) {
	auto lo = to_lower(v);
	return lo == "true" || lo == "1" || lo == "yes";
}

static double parse_double_val(const std::string& v) {
	try { return std::stod(v); } catch (...) { return 0.0; }
}

static int parse_int_val(const std::string& v) {
	try { return std::stoi(v); } catch (...) { return 0; }
}

// ── BatchMerger::load_template ────────────────────────────────────────────────

VsimDocument BatchMerger::load_template(const std::string&        path,
										 std::vector<std::string>& errors) {
	try {
		return VsimParser::parse_file(path);
	} catch (const std::exception& ex) {
		errors.push_back(std::string("[batch.base] template load failed: ") + ex.what());
		return VsimDocument{};
	}
}

// ── BatchMerger::set_path ─────────────────────────────────────────────────────

bool BatchMerger::set_path(VsimDocument&      doc,
							const std::string& path,
							const std::string& value)
{
	// Tokenise once: "environment.temperature" → ["environment","temperature"]
	auto split = [](const std::string& s) {
		std::vector<std::string> parts;
		std::string cur;
		for (char c : s) {
			if (c == '.') { if (!cur.empty()) { parts.push_back(cur); cur.clear(); } }
			else cur += c;
		}
		if (!cur.empty()) parts.push_back(cur);
		return parts;
	};

	auto parts = split(path);
	if (parts.size() < 2) return false;

	const std::string& ns  = parts[0];
	const std::string& key = parts.back();

	// ── environment.* ──────────────────────────────────────────────────────
	if (ns == "environment") {
		auto& e = doc.environment;
		if (key == "temperature" || key == "temperature_K") { e.temperature = parse_double_val(value); return true; }
		if (key == "pressure" || key == "pressure_GPa")     { e.pressure    = parse_double_val(value); return true; }
		if (key == "medium")    { e.medium    = value;                   return true; }
		if (key == "humidity")  { e.humidity  = parse_double_val(value); return true; }
		if (key == "periodic")  { e.periodic  = parse_bool_val(value);   return true; }
		return false;
	}

	// ── run.* ──────────────────────────────────────────────────────────────
	if (ns == "run") {
		auto& r = doc.run;
		if (key == "steps" || key == "max_steps") { r.max_steps     = parse_int_val(value);    return true; }
		if (key == "dt_fs")                       { r.dt_fs         = parse_double_val(value); return true; }
		if (key == "temperature_K")               { r.temperature_K = parse_double_val(value); return true; }
		if (key == "pressure_GPa")                { r.pressure_GPa  = parse_double_val(value); return true; }
		if (key == "mode")                        { r.mode          = value;                   return true; }
		if (key == "converge")                    { r.converge      = parse_bool_val(value);   return true; }
		if (key == "output_level")                { r.output_level  = value;                   return true; }
		return false;
	}

	// ── material.* ────────────────────────────────────────────────────────
	if (ns == "material") {
		auto& m = doc.material;
		if (key == "formula")       { m.formula     = value; return true; }
		if (key == "prototype")     { m.prototype   = value; return true; }
		if (key == "structure")     { m.structure   = value; return true; }
		if (key == "space_group")   { m.space_group = value; return true; }
		if (key == "lattice")       { m.lattice     = value; return true; }
		if (key == "phase")         { m.phase       = value; return true; }
		if (key == "basis")         { m.basis       = value; return true; }
		if (key == "cell")          { m.cell        = value; return true; }
		return false;
	}

	// ── simulation.* ──────────────────────────────────────────────────────
	if (ns == "simulation") {
		auto& s = doc.simulation;
		if (key == "fire_max_steps")    { s.fire_max_steps    = parse_int_val(value);    return true; }
		if (key == "fire_dt_fs")        { s.fire_dt_fs        = parse_double_val(value); return true; }
		if (key == "box_size_ang")      { s.box_size_ang      = parse_double_val(value); return true; }
		if (key == "periodic")          { s.periodic          = parse_bool_val(value);   return true; }
		if (key == "use_ewald")         { s.use_ewald         = parse_bool_val(value);   return true; }
		if (key == "step_delay_ms")     { s.step_delay_ms     = parse_int_val(value);    return true; }
		return false;
	}

	// ── pbc.* ─────────────────────────────────────────────────────────────
	if (ns == "pbc") {
		auto& p = doc.pbc;
		if (key == "minimum_image")     { p.minimum_image     = parse_bool_val(value);   return true; }
		if (key == "track_images")      { p.track_images      = parse_bool_val(value);   return true; }
		if (key == "unwrap_for_diffusion"){ p.unwrap_for_diffusion = parse_bool_val(value); return true; }
		return false;
	}

	// ── cell.* ────────────────────────────────────────────────────────────
	if (ns == "cell") {
		auto& c = doc.cell;
		if (key == "lx")    { c.lx = parse_double_val(value); return true; }
		if (key == "ly")    { c.ly = parse_double_val(value); return true; }
		if (key == "lz")    { c.lz = parse_double_val(value); return true; }
		if (key == "type")  { c.type = value;                 return true; }
		return false;
	}

	// ── observe.* ─────────────────────────────────────────────────────────
	if (ns == "observe") {
		auto& o = doc.observe;
		if (key == "every_n_steps")     { o.every_n_steps     = parse_int_val(value);    return true; }
		if (key == "output_format")     { o.output_format     = value;                   return true; }
		return false;
	}

	// ── analysis.structure.* ─────────────────────────────────────────────
	if (ns == "analysis" && parts.size() >= 3 && parts[1] == "structure") {
		auto& st = doc.pipeline_structure;
		if (key == "enabled")              { st.enabled              = parse_bool_val(value);   return true; }
		if (key == "compute_coordination") { st.compute_coordination  = parse_bool_val(value);   return true; }
		if (key == "compute_packing")      { st.compute_packing       = parse_bool_val(value);   return true; }
		if (key == "compute_displacement") { st.compute_displacement  = parse_bool_val(value);   return true; }
		if (key == "neighbor_cutoff_A")    { st.neighbor_cutoff_A     = parse_double_val(value);  return true; }
		if (key == "contact_cutoff_A")     { st.contact_cutoff_A      = parse_double_val(value);  return true; }
		return false;
	}

	// ── analysis.sampling.* ──────────────────────────────────────────────
	if (ns == "analysis" && parts.size() >= 3 && parts[1] == "sampling") {
		auto& sa = doc.pipeline_sampling;
		if (key == "enabled")               { sa.enabled                = parse_bool_val(value);   return true; }
		if (key == "compute_rdf")           { sa.compute_rdf            = parse_bool_val(value);   return true; }
		if (key == "compute_msd")           { sa.compute_msd            = parse_bool_val(value);   return true; }
		if (key == "min_frames_for_motion") { sa.min_frames_for_motion  = parse_int_val(value);    return true; }
		if (key == "unwrap_pbc")            { sa.unwrap_pbc             = parse_bool_val(value);   return true; }
		return false;
	}

	// ── analysis.scale_sampling.* ────────────────────────────────────────
	if (ns == "analysis" && parts.size() >= 3 && parts[1] == "scale_sampling") {
		auto& ss = doc.pipeline_scale_sampling;
		if (key == "enabled")                    { ss.enabled                    = parse_bool_val(value);   return true; }
		if (key == "compute_field_projection")   { ss.compute_field_projection   = parse_bool_val(value);   return true; }
		if (key == "compute_rve_sampling")       { ss.compute_rve_sampling       = parse_bool_val(value);   return true; }
		if (key == "spatial_cv_threshold")       { ss.spatial_cv_threshold       = parse_double_val(value);  return true; }
		if (key == "temporal_drift_threshold")   { ss.temporal_drift_threshold   = parse_double_val(value);  return true; }
		return false;
	}

	// ── analysis.inference.* ─────────────────────────────────────────────
	if (ns == "analysis" && parts.size() >= 3 && parts[1] == "inference") {
		auto& inf = doc.pipeline_inference;
		if (key == "enabled")               { inf.enabled                   = parse_bool_val(value);   return true; }
		if (key == "mode")                  { inf.mode                      = value;                   return true; }
		if (key == "infer_packing_regime")  { inf.infer_packing_regime      = parse_bool_val(value);   return true; }
		if (key == "infer_mobility_regime") { inf.infer_mobility_regime     = parse_bool_val(value);   return true; }
		return false;
	}

	return false;
}

// ── BatchMerger::apply_axis_values ────────────────────────────────────────────

VsimDocument BatchMerger::apply_axis_values(
	const VsimDocument&                         base,
	const std::map<std::string, std::string>&   axis_values,
	std::vector<std::string>&                   warnings_out)
{
	VsimDocument doc = base;
	for (const auto& [path, value] : axis_values) {
		if (!set_path(doc, path, value))
			warnings_out.push_back("apply_axis_values: unknown path '" + path + "' — skipped");
	}
	return doc;
}

} // namespace batch
} // namespace vsim
