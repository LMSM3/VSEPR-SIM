// src/vsim/node_accessor.cpp
//
// NodeAccessor — translates NodePath strings into typed row structs by
// querying KernelEventLog::instance().
//
// ContinualReportEvent fields beyond result_value are recovered from the
// equation_numeric string produced by ContinualReportEvent::compute():
//
//   "U=<energy> T=<T_K> eta=<eta> coord=<coord> RMSD=<rmsd>"
//
// FormationEvent fields (converged, fire_steps) are recovered from
// equation_numeric:
//
//   "U_total = <E> kcal/mol  [n=<n>, steps=<steps>, eta=<eta>]"
// and is_valid = converged.
//
// This is brittle but matches the current design where the log slices
// all events to KernelEvent. If the log gains polymorphic storage in a
// future WO, this file is the only place that needs to change.
//
// WO-VSIM-VIS-OVERHAUL-01

#include "vsim/node_accessor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_set>

namespace vsim {

// ---------------------------------------------------------------------------
// Internal string-parsing helpers (equation_numeric recovery)
// ---------------------------------------------------------------------------

namespace {

// Extract a float immediately after "KEY=" in the string.
// Returns default_val if key is not found.
float extract_kv(const std::string& s, const std::string& key, float default_val = 0.f) {
	auto pos = s.find(key + "=");
	if (pos == std::string::npos) return default_val;
	pos += key.size() + 1;
	return static_cast<float>(std::atof(s.c_str() + pos));
}

// Parse ContinualReportEvent::equation_numeric into component fields.
struct ParsedContinual {
	float total_energy     = 0.f;
	float temperature_K    = 0.f;
	float packing_fraction = 0.f;
	float mean_coord_num   = 0.f;
	float rmsd_ang         = 0.f;
};

ParsedContinual parse_continual(const std::string& eq_num) {
	ParsedContinual p;
	p.total_energy      = extract_kv(eq_num, "U");
	p.temperature_K     = extract_kv(eq_num, "T");
	p.packing_fraction  = extract_kv(eq_num, "eta");
	p.mean_coord_num    = extract_kv(eq_num, "coord");
	p.rmsd_ang          = extract_kv(eq_num, "RMSD");
	return p;
}

// Parse FormationEvent::equation_numeric: extract is_valid from base.is_valid,
// fire_steps from "[..., steps=N, ...]".
int parse_fire_steps(const std::string& eq_num) {
	auto pos = eq_num.find("steps=");
	if (pos == std::string::npos) return 0;
	pos += 6;
	return static_cast<int>(std::atoi(eq_num.c_str() + pos));
}

// Clamp to [0, 1]
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

// Linear rescale [lo, hi] -> [0, 1]; returns 0.5 if range is degenerate.
float normalise(float v, float lo, float hi) {
	if (hi <= lo) return 0.5f;
	return clamp01((v - lo) / (hi - lo));
}

} // namespace

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::vector<vsepr::kernel::ContinualReportEvent>
NodeAccessor::collect_continual_events() const {
	using namespace vsepr::kernel;
	auto raw = KernelEventLog::instance()
				   .filter_by_kind(KernelEventKind::ContinualReport);

	std::vector<ContinualReportEvent> out;
	out.reserve(raw.size());

	for (const auto& ev : raw) {
		if (!formula_filter_.empty() && ev.source_formula != formula_filter_)
			continue;

		ContinualReportEvent r;
		// Recover base fields
		r.event_id    = ev.event_id;
		r.frame_id    = ev.frame_id;
		r.source_formula = ev.source_formula;
		r.is_valid    = ev.is_valid;
		r.warning     = ev.warning;
		r.result_value = ev.result_value;
		r.result_unit  = ev.result_unit;
		r.equation_numeric = ev.equation_numeric;

		// Recover derived fields from equation_numeric
		auto p = parse_continual(ev.equation_numeric);
		r.total_energy      = p.total_energy;
		r.temperature_K     = p.temperature_K;
		r.packing_fraction  = p.packing_fraction;
		r.mean_coord_num    = p.mean_coord_num;
		r.rmsd_ang          = p.rmsd_ang;

		out.push_back(r);
	}

	// Sort by frame_id ascending
	std::sort(out.begin(), out.end(),
		[](const ContinualReportEvent& a, const ContinualReportEvent& b) {
			return a.frame_id < b.frame_id;
		});
	return out;
}

std::vector<vsepr::kernel::FormationEvent>
NodeAccessor::collect_formation_events() const {
	using namespace vsepr::kernel;
	auto raw = KernelEventLog::instance()
				   .filter_by_kind(KernelEventKind::Formation);

	std::vector<FormationEvent> out;
	out.reserve(raw.size());
	for (const auto& ev : raw) {
		if (!formula_filter_.empty() && ev.source_formula != formula_filter_)
			continue;

		FormationEvent f;
		f.event_id    = ev.event_id;
		f.frame_id    = ev.frame_id;
		f.source_formula = ev.source_formula;
		f.is_valid    = ev.is_valid;   // is_valid == converged
		f.converged   = ev.is_valid;
		f.final_energy = ev.result_value;
		f.fire_steps  = parse_fire_steps(ev.equation_numeric);
		out.push_back(f);
	}

	std::sort(out.begin(), out.end(),
		[](const FormationEvent& a, const FormationEvent& b) {
			return a.frame_id < b.frame_id;
		});
	return out;
}

// ---------------------------------------------------------------------------
// has_data
// ---------------------------------------------------------------------------

bool NodeAccessor::has_data() const {
	return !collect_continual_events().empty();
}

// ---------------------------------------------------------------------------
// helix_rows — "run.history"
// ---------------------------------------------------------------------------

std::vector<HelixRow> NodeAccessor::helix_rows(int boundary_every) const {
	auto continual = collect_continual_events();
	auto formation = collect_formation_events();

	if (continual.empty()) return {};

	// Build set of frame_ids where a FormationEvent has converged==true,
	// looking for is_valid flips relative to the previous formation event.
	std::unordered_set<uint64_t> regime_frames;
	{
		bool prev_converged = false;
		for (const auto& f : formation) {
			if (f.converged && !prev_converged)
				regime_frames.insert(f.frame_id);
			prev_converged = f.converged;
		}
	}

	// Compute T_K range for normalisation
	float t_min = static_cast<float>(continual.front().temperature_K);
	float t_max = t_min;
	for (const auto& r : continual) {
		const float tk = static_cast<float>(r.temperature_K);
		t_min = std::min(t_min, tk);
		t_max = std::max(t_max, tk);
	}

	std::vector<HelixRow> rows;
	rows.reserve(continual.size());

	for (const auto& cr : continual) {
		HelixRow hr;
		hr.frame_id      = cr.frame_id;
		hr.temperature_K = static_cast<float>(cr.temperature_K);
		hr.t_c_norm      = normalise(static_cast<float>(cr.temperature_K), t_min, t_max);
		// Severity: packing_fraction if non-zero, else fall back to |t_c_norm - 0.5| * 2
		hr.severity      = (cr.packing_fraction > 0.0)
						   ? clamp01(static_cast<float>(cr.packing_fraction))
						   : clamp01(std::abs(hr.t_c_norm - 0.5f) * 2.f);
		hr.coord_num         = static_cast<float>(cr.mean_coord_num);
		hr.regime_transition = (regime_frames.count(cr.frame_id) > 0);
		hr.loop_boundary     = (boundary_every > 0 &&
								cr.frame_id > 0 &&
								cr.frame_id % static_cast<uint64_t>(boundary_every) == 0);
		rows.push_back(hr);
	}
	return rows;
}

// ---------------------------------------------------------------------------
// heat_rows — "room.solver"
// ---------------------------------------------------------------------------

std::vector<HeatRow> NodeAccessor::heat_rows(int subsample_cap) const {
	auto continual = collect_continual_events();
	if (continual.empty()) return {};

	// Cap
	if (static_cast<int>(continual.size()) > subsample_cap)
		continual.resize(static_cast<std::size_t>(subsample_cap));

	// Compute T_K range for density normalisation
	float t_min = static_cast<float>(continual.front().temperature_K);
	float t_max = t_min;
	for (const auto& r : continual) {
		const float tk = static_cast<float>(r.temperature_K);
		t_min = std::min(t_min, tk);
		t_max = std::max(t_max, tk);
	}

	// Map each record to a 2D grid position:
	//   x = normalised frame_id (time axis, 0..1)
	//   y = normalised packing_fraction (spatial axis, 0..1)
	// density = normalised temperature_K
	const uint64_t frame_max = continual.back().frame_id;
	const uint64_t frame_min = continual.front().frame_id;

	std::vector<HeatRow> rows;
	rows.reserve(continual.size());

	for (const auto& cr : continual) {
		HeatRow hr;
		hr.x       = normalise(static_cast<float>(cr.frame_id),
							   static_cast<float>(frame_min),
							   static_cast<float>(frame_max));
		hr.y       = clamp01(static_cast<float>(cr.packing_fraction));
		hr.density = normalise(static_cast<float>(cr.temperature_K), t_min, t_max);
		hr.obstacle = false;
		rows.push_back(hr);
	}
	return rows;
}

// ---------------------------------------------------------------------------
// scalar_summary — data.scalar.panel
// ---------------------------------------------------------------------------

std::vector<std::pair<std::string, std::string>>
NodeAccessor::scalar_summary() const {
	auto continual = collect_continual_events();
	auto formation = collect_formation_events();

	std::vector<std::pair<std::string, std::string>> kv;

	kv.push_back({"Events (continual)", std::to_string(continual.size())});
	kv.push_back({"Events (formation)", std::to_string(formation.size())});

	if (!continual.empty()) {
		const auto& last = continual.back();
		// Round to 3 decimal places
		auto fmt = [](double v) {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.3f", v);
			return std::string(buf);
		};
		kv.push_back({"Frame (last)",     std::to_string(last.frame_id)});
		kv.push_back({"Energy (last)",    fmt(last.total_energy)    + " kcal/mol"});
		kv.push_back({"Temperature (last)", fmt(last.temperature_K) + " K"});
		kv.push_back({"η packing (last)",   fmt(last.packing_fraction)});
		kv.push_back({"Coord (last)",     fmt(last.mean_coord_num)});
		kv.push_back({"RMSD (last)",      fmt(last.rmsd_ang)        + " Å"});
	}

	if (!formation.empty()) {
		int converged = 0;
		for (const auto& f : formation)
			if (f.converged) ++converged;
		kv.push_back({"FIRE converged", std::to_string(converged) +
					  " / " + std::to_string(formation.size())});
	}

	return kv;
}

} // namespace vsim
