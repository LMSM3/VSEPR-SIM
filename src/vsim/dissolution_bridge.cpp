#include "vsim/dissolution_bridge.hpp"

#include "kernel/kernel_event.hpp"
#include "kernel/kernel_event_log.hpp"

#include <algorithm>
#include <sstream>

namespace vsim {

std::unique_ptr<atomistic::reaction::DissolutionEngine>
DissolutionBridge::create_engine(const VsimDocument& doc) {
	atomistic::reaction::DissolutionConfig cfg;
	const auto& src = doc.dissolution;
	cfg.dG_first_protonation = src.dG_first_protonation;
	cfg.dG_second_protonation = src.dG_second_protonation;
	cfg.Ea_bridging_cleavage = src.Ea_bridging_cleavage;
	cfg.Ea_terminal_release = src.Ea_terminal_release;
	cfg.dG_hydration_Fe3 = src.dG_hydration_Fe3;
	cfg.dG_hydration_Fe2 = src.dG_hydration_Fe2;
	cfg.dG_hydration_Al3 = src.dG_hydration_Al3;
	cfg.dG_hydration_generic = src.dG_hydration_generic;
	cfg.dG_sulfate_mono = src.dG_sulfate_mono;
	cfg.dG_sulfate_bi = src.dG_sulfate_bi;
	cfg.dG_sulfate_bridge = src.dG_sulfate_bridge;
	cfg.pH_reference = src.pH_reference;
	cfg.pH_slope = src.pH_slope;
	cfg.T_reference = src.T_reference;
	cfg.Ea_apparent = src.Ea_apparent;
	cfg.site_density_per_nm2 = src.site_density_per_nm2;
	return std::make_unique<atomistic::reaction::DissolutionEngine>(cfg);
}

double DissolutionBridge::extract_pH(const VsimDocument& doc) {
	return doc.dissolution.pH_reference;
}

double DissolutionBridge::extract_temperature(const VsimDocument& doc) {
	if (doc.environment.temperature > 0.0) return doc.environment.temperature;
	if (doc.run.temperature_K > 0.0) return doc.run.temperature_K;
	return doc.dissolution.T_reference;
}

DissolutionPassResult DissolutionBridge::run_pass(
	const VsimDocument& doc,
	atomistic::reaction::DissolutionEngine& engine,
	atomistic::State& solid,
	atomistic::State& solution,
	uint64_t frame_id)
{
	DissolutionPassResult result;
	if (!doc.dissolution.enabled || solid.N == 0) return result;

	if (engine.surface_site_count() == 0)
		engine.identify_surface_sites(solid);
	const auto before = engine.stats();
	engine.step_dissolution(
		solid, solution, extract_pH(doc), extract_temperature(doc),
		doc.run.dt_fs > 0.0 ? doc.run.dt_fs : 1.0, frame_id);
	const auto after = engine.stats();

	result.sites_identified = static_cast<int>(engine.surface_site_count());
	result.protonation_events = static_cast<int>(
		after.total_protonation_events - before.total_protonation_events);
	result.dissolution_events = static_cast<int>(
		after.total_dissolution_events - before.total_dissolution_events);
	result.ligand_exchanges = static_cast<int>(
		after.total_ligand_exchanges - before.total_ligand_exchanges);
	result.net_energy_change =
		after.net_dissolution_energy - before.net_dissolution_energy;
	result.any_cleavage = result.dissolution_events > 0;

	vsepr::kernel::ChemicalStateEvent event;
	event.frame_id = frame_id;
	event.source_formula = doc.material.formula;
	event.state_tag_before = "surface_sites_identified";
	event.state_tag_after = result.any_cleavage
		? "dissolution_cleavage_observed"
		: "dissolution_surface_evaluated";
	event.coordination_before = static_cast<double>(result.sites_identified);
	event.coordination_after = static_cast<double>(
		std::max(0, result.sites_identified - result.dissolution_events));
	event.local_energy_before = 0.0;
	event.local_energy_after = result.net_energy_change;
	event.compute();
	event.result_unit = "kcal/mol";
	event.is_valid = result.sites_identified > 0;
	event.equation_symbolic = "dissolution.surface_pass";
	event.equation_numeric = format_summary(result);
	if (result.sites_identified == 0)
		event.warning = "no surface oxygen sites inferred from the constructed state";
	vsepr::kernel::KernelEventLog::instance().record(event);

	return result;
}

DissolutionPassResult DissolutionBridge::run_pass(
	const VsimDocument& doc,
	atomistic::State& solid,
	atomistic::State& solution,
	uint64_t frame_id)
{
	auto engine = create_engine(doc);
	return run_pass(doc, *engine, solid, solution, frame_id);
}

std::string DissolutionBridge::format_summary(
	const DissolutionPassResult& result)
{
	std::ostringstream out;
	out << "sites=" << result.sites_identified
		<< " protonation=" << result.protonation_events
		<< " cleavage=" << result.dissolution_events
		<< " ligand_exchange=" << result.ligand_exchanges
		<< " net_energy_kcal_mol=" << result.net_energy_change;
	return out.str();
}

} // namespace vsim
