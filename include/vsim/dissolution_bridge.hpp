#pragma once

#include "vsim/vsim_document.hpp"
#include "atomistic/core/state.hpp"
#include "atomistic/reaction/dissolution.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace vsim {

struct DissolutionPassResult {
	int    sites_identified   = 0;
	int    protonation_events = 0;
	int    dissolution_events = 0;
	int    ligand_exchanges   = 0;
	double net_energy_change  = 0.0;
	double mobile_metal_mol   = 0.0;
	bool   any_cleavage       = false;
};

class DissolutionBridge {
public:
	static DissolutionPassResult run_pass(
		const VsimDocument& doc,
		atomistic::reaction::DissolutionEngine& engine,
		atomistic::State& solid,
		atomistic::State& solution,
		uint64_t frame_id);

	static DissolutionPassResult run_pass(
		const VsimDocument& doc,
		atomistic::State& solid,
		atomistic::State& solution,
		uint64_t frame_id);

	static std::unique_ptr<atomistic::reaction::DissolutionEngine>
	create_engine(const VsimDocument& doc);

	static double extract_pH(const VsimDocument& doc);
	static double extract_temperature(const VsimDocument& doc);
	static std::string format_summary(const DissolutionPassResult& result);
};

} // namespace vsim
