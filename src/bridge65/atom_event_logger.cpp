/**
 * atom_event_logger.cpp
 * =====================
 * WO-BRIDGE-65 per-atom event logger implementation.
 *
 * Output format reference:
 *
 *   Console (normal):
 *     [ATOM_EVENT] step=1240 t=1.240e-12 id=42 sym=Fe event=collision
 *       E_k=2.14e-2 eV E_u=-4.81e-1 eV dE=8.20e-3 eV
 *       |F|=1.92e+1 eV/A partner=77 r=2.18 A gate=pass
 *
 *   Console (limiter active):
 *     [EVENT_LIMITER] step=18400 rate=91200 events/s mode=sampled
 *       total_events=4812 sampled_console=25
 *       collisions=4301 bond_created=82 bond_broken=19
 *       energy_anomaly=3 force_spike=7
 *       full_jsonl=true aggregate_tsv=true
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include "vsim/bridge65/atom_event_logger.hpp"
#include <cstdio>
#include <filesystem>
#include <sstream>

namespace vsim::bridge65 {

// ============================================================================
// Construction / destruction
// ============================================================================

AtomEventLogger::AtomEventLogger(AtomEventLoggerConfig cfg)
	: cfg_(std::move(cfg)), limiter_(cfg_.limiter)
{
	if (!cfg_.enabled) return;

	namespace fs = std::filesystem;

	if (cfg_.jsonl) {
		fs::create_directories(fs::path(cfg_.jsonl_path).parent_path());
		jsonl_.open(cfg_.jsonl_path, std::ios::out | std::ios::trunc);
	}

	if (cfg_.jsonl) { // TSV sits next to the JSONL
		fs::create_directories(fs::path(cfg_.tsv_path).parent_path());
		tsv_.open(cfg_.tsv_path, std::ios::out | std::ios::trunc);
	}
}

AtomEventLogger::~AtomEventLogger() {
	flush();
}

// ============================================================================
// Public interface
// ============================================================================

void AtomEventLogger::begin_step(std::uint64_t step, double time_s) {
	if (!cfg_.enabled) return;
	current_step_   = step;
	current_time_s_ = time_s;
	limiter_.reset_step();
	limiter_.begin_window();
}

void AtomEventLogger::log(const AtomEvent& ev) {
	if (!cfg_.enabled) return;
	if (!accept_event(ev)) return;

	limiter_.observe(ev);

	if (cfg_.jsonl && jsonl_.is_open()) write_jsonl(ev);
	if (cfg_.console && limiter_.should_print(ev)) print_console(ev);
}

void AtomEventLogger::end_step(std::uint64_t step, double time_s) {
	if (!cfg_.enabled) return;
	limiter_.end_window(step, time_s);
	const auto st = limiter_.state();

	if (cfg_.jsonl && tsv_.is_open()) write_tsv_row(step, time_s, st);
	if (cfg_.console && st.active && cfg_.limiter.aggregate_when_limited) {
		print_limiter_summary(step, st);
	}
}

void AtomEventLogger::flush() {
	if (jsonl_.is_open()) jsonl_.flush();
	if (tsv_.is_open())   tsv_.flush();
}

// ============================================================================
// Event routing gate
// ============================================================================

bool AtomEventLogger::accept_event(const AtomEvent& ev) const {
	switch (ev.type) {
		case AtomEventType::EnergyUpdate:  return cfg_.per_atom_energy;
		case AtomEventType::Collision:     return cfg_.collision_events;
		case AtomEventType::BondCreated:   return cfg_.bond_events;
		case AtomEventType::BondBroken:    return cfg_.unbond_events;
		case AtomEventType::BondStretched: return cfg_.bond_events;
		case AtomEventType::ForceSpike:    return cfg_.force_spike_events;
		case AtomEventType::ThermalSpike:  return cfg_.thermal_spike_events;
		case AtomEventType::EnergyAnomaly: return cfg_.energy_anomaly_events;
	}
	return false;
}

// ============================================================================
// Console output
// ============================================================================

void AtomEventLogger::print_console(const AtomEvent& ev) {
	const char* etype = atom_event_type_name(ev.type);

	std::fprintf(stdout,
		"[ATOM_EVENT] step=%llu t=%.3e id=%d sym=%s event=%s\n"
		"  E_k=%.2e eV E_u=%.2e eV dE=%.2e eV\n"
		"  |F|=%.2e eV/A",
		static_cast<unsigned long long>(ev.step),
		ev.time_s,
		ev.atom_id,
		ev.symbol.c_str(),
		etype,
		ev.energy.kinetic_eV,
		ev.energy.potential_local_eV,
		ev.energy.delta_eV,
		ev.force.magnitude_eV_A);

	if (!ev.partner_ids.empty()) {
		std::fprintf(stdout, " partner=%d", ev.partner_ids[0]);
		if (!ev.sym_partner.empty())
			std::fprintf(stdout, " sym_partner=%s", ev.sym_partner.c_str());
		if (ev.partner_distance_A > 0.0)
			std::fprintf(stdout, " r=%.3f A", ev.partner_distance_A);
	}

	if (!ev.reason.empty())
		std::fprintf(stdout, " reason=%s", ev.reason.c_str());

	if (ev.confidence > 0.0)
		std::fprintf(stdout, " confidence=%.3f", ev.confidence);

	std::fprintf(stdout, " gate=%s\n", ev.state_hash.empty() ? "pending" : "pass");
}

void AtomEventLogger::print_limiter_summary(std::uint64_t step,
											 const EventLimiterState& st)
{
	std::fprintf(stdout,
		"[EVENT_LIMITER] step=%llu rate=%.0f events/s mode=sampled\n"
		"  total_events=%zu sampled_console=%zu\n"
		"  collisions=%zu bond_created=%zu bond_broken=%zu"
		" energy_anomaly=%zu force_spike=%zu\n"
		"  full_jsonl=%s aggregate_tsv=%s\n",
		static_cast<unsigned long long>(step),
		st.event_rate,
		st.total_events,
		st.printed_events,
		st.collisions,
		st.bond_created,
		st.bond_broken,
		st.energy_anomaly,
		st.force_spike,
		(cfg_.limiter.write_full_jsonl_when_limited ? "true" : "false"),
		(cfg_.limiter.aggregate_when_limited ? "true" : "false"));
}

// ============================================================================
// JSONL output
// ============================================================================

void AtomEventLogger::write_jsonl(const AtomEvent& ev) {
	// Build partner_ids array string
	std::string partner_arr = "[";
	for (std::size_t i = 0; i < ev.partner_ids.size(); ++i) {
		if (i) partner_arr += ',';
		partner_arr += std::to_string(ev.partner_ids[i]);
	}
	partner_arr += ']';

	jsonl_ << "{"
		<< "\"schema\":\"vsepr.atom_event.v1\","
		<< "\"version\":\"5.0.14\","
		<< "\"run_id\":\"" << cfg_.run_id << "\","
		<< "\"step\":" << ev.step << ","
		<< "\"time_s\":" << ev.time_s << ","
		<< "\"atom\":{"
			<< "\"id\":" << ev.atom_id << ","
			<< "\"symbol\":\"" << ev.symbol << "\","
			<< "\"class\":\"atom\""
		<< "},"
		<< "\"event\":{"
			<< "\"type\":\"" << atom_event_type_name(ev.type) << "\","
			<< "\"partner_ids\":" << partner_arr << ","
			<< "\"reason\":\"" << ev.reason << "\","
			<< "\"confidence\":" << ev.confidence
		<< "},"
		<< "\"energy\":{"
			<< "\"kinetic_eV\":" << ev.energy.kinetic_eV << ","
			<< "\"potential_local_eV\":" << ev.energy.potential_local_eV << ","
			<< "\"total_local_eV\":" << ev.energy.total_local_eV << ","
			<< "\"delta_eV\":" << ev.energy.delta_eV
		<< "},"
		<< "\"force\":{"
			<< "\"fx_eV_A\":" << ev.force.fx_eV_A << ","
			<< "\"fy_eV_A\":" << ev.force.fy_eV_A << ","
			<< "\"fz_eV_A\":" << ev.force.fz_eV_A << ","
			<< "\"magnitude_eV_A\":" << ev.force.magnitude_eV_A
		<< "},"
		<< "\"geometry\":{"
			<< "\"partner_distance_A\":" << ev.partner_distance_A << ","
			<< "\"cutoff_A\":" << ev.cutoff_A
		<< "},"
		<< "\"hash\":{"
			<< "\"state_hash\":\"" << ev.state_hash << "\","
			<< "\"event_hash\":\"" << ev.event_hash << "\""
		<< "}"
		<< "}\n";
}

// ============================================================================
// TSV output
// ============================================================================

void AtomEventLogger::write_tsv_row(std::uint64_t step, double time_s,
									 const EventLimiterState& st)
{
	if (!tsv_header_written_) {
		tsv_ << "step\ttime_s\ttotal_events\tcollisions\tbond_created"
				"\tbond_broken\tenergy_anomaly\tforce_spike"
				"\tmean_dE_eV\tmax_dE_eV\tmean_force_eV_A\tmax_force_eV_A"
				"\tlimiter_active\n";
		tsv_header_written_ = true;
	}

	tsv_ << step << '\t' << time_s << '\t'
		 << st.total_events  << '\t'
		 << st.collisions    << '\t'
		 << st.bond_created  << '\t'
		 << st.bond_broken   << '\t'
		 << st.energy_anomaly << '\t'
		 << st.force_spike   << '\t'
		 << st.mean_dE_eV    << '\t'
		 << st.max_dE_eV     << '\t'
		 << st.mean_force_eV_A << '\t'
		 << st.max_force_eV_A  << '\t'
		 << (st.active ? "true" : "false") << '\n';
}

} // namespace vsim::bridge65
